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

#include <stdbool.h>

#include "config.h"
#include "evdev.h"

#define CALDB_DEFAULT_EXT      ".db"
#define CALDB_DEFAULT_NAME     "cal" CALDB_DEFAULT_EXT

typedef struct caldb_record
{
    int        axis;
    evcal_t    cal;
} caldb_record_t;

typedef struct caldb caldb_t;

typedef bool (*caldb_reader_t)(const evdev_id_t *dev, const caldb_record_t *rec, void *arg);

bool caldb_read(caldb_t *db, const evdev_id_t *dev, caldb_reader_t reader, void *arg, char **err_msg);

typedef bool (*caldb_writer_t)(caldb_record_t *rec, void *arg);

bool caldb_write(caldb_t *db, const evdev_id_t *dev, caldb_writer_t writer, void *arg, char **err_msg);

bool caldb_delete(caldb_t *db, const evdev_id_t *dev, char **err_msg);

void caldb_err_free(char *err_msg);

caldb_t *caldb_init(const char *file, char **err_msg);

void caldb_free(caldb_t *);