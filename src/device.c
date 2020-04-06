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
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "device.h"
#include "util.h"

#define DEV_INPUT           "/dev/input"
#define EVENT_PREFIX        "event"
#define EVENT_PREFIX_LEN    (sizeof(EVENT_PREFIX) - 1)

#define eprintf(...)        fprintf(stderr, __VA_ARGS__)

///////////////////////////////////////////////////////////////////////////////
//
// Button Functions
//
///////////////////////////////////////////////////////////////////////////////

static void button_value(evidx_t index, bool value, void *arg)
{
    device_t *dev = arg;
    button_t *button = &dev->button_array[index];

    button->value = value;

    if (dev->button_cb)
        dev->button_cb(button, dev->arg_cb);
}

static void button_add(evidx_t index, void *arg)
{
    device_t *dev = arg;
    button_t *button = &dev->button_array[index];

    button->id = evkey_id(dev->evdev, index);
    button->index = index;
}

button_t *device_button_get(device_t *dev, int id)
{
    int index = evkey_map(dev->evdev, id);
    if (index < 0)
        return NULL;
    return &dev->button_array[index];
}

///////////////////////////////////////////////////////////////////////////////
//
// Axis Functions
//
///////////////////////////////////////////////////////////////////////////////

static void axis_value(evidx_t index, int value, void *arg)
{
    device_t *dev = arg;
    axis_t *axis = &dev->axis_array[index];

    if (value > axis->maximum)
        axis->maximum = value;

    if (value < axis->minimum)
        axis->minimum = value;

    axis->value = value;

    dev->axis_array[index].value = value;

    if (dev->axis_cb)
        dev->axis_cb(axis, dev->arg_cb);

}

static void axis_add(evidx_t index, void *arg)
{
    device_t *dev = arg;
    axis_t *axis = &dev->axis_array[index];

    axis->id = evabs_id(dev->evdev, index);
    axis->index = index;
    axis->name = evabs_name(dev->evdev, index);
    axis->value = evabs_value(dev->evdev, index);
    axis->minimum = axis->value;
    axis->maximum = axis->value;
    evabs_cal_get(dev->evdev, index, &axis->cal);
}

axis_t *device_axis_get(device_t *dev, int id)
{
    int index = evabs_map(dev->evdev, id);
    if (index < 0)
        return NULL;
    return &dev->axis_array[index];
}

///////////////////////////////////////////////////////////////////////////////
//
// Effect Functions
//
///////////////////////////////////////////////////////////////////////////////
#if ENABLE_EFFECTS

effect_t *effect_lookup(device_t *dev, int id)
{
    int index = evff_map(dev->evdev, id);
    if (index < 0)
        return NULL;
    return &dev->effect_array[index];
}

static void effect_add(evidx_t index, void *arg)
{
    device_t *dev = arg;
    effect_t *effect = &dev->effect_array[index];

    effect->id = evff_map(dev->evdev, index);
    effect->index = index;
    effect->name = evff_name(dev->evdev, index);
    effect->type = evff_type(dev->evdev, index);
}

bool device_effect_property(device_t *dev, effect_t *effect, unsigned value)
{
    return evff_property(dev->evdev, effect->index, value);
}

bool device_effect_constant(device_t *dev, effect_t *effect, int level, unsigned direction, unsigned length)
{
    return evff_constant(dev->evdev, effect->index, level, direction, length);
}

bool device_effect_rumble(device_t *dev, effect_t *effect, unsigned strong, unsigned weak, unsigned length)
{
    return evff_rumble(dev->evdev, effect->index, strong, weak, length);
}

bool device_effect_periodic(device_t *dev, effect_t *effect, int level, unsigned direction, unsigned length)
{
    return evff_periodic(dev->evdev, effect->index, level, direction, length);
}
#endif

///////////////////////////////////////////////////////////////////////////////
//
// Device Functions
//
///////////////////////////////////////////////////////////////////////////////

int device_fileno(device_t *dev)
{
    return evdev_fileno(dev->evdev);
}

void device_read(device_t *dev)
{
    evdev_read(dev->evdev);
}

void device_read_cb(device_t *dev, axis_cb_t axis_cb, button_cb_t button_cb, void *arg)
{
    dev->axis_cb   = axis_cb;
    dev->button_cb = button_cb;
    dev->arg_cb    = arg;
}

void device_axis_calibrate(device_t *dev, axis_t *axis)
{
    evabs_cal_set(dev->evdev, axis->index, &axis->cal);
}

