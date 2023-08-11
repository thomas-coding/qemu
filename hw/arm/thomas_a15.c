/*
 * ARM V2M THOMAS board emulation.
 *
 * Copyright (c) 2017 Linaro Limited
 * Written by Peter Maydell
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 or
 *  (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/cutils.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/arm/boot.h"
#include "hw/arm/armv7m.h"
#include "hw/or-irq.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/misc/unimp.h"
#include "hw/char/cmsdk-apb-uart.h"
#include "hw/timer/cmsdk-apb-timer.h"
#include "hw/timer/cmsdk-apb-dualtimer.h"
#include "hw/ssi/pl022.h"
#include "hw/i2c/arm_sbcon_i2c.h"
#include "hw/net/lan9118.h"
#include "net/net.h"
#include "hw/watchdog/cmsdk-apb-watchdog.h"
#include "hw/qdev-clock.h"
#include "qom/object.h"
#include "hw/char/thomas_test_device.h"
#include "exec/hwaddr.h"
#include "qemu/notify.h"
#include "hw/boards.h"
#include "hw/arm/boot.h"
#include "hw/block/flash.h"
#include "sysemu/kvm.h"
#include "hw/intc/arm_gicv3_common.h"
#include "qom/object.h"
#include "hw/char/pl011.h"
#include "hw/intc/arm_gic.h"
#include "hw/intc/arm_gicv3_common.h"
#include "hw/irq.h"
#include "hw/char/thomas_spinlock.h"

enum {
    THOMAS_A15_FLASH,
    THOMAS_A15_SRAM,
    THOMAS_A15_DDR,
    THOMAS_A15_UART,
    THOMAS_A15_SPINLOCK,
    THOMAS_A15_TEST_DEVICE,
    THOMAS_A15_GIC_DIST,
    THOMAS_A15_GIC_REDIST,
};

/* Thomas-a15 memory map
 *
 *  0x00000000 .. 0x01ffffff : Flash(32M)
 *  0x10000000 .. 0x11ffffff : SRAM(32M)
 *  0x20000000 .. 0x2fffffff : DDR(256M)
 *  0x40000000 .. 0x40000fff : UART(4K)
 *  0x50000000 .. 0x50000fff : Thomas Test Device(4K)
 *  0x60000000 .. 0x6000ffff : GIC Distributor(64K)
 *  0x61000000 .. 0x6001ffff : GIC Redistributor(128K)
 */
static const MemMapEntry base_memmap[] = {
    [THOMAS_A15_FLASH] =         { 0x00000000, 0x02000000 },
    [THOMAS_A15_SRAM] =          { 0x10000000, 0x02000000 },
    [THOMAS_A15_DDR] =           { 0x20000000, 0x10000000 },
    [THOMAS_A15_UART] =          { 0x40000000, 0x00001000 },
    [THOMAS_A15_SPINLOCK] =      { 0x41000000, 0x00001000 },
    [THOMAS_A15_TEST_DEVICE] =   { 0x50000000, 0x00001000 },
    [THOMAS_A15_GIC_DIST] =      { 0x60000000, 0x00010000 },
    /* This redistributor space allows up to 2*64kB*123 CPUs */
    [THOMAS_A15_GIC_REDIST] =    { 0x61000000, 0x00F60000 },
};

/* Number of external interrupt lines to configure the GIC with */
#define NUM_IRQS 256

#define THOMAS_TIMER_VIRT_IRQ   11
#define THOMAS_TIMER_S_EL1_IRQ  13
#define THOMAS_TIMER_NS_EL1_IRQ 14
#define THOMAS_TIMER_NS_EL2_IRQ 10

#define THOMAS_GIC_MAINT_IRQ  9

#define THOMAS_VIRTUAL_PMU_IRQ 7

#define THOMAS_A15_TEST_DEVICE_IRQ    (32 + 100)
#define THOMAS_A15_PL011_IRQ    (32 + 101)

struct THOMASA15MachineClass {
    MachineClass parent;
};

struct THOMASA15MachineState {
    MachineState parent;

    ARMCPU cpu[2];

    MemoryRegion *flash;
    MemoryRegion *sram;
    MemoryRegion *ddr;

    DeviceState *gic;

    struct arm_boot_info bootinfo;
};

#define TYPE_THOMAS_A15_MACHINE MACHINE_TYPE_NAME("thomas-a15")

OBJECT_DECLARE_TYPE(THOMASA15MachineState, THOMASA15MachineClass, THOMAS_A15_MACHINE)

