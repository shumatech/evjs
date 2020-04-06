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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#include "barray.h"
#include "util.h"
#include "evdev.h"

typedef struct evabs
{
    evabs_id_t           id;
    struct input_absinfo info;
} evabs_t;

typedef struct evkey
{
    evkey_id_t id;
    bool       value;
} evkey_t;

typedef struct evff
{
    evkey_id_t id;
} evff_t;

struct evdev
{
    int         fd;

    evabs_t     *abs_array;
    size_t      abs_num;
    evabs_id_t  abs_map[ABS_CNT];

    evkey_t     *key_array;
    size_t      key_num;
    evkey_id_t  key_map[KEY_CNT];

#if ENABLE_EFFECTS
    evff_t      *ff_array;
    size_t      ff_num;
    evff_id_t   ff_map[FF_CNT];
    int         ff_id;
#endif

    evabs_value_cb_t abs_cb;
    void             *abs_arg;
    evkey_value_cb_t key_cb;
    void             *key_arg;
};

///////////////////////////////////////////////////////////////////////////////
//
// Absolute Axis Functions
//
///////////////////////////////////////////////////////////////////////////////

const char *evabs_name(evdev_t *dev, evidx_t index)
{
#define ABS_CASE(abs) case ABS_ ## abs : return # abs

    ASSERT(index < dev->abs_num);

    switch (dev->abs_array[index].id)
    {
        ABS_CASE(X);
        ABS_CASE(Y);
        ABS_CASE(Z);
        ABS_CASE(RX);
        ABS_CASE(RY);
        ABS_CASE(RZ);
        ABS_CASE(THROTTLE);
        ABS_CASE(RUDDER);
        ABS_CASE(WHEEL);
        ABS_CASE(GAS);
        ABS_CASE(BRAKE);
        ABS_CASE(HAT0X);
        ABS_CASE(HAT0Y);
        ABS_CASE(HAT1X);
        ABS_CASE(HAT1Y);
        ABS_CASE(HAT2X);
        ABS_CASE(HAT2Y);
        ABS_CASE(HAT3X);
        ABS_CASE(HAT3Y);
        ABS_CASE(PRESSURE);
        ABS_CASE(DISTANCE);
        ABS_CASE(TILT_X);
        ABS_CASE(TILT_Y);
        ABS_CASE(TOOL_WIDTH);
        ABS_CASE(VOLUME);
        ABS_CASE(MISC);
        default: return "UNKNOWN";
    }
}

static void abs_init(bit_t id, void *arg)
{
    evdev_t *dev = arg;

    dev->abs_array[dev->abs_num].id = id;
    dev->abs_map[id] = dev->abs_num;

    xioctl(dev->fd, EVIOCGABS(id), &dev->abs_array[dev->abs_num].info);

    dev->abs_num++;
}

void evabs_init(evdev_t *dev)
{
    if (dev->abs_num != 0)
        return;

    for (int i = 0; i < ABS_CNT; i++)
        dev->abs_map[i] = ABS_CNT;

    barray_t *abs_barray = barray_init(ABS_CNT);
    xioctl(dev->fd, EVIOCGBIT(EV_ABS, ABS_CNT), barray_data(abs_barray));

    dev->abs_num = barray_count_set(abs_barray);
    if (dev->abs_num > 0)
    {
        dev->abs_array = xalloc(dev->abs_num * sizeof(dev->abs_array[0]));

        dev->abs_num = 0;
        barray_foreach_set(abs_barray, abs_init, dev);
    }

    barray_free(abs_barray);
}

size_t evabs_num(evdev_t *dev)
{
    return dev->abs_num;
}

void evabs_foreach(evdev_t *dev, evabs_cb_t abs_cb, void *arg)
{
    for (int index = 0; index < dev->abs_num; index++)
        abs_cb(index, arg);
}

bool evabs_cal_set(evdev_t *dev, evidx_t index, const evcal_t *cal)
{
    ASSERT(index < dev->abs_num);
    evabs_t *abs = &dev->abs_array[index];

    int range = cal->max - cal->min;
    if (range <= 0 || cal->fuzz > range / 2 || cal->flat > range / 2)
        return false;

    abs->info.minimum = cal->min,
    abs->info.maximum = cal->max,
    abs->info.fuzz    = cal->fuzz,
    abs->info.flat    = cal->flat,

    xioctl(dev->fd, EVIOCSABS(abs->id), &abs->info);

    return true;
}

