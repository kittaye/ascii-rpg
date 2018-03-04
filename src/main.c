#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "george_graphics.h"
#include "log_messages.h"
#include "ascii_game.h"

// Global variable definitions.
bool g_resize_error = false;
bool g_process_over = false;

// Private function.
static bool FContainsChar(FILE *fp, char char_to_find);

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Run with: ./ascii_game [num_rooms] (OPTIONAL: [filename.txt])\n");
		exit(1);
	}

	// Get command line info.
	int num_rooms_specified = 0;
	const char *filename_specified = NULL;
	{
		num_rooms_specified = (int)strtol(argv[1], 0, 0);
		if (num_rooms_specified < MIN_ROOMS) {
			num_rooms_specified = MIN_ROOMS;
		} else if (num_rooms_specified > MAX_ROOMS) {
			num_rooms_specified = MAX_ROOMS;
		}

		if (argc == 3) {
			filename_specified = argv[2];
			FILE *fp;
			fp = fopen(filename_specified, "r");
			if (fp == NULL) {
				fprintf(stderr, "File %s was not found (Ensure format is *.txt). Exiting...\n", filename_specified);
				fclose(fp);
				exit(1);
			}

			if (!FContainsChar(fp, SPR_PLAYER)) {
				fprintf(stderr, "File %s does not contain a player sprite (Represented as: %c). Exiting...\n", filename_specified, SPR_PLAYER);
				fclose(fp);
				exit(1);
			}
			fclose(fp);
		}
	}

	// Initialise curses.
	GEO_setup_screen();

	if (GEO_screen_width() < MIN_SCREEN_WIDTH || GEO_screen_height() < MIN_SCREEN_HEIGHT) {
		GEO_cleanup_screen();
		fprintf(stderr, "Terminal screen dimensions must be at least %dx%d to run this application. Exiting...\n", MIN_SCREEN_WIDTH, MIN_SCREEN_HEIGHT);
		exit(1);
	}

	game_state_t game_state;
	Init_GameState(&game_state);
	game_state.player = Create_Player();

	Draw_HelpScreen();
	Update_GameLog(&game_state.game_log, LOGMSG_WELCOME);
	InitCreate_DungeonFloor(&game_state, num_rooms_specified, filename_specified);

	// Main game loop.
	while (!g_process_over) {
		Process(&game_state);
		if (game_state.floor_complete) {
			game_state.current_floor++;
			Cleanup_DungeonFloor(&game_state);
			InitCreate_DungeonFloor(&game_state, num_rooms_specified, NULL);
			game_state.floor_complete = false;
		}
	}

	Cleanup_GameState(&game_state);

	// Terminate curses.
	GEO_cleanup_screen();

	if (g_resize_error) {
		fprintf(stderr, "*** Terminal resize detected! ***\nThis application does not support dynamic terminal resizing. Exiting...\n");
		exit(1);
	}

	return 0;
}

static bool FContainsChar(FILE *fp, char char_to_find) {
	assert(fp != NULL);

	int c;
	while ((c = fgetc(fp)) != EOF) {
		if (c == char_to_find) {
			return true;
		}
	}
	return false;
}