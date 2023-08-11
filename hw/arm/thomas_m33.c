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

enum {
    THOMAS_M33_FLASH,
    THOMAS_M33_SRAM,
    THOMAS_M33_APB_UART,
    THOMAS_M33_TEST_DEVICE,
};

/* Thomas-m33 memory map
 *
 *  0x00000000 .. 0x0fffffff : Reserved
 *  0x10000000 .. 0x103fffff : Flash(4M)
 *  0x20000000 .. 0x203fffff : SRAM(4M)
 *  0x40000000 .. 0x40000fff : APB UART(4K)
 *  0x50000000 .. 0x50000fff : Thomas Test Device(4K)
 */
static const MemMapEntry base_memmap[] = {
    [THOMAS_M33_FLASH] =         { 0x10000000, 0x00400000 },
    [THOMAS_M33_SRAM] =          { 0x20000000, 0x00400000 },
    [THOMAS_M33_APB_UART] =      { 0x40000000, 0x00001000 },
    [THOMAS_M33_TEST_DEVICE] =   { 0x50000000, 0x00001000 },
};

/* Thomas-m3 Interrupt */
#define THOMAS_M33_APB_UART_TX_IRQ    10
#define THOMAS_M33_APB_UART_RX_IRQ    11
#define THOMAS_M33_TEST_DEVICE_IRQ    15

#define THOMAS_M33_INIT_VECTOR        0x10000000

struct THOMASM33MachineClass {
    MachineClass parent;
};

struct THOMASM33MachineState {
    MachineState parent;

    ARMv7MState armv7m;
    MemoryRegion *flash;
    MemoryRegion *sram;
    Clock *sysclk;
    Clock *refclk;
};

#define TYPE_THOMAS_M33_MACHINE MACHINE_TYPE_NAME("thomas-m33")

OBJECT_DECLARE_TYPE(THOMASM33MachineState, THOMASM33MachineClass, THOMAS_M33_MACHINE)

static void thomas_m33_common_init(MachineState *machine)
{
    THOMASM33MachineState *mms = THOMAS_M33_MACHINE(machine);

    MemoryRegion *system_memory = get_system_memory();
    DeviceState *armv7m;

    /* Flash */
    mms->flash = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->flash, NULL, "thomas_m33.flash", base_memmap[THOMAS_M33_FLASH].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_M33_FLASH].base, mms->flash);

    /* Sram */
    mms->sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->sram, NULL, "thomas_m33.sram", base_memmap[THOMAS_M33_SRAM].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_M33_SRAM].base, mms->sram);

    /* Init armv7m object */
    object_initialize_child(OBJECT(mms), "armv7m", &mms->armv7m, TYPE_ARMV7M);

    armv7m = DEVICE(&mms->armv7m);

    /* Set clock and put into armv7m device */
    mms->sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(mms->sysclk, 24000000);
    mms->refclk = clock_new(OBJECT(machine), "REFCLK");
    clock_set_hz(mms->refclk, 24000000);

    qdev_connect_clock_in(armv7m, "cpuclk", mms->sysclk);
    qdev_connect_clock_in(armv7m, "refclk", mms->refclk);

    /* Set cpu type to arm cortex-m3 */
    qdev_prop_set_string(armv7m, "cpu-type", machine->cpu_type);
    object_property_set_link(OBJECT(&mms->armv7m), "memory",
                             OBJECT(system_memory), &error_abort);
    qdev_prop_set_uint32(armv7m, "init-svtor", THOMAS_M33_INIT_VECTOR);

    /* Realize armv7m object */
    sysbus_realize(SYS_BUS_DEVICE(&mms->armv7m), &error_fatal);

    /* Get armv7m device number, GPIO interrupt struct pointer */
    qemu_irq test_device_irq = qdev_get_gpio_in(armv7m, THOMAS_M33_TEST_DEVICE_IRQ);
    thomas_test_device_create(base_memmap[THOMAS_M33_TEST_DEVICE].base, test_device_irq);

#if 0
    cmsdk_apb_uart_create(base_memmap[THOMAS_M33_APB_UART].base,
                            qdev_get_gpio_in(armv7m, THOMAS_M33_APB_UART_TX_IRQ),
                            qdev_get_gpio_in(armv7m, THOMAS_M33_APB_UART_RX_IRQ),
                            NULL, NULL,
                            NULL,
                            serial_hd(0), 24000000);
#else
    DeviceState *dev;
    SysBusDevice *s;   
    dev = qdev_new(TYPE_CMSDK_APB_UART);
    s = SYS_BUS_DEVICE(dev);
    qdev_prop_set_chr(dev, "chardev", serial_hd(0));
    qdev_prop_set_uint32(dev, "pclk-frq", 24000000);
    sysbus_realize_and_unref(s, &error_fatal);
    sysbus_mmio_map(s, 0, base_memmap[THOMAS_M33_APB_UART].base);
    sysbus_connect_irq(s, 0, qdev_get_gpio_in(armv7m, THOMAS_M33_APB_UART_TX_IRQ));
    sysbus_connect_irq(s, 1, qdev_get_gpio_in(armv7m, THOMAS_M33_APB_UART_RX_IRQ));
    //sysbus_connect_irq(s, 2, txovrint);
    //sysbus_connect_irq(s, 3, rxovrint);
#endif

    fprintf(stderr, "[wjp] thomas_common_init kernel name: %s \n", machine->kernel_filename);

    armv7m_load_kernel(ARM_CPU(first_cpu), machine->kernel_filename,
                       0, 0x400000);
}

static void thomas_m33_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    fprintf(stderr, "[wjp] thomas_m33_class_init\n");
    mc->init = thomas_m33_common_init;
    mc->max_cpus = 1;
    mc->default_ram_size = 16 * MiB;
    mc->default_ram_id = "thomas.ram";

    mc->desc = "ARM THOMAS with AN385 FPGA image for Cortex-M3";
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m33");
}

static void thomas_m33_instance_init(Object *obj)
{
    fprintf(stderr, "[wjp] thomas_m33_instance_init\n");
}

static const TypeInfo thomas_m33_info = {
    .name = MACHINE_TYPE_NAME("thomas-m33"),
    .parent = TYPE_MACHINE,
    .instance_init = thomas_m33_instance_init,
    .instance_size = sizeof(THOMASM33MachineState),
    .class_size = sizeof(THOMASM33MachineClass),
    .class_init = thomas_m33_class_init,
};

static void thomas_m33_machine_init(void)
{
    fprintf(stderr, "[wjp] thomas_m33_machine_init\n");
    type_register_static(&thomas_m33_info);
}

type_init(thomas_m33_machine_init);
