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

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t  evabs_id_t;
typedef uint16_t evkey_id_t;
typedef uint16_t evff_id_t;
typedef struct evdev_id
{
    int bus;
    int vendor;
    int product;
} evdev_id_t;

typedef unsigned int evidx_t;

typedef void (*evabs_value_cb_t)(evidx_t index, int value, void *arg);
typedef void (*evkey_value_cb_t)(evidx_t index, bool value, void *arg);
typedef void (*evabs_cb_t)(evidx_t index, void *arg);
typedef void (*evkey_cb_t)(evidx_t index, void *arg);
typedef void (*evff_cb_t)(evidx_t index, void *arg);

typedef struct evdev evdev_t;

typedef struct evcal
{
    int min;
    int max;
    int fuzz;
    int flat;
} evcal_t;

typedef enum evff_type
{
    EVFF_UNKNOWN,
    EVFF_PROPERTY,
    EVFF_CONSTANT,
    EVFF_RUMBLE,
    EVFF_PERIODIC,
    // TODO: support more?
} evff_type_t;

///////////////////////////////////////////////////////////////////////////////
//
// Absolute Axis Functions
//
///////////////////////////////////////////////////////////////////////////////
void evabs_init(evdev_t *dev);
const char *evabs_name(evdev_t *dev, evidx_t index);
size_t evabs_num(evdev_t *dev);
void evabs_foreach(evdev_t *dev, evabs_cb_t abs_cb, void *arg);
bool evabs_cal_set(evdev_t *dev, evidx_t index, const evcal_t *cal);
void evabs_cal_get(evdev_t *dev, evidx_t index, evcal_t *cal);
int evabs_value(evdev_t *dev, evidx_t index);
int evabs_map(evdev_t *dev, evabs_id_t id);
evabs_id_t evabs_id(evdev_t *dev, evidx_t index);

///////////////////////////////////////////////////////////////////////////////
//
// Key Functions
//
///////////////////////////////////////////////////////////////////////////////
void evkey_init(evdev_t *dev);
size_t evkey_num(evdev_t *dev);
void evkey_foreach(evdev_t *dev, evkey_cb_t key_cb, void *arg);
bool evkey_value(evdev_t *dev, evidx_t key_id);
int evkey_map(evdev_t *dev, evkey_id_t evid);
evkey_id_t evkey_id(evdev_t *dev, evidx_t index);

///////////////////////////////////////////////////////////////////////////////
//
// Force Feedback Functions
//
///////////////////////////////////////////////////////////////////////////////
#if ENABLE_EFFECTS
void evff_init(evdev_t *dev);
const char *evff_name(evdev_t *dev, evidx_t index);
size_t evff_num(evdev_t *dev);
void evff_foreach(evdev_t *dev, evff_cb_t ff_cb, void *arg);
int evff_map(evdev_t *dev, evff_id_t evid);
evff_id_t evff_id(evdev_t *dev, evidx_t index);

evff_type_t evff_type(evdev_t *dev, evidx_t index);
bool evff_property(evdev_t *dev, evidx_t index, unsigned value);
bool evff_constant(evdev_t *dev, evidx_t index, int level, unsigned direction, unsigned length);
bool evff_rumble(evdev_t *dev, evidx_t index, unsigned strong, unsigned weak, unsigned length);
bool evff_periodic(evdev_t *dev, evidx_t index, int level, unsigned direction, unsigned length);
#endif

///////////////////////////////////////////////////////////////////////////////
//
// Device Functions
//
///////////////////////////////////////////////////////////////////////////////
void evdev_read(evdev_t *dev);
void evdev_read_cb(evdev_t *dev, evabs_value_cb_t abs_cb, void *abs_arg,
                   evkey_value_cb_t key_cb, void *key_arg);
int evdev_fileno(evdev_t *dev);
char *evdev_name(evdev_t *dev);
void evdev_id(evdev_t *dev, evdev_id_t *id);
evdev_t *evdev_init(const char *file);
void evdev_free(evdev_t *dev);
bool evdev_info(const char *file, evdev_id_t *id, char **name);
