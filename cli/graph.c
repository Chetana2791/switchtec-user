/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
 * Copyright (c) 2021, Microsemi Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "graph.h"
#include "config.h"
#include <switchtec/utils.h>

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#if defined(HAVE_LIBCURSES) || defined(HAVE_LIBNCURSES)

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_CURSES_H)
#include <ncurses/curses.h>
#endif

enum {
	PAIR_AXIS = 1,
	PAIR_TITLE = 2,
	PAIR_SHADE_START,
};

static void draw_xaxis(WINDOW *win, struct range *X, int x_scroll)
{
	int x, i = -x_scroll * 2;

	werase(win);
	for_range(x, X) {
		mvwprintw(win, 0, i, "%d", x / 10);
		mvwprintw(win, 1, i, "%d", x % 10);
		mvwprintw(win, 0, i + 1, " ");
		mvwprintw(win, 1, i + 1, " ");
		i += 2;
	}
	wrefresh(win);
}

static void draw_yaxis(WINDOW *win, struct range *Y, int y_scroll)
{
	int y, i = -y_scroll;

	werase(win);
	for_rev_range(y, Y) {
		mvwprintw(win, i, 0, "%5d", y);
		i++;
	}
	wrefresh(win);
}

static void draw_data(WINDOW *win, int x_cnt, int y_cnt, int *data,
		      int *shades, int x_scroll, int y_scroll)
{
	bool shade = COLORS == 256;
	int x, y, s, yy;

	werase(win);
	for (y = 0; y < y_cnt; y++) {
		for (x = 0; x < x_cnt; x++) {
			if (shade) {
				s = PAIR_SHADE_START + shades[y * x_cnt + x];
				wcolor_set(win, s, NULL);
			}

			yy = y_cnt - y_scroll - y - 1;
			mvwaddch(win, yy, (x - x_scroll) * 2,
				 data[y * x_cnt + x]);
			mvwaddch(win,  yy, (x - x_scroll) * 2 + 1,
				 ' ');
		}
	}
	wrefresh(win);
}

static void draw_title(int x_off, int x_cnt, const char *title)
{
	int len = strlen(title);
	int i;

	x_cnt *= 2;
	if (x_cnt > COLS - x_off)
		x_cnt = COLS - x_off;

	for (i = 0; i < x_cnt + x_off; i++)
		mvaddstr(0, i, " ");

	if (len >= x_cnt)
		mvaddnstr(0, x_off + 1, title, x_cnt - 1);
	else
		mvaddstr(0, x_off + (x_cnt - len) / 2, title);
}

static void calc_scroll_limits(int x_cnt, int y_cnt, int x_off, int y_off,
			       int *x_max, int *y_max)
{
	*x_max = x_cnt - ((COLS - x_off) / 2);
	if (*x_max < 0)
		*x_max = 0;
	*y_max = y_cnt - (LINES - y_off);
	if (*y_max < 0)
		*y_max = 0;
}

static void init_axis_color(WINDOW *xaxis, WINDOW *yaxis, char x_title, char y_title)
{
	init_pair(PAIR_AXIS, COLOR_WHITE, COLOR_BLUE);
	wattron(xaxis, A_BOLD);
	wattron(yaxis, A_BOLD);
	wcolor_set(xaxis, PAIR_AXIS, NULL);

	wcolor_set(yaxis, PAIR_AXIS, NULL);
	color_set(PAIR_AXIS, NULL);

	mvprintw(1, 0, "       ");
	mvprintw(2, 0, "     %c ", x_title);
	mvprintw(3, 0, "    %c", y_title);
}

static void init_shades(void)
{
	init_pair(PAIR_SHADE_START +  0, COLOR_WHITE, COLOR_BLACK);
	init_pair(PAIR_SHADE_START +  1, COLOR_WHITE, 0x11);
	init_pair(PAIR_SHADE_START +  2, COLOR_WHITE, 0x12);
	init_pair(PAIR_SHADE_START +  3, COLOR_WHITE, 0x13);
	init_pair(PAIR_SHADE_START +  4, COLOR_WHITE, 0x14);
	init_pair(PAIR_SHADE_START +  5, COLOR_WHITE, 0x15);
	init_pair(PAIR_SHADE_START +  6, COLOR_WHITE, 0x5d);
	init_pair(PAIR_SHADE_START +  7, COLOR_WHITE, 0x5c);
	init_pair(PAIR_SHADE_START +  8, COLOR_WHITE, 0x5b);
	init_pair(PAIR_SHADE_START +  9, COLOR_WHITE, 0x5a);
	init_pair(PAIR_SHADE_START + 10, COLOR_WHITE, 0x59);
	init_pair(PAIR_SHADE_START + 11, COLOR_WHITE, 0x58);
	init_pair(PAIR_SHADE_START + 12, COLOR_WHITE, 0x7c);
	init_pair(PAIR_SHADE_START + 13, COLOR_WHITE, 0xa0);
	init_pair(PAIR_SHADE_START + 14, COLOR_WHITE, 0xc4);
	init_pair(PAIR_SHADE_START + 15, COLOR_WHITE, 0xca);
}

