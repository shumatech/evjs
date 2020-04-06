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
#include <assert.h>

#define ASSERT(exp) assert(exp)

typedef void(*exit_callback_t)(void *arg);

void xon_exit(exit_callback_t callback, void *arg);

void xerr(const char *fmt, ...) __attribute__ ((format(printf, 1, 2)));

void xerrx(const char *fmt, ...) __attribute__ ((format(printf, 1, 2)));

void *xalloc(size_t len);

char *xstrdup(const char *str);

void xasprintf(char **strp, const char *fmt, ...) __attribute__ ((format(printf, 2, 3)));

void xsnprintf(char *str, size_t size, const char *fmt, ...) __attribute__ ((format(printf, 3, 4)));

void xfree(void *ptr);

int xioctl(int fd, int request, ...);

char *config_path(const char *file);
