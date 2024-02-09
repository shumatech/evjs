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
#if ENABLE_JOYSTICK
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <linux/input.h>
#include <linux/joystick.h>

#include "util.h"
#include "jsdev.h"

#define AXIS_CNT    ABS_CNT

struct jsdev
{
    int             fd;
    uint8_t         axis_num;
    struct js_corr  *axis_corr;
    uint8_t         axis_map[AXIS_CNT];
};

///////////////////////////////////////////////////////////////////////////////
//
// Axis Functions
//
///////////////////////////////////////////////////////////////////////////////

bool jsaxis_cal_set(jsdev_t *dev, jsidx_t index, const jscal_t *cal)
{
    ASSERT(index < AXIS_CNT);
    uint8_t id = dev->axis_map[index];
    ASSERT(id < dev->axis_num);

    if (cal->min >= cal->max || cal->center_min > cal->center_max ||
        cal->center_min <= cal->min || cal->center_max >= cal->max)
        return false;

    struct js_corr *cp = &dev->axis_corr[id];
    cp->type = JS_CORR_BROKEN;
    cp->prec = 0;
    cp->coef[0] = cal->center_min;
    cp->coef[1] = cal->center_max;
    cp->coef[2] = (32767 * 16384) / (cal->center_min - cal->min);
    cp->coef[3] = (32767 * 16384) / (cal->max - cal->center_max);

    return true;
}

void jsaxis_cal_activate(jsdev_t *dev)
{
    xioctl(dev->fd, JSIOCSCORR, dev->axis_corr);
}

int jsaxis_map(jsdev_t *dev, jsaxis_id_t id)
{
    ASSERT(id < AXIS_CNT);

    int index = dev->axis_map[id];
    if (index >= AXIS_CNT)
        return -1;
    return index;
}

void jsaxis_init(jsdev_t *dev)
{
    if (dev->axis_num != 0)
        return;

    for (int i = 0; i < AXIS_CNT; i++)
        dev->axis_map[i] = AXIS_CNT;

    xioctl(dev->fd, JSIOCGAXES, &dev->axis_num);

    if (dev->axis_num > 0)
    {
        xioctl(dev->fd, JSIOCGAXMAP, dev->axis_map);

        dev->axis_corr = xalloc(sizeof(dev->axis_corr[0]) * dev->axis_num);
        xioctl(dev->fd, JSIOCGCORR, dev->axis_corr);
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// Device Functions
//
///////////////////////////////////////////////////////////////////////////////

jsdev_t *jsdev_init(const char *file)
{
    jsdev_t *dev = xalloc(sizeof(jsdev_t));

    dev->fd = open(file, O_RDWR);
    if (dev->fd < 0)
        xerr("%s", file);

    return dev;
}

void jsdev_free(jsdev_t *dev)
{
    if (dev->fd > 0)
        close(dev->fd);
    xfree(dev->axis_corr);
    xfree(dev);
}

static int js_filter(const struct dirent *entry)
{
    int devnum;
    return (sscanf(entry->d_name, "js%d", &devnum) == 1);
}

char *jsdev_from_evdev(int fd)
{
    // Get the major/minor numbers of the device
    struct stat sb;
    if (fstat(fd, &sb) != 0)
        return NULL;
    if ((sb.st_mode & S_IFCHR) == 0)
        return NULL;

    // Scan the device's sysfs for a js* directory
    struct dirent **dent;
    char js_path[100];
    size_t len = snprintf(js_path, sizeof(js_path), "/sys/dev/char/%d:%d/device/", major(sb.st_rdev), minor(sb.st_rdev));
    if (len >= sizeof(js_path))
        return NULL;

    int entries = scandir(js_path, &dent, js_filter, versionsort);
    if (entries < 0)
        return NULL;

    // Open the joystick device if the js* was found
    char *jsfile = NULL;
    if (entries == 1)
    {
        if (strlen(dent[0]->d_name) + len + 1 >= sizeof(js_path))
            return NULL;
        xasprintf(&jsfile, "/dev/input/%s", dent[0]->d_name);
    }

    while (entries--)
        xfree(dent[entries]);
    xfree(dent);

    return jsfile;
}

#endif // ENABLE_JOYSTICK