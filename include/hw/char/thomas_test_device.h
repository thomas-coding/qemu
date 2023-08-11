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

#ifndef THOMAS_TEST_DEVICE_H
#define THOMAS_TEST_DEVICE_H

#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "qom/object.h"

void thomas_test_device_create(hwaddr addr, qemu_irq nvic_in);

#endif
