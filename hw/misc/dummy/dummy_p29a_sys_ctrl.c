/*
 * Thomas test device emulation
 *
 * Copyright (c) 2022 Jinping Wu
 * Written by Jinping Wu
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 or
 *  (at your option) any later version.
 */

/* 
 * This is a model of the dummy sys_ctrl for P29A.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "hw/registerfields.h"
#include "chardev/char-fe.h"
#include "chardev/char-serial.h"
#include "hw/misc/dummy/dummy_p29a_sys_ctrl.h"
#include "hw/irq.h"
#include "hw/qdev-properties-system.h"

struct DummyP29ASYS_CTRLDevice {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

};

#define TYPE_DUMMY_P29ASYS_CTRL "dummy-p29asys_ctrl"

OBJECT_DECLARE_SIMPLE_TYPE(DummyP29ASYS_CTRLDevice, DUMMY_P29ASYS_CTRL)


#define SYS_CTRL_BOOT_CFG 0x10

#define  BOOT_MODE_EMMC         0x0
#define  BOOT_MODE_RECOVERY     0x1
#define  BOOT_MODE_SD           0x2
#define  BOOT_MODE_MEMORY       0x3

static uint64_t dummy_p29asys_ctrl_read(void *opaque, hwaddr offset, unsigned size)
{
    //DummyP29ASYS_CTRLDevice *ttd = DUMMY_P29ASYS_CTRL(opaque);

    switch (offset) {
    case SYS_CTRL_BOOT_CFG:
        return BOOT_MODE_MEMORY;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "dummy_sys_ctrl_read: bad offset 0x%x\n", (int) offset);
        break;
    }
    return 0;
}

static void dummy_p29asys_ctrl_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{

}

static const MemoryRegionOps dummy_p29asys_ctrl_ops = {
    .read = dummy_p29asys_ctrl_read,
    .write = dummy_p29asys_ctrl_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

void dummy_p29a_sys_ctrl_create(hwaddr addr, unsigned size)
{
    DeviceState *dev;
    SysBusDevice *sbd;
    DummyP29ASYS_CTRLDevice *ttd;

    dev = qdev_new(TYPE_DUMMY_P29ASYS_CTRL);
    sbd = SYS_BUS_DEVICE(dev);
    ttd = DUMMY_P29ASYS_CTRL(dev);

    /* Alloc memory region */
    memory_region_init_io(&ttd->iomem, OBJECT(dev), &dummy_p29asys_ctrl_ops, ttd, "dummy_p29asys_ctrl", size);

    /* Bind memory region to device */
    sysbus_init_mmio(sbd, &ttd->iomem);

    /* Map device to addr */
    sysbus_mmio_map(sbd, 0, addr);

}

static void dummy_p29asys_ctrl_init(Object *obj)
{
}

static void dummy_p29asys_ctrl_class_init(ObjectClass *klass, void *data)
{
}

static const TypeInfo dummy_p29asys_ctrl_info = {
    .name = "dummy-p29asys_ctrl",
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DummyP29ASYS_CTRLDevice),
    .instance_init = dummy_p29asys_ctrl_init,
    .class_init = dummy_p29asys_ctrl_class_init,
};

static void dummy_p29asys_ctrl_register_types(void)
{
    type_register_static(&dummy_p29asys_ctrl_info);
}

type_init(dummy_p29asys_ctrl_register_types);
