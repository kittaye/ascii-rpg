/*
*	george_graphics.c
*
*	Authors:
*	 	Lawrence Buckingham, Benjamin Talbot, George Delosa
*
*	COPYRIGHT NOTICE:
*		Original source code was created by and belongs to Lawrence Buckingham and Benjamin Talbot for CAB202, 2016 semester 2. 
*		Portions of it have been heavily modified by George Delosa.
*		I, George Delosa, do not take any credit for their code.
*/

#include <string.h>
#include <stdlib.h>
#include <curses.h>
#include <assert.h>
#include "george_graphics.h"

#define ABS(x)	 (((x) >= 0) ? (x) : -(x))
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#define MAX(x,y) (((x) > (y)) ? (x) : (y))

// Private functions.
static void GEO_destroy_screen(GEO_Screen * scr);
static void GEO_update_buffer(GEO_Screen ** buffer, const int width, const int height);
static void GEO_copy_screen(GEO_Screen * old_scr, GEO_Screen * new_scr);

GEO_Screen * GEO_zdk_screen = NULL;
GEO_Screen * GEO_zdk_prev_screen = NULL;

void GEO_setup_screen(void) {
	// Enter curses mode.
	initscr();

	// Disable buffered keypresses.
	cbreak();

	// Do not echo (show) keypresses.
	noecho();

	// Turn off the cursor.
	curs_set(0);

	// Cause getch to return ERR if no key pressed within 0 milliseconds.
	timeout(0);

	// Enable the keypad.
	keypad(stdscr, TRUE);

	// Try enable colours.
	if (has_colors()) {
		start_color();

		init_pair(0, COLOR_WHITE, COLOR_BLACK);
		init_pair(1, COLOR_YELLOW, COLOR_BLACK);
		init_pair(2, COLOR_RED, COLOR_BLACK);
		init_pair(3, COLOR_BLUE, COLOR_BLACK);
		init_pair(4, COLOR_MAGENTA, COLOR_BLACK);
		init_pair(5, COLOR_CYAN, COLOR_BLACK);
		init_pair(6, COLOR_GREEN, COLOR_BLACK);
		init_pair(7, COLOR_BLACK, COLOR_WHITE);
	}

	// Erase any previous content that may be lingering in this screen.
	clear();

	// Create buffers
	GEO_fit_screen_to_window();
}

void GEO_cleanup_screen(void) {
	// cleanup curses.
	endwin();

	// cleanup the drawing buffers.
	GEO_destroy_screen(GEO_zdk_screen);
	GEO_zdk_screen = NULL;

	GEO_destroy_screen(GEO_zdk_prev_screen);
	GEO_zdk_prev_screen = NULL;
}

void GEO_clear_screen(void) {
	// Erase the contents of the current window.
	if ( GEO_zdk_screen != NULL ) {
		int w = GEO_zdk_screen->width;
		int h = GEO_zdk_screen->height;
		char * scr = GEO_zdk_screen->pixels[0];
		memset(scr, ' ', w * h);
	}
}

void GEO_show_screen(void) {
	// Draw parts of the display that are different in the front
	// buffer from the back buffer.
	char ** back_px = GEO_zdk_prev_screen->pixels;
	int ** back_px_color = GEO_zdk_prev_screen->px_color;

	char ** front_px = GEO_zdk_screen->pixels;
	int ** front_px_color = GEO_zdk_screen->px_color;

	int w = GEO_zdk_screen->width;
	int h = GEO_zdk_screen->height;

	bool changed = false;

	for ( int y = 0; y < h; y++ ) {
		for ( int x = 0; x < w; x++ ) {
			if ( front_px[y][x] != back_px[y][x] || front_px_color[y][x] != back_px_color[y][x] ) 
			{
				if (has_colors()) {
					attron(COLOR_PAIR(front_px_color[y][x]));
					mvaddch(y, x, front_px[y][x]);
					attroff(COLOR_PAIR(front_px_color[y][x]));
				}
				else {
					mvaddch(y, x, front_px[y][x]);
				}

				back_px[y][x] = front_px[y][x];
				back_px_color[y][x] = front_px_color[y][x];
				changed = true;
			}
		}
	}

	if ( !changed ) {
		return;
	}

	// Force an update of the curses display.
	refresh();
}

void GEO_draw_char(const int x, const int y, const int color, const char value) {
	if ( GEO_zdk_screen != NULL ) {
		int w = GEO_zdk_screen->width;
		int h = GEO_zdk_screen->height;

		if ( x >= 0 && x < w && y >= 0 && y < h ) {
			GEO_zdk_screen->pixels[y][x] = value;
			GEO_zdk_screen->px_color[y][x] = color;
		}
	}
}

