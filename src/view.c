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
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <ncurses.h>

#include "util.h"
#include "view.h"
#include "device.h"

#define INFO_H              4

#define AXIS_H              3
#define VALUE_X             (GRAPH_X + GRAPH_W + 1)
#define VALUE_W             6
#define GRAPH_X             (LABEL_X + LABEL_W + 1)
#define GRAPH_W             (getmaxx(w) - GRAPH_X - VALUE_W - 2)
#define LABEL_X             1
#define LABEL_W             10

#define BUTTON_X            1
#define BUTTON_Y            1
#define BUTTON_W            5
#define BUTTON_H            4

#define EFFECT_X            1
#define EFFECT_Y            1
#define EFFECT_W            11
#define EFFECT_H            1

#define STATUS_X            0
#define STATUS_Y            0
#define STATUS_H            1

struct view
{
    device_t    *dev;
    WINDOW      *info_win;
    WINDOW      *axis_box;
    WINDOW      *axis_win;
    axis_t      *axis_select;
    axis_t      *axis_scroll;
    int          axis_rows;
    WINDOW      *button_box;
    WINDOW      *button_win;
#if ENABLE_EFFECTS
    WINDOW      *effect_box;
    WINDOW      *effect_win;
    effect_t    *effect_select;
#endif
    WINDOW      *status_win;
    bool        cursors;
    const char  *db_file;
};

static bool resize;

///////////////////////////////////////////////////////////////////////////////
//
// Axis Window Functions
//
///////////////////////////////////////////////////////////////////////////////

static int view_axis_graph_x(view_t *view, axis_t *axis, int value)
{
    WINDOW *w = view->axis_win;

    if (value <= axis->cal.min)
        return GRAPH_X + 1;
    else if (value >= axis->cal.max)
        return GRAPH_X + 1 + (GRAPH_W - 2);
    else
        return GRAPH_X + 1 + (GRAPH_W - 2) * (value - axis->cal.min) /
            (axis->cal.max - axis->cal.min);
}

void view_axis_value(view_t *view, axis_t *axis, int value)
{
    WINDOW *w = view->axis_win;

    axis_t *last = view->axis_scroll + view->axis_rows;
    if (axis < view->axis_scroll || axis >= last)
        return;

    int y = (axis->index - view->axis_scroll->index) * AXIS_H;

    int x = view_axis_graph_x(view, axis, axis->value);

    wattron(w, A_REVERSE);
    mvwprintw(w, y + 1, GRAPH_X + 1, "%*s", x - GRAPH_X - 1, "");
    wattroff(w, A_REVERSE);

    mvwprintw(w, y + 1, x, "%*s", GRAPH_X + GRAPH_W - x - 1, "");

    if (wmove(w, y, GRAPH_X) == OK)
        wclrtoeol(w);

    if (view->cursors)
    {
        int max_x = view_axis_graph_x(view, axis, axis->maximum);
        int min_x = view_axis_graph_x(view, axis, axis->minimum) - 1;

        int max_ch;
        if (axis->maximum <= axis->cal.max)
            max_ch = '<';
        else
            max_ch = '>';
        mvwaddch(w, y, max_x, max_ch);

        int min_ch;
        if (axis->minimum >= axis->cal.min)
            min_ch = '>';
        else
            min_ch = '<';
        mvwaddch(w, y, min_x, min_ch);
    }

    if (mvwprintw(w, y + 1, VALUE_X, "%-*d", VALUE_W, axis->value) == OK)
        wclrtoeol(w);

    wrefresh(w);
}

static void view_axis_scrollbar(view_t *view)
{
    WINDOW *w = view->axis_box;
    int x = getmaxx(w) - 1;
    int y = 1;
    int h = getmaxy(w) - 2;

    // Draw the scrollbar if any axes are hidden
    if (view->axis_rows < view->dev->axis_num)
    {
        for (y = 1; y <= h; y++)
            mvwaddch(w, y, x, ACS_CKBOARD);

        y = 1 + (getmaxy(w) - 3) * view->axis_scroll->index / (view->dev->axis_num - view->axis_rows);
        mvwaddch(w, y, x, ACS_BLOCK);
    }
    else
    {
        mvwvline(w, y, x, ACS_VLINE, h);
    }

    wrefresh(w);
}

