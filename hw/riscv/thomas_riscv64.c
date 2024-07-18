/*
 * QEMU RISC-V Board
 *
 * Copyright (c) Thomas
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

#define THOMAS_RISCV64_PLIC_HART_CONFIG "MS"
#define THOMAS_RISCV64_PLIC_NUM_SOURCES 250
#define THOMAS_RISCV64_PLIC_NUM_PRIORITIES 7
#define THOMAS_RISCV64_PLIC_PRIORITY_BASE 0x00
#define THOMAS_RISCV64_PLIC_PENDING_BASE 0x1000
#define THOMAS_RISCV64_PLIC_ENABLE_BASE 0x2000
#define THOMAS_RISCV64_PLIC_ENABLE_STRIDE 0x80
#define THOMAS_RISCV64_PLIC_CONTEXT_BASE 0x200000
#define THOMAS_RISCV64_PLIC_CONTEXT_STRIDE 0x1000

#define THOMAS_RISCV64_TEST_DEVICE_IRQ      21
#define THOMAS_RISCV64_UART0_IRQ            20

enum {
    THOMAS_RISCV64_SRAM,
    THOMAS_RISCV64_UART0,
    THOMAS_RISCV64_CLINT,
    THOMAS_RISCV64_PLIC,
    THOMAS_RISCV64_TEST_DEVICE,
    THOMAS_RISCV64_DDR,
};

/* Thomas-RISCV64 memory map
 *
 *  0x20000000 .. 0x20400000 : SRAM(4M)
 *  0x40000000 .. 0xc0000000 : DDR(1G)
 */
static const MemMapEntry base_memmap[] = {
    [THOMAS_RISCV64_CLINT] =        {  0x2000000,    0x10000 },
    [THOMAS_RISCV64_PLIC] =         {  0xc000000,  0x4000000 },
    [THOMAS_RISCV64_UART0] =        { 0x10000000,     0x1000 },
    [THOMAS_RISCV64_SRAM] =         { 0x20000000, 0x00400000 },
    [THOMAS_RISCV64_TEST_DEVICE] =  { 0x30000000,     0x1000 },
    [THOMAS_RISCV64_DDR] =          { 0x40000000, 0x40000000 },
};

struct THOMASRISCV64MachineClass {
    MachineClass parent;
};

struct THOMASRISCV64MachineState {
    MachineState parent;

    RISCVHartArrayState cpus;
    MemoryRegion *ddr;
    MemoryRegion *sram;
    DeviceState *plic;
};

#define TYPE_THOMAS_RISCV64_MACHINE MACHINE_TYPE_NAME("thomas-riscv64")

OBJECT_DECLARE_TYPE(THOMASRISCV64MachineState, THOMASRISCV64MachineClass, THOMAS_RISCV64_MACHINE)