void GEO_draw_line(int x1, int y1, int x2, int y2, const int color, const char value) {
	if ( x1 == x2 ) {
		// Draw vertical line
		for ( int i = MIN(y1, y2); i <= MAX(y1, y2); i++ ) {
			GEO_draw_char(x1, i, color, value);
		}
	}
	else if ( y1 == y2 ) {
		// Draw horizontal line
		for ( int i = MIN(x1, x2); i <= MAX(x1, x2); i++ ) {
			GEO_draw_char(i, y1, color, value);
		}
	}
	else {
		// Inserted to ensure that lines are always drawn in the same direction, regardless of
		// the order the endpoints are presented.
		if ( x1 > x2 ) {
			int t = x1;
			x1 = x2;
			x2 = t;
			t = y1;
			y1 = y2;
			y2 = t;
		}

		// Get Bresenhaming...
		float dx = x2 - x1;
		float dy = y2 - y1;
		float err = 0.0;
		float derr = ABS(dy / dx);

		for ( int x = x1, y = y1; (dx > 0) ? x <= x2 : x >= x2; (dx > 0) ? x++ : x-- ) {
			GEO_draw_char(x, y, color, value);
			err += derr;
			while ( err >= 0.5 && ((dy > 0) ? y <= y2 : y >= y2) ) {
				GEO_draw_char(x, y, color, value);
				y += (dy > 0) - (dy < 0);

				err -= 1.0;
			}
		}
	}
}

void GEO_draw_string(const int x, const int y, const int color, const char * text) {
	for (int i = 0; text[i]; i++) {
		GEO_draw_char(x + i, y, color, text[i]);
	}
}

void GEO_draw_align_center(const int x_offset, const int y, const int color, const char *text) {
	int x = ((GEO_screen_width() + x_offset) / 2) - (strlen(text) / 2);
	GEO_draw_string(x, y, color, text);
}

void GEO_draw_align_right(const int y, const int color, const char *text) {
	int x = GEO_screen_width() - strlen(text);
	GEO_draw_string(x, y, color, text);
}

void GEO_draw_formatted(const int x, const int y, const int color, const char * format, ...) {
	va_list args;
	va_start(args, format);
	char buffer[1000];
	vsprintf(buffer, format, args);
	GEO_draw_string(x, y, color, buffer);
	va_end(args);
}

void GEO_draw_formatted_align_center(const int x_offset, const int y, const int color, const char * format, ...) {
	va_list args;
	va_start(args, format);
	char buffer[1000];
	vsprintf(buffer, format, args);
	GEO_draw_align_center(x_offset, y, color, buffer);
	va_end(args);
}

void GEO_draw_formatted_align_right(const int y, const int color, const char * format, ...) {
	va_list args;
	va_start(args, format);
	char buffer[1000];
	vsprintf(buffer, format, args);
	GEO_draw_align_right(y, color, buffer);
	va_end(args);
}

int GEO_get_char() {
	int current_char;

	current_char = getch();

	return current_char;
}

int GEO_wait_char() {
	int current_char;

	timeout(-1);
	current_char = getch();
	timeout(0);

	return current_char;
}

int GEO_screen_width(void) {
	return GEO_zdk_screen->width;
}

int GEO_screen_height(void) {
	return GEO_zdk_screen->height;
}

void GEO_override_screen_size(const int width, const int height) {
	GEO_update_buffer(&GEO_zdk_screen, width, height);
	GEO_update_buffer(&GEO_zdk_prev_screen, width, height);
}

void GEO_update_buffer(GEO_Screen **screen, const int width, const int height) {
	assert(width > 0);
	assert(height > 0);

	if ( screen == NULL ) {
		return;
	}

	GEO_Screen * old_screen = (*screen);

	if ( old_screen != NULL && width == old_screen->width && height == old_screen->height ) {
		return;
	}

	GEO_Screen * new_screen = calloc(1, sizeof(GEO_Screen));

	if ( !new_screen ) {
		*screen = NULL;
		return;
	}

	new_screen->width = width;
	new_screen->height = height;

	new_screen->pixels = calloc(height, sizeof(char *));
	new_screen->px_color = calloc(height, sizeof(int *));

	if ( !new_screen->pixels || !new_screen->px_color ) {
		free(new_screen);
		*screen = NULL;
		return;
	}

	new_screen->pixels[0] = calloc(width * height, sizeof(char));
	new_screen->px_color[0] = calloc(width * height, sizeof(int));

	if ( !new_screen->pixels[0] ) {
		free(new_screen->pixels);
		free(new_screen);
		*screen = NULL;
		return;
	}

	if (!new_screen->px_color[0]) {
		free(new_screen->px_color);
		free(new_screen);
		*screen = NULL;
		return;
	}

	for ( int y = 1; y < height; y++ ) {
		new_screen->pixels[y] = new_screen->pixels[y - 1] + width;
		new_screen->px_color[y] = new_screen->px_color[y - 1] + width;
	}

	memset(new_screen->pixels[0], ' ', width * height);

	GEO_copy_screen(old_screen, new_screen);

	GEO_destroy_screen(old_screen);

	(*screen) = new_screen;
}

void GEO_copy_screen(GEO_Screen * src, GEO_Screen * dest) {
	if ( src == NULL || dest == NULL || src == dest ) return;

	int clip_width = MIN(src->width, dest->width);
	int clip_height = MIN(src->height, dest->height);

	for ( int y = 0; y < clip_height; y++ ) {
		memcpy(dest->pixels[y], src->pixels[y], clip_width);
	}
}

void GEO_fit_screen_to_window(void) {
	GEO_override_screen_size(getmaxx(stdscr), getmaxy(stdscr));
}

void GEO_destroy_screen(GEO_Screen * scr) {
	if ( scr ) {
		if ( scr->pixels ) {
			if ( scr->pixels[0] ) {
				free(scr->pixels[0]);
			}

			free(scr->pixels);
		}

		free(scr);
	}
}