static void view_axis_select(view_t *view, axis_t *axis)
{
    WINDOW *w = view->axis_win;

    axis_t *prev = view->axis_select;
    view->axis_select = axis;

    if (axis < view->axis_scroll)
    {
        view->axis_scroll = axis;
        view_axis_refresh(view);
        return;
    }

    axis_t *last = view->axis_scroll + view->axis_rows;
    if (axis >= last)
    {
        view->axis_scroll = axis - view->axis_rows + 1;
        view_axis_refresh(view);
        return;
    }

    int y;
    if (prev >= view->axis_scroll && prev < last)
    {
        y = (prev->index - view->axis_scroll->index) * AXIS_H;
        mvwprintw(w, y + 1, LABEL_X, "%*s", LABEL_W, prev->name);
    }

    y = (axis->index - view->axis_scroll->index) * AXIS_H;
    size_t len = strlen(axis->name);
    wattron(w, A_UNDERLINE);
    mvwprintw(w, y + 1, LABEL_X + LABEL_W - len, "%s", axis->name);
    wattroff(w, A_UNDERLINE);

    wrefresh(w);
}

void view_axis_refresh(view_t *view)
{
    WINDOW *w = view->axis_win;

    werase(w);

    view->axis_rows = (getmaxy(w) - 1) / AXIS_H;
    axis_t *last = view->axis_scroll + view->axis_rows;

    // If the selection is below the scroll area then shift the scroll area to fit
    if (view->axis_select >= last)
    {
        view->axis_scroll = view->axis_select - view->axis_rows + 1;
        last = view->axis_scroll + view->axis_rows;
    }

    // If the scroll area extends below the end then shift the scroll area to fit
    axis_t *end = view->dev->axis_array + view->dev->axis_num;
    if (view->axis_scroll + view->axis_rows >= end)
    {
        view->axis_scroll = end - view->axis_rows;
        last = view->axis_scroll + view->axis_rows;
    }

    // Draw all visible axes
    int y = 0;
    for (axis_t *axis = view->axis_scroll; axis < last; axis++)
    {
        mvwprintw(w, y + 1, LABEL_X, "%*s", LABEL_W, axis->name);

        mvwaddch(w, y + 1, GRAPH_X, '[');
        mvwaddch(w, y + 1, GRAPH_X + GRAPH_W - 1, ']');

        view_axis_value(view, axis, axis->value);

        view_axis_calibration(view, axis);

        y += AXIS_H;
    }

    // Update the selection indicator
    view_axis_select(view, view->axis_select);

    // Update the scrollbar
    view_axis_scrollbar(view);

    wrefresh(w);
}

axis_t *view_axis_get(view_t *view)
{
    return view->axis_select;
}

void view_axis_prev(view_t *view)
{
    device_t *dev = view->dev;
    axis_t *axis = view->axis_select - 1;
    if (axis >= dev->axis_array)
        view_axis_select(view, axis);
}

void view_axis_next(view_t *view)
{
    device_t *dev = view->dev;
    axis_t *axis = view->axis_select + 1;
    if (axis < &dev->axis_array[dev->axis_num])
        view_axis_select(view, axis);
}

void view_axis_pageup(view_t *view)
{
    device_t *dev = view->dev;
    axis_t *axis = view->axis_select - view->axis_rows;
    if (axis < dev->axis_array)
        axis = dev->axis_array;
    view_axis_select(view, axis);
}

void view_axis_pagedn(view_t *view)
{
    device_t *dev = view->dev;
    axis_t *axis = view->axis_select + view->axis_rows;
    if (axis >= &dev->axis_array[dev->axis_num])
        axis = &dev->axis_array[dev->axis_num - 1];
    view_axis_select(view, axis);
}

bool view_axis_cursors_get(view_t *view)
{
    return view->cursors;
}

void view_axis_cursors_set(view_t *view, bool enable)
{
    device_t *dev = view->dev;
    view->cursors = enable;
    AXIS_FOREACH(dev, axis)
    {
        view_axis_calibration(view, axis);
        view_axis_value(view, axis, axis->value);
    }
}

void view_axis_calibration(view_t *view, axis_t *axis)
{
    WINDOW *w = view->axis_win;

    int y = (axis->index - view->axis_scroll->index) * AXIS_H;

    if (mvwprintw(w, y + 2, GRAPH_X, "Calibration Min:%d Max:%d Fuzz:%d Flat:%d",
        axis->cal.min, axis->cal.max, axis->cal.fuzz, axis->cal.flat) == OK)
        wclrtoeol(w);

    wrefresh(w);
}

///////////////////////////////////////////////////////////////////////////////
//
// Status Window Functions
//
///////////////////////////////////////////////////////////////////////////////

