#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "george_graphics.h"
#include "log_messages.h"
#include "ascii_game.h"

// Private struct.
struct vector2 {
	int x;
	int y;
};

// Private functions.
static bool FContainsChar(FILE *fp, char char_to_find);
static struct vector2 GetFileDimensions(FILE *fp);

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Run with: ./ascii_game [num_rooms] (OPTIONAL: [filename.txt])\n");
		exit(1);
	}

	// Get command line info.
	int num_rooms_specified = 0;
	const char *filename_specified = NULL;
	struct vector2 filename_specified_dimensions;
	{
		num_rooms_specified = (int)strtol(argv[1], 0, 0);
		num_rooms_specified = CLAMP(num_rooms_specified, MIN_ROOMS, MAX_ROOMS);

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

			filename_specified_dimensions = GetFileDimensions(fp);

			fclose(fp);
		}
	}

	// Initialise curses.
	GEO_setup_screen();

	// Ensure the terminal size is large enough to create the hub file.
	struct vector2 hub_file_dimensions;
	FILE *fp;
	fp = fopen(HUB_FILENAME, "r");
	if (fp == NULL) {
		fprintf(stderr, "The hub file could not be found (Ensure it's directory is: %s). Exiting...\n", HUB_FILENAME);
		fclose(fp);
		exit(1);
	} else {
		hub_file_dimensions = GetFileDimensions(fp);
		int min_width = hub_file_dimensions.x + RIGHT_PANEL_OFFSET;
		int min_height = hub_file_dimensions.y + BOTTOM_PANEL_OFFSET;

		if (GEO_screen_width() < min_width || GEO_screen_height() < min_height) {
			GEO_cleanup_screen();
			fprintf(stderr, "The current terminal size must be at least (%dx%d) to run the game. Exiting...\n",
				min_width, min_height);
			exit(1);
		}
	}

	// Ensure the terminal size is large enough to create the command-line specified file.
	if (filename_specified != NULL) {
		int min_width = filename_specified_dimensions.x + RIGHT_PANEL_OFFSET;
		int min_height = filename_specified_dimensions.y + BOTTOM_PANEL_OFFSET;

		if (GEO_screen_width() < min_width || GEO_screen_height() < min_height) {
			GEO_cleanup_screen();
			fprintf(stderr, "The specified file's dimensions (%dx%d) are too large for the current terminal size. Exiting...\n", 
				min_width, min_height);
			exit(1);
		}
	}

	// Initialise the game.
	game_state_t game_state;
	Init_GameState(&game_state);
	game_state.player = Create_Player();

	Draw_HelpScreen(&game_state);
	Update_GameLog(&game_state.game_log, LOGMSG_WELCOME);
	InitCreate_DungeonFloor(&game_state, num_rooms_specified, filename_specified);

	// Main game loop.
	while (!g_process_over) {
		Process(&game_state);

		if (game_state.floor_complete) {
			game_state.current_floor++;
			game_state.floor_complete = false;

			// Every few floors, the hub layout is created instead of a random dungeon layout.
			if (game_state.current_floor % HUB_MAP_FREQUENCY == 0) {
				filename_specified = HUB_FILENAME;
				game_state.player.stats.max_vision = PLAYER_MAX_VISION + 100;
			} else {
				filename_specified = NULL;
				game_state.player.stats.max_vision = PLAYER_MAX_VISION;
			}

			Cleanup_DungeonFloor(&game_state);
			InitCreate_DungeonFloor(&game_state, num_rooms_specified, filename_specified);
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

static struct vector2 GetFileDimensions(FILE *fp) {
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
	
	// TODO: find out why -1 is needed...
	struct vector2 result = {.x = longest_line - 1, .y = lineNum};
	return result;
}