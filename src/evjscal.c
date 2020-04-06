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
#include <errno.h>
#include <err.h>
#include <limits.h>
#include <getopt.h>
#include <poll.h>
#include <sys/types.h>

#include "caldb.h"
#include "util.h"
#include "evdev.h"
#if ENABLE_JOYSTICK
#include "jsdev.h"
#endif
#include "config.h"

///////////////////////////////////////////////////////////////////////////////

typedef enum op {
    OP_NONE,
    OP_LIST,
    OP_READ,
    OP_DELETE,
    OP_WRITE,
    OP_CONFIG,
    OP_CALIBRATE,
    OP_SET,
    OP_GET,
} op_t;

typedef struct cal_node
{
    caldb_record_t  rec;
    struct cal_node *next;
} cal_node_t;

typedef struct cal_state
{
    const char *name;
    int index;
    int min;
    int max;
    bool finished;
} cal_state_t;

#define VALUES_PER_AXIS 5

#define VERBOSE(...)  ({ if (verbose) printf(__VA_ARGS__); })

///////////////////////////////////////////////////////////////////////////////

static bool         verbose;
static evdev_t      *evdev;
static evdev_id_t   evid;

///////////////////////////////////////////////////////////////////////////////
//
// List Operation
//
///////////////////////////////////////////////////////////////////////////////

static bool rec_lister(const evdev_id_t *dev, const caldb_record_t *rec, void *arg)
{
    const char *comma = ",";
    static char *nl = "";
    if (evid.bus != dev->bus || evid.vendor != dev->vendor || evid.product != dev->product) {
        printf("%s%04x:%04x:%04x = ", nl, dev->bus, dev->vendor, dev->product);
        comma = "";
        nl = "\n";
        evid = *dev;
    }
    printf("%s%d,%d,%d,%d,%d", comma, rec->axis, rec->cal.min,
        rec->cal.max, rec->cal.fuzz, rec->cal.flat);

    return true;
}

static void op_list(const char *db_file)
{
    char *err_msg;

    caldb_t *db = caldb_init(db_file, &err_msg);
    if (!db)
        xerrx("%s: %s", db_file, err_msg);

    if (!caldb_read(db, NULL, rec_lister, NULL, &err_msg))
        xerrx("%s", err_msg);

    printf("\n");

    caldb_free(db);
}

///////////////////////////////////////////////////////////////////////////////
//
// Read Operation
//
///////////////////////////////////////////////////////////////////////////////

static bool rec_reader(const evdev_id_t *dev, const caldb_record_t *rec, void *arg)
{
    cal_node_t ***prevpp = (cal_node_t***) arg;

    VERBOSE("Read axis %d calibration min:%d max:%d fuzz:%d flat:%d\n",
            rec->axis, rec->cal.min, rec->cal.max, rec->cal.fuzz, rec->cal.flat);

    size_t index = evabs_map(evdev, rec->axis);
    if (index >= 0)
    {
        cal_node_t *node = xalloc(sizeof(cal_node_t));
        node->rec = *rec;

        **prevpp = node;
        *prevpp = &node->next;
    }

    return true;
}

static cal_node_t *readdb(const char *db_file)
{
    char *err_msg;

    caldb_t *db = caldb_init(db_file, &err_msg);
    if (!db)
        xerrx("%s: %s", db_file, err_msg);

    cal_node_t *list = NULL;
    cal_node_t **prev = &list;
    if (!caldb_read(db, &evid, rec_reader, &prev, &err_msg))
        xerrx("%s", err_msg);

    caldb_free(db);

    return list;
}

static void cal_list_free(cal_node_t *list)
{
    cal_node_t *next;

    while (list)
    {
        next = list->next;
        xfree(list);
        list = next;
    }
}

static void op_read(const char *db_file)
{
    cal_node_t *list = readdb(db_file);
    if (list == NULL)
    {
        VERBOSE("No calibration records in database\n");
        return;
    }

    char *comma = "";
    for (cal_node_t *node = list; node != NULL; node = node->next)
    {
        if (!verbose)
        {
            caldb_record_t *rec = &node->rec;
            printf("%s%d,%d,%d,%d,%d", comma, rec->axis, rec->cal.min,
                rec->cal.max, rec->cal.fuzz, rec->cal.flat);
            comma = ",";
        }
    }
    if (!verbose)
        printf("\n");

    cal_list_free(list);
}

///////////////////////////////////////////////////////////////////////////////
//
// Delete Operation
//
///////////////////////////////////////////////////////////////////////////////

static void op_delete(const char *db_file)
{
    char *err_msg;

    caldb_t *db= caldb_init(db_file, &err_msg);
    if (!db)
        xerrx("db_file: %s", err_msg);

    if (!caldb_delete(db, &evid, &err_msg))
        xerrx("%s", err_msg);

    caldb_free(db);
}