void evabs_cal_get(evdev_t *dev, evidx_t index, evcal_t *cal)
{
    ASSERT(index < dev->abs_num);
    evabs_t *abs = &dev->abs_array[index];

    cal->min  = abs->info.minimum;
    cal->max  = abs->info.maximum;
    cal->fuzz = abs->info.fuzz;
    cal->flat = abs->info.flat;
}

int evabs_value(evdev_t *dev, evidx_t index)
{
    ASSERT(index < dev->abs_num);

    return dev->abs_array[index].info.value;
}

int evabs_map(evdev_t *dev, evabs_id_t id)
{
    ASSERT(id < ABS_CNT);

    int index = dev->abs_map[id];
    if (index >= ABS_CNT)
        return -1;
    return index;
}

evabs_id_t evabs_id(evdev_t *dev, evidx_t index)
{
    ASSERT(index < dev->abs_num);

    return dev->abs_array[index].id;
}

///////////////////////////////////////////////////////////////////////////////
//
// Key Functions
//
///////////////////////////////////////////////////////////////////////////////
static void key_init(bit_t id, void *arg)
{
    evdev_t *dev = arg;

    dev->key_array[dev->key_num].id = id;
    dev->key_map[id] = dev->key_num;
    
    dev->key_num++;
}

static void key_value_set(bit_t id, void *arg)
{
    evdev_t *dev = arg;

    int index = evkey_map(dev, id);
    if (index >= 0)
        dev->key_array[index].value = true;
}

void evkey_init(evdev_t *dev)
{
    if (dev->key_num != 0)
        return;

    barray_t *key_barray = barray_init(KEY_CNT);
    xioctl(dev->fd, EVIOCGBIT(EV_KEY, KEY_CNT), barray_data(key_barray));

    dev->key_num = barray_count_set(key_barray);
    if (dev->key_num > 0)
    {
        dev->key_array = xalloc(dev->key_num * sizeof(dev->key_array[0]));

        dev->key_num = 0;
        barray_foreach_set(key_barray, key_init, dev);

        xioctl(dev->fd, EVIOCGKEY(KEY_CNT), barray_data(key_barray));
        barray_foreach_set(key_barray, key_value_set, dev);
    }

    barray_free(key_barray);
}

size_t evkey_num(evdev_t *dev)
{
    return dev->key_num;
}

void evkey_foreach(evdev_t *dev, evkey_cb_t key_cb, void *arg)
{
    for (int index = 0; index < dev->key_num; index++)
        key_cb(index, arg);
}

bool evkey_value(evdev_t *dev, unsigned index)
{
    ASSERT(index < dev->key_num);

    return dev->key_array[index].value;
}

int evkey_map(evdev_t *dev, evkey_id_t id)
{
    ASSERT(id < KEY_CNT);

    int index = dev->key_map[id];
    if (index >= KEY_CNT)
        return -1;
    return index;
}

evkey_id_t evkey_id(evdev_t *dev, evidx_t index)
{
    ASSERT(index < dev->key_num);

    return dev->key_array[index].id;
}

///////////////////////////////////////////////////////////////////////////////
//
// Force Feedback Functions
//
///////////////////////////////////////////////////////////////////////////////

#if ENABLE_EFFECTS
static void ff_init(bit_t id, void *arg)
{
    evdev_t *dev = arg;

    dev->ff_array[dev->ff_num].id = id;
    dev->ff_map[id] = dev->ff_num;
    
    dev->ff_num++;
    dev->ff_id = -1;
}

