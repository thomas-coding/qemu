/*
 * QEMU RISC-V Board Compatible with SiFive Freedom E SDK
 *
 * Copyright (c) 2017 SiFive, Inc.
 *
 * Provides a board compatible with the SiFive Freedom E SDK:
 *
 * 0) UART
 * 1) CLINT (Core Level Interruptor)
 * 2) PLIC (Platform Level Interrupt Controller)
 * 3) PRCI (Power, Reset, Clock, Interrupt)
 * 4) Registers emulated as RAM: AON, GPIO, QSPI, PWM
 * 5) Flash memory emulated as RAM
 *
 * The Mask ROM reset vector jumps to the flash payload at 0x2040_0000.
 * The OTP ROM and Flash boot code will be emulated in a future version.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/sysbus.h"
#include "hw/char/serial.h"
#include "hw/misc/unimp.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/sifive_e.h"
#include "hw/riscv/boot.h"
#include "hw/char/sifive_uart.h"
#include "hw/intc/riscv_aclint.h"
#include "hw/intc/sifive_plic.h"
#include "hw/misc/sifive_e_prci.h"
#include "chardev/char.h"
#include "sysemu/sysemu.h"
#include "hw/char/thomas_test_device.h"

#define THOMAS_RISCV32_PLIC_HART_CONFIG "M"
#define THOMAS_RISCV32_PLIC_NUM_SOURCES 127
#define THOMAS_RISCV32_PLIC_NUM_PRIORITIES 7
#define THOMAS_RISCV32_PLIC_PRIORITY_BASE 0x04
#define THOMAS_RISCV32_PLIC_PENDING_BASE 0x1000
#define THOMAS_RISCV32_PLIC_ENABLE_BASE 0x2000
#define THOMAS_RISCV32_PLIC_ENABLE_STRIDE 0x80
#define THOMAS_RISCV32_PLIC_CONTEXT_BASE 0x200000
#define THOMAS_RISCV32_PLIC_CONTEXT_STRIDE 0x1000

#define THOMAS_RISCV32_TEST_DEVICE_IRQ      100
#define THOMAS_RISCV32_UART0_IRQ            20

enum {
    THOMAS_RISCV32_FLASH,
    THOMAS_RISCV32_SRAM,
    THOMAS_RISCV32_UART0,
    THOMAS_RISCV32_CLINT,
    THOMAS_RISCV32_PLIC,
    THOMAS_RISCV32_TEST_DEVICE,
};

/* Thomas-RISCV32 memory map
 *
 *  0x00000000 .. 0x01ffffff : Flash(32M)
 *  0x10000000 .. 0x11ffffff : SRAM(32M)
 */
static const MemMapEntry base_memmap[] = {
    [THOMAS_RISCV32_CLINT] =        {  0x2000000,    0x10000 },
    [THOMAS_RISCV32_PLIC] =         {  0xc000000,  0x4000000 },
    [THOMAS_RISCV32_UART0] =        { 0x10000000,     0x1000 },
    [THOMAS_RISCV32_FLASH] =        { 0x20000000, 0x02000000 },
    [THOMAS_RISCV32_TEST_DEVICE] =  { 0x60000000,     0x1000 },
    [THOMAS_RISCV32_SRAM] =         { 0x80000000, 0x08000000 },
};

struct THOMASRISCV32MachineClass {
    MachineClass parent;
};

struct THOMASRISCV32MachineState {
    MachineState parent;

    RISCVHartArrayState cpus;
    MemoryRegion *flash;
    MemoryRegion *sram;
    DeviceState *plic;
};

#define TYPE_THOMAS_RISCV32_MACHINE MACHINE_TYPE_NAME("thomas-riscv32")

OBJECT_DECLARE_TYPE(THOMASRISCV32MachineState, THOMASRISCV32MachineClass, THOMAS_RISCV32_MACHINE)

