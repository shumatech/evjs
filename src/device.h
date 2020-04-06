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
#include <stdbool.h>

#include "evdev.h"
#if ENABLE_JOYSTICK
#include "jsdev.h"
#endif

typedef struct axis
{
    int         id;
    int         index;
    const char  *name;
    int         value;
    int         minimum;
    int         maximum;
    evcal_t     cal;
} axis_t;

typedef struct button
{
    int id;
    int index;
    int value;
} button_t;

typedef void (*axis_cb_t)(axis_t *axis, void *arg);
typedef void (*button_cb_t)(button_t *button, void *arg);

#if ENABLE_EFFECTS
#define EFFECT_PROPERTY EVFF_PROPERTY
#define EFFECT_CONSTANT EVFF_CONSTANT
#define EFFECT_RUMBLE   EVFF_RUMBLE
#define EFFECT_PERIODIC EVFF_PERIODIC

typedef struct effect
{
    int         id;
    int         index;
    const char  *name;
    evff_type_t type;
} effect_t;
#endif

typedef struct device
{
    const char  *file;
    char        *name;
    evdev_id_t  id;

    evdev_t     *evdev;
#if ENABLE_JOYSTICK
    jsdev_t     *jsdev;
#endif

    axis_cb_t   axis_cb;
    button_cb_t button_cb;
    void        *arg_cb;

    bool        dirty;

    size_t      axis_num;
    axis_t      *axis_array;

    size_t      button_num;
    button_t    *button_array;

#if ENABLE_JOYSTICK
    char        *jsfile;
#endif

#if ENABLE_EFFECTS
    int         effect_id;
    size_t      effect_num;
    effect_t    *effect_array;
#endif
} device_t;

#define AXIS_FOREACH(dev, axis)      for (axis_t *axis = dev->axis_array;\
                                          axis < &dev->axis_array[dev->axis_num];\
                                          axis++)

#define BUTTON_FOREACH(dev, button)  for (button_t *button = dev->button_array;\
                                          button < &dev->button_array[dev->button_num];\
                                          button++)

#if ENABLE_EFFECTS
#define EFFECT_FOREACH(dev, effect)  for (effect_t *effect = dev->effect_array;\
                                          effect < &dev->effect_array[dev->effect_num];\
                                          effect++)
#endif

int device_fileno(device_t *dev);

void device_read(device_t *dev);

void device_read_cb(device_t *dev, axis_cb_t axis_cb, button_cb_t button_cb, void *arg);

device_t *device_init(const char *dev_file);

void device_free(device_t *dev);

char *device_select(void);

axis_t *device_axis_get(device_t *dev, int id);

void device_axis_calibrate(device_t *dev, axis_t *axis);

void device_calibrate(device_t *dev);

button_t *device_button_get(device_t *dev, int id);

#if ENABLE_EFFECTS
bool device_effect_property(device_t *dev, effect_t *effect, unsigned value);
bool device_effect_constant(device_t *dev, effect_t *effect, int level, unsigned direction, unsigned length);
bool device_effect_rumble(device_t *dev, effect_t *effect, unsigned strong, unsigned weak, unsigned length);
bool device_effect_periodic(device_t *dev, effect_t *effect, int level, unsigned direction, unsigned length);
#endif