static void view_status(view_t *view)
{
    WINDOW *w = view->status_win;

    werase(w);

    wbkgdset(w, A_REVERSE);
    if (mvwprintw(w, STATUS_Y, STATUS_X,
        "Commands: '?':help 'q':quit 'c':calibrate <Enter>:set 'w':write 'r':read") == OK)
        wclrtoeol(w);
    wbkgdset(w, A_NORMAL);

    wrefresh(w);
}

static char *view_vprompt(view_t *view, const char *prompt, va_list ap)
{
    WINDOW *w = view->status_win;
    int rc;
    char input[100];

    wbkgdset(w, A_REVERSE);
    if (wmove(w, STATUS_Y, STATUS_X) == OK)
        if (vw_printw(w, prompt, ap) == OK)
            wclrtoeol(w);
    wrefresh(w);

    nodelay(w, FALSE);
    echo();
    curs_set(TRUE);

    rc = wgetnstr(w, input, sizeof(input));

    nodelay(w, TRUE);
    curs_set(FALSE);
    noecho();

    wbkgdset(w, A_NORMAL);

    view_status(view);

    if (rc != OK)
        return NULL;

    return xstrdup(input);
}

#ifdef UNUSED
static char *view_prompt(view_t *view, const char *prompt, ...)
{
    va_list ap;

    va_start(ap, prompt);
    char *input = view_vprompt(view, prompt, ap);
    va_end(ap);

    return input;
}
#endif

bool view_confirm(view_t *view, const char *prompt)
{
    WINDOW *w = view->status_win;
    char ch = 0;

    wbkgdset(w, A_REVERSE);
    if (mvwprintw(w, STATUS_Y, STATUS_X, "%s", prompt) == OK)
        wclrtoeol(w);
    wrefresh(w);

    curs_set(TRUE);
    nodelay(w, FALSE);

    while (ch != 'y' && ch != 'n')
        ch = wgetch(w);

    nodelay(w, TRUE);
    curs_set(FALSE);

    wbkgdset(w, A_NORMAL);

    view_status(view);

    return (ch == 'y');
}

void view_error(view_t *view, const char *fmt, ...)
{
    WINDOW *w = view->status_win;
    va_list ap;

    if (wmove(w, STATUS_Y, STATUS_X) == OK)
    {
        wbkgdset(w, A_REVERSE);
        va_start(ap, fmt);
        if (vw_printw(w, fmt, ap) == OK)
            wclrtoeol(w);
        va_end(ap);
        wbkgdset(w,A_NORMAL);
        wrefresh(w);

        nodelay(w, FALSE);
        wgetch(w);
        nodelay(w, TRUE);

        view_status(view);
    }
}

int view_prompt_int(view_t *view, int min, int max, const char *prompt, ...)
{
    int value = INT_MAX;
    va_list ap;

    va_start(ap, prompt);
    char *input = view_vprompt(view, prompt, ap);
    va_end(ap);

    if (input)
    {
        if (*input)
        {
            char *end;
            long ret = strtol(input, &end, 0);
            if (!*end && ret <= max && ret >= min)
                value = ret;
        }
        xfree(input);
    }

    return value;
}

///////////////////////////////////////////////////////////////////////////////
//
// Info Window Functions
//
///////////////////////////////////////////////////////////////////////////////

void view_info_refresh(view_t *view)
{
    WINDOW *w = view->info_win;
    device_t *dev = view->dev;

    werase(w);

    mvwprintw(w, 0, 0, "Device File: %s", dev->file);
#if ENABLE_JOYSTICK
    if (dev->jsfile)
        wprintw(w, " (%s)", dev->jsfile);
#endif
    mvwprintw(w, 1, 0, "Device Name: %s", dev->name);
    mvwprintw(w, 2, 0, "Device ID:   bus:%04x vendor:%04x product:%04x",
        dev->id.bus, dev->id.vendor, dev->id.product);
    mvwprintw(w, 3, 0, "Database:    %s%s", view->db_file, dev->dirty ? "[+]" : "");

    wrefresh(w);
}

///////////////////////////////////////////////////////////////////////////////
//
// Button Window Functions
//
///////////////////////////////////////////////////////////////////////////////

void view_button_value(view_t *view, button_t *button, int value)
{
    WINDOW *w = view->button_win;

    button->value = value;

    int col = (getmaxx(w) - 1) / BUTTON_W;
    int y = BUTTON_H * (button->index / col) + 1;
    int x = BUTTON_W * (button->index % col) + 1;

    if (value)
        wbkgdset(w, A_REVERSE);
    mvwprintw(w, y + 1, x + 1, "%2d", button->index);
    if (value)
        wbkgdset(w, A_NORMAL);

    wrefresh(w);
}

