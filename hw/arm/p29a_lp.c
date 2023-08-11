/*
 * ARM P29A_LP board emulation.
 *
 * Copyright (c) 20122 wunekky@gmail.com
 * Written by Jinping Wu
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
#include "hw/misc/dummy/dummy_tzc.h"
#include "hw/misc/dummy/dummy_common.h"
#include "hw/misc/dummy/dummy_p29a_otp.h"
#include "hw/misc/dummy/dummy_p29a_sys_ctrl.h"
#include "hw/char/serial.h"
#include "hw/sd/sdhci.h"

enum {
    P29A_LP_FLASH,
    P29A_LP_SRAM,
    P29A_LP_DDR,
    P29A_LP_UART,
    P29A_LP_TEST_DEVICE,
    P29A_LP_TZC_SRAM01,
    P29A_LP_TZC_SRAM23,
    P29A_LP_PMU,
    P29A_LP_LAS_PPU,
    P29A_LP_LAS_CCM,
    P29A_LP_TIMESTAMP,
    P29A_LP_SYS_OTP,
    P29A_LP_UART0,
    P29A_LP_UART1,
    P29A_LP_SDIO,
    P29A_LP_EMMC,
    P29A_LP_SYS_CTRL,
    P29A_LP_GIC_DIST,
    P29A_LP_GIC_CPU,
    P29A_LP_GIC_V2M,
    P29A_LP_GIC_HYP,
    P29A_LP_GIC_VCPU,
    P29A_LP_GIC_ITS,
    P29A_LP_GIC_REDIST,
    P29A_LP_TZC_APS_DDR,
    P29A_LP_TZC_LPS2APS,
    P29A_LP_TZC_LPS2LAS,
    P29A_LP_TZC_LPS2DDR,
    P29A_LP_TZC_LAS2LPS,
    P29A_LP_TZC_AVS_SRAM,
    P29A_LP_TZC_APS2LPS,
    P29A_LP_APS_MAILBOX,
    P29A_LP_LAS_MAILBOX,
    P29A_LP_LPS_MAILBOX,
};

/* P29A-lp memory map
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
    [P29A_LP_FLASH] =         { 0x00000000, 0x02000000 },
    [P29A_LP_SRAM] =          { 0x10000000, 0x02000000 },
    [P29A_LP_DDR] =           { 0x20000000, 0xc0000000 },
    [P29A_LP_LAS_CCM] =       { 0xf1410000, 0x00010000 },
    [P29A_LP_LAS_PPU] =       { 0xf1420000, 0x00010000 },
    [P29A_LP_PMU] =           { 0xf1430000, 0x00010000 },
    [P29A_LP_GIC_DIST] =      { 0xf5000000, 0x00001000 },
    [P29A_LP_GIC_CPU] =       { 0xf5001000, 0x00002000 },
    [P29A_LP_GIC_V2M] =       { 0x08020000, 0x00001000 },
    [P29A_LP_GIC_HYP] =       { 0x08030000, 0x00010000 },
    [P29A_LP_GIC_VCPU] =      { 0x08040000, 0x00010000 },
    [P29A_LP_GIC_ITS] =       { 0x0c040000, 0x00010000 },
    [P29A_LP_GIC_REDIST] =    { 0xf5040000, 0x00f60000 },
    [P29A_LP_EMMC] =          { 0xf5a10000, 0x00010000 },
    [P29A_LP_SDIO] =          { 0xf5a20000, 0x00010000 },
    [P29A_LP_TZC_SRAM01] =    { 0xf5a40000, 0x00010000 },
    [P29A_LP_TZC_SRAM23] =    { 0xf5a50000, 0x00010000 },
    [P29A_LP_SYS_OTP] =       { 0xf5aa0000, 0x00010000 },
    [P29A_LP_TIMESTAMP] =     { 0xf5ab0000, 0x00010000 },
    [P29A_LP_UART0] =         { 0xf5c10000, 0x00010000 },
    [P29A_LP_UART1] =         { 0xf5c20000, 0x00010000 },
    [P29A_LP_SYS_CTRL] =      { 0xf5d10000, 0x00010000 },
    [P29A_LP_TZC_APS_DDR] =   { 0xf8420000, 0x00010000 },
    [P29A_LP_TZC_LPS2APS] =   { 0xf5a80000, 0x00010000 },
    [P29A_LP_TZC_LPS2LAS] =   { 0xf5a70000, 0x00010000 },
    [P29A_LP_TZC_LPS2DDR] =   { 0xf5a60000, 0x00010000 },
    [P29A_LP_TZC_LAS2LPS] =   { 0xf1650000, 0x00010000 },
    [P29A_LP_TZC_AVS_SRAM] =  { 0xf84f0000, 0x00010000 },
    [P29A_LP_TZC_APS2LPS] =   { 0xf84e0000, 0x00010000 },
    [P29A_LP_APS_MAILBOX] =   { 0xf8400000, 0x00010000 },
    [P29A_LP_LAS_MAILBOX] =   { 0xf1040000, 0x00010000 },
    [P29A_LP_LPS_MAILBOX] =   { 0xf5900000, 0x00010000 },
};

/* Number of external interrupt lines to configure the GIC with */
#define NUM_IRQS 256

