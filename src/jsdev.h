//  evjs - Evdev Joystick Utilities
//  Copyright (C) 2020 Scott Shumate <scott@shumatech.com>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t      jsaxis_id_t;
typedef unsigned int jsidx_t;
typedef struct jsdev jsdev_t;

typedef struct jvcal
{
    int min;
    int max;
    int center_min;
    int center_max;
} jscal_t;

///////////////////////////////////////////////////////////////////////////////
//
// Axis Functions
//
///////////////////////////////////////////////////////////////////////////////
bool jsaxis_cal_set(jsdev_t *dev, jsidx_t index, const jscal_t *cal);
void jsaxis_cal_activate(jsdev_t *dev);
int jsaxis_map(jsdev_t *dev, jsaxis_id_t id);
void jsaxis_init(jsdev_t *dev);

///////////////////////////////////////////////////////////////////////////////
//
// Device Functions
//
///////////////////////////////////////////////////////////////////////////////
jsdev_t *jsdev_init(const char *file);
void jsdev_free(jsdev_t *dev);
char *jsdev_from_evdev(int fd);