static void view_button_refresh(view_t *view)
{
    int x1 = BUTTON_X;
    int y1 = BUTTON_Y;

    WINDOW *w = view->button_win;
    werase(w);

    BUTTON_FOREACH(view->dev, button)
    {
        int x2 = x1 + BUTTON_W - 2;
        int y2 = y1 + BUTTON_H - 2;

        mvwhline(w, y1, x1, 0, x2 - x1);
        mvwhline(w, y2, x1, 0, x2 - x1);
        mvwvline(w, y1, x1, 0, y2 - y1);
        mvwvline(w, y1, x2, 0, y2 - y1);
        mvwaddch(w, y1, x1, ACS_ULCORNER);
        mvwaddch(w, y2, x1, ACS_LLCORNER);
        mvwaddch(w, y1, x2, ACS_URCORNER);
        mvwaddch(w, y2, x2, ACS_LRCORNER);

        view_button_value(view, button, button->value);

        x1 += BUTTON_W;
        if (x1 > getmaxx(w) - BUTTON_W)
        {
            x1 = BUTTON_X;
            y1 += BUTTON_H;
        }
    }

    wrefresh(w);
}

///////////////////////////////////////////////////////////////////////////////
//
// Effect Window Functions
//
///////////////////////////////////////////////////////////////////////////////

#if ENABLE_EFFECTS
void view_effect_select(view_t *view, effect_t *effect)
{
    WINDOW *w = view->effect_win;

    effect_t *prev = view->effect_select;
    view->effect_select = effect;

    int col = (getmaxx(w) - 1) / EFFECT_W;
    int y = EFFECT_H * (prev->index / col) + 1;
    int x = EFFECT_W * (prev->index % col) + 1;

    mvwprintw(w, y, x, "%s", prev->name);

    y = EFFECT_H * (effect->index / col) + 1;
    x = EFFECT_W * (effect->index % col) + 1;

    wattron(w, A_UNDERLINE);
    mvwprintw(w, y, x, "%s", effect->name);
    wattroff(w, A_UNDERLINE);

    wrefresh(w);
}

static void view_refresh_effect(view_t *view)
{
    int x = EFFECT_X;
    int y = EFFECT_Y;

    WINDOW *w = view->effect_win;
    werase(w);

    EFFECT_FOREACH(view->dev, effect)
    {
        mvwprintw(w, y, x, "%s", effect->name);

        x += EFFECT_W;
        if (x > getmaxx(w) - EFFECT_W)
        {
            x = EFFECT_X;
            y += EFFECT_H;
        }
    }

    view_effect_select(view, view->effect_select);

    wrefresh(w);
}

effect_t *view_effect_get(view_t *view)
{
    return view->effect_select;
}

void view_effect_prev(view_t *view)
{
    device_t *dev = view->dev;
    effect_t *effect = view->effect_select - 1;
    if (effect >= dev->effect_array)
        view_effect_select(view, effect);
}

void view_effect_next(view_t *view)
{
    device_t *dev = view->dev;
    effect_t *effect = view->effect_select + 1;
    if (effect < &dev->effect_array[dev->effect_num])
        view_effect_select(view, effect);
}

#endif

///////////////////////////////////////////////////////////////////////////////
//
// Root Window Functions
//
///////////////////////////////////////////////////////////////////////////////