int graph_draw_win(struct range *X, struct range *Y, int *data, int *shades,
		   const char *title, char x_title, char y_title)
{
	WINDOW *xaxis, *yaxis, *datawin;
	const int x_off = 7, y_off = 4;
	int x_cnt = RANGE_CNT(X);
	int y_cnt = RANGE_CNT(Y);
	bool need_redraw = true;
	int old_lines, old_cols;
	int x_scroll, y_scroll;
	int x_max, y_max;
	int c, rem;

	if (!isatty(STDOUT_FILENO)) {
		graph_draw_text(X, Y, data, title, x_title, y_title);
		return 0;
	}

	initscr();
	noecho();
	cbreak();
	curs_set(0);
	keypad(stdscr, true);
	start_color();

	xaxis = newwin(y_off, 0, 1, x_off);
	yaxis = newwin(0, x_off, y_off, 0);
	datawin = newwin(0, 0, y_off, x_off);
	if (!xaxis || !yaxis || !datawin) {
		perror("Unable to create window");
		return 1;
	}

	init_shades();
	init_axis_color(xaxis, yaxis, x_title, y_title);
	calc_scroll_limits(x_cnt, y_cnt, x_off, y_off, &x_max, &y_max);

	x_scroll = x_max / 2;
	y_scroll = y_max / 2;

	old_lines = LINES;
	old_cols = COLS;
	rem = 0;

	while (1) {
		if (need_redraw) {
			refresh();
			draw_title(x_off, x_cnt, title);
			draw_xaxis(xaxis, X, x_scroll);
			draw_yaxis(yaxis, Y, y_scroll);
			draw_data(datawin, x_cnt, y_cnt, data, shades, x_scroll,
				  y_scroll);
			need_redraw = false;
		}

		c = getch();
		switch (c) {
		case 'q':
		case 'x':
			goto out;
		case KEY_LEFT:
			need_redraw = true;
			x_scroll--;
			break;
		case KEY_RIGHT:
			need_redraw = true;
			x_scroll++;
			break;
		case KEY_UP:
			need_redraw = true;
			y_scroll--;
			break;
		case KEY_DOWN:
			need_redraw = true;
			y_scroll++;
			break;
		case KEY_RESIZE:
			need_redraw = true;
			wresize(xaxis, y_off, COLS - x_off);
			wresize(yaxis, LINES - y_off, x_off);
			wresize(datawin, LINES - y_off, COLS - x_off);

			calc_scroll_limits(x_cnt, y_cnt, x_off, y_off, &x_max,
					   &y_max);

			/* Try to keep the graph centred */
			x_scroll -= (COLS - old_cols - rem * 3) / 4;
			y_scroll -= (LINES - old_lines - rem) / 2;
			rem = !rem;

			old_cols = COLS;
			old_lines = LINES;
			break;
		}

		if (x_scroll < 0)
			x_scroll = 0;
		if (y_scroll < 0)
			y_scroll = 0;
		if (x_scroll > x_max)
			x_scroll = x_max;
		if (y_scroll > y_max)
			y_scroll = y_max;
	}

out:
	delwin(datawin);
	delwin(yaxis);
	delwin(xaxis);
	endwin();
	return 0;
}

#else /* defined(HAVE_LIBCURSES) || defined(HAVE_LIBNCURSES) */

int graph_draw_win(struct range *X, struct range *Y, int *data, int *shades,
		   const char *title, char x_title, char y_title)

{
	graph_draw_text(X, Y, data, title, x_title, y_title);
	return 0;
}

#endif /* defined(HAVE_LIBCURSES) || defined(HAVE_LIBNCURSES) */

void graph_draw_text(struct range *X, struct range *Y, int *data,
		     const char *title, char x_title, char y_title)
{
	int stride = RANGE_CNT(X);
	int x, y, i, j = RANGE_CNT(Y) - 1;

	printf("    %s\n\n", title);

	printf("       ");
	for_range(x, X)
		printf("%d ", x / 10);
	printf("\n");

	printf("       ");
	for_range(x, X)
		printf("%d ", x % 10);
	printf("\n\n");

	for_rev_range(y, Y) {
		printf("%5d  ", y);
		i = 0;
		for_range(x, X)  {
			printf("%c ", data[j * stride + i]);
			i++;
		}
		printf("\n");
		j--;
	}
}