///////////////////////////////////////////////////////////////////////////////
//
// Write Operation
//
///////////////////////////////////////////////////////////////////////////////

static bool rec_writer(caldb_record_t *rec, void *arg)
{
    cal_node_t **nodepp = (cal_node_t **)arg;
    cal_node_t *node = *nodepp;

    if (*nodepp == NULL)
        return false;

    *rec = node->rec;

    VERBOSE("Write axis %d calibration min:%d max:%d fuzz:%d flat:%d\n",
            rec->axis, rec->cal.min, rec->cal.max, rec->cal.fuzz, rec->cal.flat);

    *nodepp = node->next;

    return true;
}

static void writedb(const char *db_file, cal_node_t *list)
{
    char *err_msg;

    caldb_t *db= caldb_init(db_file, &err_msg);
    if (!db)
        xerrx("%s: %s", db_file, err_msg);

    if (!caldb_write(db, &evid, rec_writer, &list, &err_msg))
        xerrx("%s", err_msg);

    caldb_free(db);
}

static cal_node_t *values_parse(const char *str)
{
    int abs_num = evabs_num(evdev);
    int max_values = abs_num * VALUES_PER_AXIS;    
    int intvals[max_values];
    const char *end = str;
    ssize_t count = 0;
    long result;

    while (*end != '\0' && count < max_values)
    {
        errno = 0;
        result = strtol(end, (char**)&end, 0);
        if ((errno == ERANGE && (result == LONG_MIN || result == LONG_MAX)) ||
            (errno != 0 && result == 0))
            xerrx("Invalid number format");

        if (*end == ',')
            end++;
        else if (*end != '\0')
            xerrx("Invalid number seperator");

        intvals[count++] = result;
    }

    if (count == 0)
        xerrx("Invalid values format");
    else if (count > max_values)
        xerrx("Too many values given");
    else if (count % VALUES_PER_AXIS != 0)
        xerrx("Invalid number of values");

    cal_node_t *list = NULL;
    cal_node_t **prev = &list;
    for (int i = 0; i < count; i += VALUES_PER_AXIS)
    {
        if (evabs_map(evdev, intvals[i]) < 0)
            xerrx("Axis %d is not valid for device", intvals[i]);

        cal_node_t *node = xalloc(sizeof(cal_node_t));

        caldb_record_t *rec = &node->rec;
        rec->axis     = intvals[i];
        rec->cal.min  = intvals[i + 1];
        rec->cal.max  = intvals[i + 2];
        rec->cal.fuzz = intvals[i + 3];
        rec->cal.flat = intvals[i + 4];

        *prev = node;
        prev = &node->next;

        int range = (rec->cal.max - rec->cal.min) / 2;
        if (rec->cal.min >= rec->cal.max)
            xerrx("Minimum exceeds maximum");
        else if (rec->cal.fuzz >= range)
            xerrx("Fuzz value is out of range");
        else if (rec->cal.flat >= range)
            xerrx("Flat value is out of range");
    }

    return list;
}

static void op_write(const char *db_file, const char *values)
{
    cal_node_t *list = values_parse(values);

    writedb(db_file, list);

    cal_list_free(list);
}

///////////////////////////////////////////////////////////////////////////////
//
// Configure Operation
//
///////////////////////////////////////////////////////////////////////////////
#if ENABLE_JOYSTICK
static jsdev_t *joystick_open(void)
{
    jsdev_t *jsdev = NULL;
    char *jsfile = jsdev_from_evdev(evdev_fileno(evdev));
    if (jsfile)
    {
        VERBOSE("Joystick device: %s\n", jsfile);
        jsdev = jsdev_init(jsfile);
        jsaxis_init(jsdev);
        xfree(jsfile);
    }
    return jsdev;
}

static void joystick_cal(jsdev_t *jsdev, int index, evcal_t *cal)
{
    if (!jsdev)
        return;

    // jsdev index is not necessarily the same as the evdev index
    // so map the jsdev index via the ID which is always the same
    int jsidx = jsaxis_map(jsdev, evabs_id(evdev, index));
    if (jsidx >= 0)
    {
        // evdev does not support deadzones so just split the difference
        int center = cal->min + (cal->max - cal->min) / 2;
        jscal_t jscal = {
            .min        = cal->min,
            .max        = cal->max,
            .center_min = center,
            .center_max = center,
        };
        jsaxis_cal_set(jsdev, jsidx, &jscal);
    }
}

static void joystick_close(jsdev_t *jsdev)
{
    if (!jsdev)
        return;

    jsaxis_cal_activate(jsdev);
    jsdev_free(jsdev);
}
#endif // ENABLE_JOYSTICK

