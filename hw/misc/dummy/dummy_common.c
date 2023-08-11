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
 * This is a model of the dummy device.
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
#include "hw/misc/dummy/dummy_common.h"
#include "hw/irq.h"
#include "hw/qdev-properties-system.h"

struct DummyCommonDevice {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

};

#define TYPE_DUMMY_COMMON "dummy-common"

OBJECT_DECLARE_SIMPLE_TYPE(DummyCommonDevice, DUMMY_COMMON)

static uint64_t dummy_common_read(void *opaque, hwaddr offset, unsigned size)
{

    return 0;
}

static void dummy_common_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{

}

static const MemoryRegionOps dummy_common_ops = {
    .read = dummy_common_read,
    .write = dummy_common_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

void dummy_common_create(hwaddr addr, unsigned size)
{
    DeviceState *dev;
    SysBusDevice *sbd;
    DummyCommonDevice *ttd;

    dev = qdev_new(TYPE_DUMMY_COMMON);
    sbd = SYS_BUS_DEVICE(dev);
    ttd = DUMMY_COMMON(dev);

    /* Alloc memory region */
    memory_region_init_io(&ttd->iomem, OBJECT(dev), &dummy_common_ops, ttd, "dummy_common", size);

    /* Bind memory region to device */
    sysbus_init_mmio(sbd, &ttd->iomem);

    /* Map device to addr */
    sysbus_mmio_map(sbd, 0, addr);

}

static void dummy_common_init(Object *obj)
{
}

static void dummy_common_class_init(ObjectClass *klass, void *data)
{
}

static const TypeInfo dummy_common_info = {
    .name = "dummy-common",
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DummyCommonDevice),
    .instance_init = dummy_common_init,
    .class_init = dummy_common_class_init,
};

static void dummy_common_register_types(void)
{
    type_register_static(&dummy_common_info);
}

type_init(dummy_common_register_types);
