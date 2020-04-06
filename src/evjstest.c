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
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <getopt.h>
#include <errno.h>
#include <err.h>
#include <limits.h>

#include <linux/input.h>

#include "util.h"
#include "device.h"
#include "view.h"
#include "caldb.h"

#if ENABLE_EFFECTS
static void handle_effect(device_t *dev, view_t *view)
{
    effect_t *effect = view_effect_get(view);
    switch (effect->type)
    {
        case EFFECT_PROPERTY:
        {
            int value = view_prompt_int(view, 0, 100, "Enter %s value [0-100]> ", effect->name);
            if (value != INT_MAX)
            {
                if (!device_effect_property(dev, effect, value))
                    view_error(view, "Effect failed: %s", strerror(errno));
            }
            break;
        }
        case EFFECT_CONSTANT:
        {
            int level = view_prompt_int(view, -100, 100, "Enter %s level [-100-100]> ", effect->name);
            if (level != INT_MAX)
            {
                int direction = view_prompt_int(view, 0, 359, "Enter %s direction [0-359]> ", effect->name);
                if (direction != INT_MAX)
                {
                    int length = view_prompt_int(view, 1, 60, "Enter %s seconds [1-60]> ", effect->name);
                    if (length != INT_MAX)
                    {
                        if (!device_effect_constant(dev, effect, level, direction, length))
                            view_error(view, "Effect failed: %s", strerror(errno));
                    }
                }
            }
            break;
        }
        case EFFECT_RUMBLE:
        {
            int strong = view_prompt_int(view, 0, 100, "Enter %s strong level [0-100]> ", effect->name);
            if (strong != INT_MAX)
            {
                int weak = view_prompt_int(view, 0, 100, "Enter %s weak level [0-100]> ", effect->name);
                if (weak != INT_MAX)
                {
                    int length = view_prompt_int(view, 1, 60, "Enter %s seconds [1-60]> ", effect->name);
                    if (length != INT_MAX)
                    {
                        if (!device_effect_rumble(dev, effect, strong, weak, length))
                            view_error(view, "Effect failed: %s", strerror(errno));
                    }
                }
            }
            break;
        }
        case EFFECT_PERIODIC:
        {
            int level = view_prompt_int(view, -100, 100, "Enter %s level [-100-100]> ", effect->name);
            if (level != INT_MAX)
            {
                int direction = view_prompt_int(view, 0, 359, "Enter %s direction [0-359]> ", effect->name);
                if (direction != INT_MAX)
                {
                    int length = view_prompt_int(view, 1, 60, "Enter %s seconds [1-60]> ", effect->name);
                    if (length != INT_MAX)
                    {
                        if (!device_effect_periodic(dev, effect, level, direction, length))
                            view_error(view, "Effect failed: %s", strerror(errno));
                    }
                }
            }
            break;
        }
        default:
            view_error(view, "Unsupported effect");
            break;
    }
}
#endif

static bool write_axis(caldb_record_t *rec, void **arg)
{
    device_t *dev = arg[0];
    axis_t **axis_ptr = arg[1];
    axis_t *axis = *axis_ptr;

    if (axis >= &dev->axis_array[dev->axis_num])
        return false;

    rec->axis = axis->id;
    rec->cal = axis->cal;

    *axis_ptr = ++axis;

    return true;
}

static void write_device(caldb_t *db, device_t *dev, view_t *view)
{
    char *err_msg;
    evdev_id_t caldb_dev = {
        .bus     = dev->id.bus,
        .vendor  = dev->id.vendor,
        .product = dev->id.product
    };
    axis_t *axis = dev->axis_array;
    void *args[] = { dev, &axis };
    if (!caldb_write(db, &caldb_dev, (caldb_writer_t)write_axis, &args, &err_msg))
    {
        view_error(view, err_msg);
        caldb_err_free(err_msg);
        return;
    }

    dev->dirty = false;
}

static bool read_axis(const evdev_id_t *unused, const caldb_record_t *rec, void *arg)
{
    device_t *dev = arg;

    axis_t *axis = device_axis_get(dev, rec->axis);
    if (axis)
        axis->cal  = rec->cal;

    return true;
}

static void read_device(caldb_t *db, device_t *dev, view_t *view)
{
    char *err_msg;

    if (!caldb_read(db, &dev->id, read_axis, dev, &err_msg))
    {
        view_error(view, err_msg);
        caldb_err_free(err_msg);
        return;
    }

    dev->dirty = false;
}

static void axis_change(axis_t *axis, void *arg)
{
    view_t *view = arg;
    view_axis_value(view, axis, axis->value);
}

static void button_change(button_t *button, void *arg)
{
    view_t *view = arg;
    view_button_value(view, button, button->value);
}