static void calibrate(cal_node_t *list)
{
#if ENABLE_JOYSTICK
    jsdev_t *jsdev = joystick_open();
#endif

    for (cal_node_t *node = list; node != NULL; node = node->next)
    {
        caldb_record_t *rec = &node->rec;
        int index = evabs_map(evdev, rec->axis);
        if (index >= 0)
        {
            VERBOSE("Set axis %d calibration min:%d max:%d fuzz:%d flat:%d\n",
                    rec->axis, rec->cal.min, rec->cal.max, rec->cal.fuzz, rec->cal.flat);

            evabs_cal_set(evdev, index, &rec->cal);
#if ENABLE_JOYSTICK
            joystick_cal(jsdev, index, &rec->cal);
#endif            
        }
    }

#if ENABLE_JOYSTICK
    joystick_close(jsdev);
#endif
}

static void op_config(const char *db_file)
{
    cal_node_t *list = readdb(db_file);
    if (list == NULL)
    {
        VERBOSE("No calibration records in database\n");
        return;
    }

    calibrate(list);

    cal_list_free(list);
}

///////////////////////////////////////////////////////////////////////////////
//
// Set Operation
//
///////////////////////////////////////////////////////////////////////////////
static void op_set(const char *values)
{
    cal_node_t *list = values_parse(values);

    calibrate(list);
}

///////////////////////////////////////////////////////////////////////////////
//
// Get Operation
//
///////////////////////////////////////////////////////////////////////////////
static void op_get(void)
{
    int abs_num = evabs_num(evdev);
    evcal_t cal;

    char *comma = "";
    for (int index = 0; index < abs_num; index++)
    {
        int id = evabs_id(evdev, index);
        evabs_cal_get(evdev, index, &cal);
        if (verbose)
        {
            VERBOSE("Get axis %d calibration min:%d max:%d fuzz:%d flat:%d\n",
                    id, cal.min, cal.max, cal.fuzz, cal.flat);
        }
        else
        {
            printf("%s%d,%d,%d,%d,%d", comma,
                    id, cal.min, cal.max, cal.fuzz, cal.flat);
            comma = ",";                
        }
    }
    if (!verbose)
        printf("\n");
}

///////////////////////////////////////////////////////////////////////////////
//
// Calibrate Operation
//
///////////////////////////////////////////////////////////////////////////////

void abs_event(evidx_t index, int value, void *arg)
{
    cal_state_t *state = arg;

    if (index == state->index)
    {
        if (value < state->min)
            state->min = value;
        if (value > state->max)
            state->max = value;
        printf("Axis %s Value:%d Min:%d Max:%d               \r",
                state->name, value, state->min, state->max);
        fflush(stdout);
    }
}

void key_event(evidx_t index, bool value, void *arg)
{
    cal_state_t *state = arg;

    if (value)
    {
        state->finished = true;
        printf("\n\n");
    }
}

static void op_calibrate(const char *db_file)
{
    int abs_num = evabs_num(evdev);

    evkey_init(evdev);

    cal_state_t state;
    evdev_read_cb(evdev, abs_event, &state, key_event, &state);

    struct pollfd fds[] =
    {
        { .fd = evdev_fileno(evdev), .events = POLLIN | POLLPRI },
    };

    cal_node_t *list = NULL;
    cal_node_t **prev = &list;
    for (int index = 0; index < abs_num; index++)
    {
        const char *name = evabs_name(evdev, index);
        int value = evabs_value(evdev, index);

        printf("Move %s axis to its extremes and press a button to continue.\n", name);

        state = (cal_state_t) {
            .name     = name,
            .index    = index,
            .min      = value,
            .max      = value,
            .finished = false
        };
        abs_event(index, value, &state);

        while (!state.finished)
        {
            int nfds = poll(fds, 1, -1);
            if (nfds < 0)
                xerrx("poll");

            if (fds[0].revents & (POLLIN | POLLPRI))
            {
                evdev_read(evdev);
                fds[0].revents = 0;
            }
        }

        cal_node_t *node = xalloc(sizeof(cal_node_t));
        node->rec.axis = evabs_id(evdev, index);

        evcal_t *cal = &node->rec.cal;
        evabs_cal_get(evdev, index, cal);
        cal->min = state.min;
        cal->max = state.max;

        *prev = node;
        prev = &node->next;
    }

    printf("Saving calibration\n");

    calibrate(list);

    writedb(db_file, list);

    cal_list_free(list);
}

///////////////////////////////////////////////////////////////////////////////

static void op_check(op_t *op, op_t val)
{
    if (*op != OP_NONE)
        xerrx("Only one operation is allowed at a time");
    *op = val;
}

