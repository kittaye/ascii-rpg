#ifndef GRAPHICS_H_
#define GRAPHICS_H_

#include <stdarg.h>

typedef struct GEO_Screen {
	int width;
	int height;
	char **pixels;
	int **px_color;
} GEO_Screen;

GEO_Screen *GEO_zdk_screen;
GEO_Screen *GEO_zdk_prev_screen;

void GEO_setup_screen(void);
void GEO_cleanup_screen(void);
void GEO_clear_screen(void);
void GEO_show_screen(void);

void GEO_draw_char(int x, int y, int color, char value);
void GEO_draw_printf(int x, int y, int color, const char *format, ...);
void GEO_draw_printf_align_center(int x_offset, int y, int color, const char *format, ...);
void GEO_draw_printf_align_right(int x_offset, int y, int color, const char *format, ...);
void GEO_draw_line(int x1, int y1, int x2, int y2, int color, char value);

int GEO_screen_width(void);
int GEO_screen_height(void);

int GEO_wait_char(void);
int GEO_get_char(void);

void GEO_override_screen_size(int width, int height);
void GEO_fit_screen_to_window(void);

#endif /* GRAPHICS_H_ */
