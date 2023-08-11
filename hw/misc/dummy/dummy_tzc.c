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
 * This is a model of the dummy TZC. not actual control access, only read/write register.
 * DDI0504C_tzc400_r0p1_trm.pdf  
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
#include "hw/misc/dummy/dummy_tzc.h"
#include "hw/irq.h"
#include "hw/qdev-properties-system.h"

struct DummyTZCDevice {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    uint32_t build_config;
    uint32_t action;
    uint32_t gate_keeper;
    uint32_t speculation_ctrl;
    uint32_t int_status;
    uint32_t int_clear;

};

#define TZC_BUILD_CONFIG        0x0
#define TZC_ACTION              0x4
#define TZC_GATE_KEEPER         0x8
#define TZC_SPECULATION_CTRL    0xc
#define TZC_INT_STATUS          0x10
#define TZC_INT_CLEAR           0x14
#define TZC_CID0                0xff0
#define TZC_CID1                0xff4
#define TZC_CID2                0xff8
#define TZC_CID3                0xffc

#define TYPE_DUMMY_TZC "dummy-tzc"

OBJECT_DECLARE_SIMPLE_TYPE(DummyTZCDevice, DUMMY_TZC)

void dummy_tzc_create(hwaddr addr)
{
    DeviceState *dev;
    SysBusDevice *sbd;

    dev = qdev_new(TYPE_DUMMY_TZC);
    sbd = SYS_BUS_DEVICE(dev);


    /* Map device to addr */
    sysbus_mmio_map(sbd, 0, addr);

}

static uint64_t dummy_tzc_read(void *opaque, hwaddr offset, unsigned size)
{

    //fprintf(stderr, "[wjp] thomas_test_device_read opaque:%p offset:%ld size:%d\n", opaque, offset, size);
    DummyTZCDevice *ttd = DUMMY_TZC(opaque);

    switch (offset) {
    case TZC_BUILD_CONFIG:
        return ttd->build_config;
    case TZC_ACTION:
        return ttd->action;
    case TZC_GATE_KEEPER:
        return ttd->gate_keeper;
    case TZC_SPECULATION_CTRL:
        return ttd->speculation_ctrl;
    case TZC_INT_STATUS:
        return ttd->int_status;
    case TZC_INT_CLEAR:
        return ttd->int_clear;
    case TZC_CID0:
        return 0xd;
    case TZC_CID1:
        return 0xf0;
    case TZC_CID2:
        return 0x5;
    case TZC_CID3:
        return 0xb1;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "dummy_tzc_read: bad offset 0x%x\n", (int) offset);
        break;
    }
    return 0;
}

static void dummy_tzc_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    //fprintf(stderr, "[wjp] thomas_test_device_write opaque:%p offset:%ld size:%d value:%ld\n", opaque, offset, size, value);
    DummyTZCDevice *ttd = DUMMY_TZC(opaque);

    switch (offset) {
    case TZC_BUILD_CONFIG:
        ttd->build_config = value;
        break;
    case TZC_ACTION:
        ttd->action = value;
        break;
    case TZC_GATE_KEEPER:
        ttd->gate_keeper = value << 16;
        break;
    case TZC_SPECULATION_CTRL:
        ttd->speculation_ctrl = value;
        break;
    case TZC_INT_STATUS:
        ttd->int_status = value;
        break;
    case TZC_INT_CLEAR:
        ttd->int_clear = value;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "dummy_tzc_write: bad offset 0x%x\n", (int) offset);
        break;
    }
}

static const MemoryRegionOps dummy_tzc_ops = {
    .read = dummy_tzc_read,
    .write = dummy_tzc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void dummy_tzc_init(Object *obj)
{
    //fprintf(stderr, "[wjp] thomas_test_device_init\n");
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    DummyTZCDevice *ttd = DUMMY_TZC(obj);

    /* Alloc memory region */
    memory_region_init_io(&ttd->iomem, obj, &dummy_tzc_ops, ttd, "dummy_tzc", 0x1000);

    /* Bind memory region to device */
    sysbus_init_mmio(sbd, &ttd->iomem);

}

static void dummy_tzc_class_init(ObjectClass *klass, void *data)
{
    //fprintf(stderr, "thomas_test_device_class_init\n");
}

static const TypeInfo dummy_tzc_info = {
    .name = "dummy-tzc",
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DummyTZCDevice),
    .instance_init = dummy_tzc_init,
    .class_init = dummy_tzc_class_init,
};

static void dummy_tzc_register_types(void)
{
    type_register_static(&dummy_tzc_info);
}

type_init(dummy_tzc_register_types);
