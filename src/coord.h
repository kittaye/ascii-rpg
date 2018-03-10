#ifndef COORD_H_
#define COORD_H_

#include <stdbool.h>

typedef struct coord_t {
	int x;
	int y;
} coord_t;

/*
	Creates a new coordinate (x, y).
*/
coord_t New_Coord(int x, int y);

/*
	Compares equality between two coordinates a and b. Returns true if equal.
*/
bool Check_CoordsEqual(coord_t a, coord_t b);

#endif // !COORD_H_