#ifndef HELPERS_H_
#define HELPERS_H_

#include <stdio.h>

typedef struct dimensions_t {
	int x;
	int y;
} dimensions_t;

/*
	Returns true if the specified char was found in the file.
*/
bool Check_FContainsChar(FILE *fp, char char_to_find);

/*
	Returns the length of the longest line and the number of lines as (x, y) dimensions of the file respectively.
*/
dimensions_t Get_FileDimensions(FILE *fp);

#endif // !HELPERS_H_