#define P29A_LP_TIMER_VIRT_IRQ   11
#define P29A_LP_TIMER_S_EL1_IRQ  13
#define P29A_LP_TIMER_NS_EL1_IRQ 14
#define P29A_LP_TIMER_NS_EL2_IRQ 10

#define P29A_LP_GIC_MAINT_IRQ  9

#define P29A_LP_VIRTUAL_PMU_IRQ 7

#define P29A_LP_TEST_DEVICE_IRQ    (32 + 100)
#define P29A_LP_UART1_IRQ    (32 + 104)
#define P29A_LP_SDIO_IRQ    (32 + 43)

#define SDHCI_CAPABILITIES  0x28073ffc1898

struct P29A_LPA15MachineClass {
    MachineClass parent;
};

struct P29A_LPA15MachineState {
    MachineState parent;

    ARMCPU cpu;

    MemoryRegion *flash;
    MemoryRegion *sram;
    MemoryRegion *ddr;

    DeviceState *gic;

    struct arm_boot_info bootinfo;
};

#define TYPE_P29A_LP_MACHINE MACHINE_TYPE_NAME("p29a-lp")

OBJECT_DECLARE_TYPE(P29A_LPA15MachineState, P29A_LPA15MachineClass, P29A_LP_MACHINE)

static void create_gicv3(P29A_LPA15MachineState *mms)
{
    SysBusDevice *gicbusdev;

    mms->gic = qdev_new("arm-gicv3");

    qdev_prop_set_uint32(mms->gic, "revision", 3);
    qdev_prop_set_uint32(mms->gic, "num-cpu", 1);

    /* Note that the num-irq property counts both internal and external
     * interrupts; there are always 32 of the former (mandated by GIC spec).
     */
    qdev_prop_set_uint32(mms->gic, "num-irq", NUM_IRQS + 32);

    qdev_prop_set_bit(mms->gic, "has-security-extensions", true);

    /* One core with one gicr */
    qdev_prop_set_uint32(mms->gic, "len-redist-region-count", 1);
    qdev_prop_set_uint32(mms->gic, "redist-region-count[0]", 1);

    gicbusdev = SYS_BUS_DEVICE(mms->gic);
    sysbus_realize_and_unref(gicbusdev, &error_fatal);

    /* Memory map for GICD and GICR, MR is defined in qemu/hw/intc/arm_gicv3_common.c */
    sysbus_mmio_map(gicbusdev, 0, base_memmap[P29A_LP_GIC_DIST].base);
    sysbus_mmio_map(gicbusdev, 1, base_memmap[P29A_LP_GIC_REDIST].base);

    DeviceState *cpudev = DEVICE(qemu_get_cpu(0));

    int irq;
    int ppibase = NUM_IRQS + GIC_NR_SGIS;
    const int timer_irq[] = {
        [GTIMER_PHYS] = P29A_LP_TIMER_NS_EL1_IRQ,
        [GTIMER_VIRT] = P29A_LP_TIMER_VIRT_IRQ,
        [GTIMER_HYP]  = P29A_LP_TIMER_NS_EL2_IRQ,
        [GTIMER_SEC]  = P29A_LP_TIMER_S_EL1_IRQ,
    };

    for (irq = 0; irq < ARRAY_SIZE(timer_irq); irq++) {
        qdev_connect_gpio_out(cpudev, irq, qdev_get_gpio_in(mms->gic, ppibase + timer_irq[irq]));
    }

    qemu_irq m_irq = qdev_get_gpio_in(mms->gic, ppibase + P29A_LP_GIC_MAINT_IRQ);
    qdev_connect_gpio_out_named(cpudev, "gicv3-maintenance-interrupt", 0, m_irq);

    qdev_connect_gpio_out_named(cpudev, "pmu-interrupt", 0,
                                qdev_get_gpio_in(mms->gic, ppibase + P29A_LP_VIRTUAL_PMU_IRQ));
    /* Connect cpu and gic */
    sysbus_connect_irq(gicbusdev, 0, qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
    sysbus_connect_irq(gicbusdev, 1, qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
}

static void p29a_lp_common_init(MachineState *machine)
{
    P29A_LPA15MachineState *mms = P29A_LP_MACHINE(machine);

    /* Flash */
    mms->flash = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->flash, NULL, "P29A_LP.flash", base_memmap[P29A_LP_FLASH].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[P29A_LP_FLASH].base, mms->flash);

    /* Sram */
    mms->sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->sram, NULL, "P29A_LP.sram", base_memmap[P29A_LP_SRAM].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[P29A_LP_SRAM].base, mms->sram);

    /* DDR */
    mms->sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->sram, NULL, "P29A_LP.ddr", base_memmap[P29A_LP_DDR].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[P29A_LP_DDR].base, mms->sram);

    /* Init arm cpu object */
    object_initialize_child(OBJECT(mms), "cpu", &mms->cpu, ARM_CPU_TYPE_NAME("cortex-a32"));

    /* All exception levels required */
    qdev_prop_set_bit(DEVICE(&mms->cpu), "has_el3", true);
    qdev_prop_set_bit(DEVICE(&mms->cpu), "has_el2", true);

    /* Mark realized */
    qdev_realize(DEVICE(&mms->cpu), NULL, &error_fatal);

    create_gicv3(mms);

    /* Uart */
    qemu_irq lp_uart1_irq = qdev_get_gpio_in(mms->gic, P29A_LP_UART1_IRQ);
    serial_mm_init(get_system_memory(), base_memmap[P29A_LP_UART1].base, 2,
                   lp_uart1_irq, 115200, serial_hd(0),
                   DEVICE_LITTLE_ENDIAN);

    /* Test device, its GPIO OUT connect to GICV3 GPIO IN */
    //qemu_irq test_device_irq = qdev_get_gpio_in(mms->gic, P29A_LP_TEST_DEVICE_IRQ);
    //thomas_test_device_create(base_memmap[P29A_LP_TEST_DEVICE].base, test_device_irq);

    /* dummy device */
    dummy_common_create(base_memmap[P29A_LP_APS_MAILBOX].base, base_memmap[P29A_LP_APS_MAILBOX].size);
    dummy_common_create(base_memmap[P29A_LP_LAS_MAILBOX].base, base_memmap[P29A_LP_LAS_MAILBOX].size);
    dummy_common_create(base_memmap[P29A_LP_LPS_MAILBOX].base, base_memmap[P29A_LP_LPS_MAILBOX].size);
    dummy_tzc_create(base_memmap[P29A_LP_TZC_SRAM01].base);
    dummy_tzc_create(base_memmap[P29A_LP_TZC_SRAM23].base);
    dummy_tzc_create(base_memmap[P29A_LP_TZC_APS_DDR].base);
    dummy_tzc_create(base_memmap[P29A_LP_TZC_LPS2APS].base);
    dummy_tzc_create(base_memmap[P29A_LP_TZC_LPS2LAS].base);
    dummy_tzc_create(base_memmap[P29A_LP_TZC_LPS2DDR].base);
    dummy_tzc_create(base_memmap[P29A_LP_TZC_LAS2LPS].base);
    dummy_tzc_create(base_memmap[P29A_LP_TZC_AVS_SRAM].base);
    dummy_tzc_create(base_memmap[P29A_LP_TZC_APS2LPS].base);
    dummy_common_create(base_memmap[P29A_LP_PMU].base, base_memmap[P29A_LP_PMU].size);
    dummy_common_create(base_memmap[P29A_LP_LAS_PPU].base, base_memmap[P29A_LP_LAS_PPU].size);
    dummy_p29a_sys_ctrl_create(base_memmap[P29A_LP_SYS_CTRL].base, base_memmap[P29A_LP_SYS_CTRL].size);
    dummy_common_create(base_memmap[P29A_LP_LAS_CCM].base, base_memmap[P29A_LP_LAS_CCM].size);
    dummy_common_create(base_memmap[P29A_LP_TIMESTAMP].base, base_memmap[P29A_LP_TIMESTAMP].size);
    dummy_p29a_otp_create(base_memmap[P29A_LP_SYS_OTP].base, base_memmap[P29A_LP_SYS_OTP].size);

    /* sdhci */
    /* usage: -drive if=sd,file=sd.img,format=raw*/
    qemu_irq sdio_irq = qdev_get_gpio_in(mms->gic, P29A_LP_SDIO_IRQ);
    DeviceState *dev = qdev_new(TYPE_SYSBUS_SDHCI);
    qdev_prop_set_uint8(dev, "sd-spec-version", 3);
    qdev_prop_set_uint64(dev, "capareg", SDHCI_CAPABILITIES);
    qdev_prop_set_uint64(dev, "uhs", UHS_I);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, base_memmap[P29A_LP_SDIO].base);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, sdio_irq);
    object_property_set_bool(OBJECT(dev), "realized", true, &error_fatal);

    DriveInfo *di = drive_get(IF_SD, 0, 0);
    BlockBackend *blk = di ? blk_by_legacy_dinfo(di) : NULL;
    DeviceState *carddev = qdev_new(TYPE_SD_CARD);
    qdev_prop_set_drive(carddev, "drive", blk);
    qdev_realize_and_unref(carddev, qdev_get_child_bus(dev, "sd-bus"),
                               &error_fatal);

    object_property_set_bool(OBJECT(carddev), "realized", true,
                             &error_fatal);



    fprintf(stderr, "[wjp] p29a_common_init kernel name: %s \n", machine->kernel_filename);

    mms->bootinfo.ram_size = 256 * MiB;//machine->ram_size;
    mms->bootinfo.board_id = -1;
    arm_load_kernel(ARM_CPU(first_cpu), machine, &mms->bootinfo);

}

static void p29a_lp_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    fprintf(stderr, "[wjp] p29a_lp_class_init\n");
    mc->init = p29a_lp_common_init;
    mc->max_cpus = 1;
    mc->default_ram_size = 256 * MiB;
    mc->default_ram_id = "p29a.ram";

    mc->desc = "ARM P29A LP for Cortex-A32";
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a32");
}

static void p29a_lp_instance_init(Object *obj)
{
    fprintf(stderr, "[wjp] p29a_lp_instance_init\n");
}

static const TypeInfo p29a_lp_info = {
    .name = MACHINE_TYPE_NAME("p29a-lp"),
    .parent = TYPE_MACHINE,
    .instance_init = p29a_lp_instance_init,
    .instance_size = sizeof(P29A_LPA15MachineState),
    .class_size = sizeof(P29A_LPA15MachineClass),
    .class_init = p29a_lp_class_init,
};

static void p29a_lp_machine_init(void)
{
    fprintf(stderr, "[wjp] p29a_lp_machine_init\n");
    type_register_static(&p29a_lp_info);
}

type_init(p29a_lp_machine_init);
