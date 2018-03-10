#include "coord.h"

coord_t New_Coord(int x, int y) {
	coord_t coord;
	coord.x = x;
	coord.y = y;
	return coord;
}

bool Check_CoordsEqual(coord_t a, coord_t b) {
	if (a.x == b.x && a.y == b.y) return true;
	return false;
}