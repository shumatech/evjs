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
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "util.h"

#define ERR_STATUS          1

static exit_callback_t exit_callback;
static void *exit_arg;

void xon_exit(exit_callback_t callback, void *arg)
{
    exit_callback = callback;
    exit_arg = arg;
}

void xerr(const char *fmt, ...)
{
    va_list ap;

    if (exit_callback)
        exit_callback(exit_arg);

    va_start(ap, fmt);
    verr(ERR_STATUS, fmt, ap);
    va_end(ap);
}

void xerrx(const char *fmt, ...)
{
    va_list ap;

    if (exit_callback)
        exit_callback(exit_arg);

    va_start(ap, fmt);
    verrx(ERR_STATUS, fmt, ap);
    va_end(ap);
}

void *xalloc(size_t len)
{
    void * ptr = calloc(1, len);
    if (!ptr) {
        if (exit_callback)
            exit_callback(exit_arg);
        xerrx("memory allocation failed");
    }
    return ptr;
}

char *xstrdup(const char *str)
{
    char *dup = strdup(str);
    if (!str)
        xerrx("memory allocation failed");
    return dup;
}

void xasprintf(char **strp, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (vasprintf(strp, fmt, ap) < 0)
        xerrx("memory allocation failed");
    va_end(ap);
}

void xsnprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    int len = vsnprintf(str, size, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= size)
        xerrx("string length error");
}

void xfree(void *ptr)
{
    if (ptr)
        free(ptr);
}

int xioctl(int fd, int request, ...)
{
    va_list ap;

    va_start(ap, request);
    void *arg = va_arg(ap, void*);
    va_end(ap);

    int rc = ioctl(fd, request, arg);
    if (rc == -1) {
        if (exit_callback)
            exit_callback(exit_arg);
        xerr("ioctl");
    }

    return rc;
}

char *config_path(const char *file)
{
    char dir[PATH_MAX];
    char *path;

    dir[0] = '\0';

    if (geteuid() == 0)
    {
        xsnprintf(dir, sizeof(dir), "/etc/%s", PACKAGE);
    }
    else
    {
        char *home = getenv("XDG_CONFIG_HOME");
        if (home)
            xsnprintf(dir, sizeof(dir), "%s/%s", home, PACKAGE);
        else
        {
            home = getenv("HOME");
            if (home)
                xsnprintf(dir, sizeof(dir), "%s/.config/%s", home, PACKAGE);
        }
    }

    // Attempt to make the directory and ignore any errors
    if (dir[0] != '\0')
        mkdir(dir, 0777);

    xasprintf(&path, "%s/%s", dir, file);

    return path;
}