static void create_gicv3(THOMASA15MachineState *mms)
{
    SysBusDevice *gicbusdev;
    MachineState *ms = MACHINE(mms);
    int i;

    unsigned int smp_cpus = ms->smp.cpus;

    mms->gic = qdev_new("arm-gicv3");

    qdev_prop_set_uint32(mms->gic, "revision", 3);
    qdev_prop_set_uint32(mms->gic, "num-cpu", smp_cpus);

    /* Note that the num-irq property counts both internal and external
     * interrupts; there are always 32 of the former (mandated by GIC spec).
     */
    qdev_prop_set_uint32(mms->gic, "num-irq", NUM_IRQS + 32);

    qdev_prop_set_bit(mms->gic, "has-security-extensions", true);

    /* Get redist count*/
    uint32_t redist0_capacity = base_memmap[THOMAS_A15_GIC_REDIST].size / GICV3_REDIST_SIZE;
    uint32_t redist0_count = MIN(smp_cpus, redist0_capacity);

    /* One core with one gicr */
    qdev_prop_set_uint32(mms->gic, "len-redist-region-count", 1);
    qdev_prop_set_uint32(mms->gic, "redist-region-count[0]", redist0_count);

    gicbusdev = SYS_BUS_DEVICE(mms->gic);
    sysbus_realize_and_unref(gicbusdev, &error_fatal);

    /* Memory map for GICD and GICR, MR is defined in qemu/hw/intc/arm_gicv3_common.c */
    sysbus_mmio_map(gicbusdev, 0, base_memmap[THOMAS_A15_GIC_DIST].base);
    sysbus_mmio_map(gicbusdev, 1, base_memmap[THOMAS_A15_GIC_REDIST].base);

    for(i = 0; i < smp_cpus; i++) {
        DeviceState *cpudev = DEVICE(qemu_get_cpu(i));
        int ppibase = NUM_IRQS + i * GIC_INTERNAL + GIC_NR_SGIS;
        int irq;

        const int timer_irq[] = {
            [GTIMER_PHYS] = THOMAS_TIMER_NS_EL1_IRQ,
            [GTIMER_VIRT] = THOMAS_TIMER_VIRT_IRQ,
            [GTIMER_HYP]  = THOMAS_TIMER_NS_EL2_IRQ,
            [GTIMER_SEC]  = THOMAS_TIMER_S_EL1_IRQ,
        };

        for (irq = 0; irq < ARRAY_SIZE(timer_irq); irq++) {
            qdev_connect_gpio_out(cpudev, irq, qdev_get_gpio_in(mms->gic, ppibase + timer_irq[irq]));
        }

        qemu_irq m_irq = qdev_get_gpio_in(mms->gic, ppibase + THOMAS_GIC_MAINT_IRQ);
        qdev_connect_gpio_out_named(cpudev, "gicv3-maintenance-interrupt", 0, m_irq);

        /* Connect cpu and gic */
        sysbus_connect_irq(gicbusdev, i, qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
        sysbus_connect_irq(gicbusdev,  i + smp_cpus, qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
    }
}

static void thomas_a15_common_init(MachineState *machine)
{
    THOMASA15MachineState *mms = THOMAS_A15_MACHINE(machine);
    unsigned int smp_cpus = machine->smp.cpus;
    int n;

    /* Flash */
    mms->flash = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->flash, NULL, "thomas_a15.flash", base_memmap[THOMAS_A15_FLASH].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_A15_FLASH].base, mms->flash);

    /* Sram */
    mms->sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->sram, NULL, "thomas_a15.sram", base_memmap[THOMAS_A15_SRAM].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_A15_SRAM].base, mms->sram);

    /* DDR */
    mms->sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->sram, NULL, "thomas_a15.ddr", base_memmap[THOMAS_A15_DDR].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_A15_DDR].base, mms->sram);

    /* Create the actual CPUs */
    for (n = 0; n < smp_cpus; n++) {
        /* Init arm cpu object */
        object_initialize_child(OBJECT(mms), "cpu[*]", &mms->cpu[n], ARM_CPU_TYPE_NAME("cortex-a15"));

        /* All exception levels required */
        qdev_prop_set_bit(DEVICE(&mms->cpu[n]), "has_el3", true);
        qdev_prop_set_bit(DEVICE(&mms->cpu[n]), "has_el2", true);

        /* Mark realized */
        qdev_realize(DEVICE(&mms->cpu[n]), NULL, &error_fatal);
    }

    create_gicv3(mms);

    /* Uart */
    qemu_irq pl011_device_irq = qdev_get_gpio_in(mms->gic, THOMAS_A15_PL011_IRQ);
    pl011_create( base_memmap[THOMAS_A15_UART].base, pl011_device_irq, serial_hd(0));

    /* Test device, its GPIO OUT connect to GICV3 GPIO IN */
    qemu_irq test_device_irq = qdev_get_gpio_in(mms->gic, THOMAS_A15_TEST_DEVICE_IRQ);
    thomas_test_device_create(base_memmap[THOMAS_A15_TEST_DEVICE].base, test_device_irq);

    /* Create spinlock for smp */
    thomas_spinlock_create(base_memmap[THOMAS_A15_SPINLOCK].base);

    fprintf(stderr, "[wjp] thomas_common_init kernel name: %s \n", machine->kernel_filename);

    mms->bootinfo.ram_size = 256 * MiB;//machine->ram_size;
    mms->bootinfo.board_id = -1;
    //mms->bootinfo.nb_cpus = machine->smp.cpus;
    mms->bootinfo.loader_start = base_memmap[THOMAS_A15_FLASH].base;
    mms->bootinfo.smp_loader_start = base_memmap[THOMAS_A15_FLASH].base;
    arm_load_kernel(ARM_CPU(first_cpu), machine, &mms->bootinfo);

}

static void thomas_a15_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    fprintf(stderr, "[wjp] thomas_a15_class_init\n");
    mc->init = thomas_a15_common_init;
    mc->max_cpus = 256;
    mc->default_ram_size = 256 * MiB;
    mc->default_ram_id = "thomas.ram";

    mc->desc = "ARM THOMAS for Cortex-A15";
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a15");
}

static void thomas_a15_instance_init(Object *obj)
{
    fprintf(stderr, "[wjp] thomas_a15_instance_init\n");
}

static const TypeInfo thomas_a15_info = {
    .name = MACHINE_TYPE_NAME("thomas-a15"),
    .parent = TYPE_MACHINE,
    .instance_init = thomas_a15_instance_init,
    .instance_size = sizeof(THOMASA15MachineState),
    .class_size = sizeof(THOMASA15MachineClass),
    .class_init = thomas_a15_class_init,
};

static void thomas_a15_machine_init(void)
{
    fprintf(stderr, "[wjp] thomas_a15_machine_init\n");
    type_register_static(&thomas_a15_info);
}

type_init(thomas_a15_machine_init);
