#ifndef COORD_H_
#define COORD_H_

#include <stdbool.h>

typedef struct coord_t {
	int x;
	int y;
} coord_t;

coord_t NewCoord(int x, int y);
bool CoordsEqual(coord_t a, coord_t b);

#endif // !COORD_H_