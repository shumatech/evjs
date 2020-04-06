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
#include <stdio.h>
#include <string.h>
#include <err.h>

#include <sqlite3.h>

#include "caldb.h"
#include "util.h"

struct caldb
{
    sqlite3 *sqlite3;
};

bool caldb_write(caldb_t *db, const evdev_id_t *dev, caldb_writer_t writer, void *arg, char **err_msg)
{
    if (err_msg)
        *err_msg = NULL;

    if (sqlite3_exec(db->sqlite3, "BEGIN;", NULL, 0, err_msg) != SQLITE_OK)
        return false;

    caldb_record_t rec;
    while (writer(&rec, arg))
    {
        char *sql = NULL;
        xasprintf(&sql,
            "REPLACE INTO calibration "
            "VALUES(%d,%d,%d,%d,%d,%d,%d,%d);",
            dev->bus, dev->vendor, dev->product, rec.axis,
            rec.cal.min, rec.cal.max, rec.cal.fuzz, rec.cal.flat);

        int rc = sqlite3_exec(db->sqlite3, sql, NULL, 0, err_msg);
        xfree(sql);

        if (rc != SQLITE_OK)
        {
            sqlite3_exec(db->sqlite3, "ROLLBACK;", NULL, 0, NULL);
            return false;
        }
    }

    if (sqlite3_exec(db->sqlite3, "COMMIT;", NULL, 0, err_msg) != SQLITE_OK)
        return false;

    if (err_msg && *err_msg) {
        xfree(*err_msg);
        *err_msg = NULL;
    }
    
    return true;
}

static int caldb_read_callback(void **data, int argc, char **argv, char **col_name)
{
    caldb_reader_t reader = data[0];
    void *arg = data[1];
    caldb_record_t rec = { 0 };
    evdev_id_t dev = { 0 };

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(col_name[i], "bus") == 0)
            dev.bus = atoi(argv[i]);
        else if (strcmp(col_name[i], "vendor") == 0)
            dev.vendor = atoi(argv[i]);
        else if (strcmp(col_name[i], "product") == 0)
            dev.product = atoi(argv[i]);
        else if (strcmp(col_name[i], "axis") == 0)
            rec.axis = atoi(argv[i]);
        else if (strcmp(col_name[i], "min") == 0)
            rec.cal.min = atoi(argv[i]);
        else if (strcmp(col_name[i], "max") == 0)
            rec.cal.max = atoi(argv[i]);
        else if (strcmp(col_name[i], "fuzz") == 0)
            rec.cal.fuzz = atoi(argv[i]);
        else if (strcmp(col_name[i], "flat") == 0)
            rec.cal.flat = atoi(argv[i]);
    }

    if (!reader(&dev, &rec, arg))
        return -1;

    return 0;
}

bool caldb_read(caldb_t *db, const evdev_id_t *dev, caldb_reader_t reader, void *arg, char **err_msg)
{
    if (err_msg)
        *err_msg = NULL;

    char *sql = NULL;
    if (!dev) {
        xasprintf(&sql, "SELECT * FROM calibration;");
    }
    else {
        xasprintf(&sql, "SELECT * FROM calibration WHERE bus=%d and vendor=%d AND product=%d;",
                  dev->bus, dev->vendor, dev->product);
    }

    void *data[] = { reader, arg };
    int rc = sqlite3_exec(db->sqlite3, sql, (void*)caldb_read_callback, data, err_msg);
    xfree(sql);

    if (rc != SQLITE_OK && rc != SQLITE_ABORT)
        return false;

    if (err_msg && *err_msg) {
        xfree(*err_msg);
        *err_msg = NULL;
    }

    return true;
}

bool caldb_delete(caldb_t *db, const evdev_id_t *dev, char **err_msg)
{
    if (err_msg)
        *err_msg = NULL;

    char *sql = NULL;
    xasprintf(&sql, "DELETE FROM calibration WHERE bus=%d and vendor=%d AND product=%d;",
              dev->bus, dev->vendor, dev->product);

    int rc = sqlite3_exec(db->sqlite3, sql, NULL, NULL, err_msg);
    xfree(sql);

    if (rc != SQLITE_OK && rc != SQLITE_ABORT)
        return false;

    if (err_msg && *err_msg) {
        xfree(*err_msg);
        *err_msg = NULL;
    }

    return true;

}


void caldb_err_free(char *err_msg)
{
    sqlite3_free(err_msg);
}

caldb_t *caldb_init(const char *file, char **err_msg)
{
    if (err_msg)
        *err_msg = NULL;

    caldb_t *db = xalloc(sizeof(caldb_t));

    int rc = sqlite3_open(file, &db->sqlite3);
    if (rc)
    {
        *err_msg = xstrdup(sqlite3_errmsg(db->sqlite3));
        caldb_free(db);
        return NULL;
    }

    char *sql =
        "CREATE TABLE IF NOT EXISTS calibration("
        "bus     INT KEY NOT NULL,"
        "vendor  INT KEY NOT NULL,"
        "product INT KEY NOT NULL,"
        "axis    INT KEY NOT NULL,"
        "min     INT,"
        "max     INT,"
        "fuzz    INT,"
        "flat    INT,"
        "PRIMARY KEY (bus, vendor, product, axis)"
        ");";

    rc = sqlite3_exec(db->sqlite3, sql, NULL, 0, err_msg);
    if (rc != SQLITE_OK) {
        caldb_free(db);
        return NULL;
    }

    return db;
}

void caldb_free(caldb_t *db)
{
    if (db->sqlite3)
        sqlite3_close(db->sqlite3);

    xfree(db);
}
