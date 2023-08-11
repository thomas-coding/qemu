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
 * This is a model of the dummy otp for P29A.
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
#include "hw/misc/dummy/dummy_p29a_otp.h"
#include "hw/irq.h"
#include "hw/qdev-properties-system.h"

struct DummyP29AOTPDevice {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

};

#define TYPE_DUMMY_P29AOTP "dummy-p29aotp"

OBJECT_DECLARE_SIMPLE_TYPE(DummyP29AOTPDevice, DUMMY_P29AOTP)


#define OTP_LIFECYCLE 0x4038

static uint64_t dummy_p29aotp_read(void *opaque, hwaddr offset, unsigned size)
{
    //DummyP29AOTPDevice *ttd = DUMMY_P29AOTP(opaque);

    switch (offset) {
    case OTP_LIFECYCLE:
        return 0X80000384;//DEV
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "dummy_otp_read: bad offset 0x%x\n", (int) offset);
        break;
    }
    return 0;
}

static void dummy_p29aotp_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{

}

static const MemoryRegionOps dummy_p29aotp_ops = {
    .read = dummy_p29aotp_read,
    .write = dummy_p29aotp_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

void dummy_p29a_otp_create(hwaddr addr, unsigned size)
{
    DeviceState *dev;
    SysBusDevice *sbd;
    DummyP29AOTPDevice *ttd;

    dev = qdev_new(TYPE_DUMMY_P29AOTP);
    sbd = SYS_BUS_DEVICE(dev);
    ttd = DUMMY_P29AOTP(dev);

    /* Alloc memory region */
    memory_region_init_io(&ttd->iomem, OBJECT(dev), &dummy_p29aotp_ops, ttd, "dummy_p29aotp", size);

    /* Bind memory region to device */
    sysbus_init_mmio(sbd, &ttd->iomem);

    /* Map device to addr */
    sysbus_mmio_map(sbd, 0, addr);

}

static void dummy_p29aotp_init(Object *obj)
{
}

static void dummy_p29aotp_class_init(ObjectClass *klass, void *data)
{
}

static const TypeInfo dummy_p29aotp_info = {
    .name = "dummy-p29aotp",
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DummyP29AOTPDevice),
    .instance_init = dummy_p29aotp_init,
    .class_init = dummy_p29aotp_class_init,
};

static void dummy_p29aotp_register_types(void)
{
    type_register_static(&dummy_p29aotp_info);
}

type_init(dummy_p29aotp_register_types);