void device_calibrate(device_t *dev)
{

    AXIS_FOREACH(dev, axis)
    {
        evabs_cal_set(dev->evdev, axis->index, &axis->cal);
#if ENABLE_JOYSTICK
        if (dev->jsdev)
        {
            // jsdev index is not necessarily the same as the evdev index
            // so map the jsdev index via the ID which is always the same
            int jsidx = jsaxis_map(dev->jsdev, evabs_id(dev->evdev, axis->index));
            if (jsidx >= 0)
            {
                // evdev does not support deadzones so just split the difference
                int center = axis->cal.min + (axis->cal.max - axis->cal.min) / 2;
                jscal_t jscal = {
                    .min        = axis->cal.min,
                    .max        = axis->cal.max,
                    .center_min = center,
                    .center_max = center,
                };
                jsaxis_cal_set(dev->jsdev, jsidx, &jscal);
            }
        }
#endif
    }

#if ENABLE_JOYSTICK
    jsaxis_cal_activate(dev->jsdev);
#endif

    dev->dirty = true;
}

void device_free(device_t *dev)
{
    evdev_free(dev->evdev);

#if ENABLE_JOYSTICK
    jsdev_free(dev->jsdev);
    xfree(dev->jsfile);
#endif

    xfree(dev->axis_array);
    xfree(dev->button_array);
#if ENABLE_EFFECTS
    xfree(dev->effect_array);
#endif

    xfree(dev->name);
    xfree(dev);
}

device_t *device_init(const char *dev_file)
{
    device_t *dev = xalloc(sizeof(device_t));
    dev->file = dev_file;

    dev->evdev = evdev_init(dev_file);
    evabs_init(dev->evdev);
    evkey_init(dev->evdev);
#if ENABLE_EFFECTS    
    evff_init(dev->evdev);
#endif
    
    evdev_read_cb(dev->evdev, axis_value, dev, button_value, dev);

    dev->name = evdev_name(dev->evdev);
    evdev_id(dev->evdev, &dev->id);

    dev->axis_num = evabs_num(dev->evdev);
    if (!dev->axis_num)
        xerrx("Device does not have any axes");
    dev->axis_array = xalloc(sizeof(axis_t) * dev->axis_num);
    evabs_foreach(dev->evdev, axis_add, dev);

    dev->button_num = evkey_num(dev->evdev);
    if (dev->button_num > 0)
    {
        dev->button_array = xalloc(sizeof(button_t) * dev->button_num);
        evkey_foreach(dev->evdev, button_add, dev);
    }

#if ENABLE_EFFECTS
    dev->effect_num = evff_num(dev->evdev);
    if (dev->effect_num)
    {
        dev->effect_array = xalloc(sizeof(effect_t) * dev->effect_num);
        evff_foreach(dev->evdev, effect_add, dev);
    }
#endif

#if ENABLE_JOYSTICK
    dev->jsfile = jsdev_from_evdev(evdev_fileno(dev->evdev));
    if (dev->jsfile)
    {
        dev->jsdev = jsdev_init(dev->jsfile);
        jsaxis_init(dev->jsdev);
    }
#endif

    return dev;
}

static int event_filter(const struct dirent *entry)
{
    int devnum;
    return (sscanf(entry->d_name, EVENT_PREFIX "%d", &devnum) == 1);
}

char *device_select(void)
{
    struct dirent **dent;

    eprintf("No device specified, scanning " DEV_INPUT "/" EVENT_PREFIX "*\n");

    int devices = scandir(DEV_INPUT, &dent, event_filter, versionsort);
    if (devices < 0)
        xerrx(DEV_INPUT);

    int found = 0;
    for (int i = 0; i < devices; i++)
    {
        char file[PATH_MAX];

        snprintf(file, sizeof(file), "%s/%s", DEV_INPUT, dent[i]->d_name);
        xfree(dent[i]);

        char *name;
        if (evdev_info(file, NULL, &name))
        {
            if (!found)
                eprintf("Available devices:\n");

            eprintf("%-*s: %s\n", (int)(sizeof(DEV_INPUT) + sizeof(EVENT_PREFIX) + 3), file, name);

            xfree(name);

            found++;
        }
    }

    xfree(dent);

    if (found == 0)
    {
        eprintf("No available devices\n");
        return NULL;
    }

    eprintf("Select the device number: ");

    int devnum;
    if (scanf("%d", &devnum) != 1)
    {
        warnx("Invalid device number");
        return NULL;
    }

    char *dev_file = NULL;
    if (asprintf(&dev_file, "%s/%s%d", DEV_INPUT, EVENT_PREFIX, devnum) < 0)
        xerrx("memory allocation failed");

    return dev_file;
}