static void view_refresh(view_t *view)
{
    clear();

    //
    // Calculate Sizes
    //
    device_t *dev = view->dev;
    int max_x = getmaxx(stdscr);
    int max_y = getmaxy(stdscr);

    int button_h = 0;
    if (dev->button_num > 0)
    {
        int col = (max_x - 3) / BUTTON_W;
        int row = (dev->button_num + col - 1) / col;
        button_h = row * BUTTON_H + 3;
    }

    int effect_h = 0;
#if ENABLE_EFFECTS
    if (dev->effect_num > 0)
    {
        int col = (max_x - 3) / EFFECT_W;
        int row = (dev->effect_num + col - 1) / col;
        effect_h = row * EFFECT_H + 4;
    }
#endif

    int axis_h = dev->axis_num * AXIS_H + 3;
    if (INFO_H + axis_h + button_h + effect_h + STATUS_H > max_y)
        axis_h = max_y - INFO_H - button_h - effect_h - STATUS_H;
    if (axis_h < AXIS_H + 3)
        axis_h = AXIS_H + 3;

    //
    // Info Window
    //
    if (view->info_win)
        delwin(view->info_win);
    view->info_win = newwin(INFO_H, max_x, 0, 0);

    view_info_refresh(view);

    //
    // Axis Window
    //

    if (view->axis_box)
        delwin(view->axis_box);
    view->axis_box = newwin(axis_h, max_x, INFO_H, 0);

    box(view->axis_box, 0, 0);
    mvwprintw(view->axis_box, 0, 1, "Axes");
    wrefresh(view->axis_box);

    if (view->axis_win)
        delwin(view->axis_win);
    view->axis_win = derwin(view->axis_box, axis_h - 2, max_x - 2, 1, 1);

    view_axis_refresh(view);

    //
    // Button Window
    //
    if (button_h > 0)
    {
        if (view->button_box)
            delwin(view->button_box);
        view->button_box = newwin(button_h, max_x, INFO_H + axis_h, 0);

        box(view->button_box, 0, 0);
        mvwprintw(view->button_box, 0, 1, "Buttons");
        wrefresh(view->button_box);

        if (view->button_win)
            delwin(view->button_win);
        view->button_win = derwin(view->button_box, button_h - 2, max_x - 2, 1, 1);

        view_button_refresh(view);
    }

    //
    // Effect Window
    //
#if ENABLE_EFFECTS
    if (effect_h > 0)
    {
        if (view->effect_box)
            delwin(view->effect_box);
        view->effect_box = newwin(effect_h, max_x, INFO_H + axis_h + button_h, 0);

        box(view->effect_box, 0, 0);
        mvwprintw(view->effect_box, 0, 1, "Effects");
        wrefresh(view->effect_box);

        if (view->effect_win)
            delwin(view->effect_win);
        view->effect_win = derwin(view->effect_box, effect_h - 2, max_x - 2, 1, 1);

        view_refresh_effect(view);
    }
#endif

    //
    // Status Window
    //
    if (view->status_win)
        delwin(view->status_win);

    view->status_win = newwin(STATUS_H, max_x, INFO_H + button_h + effect_h + axis_h, 0);
    nodelay(view->status_win, TRUE);
    keypad(view->status_win, TRUE);

    view_status(view);
}

void view_help(view_t *view)
{
    clear();

    move(0, 0);
    addstr(
        "Navigation:\n"
        "  <up> or j    : move axis selection up\n"
        "  <down> or k  : move axis selection down\n"
        "  <page up>    : move axis selection up one screen\n"
        "  <page dn>    : move axis selection down one screen\n"
#if ENABLE_EFFECTS
        "  <left> of h  : move effect selection left\n"
        "  <right> or l : move effect selection right\n"
#endif
        "General:\n"
        "  q            : Quit application\n"
        "  c            : Toggle calibration cursors\n"
        "  <ENTER>      : Set calibration from current cursors \n"
        "  f            : Set fuzz factor for selected axis\n"
        "  t            : Set flatness for selected axis\n"
#if ENABLE_EFFECTS
        "  e            : Activate selected effect\n"
#endif
        "Database:\n"
        "  r            : Read axis calibration\n"
        "  w            : Write axis calibration\n"
        "\n"
        "To calibrate a device, first turn on the calibration cursors with 'c'. Then\n"
        "move all axes to their minimum and maximum values. Press <ENTER> to set the\n"
        "calibration values from the cursor positions or 'c' to cancel and turn off\n"
        "the cursors.\n"
        "\n"
        "Use the evjscfg command in combination with udev to automatically restore\n"
        "calibrations when the system is restarted or the device is plugged in.\n"
        "\n"
        "Press any key to return"
    );

    refresh();

    nodelay(stdscr, FALSE);
    getch();
    nodelay(stdscr, TRUE);

    view_refresh(view);
}

void view_resize(view_t *view)
{
    if (!resize)
        return;

    resize = false;

    endwin();
    refresh();
    view_refresh(view);
}

static void view_winch(int in)
{
    resize = true;
}

view_t *view_init(device_t *dev, const char *db_file)
{
    view_t *view;

    view = xalloc(sizeof(view_t));

    signal(SIGWINCH, view_winch);

    initscr();
    cbreak();
    noecho();
    curs_set(FALSE);

    view->dev = dev;
    view->db_file = db_file;
    view->axis_select = dev->axis_array;
    view->axis_scroll = dev->axis_array;
#if ENABLE_EFFECTS
    view->effect_select = dev->effect_array;
#endif

    view_refresh(view);

    return view;
}

void view_free(view_t *view)
{
    if (view->info_win)
        delwin(view->info_win);

    if (view->axis_box)
        delwin(view->axis_box);

    if (view->axis_win)
        delwin(view->axis_win);

    if (view->status_win)
        delwin(view->status_win);

    endwin();

    xfree(view);
}

int view_key(view_t *view)
{
    return wgetch(view->status_win);
}