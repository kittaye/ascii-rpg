#ifndef GRAPHICS_H_
#define GRAPHICS_H_

#include <stdarg.h>

typedef struct GEO_Screen {
	int width;
	int height;
	char ** pixels;
	int ** px_color;
} GEO_Screen;

GEO_Screen * GEO_zdk_screen;
GEO_Screen * GEO_zdk_prev_screen;

void GEO_setup_screen( void );
void GEO_cleanup_screen( void );
void GEO_clear_screen( void );
void GEO_show_screen( void );

void GEO_draw_char( int x, int y, char value, int color );
void GEO_draw_string( int x, int y, char * text, int color );
void GEO_draw_align_center(int y, char *text, int color);
void GEO_draw_align_right(int y, char *text, int color);
void GEO_draw_formatted(int x, int y, int color, const char * format, ...);
void GEO_draw_formatted_align_center(int y, int color, const char * format, ...);
void GEO_draw_formatted_align_right(int y, int color, const char * format, ...);
void GEO_draw_line( int x1, int y1, int x2, int y2, char value, int color );

int GEO_screen_width( void );
int GEO_screen_height( void );

int GEO_wait_char( void );
int GEO_get_char( void );

void GEO_override_screen_size( int width, int height );
void GEO_fit_screen_to_window( void );

#endif /* GRAPHICS_H_ */
