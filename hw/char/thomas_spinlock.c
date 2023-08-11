/*
 * Thomas spinlock device emulation
 *
 * Copyright (c) 2022 Jinping Wu
 * Written by Jinping Wu
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 or
 *  (at your option) any later version.
 */

/* This is a model of the "Spinlock device" which is used for study
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
#include "hw/char/thomas_spinlock.h"
#include "hw/qdev-properties-system.h"

/* 4 sbinlock */
#define LOCK_TOTAL 4
struct ThomasSpinlockDevice {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    uint32_t lock[LOCK_TOTAL];
};

#define REG0 0x0
#define REG1 0x4
#define REG2 0x8
#define REG3 0xc

#define TYPE_THOMAS_SPINLOCK_DEVICE "thomas-spinlock-device"

OBJECT_DECLARE_SIMPLE_TYPE(ThomasSpinlockDevice, THOMAS_SPINLOCK_DEVICE)

void thomas_spinlock_create(hwaddr addr)
{
    DeviceState *dev;
    SysBusDevice *sbd;

    dev = qdev_new(TYPE_THOMAS_SPINLOCK_DEVICE);
    sbd = SYS_BUS_DEVICE(dev);

    /* Map device to addr */
    sysbus_mmio_map(sbd, 0, addr);

}

static uint64_t thomas_spinlock_device_read(void *opaque, hwaddr offset, unsigned size)
{

    //fprintf(stderr, "[wjp] thomas_spinlock_device_read opaque:%p offset:%ld size:%d\n", opaque, offset, size);
    ThomasSpinlockDevice *ttd = THOMAS_SPINLOCK_DEVICE(opaque);

    switch (offset) {
    case REG0:
        if(ttd->lock[0] == 0) {//unlock
            ttd->lock[0] = 1;
            return 1;
        }
        return 0;//lock
    case REG1:
        if(ttd->lock[1] == 0) {//unlock
            ttd->lock[1] = 1;
            return 1;
        }
        return 0;//lock
    case REG2:
        if(ttd->lock[2] == 0) {//unlock
            ttd->lock[2] = 1;
            return 1;
        }
        return 0;//lock
    case REG3:
        if(ttd->lock[3] == 0) {//unlock
            ttd->lock[3] = 1;
            return 1;
        }
        return 0;//lock
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "thomas_spinlock_device_read: bad offset 0x%x\n", (int) offset);
        break;
    }
    return 0;
}

static void thomas_spinlock_device_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    //fprintf(stderr, "[wjp] thomas_spinlock_device_write opaque:%p offset:%ld size:%d value:%ld\n", opaque, offset, size, value);
    ThomasSpinlockDevice *ttd = THOMAS_SPINLOCK_DEVICE(opaque);

    switch (offset) {
    case REG0:
        ttd->lock[0] = 0;
        break;
    case REG1:
        ttd->lock[1] = 0;
        break;
    case REG2:
        ttd->lock[2] = 0;
        break;
    case REG3:
        ttd->lock[3] = 0;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "thomas_spinlock_device_write: bad offset 0x%x\n", (int) offset);
        break;
    }
}

static const MemoryRegionOps thomas_spinlock_device_ops = {
    .read = thomas_spinlock_device_read,
    .write = thomas_spinlock_device_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void thomas_spinlock_device_init(Object *obj)
{
    //fprintf(stderr, "[wjp] thomas_spinlock_device_init\n");
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    ThomasSpinlockDevice *ttd = THOMAS_SPINLOCK_DEVICE(obj);

    /* Alloc memory region */
    memory_region_init_io(&ttd->iomem, obj, &thomas_spinlock_device_ops, ttd, "thomas_spinlock", 0x1000);

    /* Bind memory region to device */
    sysbus_init_mmio(sbd, &ttd->iomem);

    /* Init spinlock to unlock state */
    for(int i = 0; i < LOCK_TOTAL; i++)
        ttd->lock[i] = 0;

}

static void thomas_spinlock_device_class_init(ObjectClass *klass, void *data)
{
    //fprintf(stderr, "thomas_spinlock_device_class_init\n");
}

static const TypeInfo thomas_spinlock_device_info = {
    .name = "thomas-spinlock-device",
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ThomasSpinlockDevice),
    .instance_init = thomas_spinlock_device_init,
    .class_init = thomas_spinlock_device_class_init,
};

static void thomas_spinlock_device_register_types(void)
{
    type_register_static(&thomas_spinlock_device_info);
}

type_init(thomas_spinlock_device_register_types);