static void event_loop(caldb_t *db, device_t *dev, view_t *view)
{
    struct pollfd fds[2] =
    {
        { .fd = STDIN_FILENO,       .events = POLLIN },
        { .fd = device_fileno(dev), .events = POLLIN | POLLPRI },
    };

    device_read_cb(dev, axis_change, button_change, view);

    bool running = true;
    while (running)
    {
        if (poll(fds, 2, -1) < 0 && errno != EINTR)
            xerr("poll");

        view_resize(view);

        if (fds[0].revents & POLLIN)
        {
            int key = view_key(view);
            axis_t *axis = view_axis_get(view);

            if (key == 'q')
            {
                if (dev->dirty)
                {
                    if (view_confirm(view, "Save changed calibrations? [Y/N]> "))
                        write_device(db, dev, view);
                }
                running = false;
            }
            else if (key == '\n' || key == '\r' || key == KEY_ENTER)
            {
                bool cursors = view_axis_cursors_get(view);
                if (cursors)
                {
                    AXIS_FOREACH(dev, axis)
                    {
                        axis->cal.min = axis->minimum;
                        axis->cal.max = axis->maximum;
                    }

                    device_calibrate(dev);

                    dev->dirty = true;
                    view_info_refresh(view);
                    view_axis_cursors_set(view, false);
                }
            }
            else if (key == 'c')
            {
                bool cursors = view_axis_cursors_get(view);
                if (!cursors)
                {
                    AXIS_FOREACH(dev, axis)
                    {
                        axis->maximum = axis->value;
                        axis->minimum = axis->value;
                    }
                }
                view_axis_cursors_set(view, !cursors);
            }
            else if (key == 'f')
            {
                int fuzz = view_prompt_int(view, 0,
                    (axis->cal.max - axis->cal.min) / 2,
                    "Enter %s fuzz value> ", axis->name);
                if (fuzz != INT_MAX)
                {
                    axis->cal.fuzz = fuzz;
                    device_axis_calibrate(dev, axis);
                    view_axis_calibration(view, axis);
                }
            }
            else if (key == 't')
            {
                int flat = view_prompt_int(view, 0,
                    (axis->cal.max - axis->cal.min) / 2,
                    "Enter %s flat value> ", axis->name);
                if (flat != INT_MAX)
                {
                    axis->cal.flat = flat;
                    device_axis_calibrate(dev, axis);
                    view_axis_calibration(view, axis);
                }
            }
            else if (key == KEY_UP || key == 'k')
            {
                view_axis_prev(view);
            }
            else if (key == KEY_DOWN || key == 'j')
            {
                view_axis_next(view);
            }
            else if (key == KEY_PPAGE)
            {
                view_axis_pageup(view);
            }
            else if (key == KEY_NPAGE)
            {
                view_axis_pagedn(view);
            }
#if ENABLE_EFFECTS
            else if (key == KEY_LEFT || key == 'h')
            {
                view_effect_prev(view);
            }
            else if (key == KEY_RIGHT || key == 'l')
            {
                view_effect_next(view);
            }
            else if (key == 'e')
            {
                handle_effect(dev, view);
            }
#endif
            else if (key == 'r')
            {
                read_device(db, dev, view);
                view_info_refresh(view);
                view_axis_refresh(view);
            }
            else if (key == 'w')
            {
                write_device(db, dev, view);
                view_info_refresh(view);
            }
            else if (key == '?')
            {
                view_help(view);
            }

            fds[0].revents = 0;
        }

        if (fds[1].revents & (POLLIN | POLLPRI))
        {
            device_read(dev);

            fds[1].revents = 0;
        }
    }
}

static int usage(void)
{
    fprintf(stderr,
        "Usage: evjstest [OPTIONS] [DEVICE]\n"
        "Calibrate the absolute axes for a joystick event DEVICE.\n"
        "A selection list is presented if no DEVICE is given.\n"
        "\n"
        "Options:\n"
        "  -h, --help            Print this help\n"
        "  -d, --database FILE   Use the specified database FILE\n"
        "\n"
        "Examples:\n"
        "  evjstest\n"
        "  evjstest /dev/input/event11\n"
        "  evjstest -d ~/evutils.db /dev/input/event4\n"
    );

    return 1;
}

int main(int argc, char *argv[])
{
    char *dev_file = NULL;
    char *db_file = NULL;

    static struct option long_options[] = {
        { "help",       no_argument,       NULL, 'h' },
        { "database",   required_argument, NULL, 'd' },
        { 0,            0,                 NULL,  0  }
    };

    while (1)
    {
        int option_index = 0;
        int c = getopt_long(argc, argv, "hd:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c)
        {
            case 'd':
                if (!db_file)
                    db_file = xstrdup(optarg);
                break;
            case 'h':
            default:
                return usage();
        }
    }

    if (optind == argc - 1)
    {
        dev_file = xstrdup(argv[optind]);
    }
    else if (optind != argc)
    {
        warnx("Extra parameters on command line");
        return usage();
    }
    else
    {
        dev_file = device_select();
        if (!dev_file)
            return 0;
    }

    if (!db_file)
        db_file = config_path(CALDB_DEFAULT_NAME);

    char *err_msg = NULL;
    caldb_t *db = caldb_init(db_file, &err_msg);
    if (!db)
        xerrx("%s: %s", db_file, err_msg);

    device_t *dev = device_init(dev_file);
    view_t *view = view_init(dev, db_file);

    xon_exit((exit_callback_t) view_free, view);

    event_loop(db, dev, view);

    view_free(view);
    device_free(dev);
    caldb_free(db);

    xfree(dev_file);
        xfree(db_file);

    return 0;
}