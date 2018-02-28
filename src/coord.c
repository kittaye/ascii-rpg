#include "coord.h"

coord_t NewCoord(int x, int y) {
	coord_t coord;
	coord.x = x;
	coord.y = y;
	return coord;
}

bool CoordsEqual(coord_t a, coord_t b) {
	if (a.x == b.x && a.y == b.y) return true;
	return false;
}