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

#ifndef THOMAS_SPINLOCK_H
#define THOMAS_SPINLOCK_H

#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "qom/object.h"

void thomas_spinlock_create(hwaddr addr);

#endif