static void thomas_riscv32_common_init(MachineState *machine)
{
    THOMASRISCV32MachineState *mms = THOMAS_RISCV32_MACHINE(machine);
    MachineState *ms = MACHINE(qdev_get_machine());
    qemu_irq test_device_irq, uart0_irq;

    /* Init hart */
    object_initialize_child(OBJECT(mms), "cpus", &mms->cpus, TYPE_RISCV_HART_ARRAY);
    object_property_set_int(OBJECT(&mms->cpus), "num-harts", 1,
                            &error_abort);
    object_property_set_int(OBJECT(&mms->cpus), "resetvec", base_memmap[THOMAS_RISCV32_FLASH].base, &error_abort);

    /* Realize */
    object_property_set_str(OBJECT(&mms->cpus), "cpu-type", ms->cpu_type,
                            &error_abort);
    sysbus_realize(SYS_BUS_DEVICE(&mms->cpus), &error_abort);

    /* Flash */
    mms->flash = g_new(MemoryRegion, 1);
    memory_region_init_rom(mms->flash, NULL, "thomas_riscv32.flash",
                           base_memmap[THOMAS_RISCV32_FLASH].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_RISCV32_FLASH].base, mms->flash);

    /* Sram */
    mms->sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->sram, NULL, "thomas_riscv32.sram", base_memmap[THOMAS_RISCV32_SRAM].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_RISCV32_SRAM].base, mms->sram);

    /* Clint */
    riscv_aclint_swi_create(base_memmap[THOMAS_RISCV32_CLINT].base,
                            0, ms->smp.cpus, false);
    riscv_aclint_mtimer_create(base_memmap[THOMAS_RISCV32_CLINT].base +
                               RISCV_ACLINT_SWI_SIZE,
        RISCV_ACLINT_DEFAULT_MTIMER_SIZE, 0, ms->smp.cpus,
        RISCV_ACLINT_DEFAULT_MTIMECMP, RISCV_ACLINT_DEFAULT_MTIME,
        RISCV_ACLINT_DEFAULT_TIMEBASE_FREQ, true);

    /* Plic */
    mms->plic = sifive_plic_create(base_memmap[THOMAS_RISCV32_PLIC].base,
                                   (char *)THOMAS_RISCV32_PLIC_HART_CONFIG, ms->smp.cpus, 0,
                                   THOMAS_RISCV32_PLIC_NUM_SOURCES,
                                   THOMAS_RISCV32_PLIC_NUM_PRIORITIES,
                                   THOMAS_RISCV32_PLIC_PRIORITY_BASE,
                                   THOMAS_RISCV32_PLIC_PENDING_BASE,
                                   THOMAS_RISCV32_PLIC_ENABLE_BASE,
                                   THOMAS_RISCV32_PLIC_ENABLE_STRIDE,
                                   THOMAS_RISCV32_PLIC_CONTEXT_BASE,
                                   THOMAS_RISCV32_PLIC_CONTEXT_STRIDE,
                                   base_memmap[THOMAS_RISCV32_PLIC].size);

    /* Test device, its GPIO OUT connect to PLIC GPIO IN */
    test_device_irq = qdev_get_gpio_in(mms->plic, THOMAS_RISCV32_TEST_DEVICE_IRQ);
    thomas_test_device_create(base_memmap[THOMAS_RISCV32_TEST_DEVICE].base, test_device_irq);

    /* Uart */
    uart0_irq = qdev_get_gpio_in(mms->plic, THOMAS_RISCV32_UART0_IRQ);
    serial_mm_init(get_system_memory(), base_memmap[THOMAS_RISCV32_UART0].base, 2,
                   uart0_irq, 115200, serial_hd(0),
                   DEVICE_LITTLE_ENDIAN);

    if (machine->kernel_filename) {
        riscv_load_kernel(machine, &mms->cpus,
                          base_memmap[THOMAS_RISCV32_FLASH].base, true, NULL);
    }
}

static void thomas_riscv32_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    //fprintf(stderr, "[wjp] thomas_riscv32_class_init\n");
    mc->init = thomas_riscv32_common_init;
    mc->max_cpus = 1;
    mc->default_ram_size = base_memmap[THOMAS_RISCV32_SRAM].size;
    mc->default_ram_id = "thomas.ram";

    mc->desc = "THOMAS for RISCV32";
    mc->default_cpu_type = TYPE_RISCV_CPU_BASE;
}

static void thomas_riscv32_instance_init(Object *obj)
{
    //fprintf(stderr, "[wjp] thomas_riscv32_instance_init\n");
}

static const TypeInfo thomas_riscv32_info = {
    .name = MACHINE_TYPE_NAME("thomas-riscv32"),
    .parent = TYPE_MACHINE,
    .instance_init = thomas_riscv32_instance_init,
    .instance_size = sizeof(THOMASRISCV32MachineState),
    .class_size = sizeof(THOMASRISCV32MachineClass),
    .class_init = thomas_riscv32_class_init,
};

static void thomas_riscv32_machine_init(void)
{
    //fprintf(stderr, "[wjp] thomas_riscv32_machine_init\n");
    type_register_static(&thomas_riscv32_info);
}

type_init(thomas_riscv32_machine_init);
