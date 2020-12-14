#include <stdbool.h>
#include "helpers.h"

bool Check_FContainsChar(FILE *fp, char char_to_find) {
	assert(fp != NULL);

	int c;
	while ((c = fgetc(fp)) != EOF) {
		if (c == char_to_find) {
			return true;
		}
	}
	return false;
}

dimensions_t Get_FileDimensions(FILE *fp) {
	int lineNum = 0;
	size_t len = 0;
	char *line = NULL;
	ssize_t read = 0;

	int longest_line = 0;
	while ((read = getline(&line, &len, fp)) != -1) {
		if (read > longest_line) {
			longest_line = read;
		}
		lineNum++;
	}
	
	dimensions_t result = {.x = longest_line - 1, .y = lineNum};	//Longest line - 1 to remove the newline character.
	return result;
}