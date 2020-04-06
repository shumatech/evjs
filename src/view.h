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
#include <ncurses.h>

#include "device.h"

typedef struct view view_t;

void view_axis_value(view_t *view, axis_t *axis, int value);

void view_axis_calibration(view_t *view, axis_t *axis);

axis_t *view_axis_get(view_t *view);
void view_axis_prev(view_t *view);
void view_axis_next(view_t *view);
void view_axis_pageup(view_t *view);
void view_axis_pagedn(view_t *view);

bool view_axis_cursors_get(view_t *view);
void view_axis_cursors_set(view_t *view, bool enable);

void view_axis_refresh(view_t *view);

void view_button_value(view_t *view, button_t *button, int value);

#if ENABLE_EFFECTS
effect_t *view_effect_get(view_t *view);
void view_effect_prev(view_t *view);
void view_effect_next(view_t *view);
#endif

int view_prompt_int(view_t *view, int min, int max, const char *prompt, ...);

void view_error(view_t *view, const char *fmt, ...);

bool view_confirm(view_t *view, const char *prompt);

void view_info_refresh(view_t *view);

void view_help(view_t *view);

view_t *view_init(device_t *dev, const char *db_file);

void view_free(view_t *view);

void view_resize(view_t *view);

int view_key(view_t *view);
