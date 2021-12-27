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

struct THOMASMachineClass {
    MachineClass parent;
};

struct THOMASMachineState {
    MachineState parent;

    ARMv7MState armv7m;
    MemoryRegion ssram1;
    Clock *sysclk;
    Clock *refclk;
};

#define TYPE_THOMAS_MACHINE MACHINE_TYPE_NAME("thomas-m3")

OBJECT_DECLARE_TYPE(THOMASMachineState, THOMASMachineClass, THOMAS_MACHINE)

static void thomas_common_init(MachineState *machine)
{
    THOMASMachineState *mms = THOMAS_MACHINE(machine);

    MemoryRegion *system_memory = get_system_memory();
    MachineClass *mc = MACHINE_GET_CLASS(machine);
    DeviceState *armv7m;

    fprintf(stderr, "[wjp] thomas_common_init name: %s cpu type: %s\n", mc->name , machine->cpu_type);

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(sram, NULL, "thomas.ssram1", 0x400000, &error_fatal);
    memory_region_add_subregion(get_system_memory(), 0x0, sram);

    object_initialize_child(OBJECT(mms), "armv7m", &mms->armv7m, TYPE_ARMV7M);


    armv7m = DEVICE(&mms->armv7m);

    qdev_prop_set_string(armv7m, "cpu-type", machine->cpu_type);
    qdev_prop_set_bit(armv7m, "enable-bitband", true);

    object_property_set_link(OBJECT(&mms->armv7m), "memory",
                             OBJECT(system_memory), &error_abort);
    sysbus_realize(SYS_BUS_DEVICE(&mms->armv7m), &error_fatal);



    cmsdk_apb_uart_create(0x40004000,
                            NULL,
                            NULL,
                            NULL, NULL,
                            NULL,
                            serial_hd(0), 25000000);

	fprintf(stderr, "[wjp] thomas_common_init kernel name: %s \n", machine->kernel_filename);

    armv7m_load_kernel(ARM_CPU(first_cpu), machine->kernel_filename,
                       0x400000);
}

static void thomas_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    fprintf(stderr, "[wjp] thomas_class_init\n");
    mc->init = thomas_common_init;
    mc->max_cpus = 1;
    mc->default_ram_size = 16 * MiB;
    mc->default_ram_id = "thomas.ram";

    mc->desc = "ARM THOMAS with AN385 FPGA image for Cortex-M3";
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m3");
}


static const TypeInfo thomas_info = {
    .name = MACHINE_TYPE_NAME("thomas-m3"),
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(THOMASMachineState),
    .class_size = sizeof(THOMASMachineClass),
    .class_init = thomas_class_init,
};

static void thomas_machine_init(void)
{
    fprintf(stderr, "[wjp] thomas_machine_init\n");
    type_register_static(&thomas_info);
}

type_init(thomas_machine_init);