const char *evff_name(evdev_t *dev, evidx_t index)
{
#define EFFECT_CASE(ff) case FF_ ## ff : return # ff

    ASSERT(index < dev->ff_num);

    switch (dev->ff_array[index].id)
    {
        EFFECT_CASE(RUMBLE);
        EFFECT_CASE(PERIODIC);
        EFFECT_CASE(CONSTANT);
        EFFECT_CASE(SPRING);
        EFFECT_CASE(FRICTION);
        EFFECT_CASE(DAMPER);
        EFFECT_CASE(INERTIA);
        EFFECT_CASE(RAMP);

        EFFECT_CASE(SQUARE);
        EFFECT_CASE(TRIANGLE);
        EFFECT_CASE(SINE);
        EFFECT_CASE(SAW_UP);
        EFFECT_CASE(SAW_DOWN);
        EFFECT_CASE(CUSTOM);

        EFFECT_CASE(GAIN);
        EFFECT_CASE(AUTOCENTER);
        
        default: return "UNKNOWN";
    }
};

void evff_init(evdev_t *dev)
{
    if (dev->ff_num != 0)
        return;

    barray_t *ff_barray = barray_init(FF_CNT);
    xioctl(dev->fd, EVIOCGBIT(EV_FF, FF_CNT), barray_data(ff_barray));

    dev->ff_num = barray_count_set(ff_barray);
    if (dev->ff_num > 0)
    {
        dev->ff_array = xalloc(dev->ff_num * sizeof(dev->ff_array[0]));

        dev->ff_num = 0;
        barray_foreach_set(ff_barray, ff_init, dev);
    }

    barray_free(ff_barray);
}

size_t evff_num(evdev_t *dev)
{
    return dev->ff_num;
}

void evff_foreach(evdev_t *dev, evff_cb_t ff_cb, void *arg)
{
    for (int index = 0; index < dev->ff_num; index++)
        ff_cb(index, arg);
}

int evff_map(evdev_t *dev, evff_id_t id)
{
    ASSERT(id < FF_CNT);

    int index = dev->ff_map[id];
    if (index >= FF_CNT)
        return -1;
    return index;
}

evff_id_t evff_id(evdev_t *dev, evidx_t index)
{
    ASSERT(index < dev->ff_num);

    return dev->ff_array[index].id;
}

evff_type_t evff_type(evdev_t *dev, evidx_t index)
{
    ASSERT(index < dev->ff_num);

    switch (dev->ff_array[index].id)
    {
        case FF_GAIN:
        case FF_AUTOCENTER:
            return EVFF_PROPERTY;
            break;
        case FF_CONSTANT:
            return EVFF_CONSTANT;
            break;
        case FF_RUMBLE:
            return EVFF_RUMBLE;
            break;
        case FF_PERIODIC:
            return EVFF_PERIODIC;
            break;
        default:
            return EVFF_UNKNOWN;
            break;
    }
}

bool evff_property(evdev_t *dev, evidx_t index, unsigned value)
{
    ASSERT(index < dev->ff_num);

    struct input_event ie = { 0 };

    ie.type = EV_FF;
    ie.code = dev->ff_array[index].id;
    ie.value = 0xffff * value / 100;

    return (write(dev->fd, &ie, sizeof(ie)) == sizeof(ie));
}

static bool effect_play(evdev_t *dev)
{
    struct input_event ie = { 0 };

    ie.type = EV_FF;
    ie.code = dev->ff_id;
    ie.value = 1;

    return (write(dev->fd, &ie, sizeof(ie)) == sizeof(ie));
}

bool evff_constant(evdev_t *dev, evidx_t index, int level, unsigned direction, unsigned length)
{
    ASSERT(index < dev->ff_num);

    struct ff_effect fe = { 0 };

    fe.type = FF_CONSTANT;
    fe.id = dev->ff_id;
    fe.u.constant.level = 0x7fff * level / 100;
    fe.direction = 0x10000 * direction / 360;
    fe.replay.length = length * 1000;

    if (ioctl(dev->fd, EVIOCSFF, &fe) == -1)
        return false;

    dev->ff_id = fe.id;

    return effect_play(dev);
}

bool evff_rumble(evdev_t *dev, evidx_t index, unsigned strong, unsigned weak, unsigned length)
{
    ASSERT(index < dev->ff_num);

    struct ff_effect fe = { 0 };

    fe.type = FF_RUMBLE;
    fe.id = dev->ff_id;
    fe.u.rumble.strong_magnitude = 0xffff * strong / 100;
    fe.u.rumble.weak_magnitude = 0xffff * weak / 100;
    fe.replay.length = length * 1000;

    if (ioctl(dev->fd, EVIOCSFF, &fe) == -1)
        return false;

    dev->ff_id = fe.id;

    return effect_play(dev);
}

