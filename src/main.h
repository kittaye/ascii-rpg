#ifndef MAIN_H_
#define MAIN_H_

#include <stdio.h>

typedef struct dimensions_t {
	int x;
	int y;
} dimensions_t;

bool FContainsChar(FILE *fp, char char_to_find);
dimensions_t GetFileDimensions(FILE *fp);

#endif // !MAIN_H_