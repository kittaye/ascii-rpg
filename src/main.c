#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "george_graphics.h"
#include "log_messages.h"
#include "ascii_game.h"
#include "helpers.h"

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Run with: ./ascii_game [num_rooms]\n");
		exit(1);
	}

	// Get command line info.
	int num_rooms_specified = (int)strtol(argv[1], 0, 0);
	num_rooms_specified = CLAMP(num_rooms_specified, MIN_ROOMS, MAX_ROOMS);

	// Initialise curses.
	GEO_setup_screen();

	// Ensure terminal size is bigger than the minimum values.
	if (GEO_screen_width() < MIN_TERMINAL_WIDTH || GEO_screen_height() < MIN_TERMINAL_HEIGHT) {
		GEO_cleanup_screen();
		fprintf(stderr, "The terminal size must be at least (%dx%d) to run the game. Exiting...\n",
			MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT);
		exit(1);
	}

	// Ensure the terminal size is large enough to create the hub file.
	//FILE *fp;
	//fp = fopen(HUB_FILENAME, "r");
	//if (fp == NULL) {
	//	fprintf(stderr, "The game's hub file \"%s\" could not be found. Exiting...\n", HUB_FILENAME);
	//	GEO_cleanup_screen();
	//	fclose(fp);
	//	exit(1);
	//} else {
	//	dimensions_t hub_file_dimensions = Get_FileDimensions(fp);
	//	int min_width = hub_file_dimensions.x + RIGHT_PANEL_OFFSET;
	//	int min_height = hub_file_dimensions.y + BOTTOM_PANEL_OFFSET;

	//	if (GEO_screen_width() < min_width || GEO_screen_height() < min_height) {
	//		GEO_cleanup_screen();
	//		fprintf(stderr, "The terminal size must be at least (%dx%d) to run the game. Exiting...\n",
	//			min_width, min_height);
	//		exit(1);
	//	}
	//}

	// Initialise the game.
	game_state_t game_state;
	Init_GameState(&game_state);
	game_state.player = Create_Player();

	Draw_HelpScreen(&game_state);
	Update_GameLog(&game_state.game_log, LOGMSG_WELCOME);
	InitCreate_DungeonFloor(&game_state, num_rooms_specified, NULL);

	// Main game loop.
	while (!g_process_over) {
		Process(&game_state);

		if (game_state.floor_complete) {
			Cleanup_DungeonFloor(&game_state);
			game_state.current_floor++;
			game_state.floor_complete = false;
			InitCreate_DungeonFloor(&game_state, num_rooms_specified, NULL);
		}
	}

	// Cleanup dynamically allocated memory.
	Cleanup_DungeonFloor(&game_state);
	Cleanup_GameState(&game_state);

	// Terminate curses.
	GEO_cleanup_screen();

	if (g_resize_error) {
		fprintf(stderr, "*** Terminal resize detected! ***\nThis application does not support dynamic terminal resizing. Exiting...\n");
		exit(1);
	}

	return 0;
}

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