static int usage(void)
{
    fprintf(stderr,
        "Usage: evjscal [OPTION]... [DEVICE]\n"
        "Manage calibration values for joystick event DEVICE.\n"
        "\n"
        "Options:\n"
        "  -h, --help            Print this help\n"
        "  -v, --verbose         Display verbose information\n"
        "  -d, --database FILE   Use the specified database FILE\n"
        "  -l, --list            List all calibration values in database\n"
        "  -r, --read            Read and display calibration values from database\n"
        "  -D, --delete          Delete calibration values from database\n"
        "  -w, --write VALUES    Write calibration VALUES to database\n"
        "  -c, --config          Read calibration values from database and configure\n"
        "                        them in DEVICE\n"
        "  -s  --set VALUES      Set new calibration VALUES in DEVICE\n"
        "  -g  --get             Get the calibration VALUES configured in DEVICE\n"
        "  -C, --calibrate       Execute calibration procedure\n"
        "\n"
        "  VALUES is a comma separated list: [axis],[min],[max],[fuzz],[flat],...\n"
        "\n"
        "Examples:\n"
        "  Read the database values with concise output:\n"
        "    evjscal -r /dev/input/event11\n"
        "  Read the database values with human output:\n"
        "    evjscal -v -r /dev/input/event11\n"
        "  Write new database values:\n"
        "    evjscal -w 2,255,2,15 /dev/input/event11\n"
        "  Delete database values:\n"
        "    evjscal -D /dev/input/event11\n"
    );

    return 1;
}

int main(int argc, char *argv[])
{
    op_t op = OP_NONE;
    static struct option long_options[] = {
        { "help",       no_argument,       NULL,  'h' },
        { "verbose",    no_argument,       NULL,  'v' },
        { "database",   required_argument, NULL,  'd' },
        { "list",       no_argument,       NULL,  'l' },
        { "read",       no_argument,       NULL,  'r' },
        { "delete",     no_argument,       NULL,  'D' },
        { "write",      required_argument, NULL,  'w' },
        { "config",     no_argument,       NULL,  'c' },
        { "calibrate",  no_argument,       NULL,  'C' },
        { "set",        required_argument, NULL,  's' },
        { "get",        no_argument,       NULL,  'g' },
        { 0,            0,                 NULL,  0   }
    };
    char *values = "";
    char *db_file = NULL;

    while (1)
    {
        int option_index = 0;
        int c = getopt_long(argc, argv, "hvd:lrDw:cCs:g", long_options, &option_index);
        if (c == -1)
            break;

        switch (c)
        {
            case 'v':
                verbose = true;
                break;
            case 'd':
                if (!db_file)
                    db_file = xstrdup(optarg);
                break;
            case 'l':
                op_check(&op, OP_LIST);
                break;
            case 'r':
                op_check(&op, OP_READ);
                break;
            case 'D':
                op_check(&op, OP_DELETE);
                break;
            case 'w':
                op_check(&op, OP_WRITE);
                values = optarg;
                break;
            case 'c':
                op_check(&op, OP_CONFIG);
                break;
            case 'C':
                op_check(&op, OP_CALIBRATE);
                break;
            case 's':
                op_check(&op, OP_SET);
                values = optarg;
                break;
            case 'g':
                op_check(&op, OP_GET);
                break;
            default:
            case 'h':
                return usage();
        }
    }

    if (op != OP_LIST) {
        if (optind == argc)
        {
            warnx("Missing input DEVICE");
            usage();
            return 1;
        }
        else if (optind != argc - 1)
        {
            warnx("Extra parameters on command line");
            usage();
            return 1;
        }
    }

    if (!db_file)
    {
        db_file = config_path(CALDB_DEFAULT_NAME);
        VERBOSE("Database file: %s\n", db_file);
    }

    if (op == OP_LIST) {
        op_list(db_file);
    }
    else {
        evdev = evdev_init(argv[optind]);

        evdev_id(evdev, &evid);
        VERBOSE("Device: %04x:%04x on bus %d\n", evid.vendor, evid.product, evid.bus);

        evabs_init(evdev);
        if (evabs_num(evdev) == 0)
            xerrx("Device does not have absolute axes");

        switch (op)
        {
            case OP_READ:
                op_read(db_file);
                break;
            case OP_DELETE:
                op_delete(db_file);
                break;
            case OP_WRITE:
                op_write(db_file, values);
                break;
            case OP_CONFIG:
                op_config(db_file);
                break;
            case OP_CALIBRATE:
                op_calibrate(db_file);
                break;
            case OP_SET:
                op_set(values);
                break;
            case OP_GET:
                op_get();
                break;
            default:
                xerrx("No operation specified");
                break;
        }

        evdev_free(evdev);
    }

    xfree(db_file);

    return 0;
}
