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

/* This is a model of the "Test device" which is used for study
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "trace.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "hw/registerfields.h"
#include "chardev/char-fe.h"
#include "chardev/char-serial.h"
#include "hw/char/thomas_test_device.h"
#include "hw/irq.h"
#include "hw/qdev-properties-system.h"

struct ThomasTestDevice {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    uint32_t reg0; //value1
    uint32_t reg1; //value2
    uint32_t reg2; //value1 + value2
    uint32_t reg3; //write 0xa5 trigger interrupt
    uint32_t reg4; //write 0xa5 clear interrupt

    qemu_irq irq;
};

#define REG0 0x0
#define REG1 0x4
#define REG2 0x8
#define REG3 0xc
#define REG4 0x10

#define TYPE_THOMAS_TEST_DEVICE "thomas-test-device"

OBJECT_DECLARE_SIMPLE_TYPE(ThomasTestDevice, THOMAS_TEST_DEVICE)

void thomas_test_device_create(hwaddr addr, qemu_irq nvic_in)
{
    DeviceState *dev;
    SysBusDevice *sbd;

    dev = qdev_new(TYPE_THOMAS_TEST_DEVICE);
    sbd = SYS_BUS_DEVICE(dev);

    /* Add device to system bus? */
    //sysbus_realize_and_unref(sbd, &error_fatal);

    /* Map device to addr */
    sysbus_mmio_map(sbd, 0, addr);

    /* connect, actually set prop sysbus-irq[0] link to NVIC irq struct */
    sysbus_connect_irq(sbd, 0, nvic_in);
}

static uint64_t thomas_test_device_read(void *opaque, hwaddr offset, unsigned int size)
{
    //fprintf(stderr, "[wjp] thomas_test_device_read opaque:%p offset:%ld size:%d\n", opaque, offset, size);
    ThomasTestDevice *ttd = THOMAS_TEST_DEVICE(opaque);

    switch (offset) {
    case REG0:
        return ttd->reg0;
    case REG1:
        return ttd->reg1;
    case REG2:
        return ttd->reg0 + ttd->reg1;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "thomas test device read: bad offset 0x%x\n", (int)offset);
        break;
    }
    return 0;
}

static void thomas_test_device_write(void *opaque, hwaddr offset, uint64_t value, unsigned int size)
{
    //fprintf(stderr, "[wjp] thomas_test_device_write opaque:%p offset:%ld size:%d value:%ld\n", opaque, offset, size, value);
    ThomasTestDevice *ttd = THOMAS_TEST_DEVICE(opaque);

    switch (offset) {
    case REG0:
        ttd->reg0 = value;
        break;
    case REG1:
        ttd->reg1 = value;
        break;
    case REG3:
        if (value == 0xa5) {//trigger interrupt, actually call NVIC irq struct handler callback function
            qemu_set_irq(ttd->irq, 1);
        }
        break;
    case REG4:
        if (value == 0xa5) {//clear interrupt
            qemu_set_irq(ttd->irq, 0);
        }
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "thomas test device write: bad offset 0x%x\n", (int)offset);
        break;
    }
}

static const MemoryRegionOps thomas_test_device_ops = {
    .read = thomas_test_device_read,
    .write = thomas_test_device_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void thomas_test_device_init(Object *obj)
{
    //fprintf(stderr, "[wjp] thomas_test_device_init\n");
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    ThomasTestDevice *ttd = THOMAS_TEST_DEVICE(obj);

    /* Alloc memory region */
    memory_region_init_io(&ttd->iomem, obj, &thomas_test_device_ops, ttd, "thomas_test", 0x1000);

    /* Bind memory region to device */
    sysbus_init_mmio(sbd, &ttd->iomem);

    /* Define device irq link to prop sysbus-irq[0] */
    sysbus_init_irq(sbd, &ttd->irq);
}

static void thomas_test_device_class_init(ObjectClass *klass, void *data)
{
    //fprintf(stderr, "thomas_test_device_class_init\n");
}

static const TypeInfo thomas_test_device_info = {
    .name = "thomas-test-device",
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ThomasTestDevice),
    .instance_init = thomas_test_device_init,
    .class_init = thomas_test_device_class_init,
};

static void thomas_test_device_register_types(void)
{
    type_register_static(&thomas_test_device_info);
}

type_init(thomas_test_device_register_types);