static void thomas_riscv64_common_init(MachineState *machine)
{
    THOMASRISCV64MachineState *mms = THOMAS_RISCV64_MACHINE(machine);
    MachineState *ms = MACHINE(qdev_get_machine());
    qemu_irq test_device_irq, uart0_irq;
    g_autofree char *plic_hart_config = NULL;

    /* Init hart */
    object_initialize_child(OBJECT(mms), "cpus", &mms->cpus, TYPE_RISCV_HART_ARRAY);
    object_property_set_int(OBJECT(&mms->cpus), "num-harts", ms->smp.cpus,
                            &error_abort);
    object_property_set_int(OBJECT(&mms->cpus), "resetvec", base_memmap[THOMAS_RISCV64_SRAM].base, &error_abort);

    /* Realize */
    object_property_set_str(OBJECT(&mms->cpus), "cpu-type", ms->cpu_type,
                            &error_abort);
    sysbus_realize(SYS_BUS_DEVICE(&mms->cpus), &error_abort);

    /* DDR */
    mms->ddr = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->ddr, NULL, "thomas_riscv64.ddr",
                           base_memmap[THOMAS_RISCV64_DDR].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_RISCV64_DDR].base, mms->ddr);

    /* Sram */
    mms->sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->sram, NULL, "thomas_riscv64.sram", base_memmap[THOMAS_RISCV64_SRAM].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_RISCV64_SRAM].base, mms->sram);

    /* Clint */
    riscv_aclint_swi_create(base_memmap[THOMAS_RISCV64_CLINT].base,
                            0, ms->smp.cpus, false);
    riscv_aclint_mtimer_create(base_memmap[THOMAS_RISCV64_CLINT].base +
                               RISCV_ACLINT_SWI_SIZE,
        RISCV_ACLINT_DEFAULT_MTIMER_SIZE, 0, ms->smp.cpus,
        RISCV_ACLINT_DEFAULT_MTIMECMP, RISCV_ACLINT_DEFAULT_MTIME,
        RISCV_ACLINT_DEFAULT_TIMEBASE_FREQ, true);

    /* Plic */
    plic_hart_config = riscv_plic_hart_config_string(ms->smp.cpus);
    mms->plic = sifive_plic_create(base_memmap[THOMAS_RISCV64_PLIC].base,
                                   plic_hart_config, ms->smp.cpus, 0,
                                   THOMAS_RISCV64_PLIC_NUM_SOURCES,
                                   THOMAS_RISCV64_PLIC_NUM_PRIORITIES,
                                   THOMAS_RISCV64_PLIC_PRIORITY_BASE,
                                   THOMAS_RISCV64_PLIC_PENDING_BASE,
                                   THOMAS_RISCV64_PLIC_ENABLE_BASE,
                                   THOMAS_RISCV64_PLIC_ENABLE_STRIDE,
                                   THOMAS_RISCV64_PLIC_CONTEXT_BASE,
                                   THOMAS_RISCV64_PLIC_CONTEXT_STRIDE,
                                   base_memmap[THOMAS_RISCV64_PLIC].size);

    /* Test device, its GPIO OUT connect to PLIC GPIO IN */
    test_device_irq = qdev_get_gpio_in(mms->plic, THOMAS_RISCV64_TEST_DEVICE_IRQ);
    thomas_test_device_create(base_memmap[THOMAS_RISCV64_TEST_DEVICE].base, test_device_irq);

    /* Uart */
    uart0_irq = qdev_get_gpio_in(mms->plic, THOMAS_RISCV64_UART0_IRQ);
    serial_mm_init(get_system_memory(), base_memmap[THOMAS_RISCV64_UART0].base, 2,
                   uart0_irq, 115200, serial_hd(0),
                   DEVICE_LITTLE_ENDIAN);

    if (machine->kernel_filename) {
        riscv_load_kernel(machine, &mms->cpus, 0, true, NULL);
    }

}

static void thomas_riscv64_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    //fprintf(stderr, "[wjp] thomas_riscv64_class_init\n");
    mc->init = thomas_riscv64_common_init;
    mc->max_cpus = 16;
    mc->default_ram_size = base_memmap[THOMAS_RISCV64_DDR].size;
    mc->default_ram_id = "thomas.ram";

    mc->desc = "THOMAS for RISCV64";
    mc->default_cpu_type = TYPE_RISCV_CPU_BASE;
}

static void thomas_riscv64_instance_init(Object *obj)
{
    //fprintf(stderr, "[wjp] thomas_riscv64_instance_init\n");
}

static const TypeInfo thomas_riscv64_info = {
    .name = MACHINE_TYPE_NAME("thomas-riscv64"),
    .parent = TYPE_MACHINE,
    .instance_init = thomas_riscv64_instance_init,
    .instance_size = sizeof(THOMASRISCV64MachineState),
    .class_size = sizeof(THOMASRISCV64MachineClass),
    .class_init = thomas_riscv64_class_init,
};

static void thomas_riscv64_machine_init(void)
{
    //fprintf(stderr, "[wjp] thomas_riscv64_machine_init\n");
    type_register_static(&thomas_riscv64_info);
}

type_init(thomas_riscv64_machine_init);