bool evff_periodic(evdev_t *dev, evidx_t index, int level, unsigned direction, unsigned length)
{
    ASSERT(index < dev->ff_num);

    struct ff_effect fe = { 0 };

    fe.type = FF_PERIODIC;
    fe.id = dev->ff_id;
    fe.u.periodic.waveform = FF_SINE;
    fe.u.periodic.period = 1000;
    fe.u.periodic.magnitude = 0x7fff * level / 100;
    fe.direction = 0x10000 * direction / 360;
    fe.replay.length = length * 1000;

    if (ioctl(dev->fd, EVIOCSFF, &fe) == -1)
        return false;

    dev->ff_id = fe.id;

    return effect_play(dev);
}

#endif

///////////////////////////////////////////////////////////////////////////////
//
// Device Functions
//
///////////////////////////////////////////////////////////////////////////////

void evdev_read(evdev_t *dev)
{
    struct input_event ev;

    int got = read(dev->fd, &ev, sizeof(ev));
    if (got == sizeof(ev))
    {
        if (ev.type == EV_ABS && dev->abs_num > 0)
        {
            int index = evabs_map(dev, ev.code);
            if (index >= 0)
            {
                dev->abs_array[index].info.value = ev.value;
                if (dev->abs_cb)
                    dev->abs_cb(index, ev.value, dev->abs_arg);
            }
        }
        else if (ev.type == EV_KEY && dev->key_num > 0)
        {
            int index = evkey_map(dev, ev.code);
            if (index >= 0)
            {
                dev->key_array[index].value = ev.value;
                if (dev->key_cb)
                    dev->key_cb(index, ev.value, dev->key_arg);
            }
        }
    }
    else if (got < 0)
    {
        xerr("read");
    }
}

void evdev_read_cb(evdev_t *dev, evabs_value_cb_t abs_cb, void *abs_arg,
                   evkey_value_cb_t key_cb, void *key_arg)
{
    dev->abs_cb  = abs_cb;
    dev->abs_arg = abs_arg;
    dev->key_cb  = key_cb;
    dev->key_arg = key_arg;
}

int evdev_fileno(evdev_t *dev)
{
    return dev->fd;
}

char *evdev_name(evdev_t *dev)
{
    char name[100] = "?";

    xioctl(dev->fd, EVIOCGNAME(sizeof(name)), name);

    return xstrdup(name);
}

void evdev_id(evdev_t *dev, evdev_id_t *id)
{
    struct input_id input_id;
    xioctl(dev->fd, EVIOCGID, &input_id);
    id->bus     = input_id.bustype;
    id->vendor  = input_id.vendor;
    id->product = input_id.product;
}

evdev_t *evdev_init(const char *file)
{
    evdev_t *dev = xalloc(sizeof(evdev_t));
    dev->fd = open(file, O_RDWR);
    if (dev->fd < 0)
        xerr("%s", file);

    return dev;
}

void evdev_free(evdev_t *dev)
{
    if (dev->fd > 0)
        close(dev->fd);
    xfree(dev->abs_array);
    xfree(dev->key_array);
#if ENABLE_EFFECTS    
    xfree(dev->ff_array);
#endif
    xfree(dev);
}

bool evdev_info(const char *file, evdev_id_t *id, char **name)
{
    int fd = open(file, O_RDONLY);
    if (fd < 0)
        return false;

    barray_t *abs_barray = barray_init(ABS_CNT);
    xioctl(fd, EVIOCGBIT(EV_ABS, ABS_CNT), barray_data(abs_barray));

    size_t count = barray_count_set(abs_barray);

    barray_free(abs_barray);
    if (count == 0)
    {
        close(fd);
        return false;
    }

    if (id)
    {
        xioctl(fd, EVIOCGID, id);
    }

    if (name)
    {
        char tmp[100] = "?";
        xioctl(fd, EVIOCGNAME(sizeof(tmp)), tmp);
        *name = xstrdup(tmp);
    }

    close(fd);

    return true;
}
