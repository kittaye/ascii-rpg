#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include <curses.h>
#include <stdbool.h>
#include "george_graphics.h"
#include "log_messages.h"
#include "ascii_game.h"

bool g_resize_error = false;
bool g_process_over = false;

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Run with: ./ascii_game [num_rooms] (OPTIONAL: [filename.txt])\n");
		exit(1);
	}

	// Get command line info.
	int num_rooms_specified = 0;
	int room_size_specified = 5;
	char *filename_specified = NULL;
	{
		num_rooms_specified = atoi(argv[1]);
		if (num_rooms_specified < MIN_ROOMS) {
			num_rooms_specified = MIN_ROOMS;
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
	InitGameState(&game_state);

	DrawHelpScreen();
	UpdateGameLog(&game_state.game_log, "Welcome to Asciiscape!");
	CreateDungeonFloor(&game_state, num_rooms_specified, room_size_specified, filename_specified);

	// Main game loop.
	while (!g_process_over) {
		Process(&game_state);
		if (game_state.floor_complete) {
			game_state.current_floor++;
			ResetDungeonFloor(&game_state);
			CreateDungeonFloor(&game_state, num_rooms_specified, room_size_specified, NULL);
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

bool FContainsChar(FILE *fp, char char_to_find) {
	assert(fp != NULL);

	int c;
	while ((c = fgetc(fp)) != EOF) {
		if (c == char_to_find) {
			return true;
		}
	}
	return false;
}

void InitGameState(game_state_t *state) {
	assert(state != NULL);

	int w = GEO_screen_width();
	int h = GEO_screen_height();

	state->debug_seed = time(NULL);
	srand(state->debug_seed);

	state->game_turns = 0;
	state->num_rooms_created = 0;
	state->current_floor = 1;
	state->fog_of_war = true;
	state->player_turn_over = false;
	state->floor_complete = false;
	state->current_target = ' ';

	state->player = InitPlayer(SPR_PLAYER);
	state->enemy_list = (entity_node_t*)NULL;
	state->num_enemies = 0;

	state->world_tiles = malloc(sizeof(*state->world_tiles) * (w + 1));
	assert(state->world_tiles != NULL);
	for (int i = 0; i < w; i++) {
		state->world_tiles[i] = malloc(sizeof(*state->world_tiles[i]) * (h + 1));
		assert(state->world_tiles[i] != NULL);
	}

	snprintf(state->game_log.line1, LOG_BUFFER_SIZE, LOGMSG_EMPTY_SPACE);
	snprintf(state->game_log.line2, LOG_BUFFER_SIZE, LOGMSG_EMPTY_SPACE);
	snprintf(state->game_log.line3, LOG_BUFFER_SIZE, LOGMSG_EMPTY_SPACE);

	state->debug_rcs = 0;

	// Initialise global zombie data.
	state->data_zombie.name = "Zombie";
	state->data_zombie.type = T_Enemy;
	state->data_zombie.max_health = 5;
	state->data_zombie.sprite = SPR_ZOMBIE;
	state->data_zombie.color = CLR_RED;

	// Initialise global wolf data
	state->data_werewolf.name = "Werewolf";
	state->data_werewolf.type = T_Enemy;
	state->data_werewolf.max_health = 3;
	state->data_werewolf.sprite = SPR_WEREWOLF;
	state->data_werewolf.color = CLR_RED;
}

void Cleanup_GameState(game_state_t *state) {
	assert(state != NULL);

	int w = GEO_screen_width();
	for (int i = 0; i < w; i++) {
		free(state->world_tiles[i]);
	}
	free(state->world_tiles);
	free(state->rooms);
	FreeEnemyList(&state->enemy_list);
}

void FreeEnemyList(entity_node_t **list) {
	assert(list != NULL);

	entity_node_t *tmp;
	while ((*list) != NULL) {
		tmp = (*list);
		(*list) = (*list)->next;
		free(tmp->entity);
		free(tmp);
	}
}

void ResetDungeonFloor(game_state_t *state) {
	assert(state != NULL);

	FreeEnemyList(&state->enemy_list);
	state->enemy_list = (entity_node_t*)NULL;

	free(state->rooms);
	state->rooms = (room_t*)NULL;

	// Reset other floor-specific variables.
	state->fog_of_war = true;
	state->num_enemies = 0;
	state->num_rooms_created = 0;
	state->debug_rcs = 0;			
}

void CreateDungeonFloor(game_state_t *state, int num_rooms_specified, int room_size_specified, char *opt_filename_specified) {
	assert(state != NULL);

	int w = GEO_screen_width();
	int h = GEO_screen_height();
	const int HUB_MAP_FREQUENCY = 4;

	// Create empty space.
	for (int x = 0; x < w; x++) {
		for (int y = 0; y < h; y++) {
			UpdateWorldTile(state->world_tiles, NewCoord(x, y), SPR_EMPTY, T_Empty, CLR_WHITE, NULL);
		}
	}

	state->rooms = malloc(sizeof(*state->rooms) * num_rooms_specified);
	assert(state->rooms != NULL);

	// Every few floors, the hub layout is created instead of a random dungeon layout.
	if (state->current_floor % HUB_MAP_FREQUENCY == 0) {
		opt_filename_specified = "hub.txt";
		state->player.stats.max_vision = PLAYER_MAX_VISION + 100;
	}
	else {
		state->player.stats.max_vision = PLAYER_MAX_VISION;
	}

	// Create the floor.
	if (opt_filename_specified != NULL) {
		CreateRoomsFromFile(state, opt_filename_specified);
	}
	else {
		//CreateOpenRooms(state, num_rooms_specified, room_size_specified);
		CreateClosedRooms(state, num_rooms_specified, room_size_specified);

		PopulateRooms(state);
	}

	UpdateGameLog(&state->game_log, LOGMSG_PLR_NEW_FLOOR, state->current_floor);
}

void PopulateRooms(game_state_t *state) {
	assert(state != NULL);
	assert(state->num_rooms_created > 0);

	// Choose a random room for the player spawn (except the last room created).
	int player_spawn_room_index = rand() % (state->num_rooms_created - 1);
	for (int i = 0; i < state->num_rooms_created; i++) {
		if (i == player_spawn_room_index) {
			coord_t pos = NewCoord(
				state->rooms[i].TL_corner.x + ((state->rooms[i].TR_corner.x - state->rooms[i].TL_corner.x) / 2),
				state->rooms[i].TR_corner.y + ((state->rooms[i].BR_corner.y - state->rooms[i].TR_corner.y) / 2)
			);
			SetPlayerPos(&state->player, pos);
			continue;
		}

		// Create gold, food, and enemies in rooms.
		for (int x = state->rooms[i].TL_corner.x + 1; x < state->rooms[i].TR_corner.x; x++) {
			for (int y = state->rooms[i].TL_corner.y + 1; y < state->rooms[i].BL_corner.y; y++) {
				int val = (rand() % 100) + 1;

				if (val >= 99) {
					if (state->num_enemies < MAX_ENEMIES) {
						entity_t *enemy = NULL;
						if (val == 99) {
							enemy = InitAndCreateEnemy(&state->data_zombie, NewCoord(x, y));
						}
						else if (val == 100) {
							enemy = InitAndCreateEnemy(&state->data_werewolf, NewCoord(x, y));
						}
						state->num_enemies++;
						UpdateWorldTile(state->world_tiles, enemy->pos, enemy->data->sprite, enemy->data->type, enemy->data->color, enemy);
						AddToEnemyList(&state->enemy_list, enemy);
					}
				}
				else if (val <= 2) {
					UpdateWorldTile(state->world_tiles, NewCoord(x, y), SPR_BIGGOLD, T_Item, CLR_GREEN, NULL);
				}
				else if (val <= 4) {
					UpdateWorldTile(state->world_tiles, NewCoord(x, y), SPR_GOLD, T_Item, CLR_GREEN, NULL);
				}
				else if (val <= 5) {
					UpdateWorldTile(state->world_tiles, NewCoord(x, y), SPR_FOOD, T_Item, CLR_GREEN, NULL);
				}
			}
		}

		// Spawn staircase in the last room created (tends to be near center of map due to dungeon creation algorithm).
		if (i == state->num_rooms_created - 1) {
			coord_t pos = NewCoord(
				state->rooms[i].TL_corner.x + ((state->rooms[i].TR_corner.x - state->rooms[i].TL_corner.x) / 2),
				state->rooms[i].TR_corner.y + ((state->rooms[i].BR_corner.y - state->rooms[i].TR_corner.y) / 2)
			);
			// TODO: staircase may spawn ontop of enemy, removing them from the world in an unexpected way. Find fix.
			UpdateWorldTile(state->world_tiles, pos, SPR_STAIRCASE, T_Special, CLR_YELLOW, NULL);
		}
	}
}

entity_t* InitAndCreateEnemy(entity_data_t *enemy_data, coord_t pos) {
	assert(enemy_data != NULL);

	entity_t *enemy = malloc(sizeof(*enemy));
	assert(enemy != NULL);

	enemy->data = enemy_data;
	enemy->curr_health = enemy->data->max_health;
	enemy->pos = pos;
	enemy->is_alive = true;
	if (rand() % 10 == 0) {
		enemy->loot = I_Map;
	}
	else {
		enemy->loot = I_None;
	}

	return enemy;
}

player_t InitPlayer(char sprite) {
	player_t player;

	player.sprite = sprite;
	player.pos = NewCoord(0, 0);
	player.color = CLR_CYAN;

	player.stats.level = 1;
	player.stats.max_health = 10;
	player.stats.curr_health = 10;
	player.stats.max_mana = 10;
	player.stats.curr_mana = 10;

	player.stats.max_vision = PLAYER_MAX_VISION;

	player.stats.s_STR = 1;
	player.stats.s_DEF = 1;
	player.stats.s_VIT = 1;
	player.stats.s_INT = 1;
	player.stats.s_LCK = 1;

	player.stats.enemies_slain = 0;
	player.stats.num_gold = 0;

	return player;
}

void SetPlayerPos(player_t *player, coord_t pos) {
	player->pos = pos;
}

void Process(game_state_t *state) {
	int w = GEO_screen_width();
	int h = GEO_screen_height();

	// Clear drawn elements from screen.
	GEO_clear_screen();

	// Draw all world tiles.
	for (int x = 0; x < w; x++) {
		for (int y = 0; y < h; y++) {
			ApplyVision(state, NewCoord(x, y));
		}
	}

	// Draw player.
	GEO_draw_char(state->player.pos.x, state->player.pos.y, state->player.sprite, state->player.color);

	// Draw last 3 game log lines.
	int y_start_pos = h - BOTTOM_PANEL_OFFSET;
	GEO_draw_formatted(0, y_start_pos + 0, CLR_WHITE, "* %s", state->game_log.line3);
	GEO_draw_formatted(0, y_start_pos + 1, CLR_WHITE, "* %s", state->game_log.line2);
	GEO_draw_formatted(0, y_start_pos + 2, CLR_WHITE, "* %s", state->game_log.line1);

	// Draw basic player info.
	GEO_draw_formatted_align_center(y_start_pos + 3, CLR_CYAN, "Health - %d/%d   Mana - %d/%d   Gold - %d",
		state->player.stats.curr_health, state->player.stats.max_health, state->player.stats.curr_mana, state->player.stats.max_mana, state->player.stats.num_gold);

	// Draw DEBUG line.
	GEO_draw_line(0, y_start_pos + 4, w - 1, y_start_pos + 4, '_', CLR_MAGENTA);

	// Draw DEBUG label.
	GEO_draw_formatted(0, y_start_pos + 5, CLR_MAGENTA, "   xy: (%d, %d)   rc(s): %d   seed: %d   |   Rooms: %d   Turns: %d",
		state->player.pos.x, state->player.pos.y, state->debug_rcs, (int)state->debug_seed, state->num_rooms_created, state->game_turns);

	// Display drawn elements to screen.
	GEO_show_screen();

	// Remember player's current position before a move is made.
	coord_t oldPos = state->player.pos;
	
	// Wait for next player input.
	NextPlayerInput(state);

	// If the player made a move, do world logic for this turn.
	if (state->player_turn_over) {
		PerformWorldLogic(state, &(state->world_tiles[state->player.pos.x][state->player.pos.y]), oldPos);
		if (state->player.stats.curr_health <= 0) {
			// GAME OVER.
			DrawDeathScreen();
			g_process_over = true;
		}
		state->player_turn_over = false;
		state->game_turns++;
	}
}

void PerformWorldLogic(game_state_t *state, const tile_t *curr_world_tile, coord_t oldPos) {
	assert(state != NULL);
	assert(curr_world_tile != NULL);

	state->current_target = ' ';

	switch (curr_world_tile->type) {
	case T_Item:
		if (curr_world_tile->sprite == SPR_GOLD || curr_world_tile->sprite == SPR_BIGGOLD) {
			int amt = 0;

			if (curr_world_tile->sprite == SPR_GOLD) {
				amt = (rand() % 4) + 1;
			}
			else if (curr_world_tile->sprite == SPR_BIGGOLD) {
				amt = (rand() % 5) + 5;
			}

			state->player.stats.num_gold += amt;
			if (amt == 1) {
				UpdateGameLog(&state->game_log, LOGMSG_PLR_GET_GOLD_SINGLE, amt);
			}
			else {
				UpdateGameLog(&state->game_log, LOGMSG_PLR_GET_GOLD_PLURAL, amt);
			}
		}
		else if (curr_world_tile->sprite == SPR_FOOD) {
			if (state->player.stats.curr_health < state->player.stats.max_health) {
				int amt = (rand() % 2) + 1;
				AddHealth(&state->player, amt);
				UpdateGameLog(&state->game_log, LOGMSG_PLR_GET_FOOD, amt);
			}
			else {
				UpdateGameLog(&state->game_log, LOGMSG_PLR_GET_FOOD_FULL);
			}
		}

		// All items are "picked up" (removed from world) when moved over.
		UpdateWorldTile(state->world_tiles, state->player.pos, SPR_EMPTY, T_Empty, CLR_WHITE, NULL);
		break;
	case T_Enemy:;
		entity_t *attackedEnemy = curr_world_tile->occupier;
		attackedEnemy->curr_health--;
		UpdateGameLog(&state->game_log, LOGMSG_PLR_DMG_ENEMY, attackedEnemy->data->name, 1);

		if (attackedEnemy->curr_health <= 0) {
			UpdateGameLog(&state->game_log, LOGMSG_PLR_KILL_ENEMY, attackedEnemy->data->name);
			attackedEnemy->is_alive = false;
			state->player.stats.enemies_slain++;
			UpdateWorldTile(state->world_tiles, attackedEnemy->pos, SPR_EMPTY, T_Empty, CLR_WHITE, NULL);

			if (attackedEnemy->loot == I_Map && state->fog_of_war) {
				UpdateGameLog(&state->game_log, LOGMSG_PLR_GET_MAP);
				state->fog_of_war = false;
			}
		}
		else {
			state->player.stats.curr_health--;
			UpdateGameLog(&state->game_log, LOGMSG_ENEMY_DMG_PLR, attackedEnemy->data->name, 1);
		}

		// Moving into any enemy results in no movement from the player.
		state->player.pos = oldPos;
		break;
	case T_Special:
		if (curr_world_tile->sprite == SPR_STAIRCASE) {
			UpdateGameLog(&state->game_log, LOGMSG_PLR_INTERACT_STAIRCASE);
			state->floor_complete = true;
		}

		// Moving into a special object results in no movement from the player.
		state->player.pos = oldPos;
		break;
	case T_Npc:
		if (curr_world_tile->sprite == SPR_MERCHANT) {
			UpdateGameLog(&state->game_log, LOGMSG_PLR_INTERACT_MERCHANT);
			state->current_target = SPR_MERCHANT;
		}
		// Moving into an NPC results in no movement from the player.
		state->player.pos = oldPos;
		break;
	default:
		if (state->game_log.line1[0] != ' ' || state->game_log.line2[0] != ' ' || state->game_log.line3[0] != ' ') {
			UpdateGameLog(&state->game_log, LOGMSG_EMPTY_SPACE);
		}
		break;
	}
}

bool CheckRoomCollision(const tile_t **world_tiles, const room_t *a) {
	assert(world_tiles != NULL);
	assert(a != NULL);

	if (CheckRoomMapBounds(a)) {
		return true;
	}

	int room_width = a->TR_corner.x - a->TL_corner.x;
	int room_height = a->BL_corner.y - a->TL_corner.y;

	for (int y = 0; y <= room_height; y++) {
		for (int x = 0; x <= room_width; x++) {
			if (world_tiles[a->TL_corner.x + x][a->TL_corner.y + y].type == T_Solid) {
				return true;
			}
		}
	}
	return false;
}

bool CheckRoomMapBounds(const room_t *a) {
	assert(a != NULL);

	if (CheckMapBounds(a->TL_corner) || CheckMapBounds(a->TR_corner) || CheckMapBounds(a->BL_corner)) {
		return true;
	}
	return false;
}

bool CheckMapBounds(coord_t coord) {
	int w = GEO_screen_width();
	int h = GEO_screen_height();

	if (coord.x >= w || coord.x < 0 || coord.y < 0 + TOP_PANEL_OFFSET || coord.y >= h - BOTTOM_PANEL_OFFSET) {
		return true;
	}
	return false;
}

void CreateRoomsFromFile(game_state_t *state, const char *filename) {
	assert(state != NULL);
	assert(filename != NULL);

	int w = GEO_screen_width();
	int h = GEO_screen_height();

	// Attempt to open the file.
	FILE *fp = NULL;
	fp = fopen(filename, "r");
	if (fp == NULL) {
		return;
	}
	else {
		int lineNum = 0;
		size_t len = 0;
		char *line = NULL;
		ssize_t read = 0;

		// FIRST PASS: Get map length and height, use it to find the anchor point (Top-left corner) to center the map on.
		int lineWidth = 0;
		while ((read = getline(&line, &len, fp)) != -1) {
			lineWidth = read;
			lineNum++;
		}
		coord_t anchor_centered_map_offset = NewCoord((w / 2) - (lineWidth / 2), (((h - BOTTOM_PANEL_OFFSET) / 2) - (lineNum / 2)));
		lineNum = 0;
		len = 0;
		free(line);
		line = NULL;
		read = 0;

		// Reset file read-ptr.
		rewind(fp);

		// SECOND PASS: generate the map relative to anchor point.
		while ((read = getline(&line, &len, fp)) != -1) {
			for (int i = 0; i < read - 1; i++) {
				tile_type_en type = T_Empty;
				int colour = CLR_WHITE;
				coord_t pos = NewCoord(anchor_centered_map_offset.x + i, anchor_centered_map_offset.y + lineNum);
				bool isEnemy = false;
				entity_t *entity = NULL;

				// Replace any /n, /t, etc. with a whitespace character.
				if (!isgraph(line[i])) {
					line[i] = SPR_EMPTY;
				}

				switch (line[i]) {
				case SPR_PLAYER:
					SetPlayerPos(&state->player, pos);
					line[i] = SPR_EMPTY;
					break;
				case SPR_WALL:
					type = T_Solid;
					break;
				case SPR_FOOD:
				case SPR_GOLD:
				case SPR_BIGGOLD:
					type = T_Item;
					colour = CLR_GREEN;
					break;
				case SPR_ZOMBIE:
					isEnemy = true;
					entity = InitAndCreateEnemy(&state->data_zombie, pos);
					break;
				case SPR_WEREWOLF:
					isEnemy = true;
					entity = InitAndCreateEnemy(&state->data_werewolf, pos);
					break;
				case SPR_STAIRCASE:
					type = T_Special;
					colour = CLR_YELLOW;
					break;
				case SPR_MERCHANT:
					type = T_Npc;
					colour = CLR_MAGENTA;
					break;
				default:
					break;
				}

				if (isEnemy) {
					type = T_Enemy;
					colour = CLR_RED;
					state->num_enemies++;
					AddToEnemyList(&state->enemy_list, entity);
				}

				UpdateWorldTile(state->world_tiles, pos, line[i], type, colour, entity);
			}
			line = NULL;
			lineNum++;
		}
		free(line);
		fclose(fp);
	}
}

coord_t NewCoord(int x, int y) {
	coord_t coord;
	coord.x = x;
	coord.y = y;
	return coord;
}

void CreateOpenRooms(game_state_t *state, int num_rooms_specified, int room_size_specified) {
	assert(state != NULL);

	int w = GEO_screen_width();
	int h = GEO_screen_height();
	const int EXTENDED_BOUNDS_OFFSET = 2;

	// If room size is larger than the smallest terminal dimension, clamp it to that dimension size.
	if (room_size_specified > fmin(w - EXTENDED_BOUNDS_OFFSET, h - BOTTOM_PANEL_OFFSET - EXTENDED_BOUNDS_OFFSET - TOP_PANEL_OFFSET)) {
		room_size_specified = fmin(w - EXTENDED_BOUNDS_OFFSET, h - BOTTOM_PANEL_OFFSET - EXTENDED_BOUNDS_OFFSET - TOP_PANEL_OFFSET);
	}

	for (int i = 0; i < num_rooms_specified; i++) {
		bool valid_room;
		do {
			valid_room = true;

			// Define a new room.
			DefineOpenRoom(&state->rooms[i], room_size_specified);

			// Check that this new room doesnt collide with anything solid (extended room hitbox: means completely seperated rooms).
			room_t extended_bounds_room = state->rooms[i];
			extended_bounds_room.TL_corner.x--;
			extended_bounds_room.TL_corner.y--;
			extended_bounds_room.TR_corner.x++;
			extended_bounds_room.TR_corner.y--;
			extended_bounds_room.BL_corner.x--;
			extended_bounds_room.BL_corner.y++;
			extended_bounds_room.BR_corner.x++;
			extended_bounds_room.BR_corner.y++;
			if (CheckRoomCollision((const tile_t **)state->world_tiles, &extended_bounds_room)) {
				state->debug_rcs++;
				valid_room = false;
			}

			// If number of room collisions passes threshold, prematurely stop room creation.
			if (state->debug_rcs >= DEBUG_RCS_LIMIT) {
				return;
			}
		} while (!valid_room);

		// Pick a random position for an opening.
		coord_t opening = GetRandRoomOpeningPos(&state->rooms[i]);

		// Create marked opening.
		UpdateWorldTile(state->world_tiles, opening, '?', T_Empty, CLR_WHITE, NULL);

		//Connect corners with walls and convert marked opening to a real opening.
		GenerateRoom(state->world_tiles, &state->rooms[i]);

		state->num_rooms_created++;
	}
}

void DefineOpenRoom(room_t *room, int room_size_specified) {
	assert(room != NULL);

	int w = GEO_screen_width();
	int h = GEO_screen_height();

	// TOP LEFT CORNER
	do {
		room->TL_corner.x = rand() % w;
		room->TL_corner.y = (rand() % (h - BOTTOM_PANEL_OFFSET - TOP_PANEL_OFFSET)) + TOP_PANEL_OFFSET;
	} while (room->TL_corner.x + room_size_specified - 1 >= w || room->TL_corner.y + room_size_specified - 1 >= (h - BOTTOM_PANEL_OFFSET));

	// TOP RIGHT CORNER
	room->TR_corner.x = room->TL_corner.x + room_size_specified - 1;
	room->TR_corner.y = room->TL_corner.y;

	// BOTTOM LEFT CORNER
	room->BL_corner.x = room->TL_corner.x;
	room->BL_corner.y = room->TL_corner.y + room_size_specified - 1;

	// BOTTOM RIGHT CORNER
	room->BR_corner.x = room->TL_corner.x + room_size_specified - 1;
	room->BR_corner.y = room->TL_corner.y + room_size_specified - 1;
}

coord_t GetRandRoomOpeningPos(const room_t *room) {
	assert(room != NULL);

	coord_t opening;
	if (rand() % 2 == 0) {
		opening.x = room->TL_corner.x + 1 + (rand() % (room->TR_corner.x - room->TL_corner.x - 1));
		if (rand() % 2 == 0) {
			opening.y = room->TL_corner.y;
		}
		else {
			opening.y = room->BL_corner.y;
		}
	}
	else {
		opening.y = room->TL_corner.y + 1 + (rand() % (room->BR_corner.y - room->TL_corner.y - 1));
		if (rand() % 2 == 0) {
			opening.x = room->TL_corner.x;
		}
		else {
			opening.x = room->TR_corner.x;
		}
	}

	return opening;
}

void CreateClosedRooms(game_state_t *state, int num_rooms_specified, int room_size_specified) {
	assert(state != NULL);

	int w = GEO_screen_width();
	int h = GEO_screen_height();
	const int NUM_INITIAL_OPENINGS = 4;
	int radius = (room_size_specified - 1) / 2;

	// Ensure space for first room created in center of map. Reduce radius if room is too big.
	coord_t pos = NewCoord(w / 2, h / 2);
	bool isValid;
	do {
		DefineClosedRoom(&state->rooms[0], pos, radius);
		isValid = true;
		if (CheckRoomCollision((const tile_t**)state->world_tiles, &state->rooms[0])) {
			isValid = false;
			state->debug_rcs++;
			radius--;
		}
	} while (!isValid);

	// Create room.
	InstantiateClosedRoomRecursive(state, pos, radius, NUM_INITIAL_OPENINGS, num_rooms_specified);
}

void InstantiateClosedRoomRecursive(game_state_t *state, coord_t pos, int radius, int iterations, int max_rooms) {
	assert(state != NULL);

	// Create the room.
	GenerateRoom(state->world_tiles, &state->rooms[state->num_rooms_created]);
	state->num_rooms_created++;

	coord_t oldPos = pos;
	int randDirection = rand() % 4;
	int nextRadius = GetNextRoomRadius();

	// Try to initialise new rooms and draw corridors to them, until all retry iterations are used up OR max rooms are reached.
	for (int i = 0; i < iterations; i++) {
		// Check if max rooms hasn't been met, otherwise all nested calls will return to stop further room creation.
		if (state->num_rooms_created == max_rooms) {
			return;
		}

		int oldDirection = randDirection;

		// Make sure the next random direction is different from the last.
		if (i > 0) {
			while (randDirection == oldDirection) {
				randDirection = rand() % 4;
			}
		}

		// Get the new room's position coordinates.
		coord_t newPos = oldPos;
		switch (randDirection) {
		case D_Up:
			newPos.y = oldPos.y - (radius * 2) - nextRadius;
			break;
		case D_Down:
			newPos.y = oldPos.y + (radius * 2) + nextRadius;
			break;
		case D_Left:
			newPos.x = oldPos.x - (radius * 2) - nextRadius;
			break;
		case D_Right:
			newPos.x = oldPos.x + (radius * 2) + nextRadius;
			break;
		default:
			break;
		}

		// Define the new room.
		DefineClosedRoom(&state->rooms[state->num_rooms_created], newPos, nextRadius);

		// Check that this new room doesnt collide with map boundaries or anything solid.
		if (CheckRoomCollision((const tile_t **)state->world_tiles, &state->rooms[state->num_rooms_created])) {
			state->debug_rcs++;
			continue;
		}

		// Check that the corridor that will connect this room and the new room doesnt collide with anything solid.
		if (CheckCorridorCollision((const tile_t**)state->world_tiles, oldPos, radius, randDirection)) {
			state->debug_rcs++;
			continue;
		}

		// Generate the corridor, connecting this room to the next (next room's opening is marked with '?').
		GenerateCorridor(state->world_tiles, oldPos, radius, randDirection);

		// Instantiate conjoined room.
		int newMaxIterations = 100;
		InstantiateClosedRoomRecursive(state, newPos, nextRadius, newMaxIterations, max_rooms);
	}
	return;
}

int GetNextRoomRadius() {
	return (rand() % 6) + 2;
}

void GenerateCorridor(tile_t **world_tiles, coord_t starting_room, int corridor_size, direction_en direction) {
	assert(world_tiles != NULL);

	switch (direction) {
	
	case D_Up:
		// Create opening for THIS room; create marked opening for NEXT room.
		UpdateWorldTile(world_tiles, NewCoord(starting_room.x, starting_room.y - corridor_size), SPR_EMPTY, T_Empty, CLR_WHITE, NULL);
		UpdateWorldTile(world_tiles, NewCoord(starting_room.x, starting_room.y - (corridor_size * 2)), '?', T_Empty, CLR_WHITE, NULL);

		// Connect rooms with the corridor.
		for (int i = 0; i < corridor_size; i++) {
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x - 1, starting_room.y - corridor_size - (i + 1)), SPR_WALL, T_Solid, CLR_WHITE, NULL);
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x + 1, starting_room.y - corridor_size - (i + 1)), SPR_WALL, T_Solid, CLR_WHITE, NULL);
		}
		break;
	case D_Down:
		UpdateWorldTile(world_tiles, NewCoord(starting_room.x, starting_room.y + corridor_size), SPR_EMPTY, T_Empty, CLR_WHITE, NULL);
		UpdateWorldTile(world_tiles, NewCoord(starting_room.x, starting_room.y + (corridor_size * 2)), '?', T_Empty, CLR_WHITE, NULL);

		for (int i = 0; i < corridor_size; i++) {
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x - 1, starting_room.y + corridor_size + (i + 1)), SPR_WALL, T_Solid, CLR_WHITE, NULL);
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x + 1, starting_room.y + corridor_size + (i + 1)), SPR_WALL, T_Solid, CLR_WHITE, NULL);
		}
		break;
	case D_Left:
		UpdateWorldTile(world_tiles, NewCoord(starting_room.x - corridor_size, starting_room.y), SPR_EMPTY, T_Empty, CLR_WHITE, NULL);
		UpdateWorldTile(world_tiles, NewCoord(starting_room.x - (corridor_size * 2), starting_room.y), '?', T_Empty, CLR_WHITE, NULL);

		for (int i = 0; i < corridor_size; i++) {
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x - corridor_size - (i + 1), starting_room.y - 1), SPR_WALL, T_Solid, CLR_WHITE, NULL);
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x - corridor_size - (i + 1), starting_room.y + 1), SPR_WALL, T_Solid, CLR_WHITE, NULL);
		}
		break;
	case D_Right:
		UpdateWorldTile(world_tiles, NewCoord(starting_room.x + corridor_size, starting_room.y), SPR_EMPTY, T_Empty, CLR_WHITE, NULL);
		UpdateWorldTile(world_tiles, NewCoord(starting_room.x + (corridor_size * 2), starting_room.y), '?', T_Empty, CLR_WHITE, NULL);

		for (int i = 0; i < corridor_size; i++) {
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x + corridor_size + (i + 1), starting_room.y - 1), SPR_WALL, T_Solid, CLR_WHITE, NULL);
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x + corridor_size + (i + 1), starting_room.y + 1), SPR_WALL, T_Solid, CLR_WHITE, NULL);
		}
		break;
	default:
		break;
	}
}

bool CheckCorridorCollision(const tile_t **world_tiles, coord_t starting_room, int corridor_size, direction_en direction) {
	assert(world_tiles != NULL);

	switch (direction) {
	case D_Up:
		for (int i = 0; i < corridor_size; i++) {
			if (CheckMapBounds(NewCoord(starting_room.x, starting_room.y - corridor_size - (i + 1)))) {
				return true;
			}
			else if (world_tiles[starting_room.x][starting_room.y - corridor_size - (i + 1)].type == T_Solid
				|| world_tiles[starting_room.x - 1][starting_room.y - corridor_size - (i + 1)].type == T_Solid
				|| world_tiles[starting_room.x + 1][starting_room.y - corridor_size - (i + 1)].type == T_Solid) {
				return true;
			}
		}
		return false;
	case D_Down:
		for (int i = 0; i < corridor_size; i++) {
			if (CheckMapBounds(NewCoord(starting_room.x, starting_room.y + corridor_size + (i + 1)))) {
				return true;
			}
			else if (world_tiles[starting_room.x][starting_room.y + corridor_size + (i + 1)].type == T_Solid
				|| world_tiles[starting_room.x - 1][starting_room.y + corridor_size + (i + 1)].type == T_Solid
				|| world_tiles[starting_room.x + 1][starting_room.y + corridor_size + (i + 1)].type == T_Solid) {
				return true;
			}
		}
		return false;
	case D_Left:
		for (int i = 0; i < corridor_size; i++) {
			if (CheckMapBounds(NewCoord(starting_room.x - corridor_size - (i + 1), starting_room.y))) {
				return true;
			}
			else if (world_tiles[starting_room.x - corridor_size - (i + 1)][starting_room.y].type == T_Solid
				|| world_tiles[starting_room.x - corridor_size - (i + 1)][starting_room.y - 1].type == T_Solid
				|| world_tiles[starting_room.x - corridor_size - (i + 1)][starting_room.y + 1].type == T_Solid) {
				return true;
			}
		}
		return false;
	case D_Right:
		for (int i = 0; i < corridor_size; i++) {
			if (CheckMapBounds(NewCoord(starting_room.x + corridor_size + (i + 1), starting_room.y))) {
				return true;
			}
			else if (world_tiles[starting_room.x + corridor_size + (i + 1)][starting_room.y].type == T_Solid
				|| world_tiles[starting_room.x + corridor_size + (i + 1)][starting_room.y - 1].type == T_Solid
				|| world_tiles[starting_room.x + corridor_size + (i + 1)][starting_room.y + 1].type == T_Solid) {
				return true;
			}
		}
		return false;
	default:
		return true;
	}
}

void DefineClosedRoom(room_t *room, coord_t pos, int radius) {
	assert(room != NULL);

	room->TL_corner.x = pos.x - radius;
	room->TL_corner.y = pos.y - radius;

	room->TR_corner.x = pos.x + radius;
	room->TR_corner.y = pos.y - radius;

	room->BL_corner.x = pos.x - radius;
	room->BL_corner.y = pos.y + radius;

	room->BR_corner.x = pos.x + radius;
	room->BR_corner.y = pos.y + radius;
}

void GenerateRoom(tile_t **world_tiles, const room_t *room) {
	assert(world_tiles != NULL);
	assert(room != NULL);

	//Connect corners with walls; marked tiles (?) become openings.

	// Bottom and top walls.
	for (int x = room->TL_corner.x; x <= room->TR_corner.x; x++) {
		if (world_tiles[x][room->TL_corner.y].sprite != '?') {
			UpdateWorldTile(world_tiles, NewCoord(x, room->TL_corner.y), SPR_WALL, T_Solid, CLR_WHITE, NULL);
		}
		else {
			UpdateWorldTile(world_tiles, NewCoord(x, room->TL_corner.y), SPR_EMPTY, T_Empty, CLR_WHITE, NULL);
		}

		if (world_tiles[x][room->BL_corner.y].sprite != '?') {
			UpdateWorldTile(world_tiles, NewCoord(x, room->BL_corner.y), SPR_WALL, T_Solid, CLR_WHITE, NULL);
		}
		else {
			UpdateWorldTile(world_tiles, NewCoord(x, room->BL_corner.y), SPR_EMPTY, T_Empty, CLR_WHITE, NULL);
		}
	}

	// Left and right walls.
	for (int y = room->TR_corner.y; y <= room->BR_corner.y; y++) {
		if (world_tiles[room->TR_corner.x][y].sprite != '?') {
			UpdateWorldTile(world_tiles, NewCoord(room->TR_corner.x, y), SPR_WALL, T_Solid, CLR_WHITE, NULL);
		}
		else {
			UpdateWorldTile(world_tiles, NewCoord(room->TR_corner.x, y), SPR_EMPTY, T_Empty, CLR_WHITE, NULL);
		}
		if (world_tiles[room->TL_corner.x][y].sprite != '?') {
			UpdateWorldTile(world_tiles, NewCoord(room->TL_corner.x, y), SPR_WALL, T_Solid, CLR_WHITE, NULL);
		}
		else {
			UpdateWorldTile(world_tiles, NewCoord(room->TL_corner.x, y), SPR_EMPTY, T_Empty, CLR_WHITE, NULL);
		}
	}
}

void UpdateWorldTile(tile_t **world_tiles, coord_t pos, char sprite, tile_type_en type, int color, entity_t *occupier) {
	assert(world_tiles != NULL);

	world_tiles[pos.x][pos.y].sprite = sprite;
	world_tiles[pos.x][pos.y].type = type;
	world_tiles[pos.x][pos.y].color = color;
	world_tiles[pos.x][pos.y].occupier = occupier;
}

void ApplyVision(const game_state_t *state, coord_t pos) {
	assert(state != NULL);
	
	int vision_to_tile = abs((pos.x - state->player.pos.x) * (pos.x - state->player.pos.x)) + abs((pos.y - state->player.pos.y) * (pos.y - state->player.pos.y));

	if (state->fog_of_war) {
		if (vision_to_tile < state->player.stats.max_vision) {
			GEO_draw_char(pos.x, pos.y, state->world_tiles[pos.x][pos.y].sprite, state->world_tiles[pos.x][pos.y].color);
		}
	}
	else {
		GEO_draw_char(pos.x, pos.y, state->world_tiles[pos.x][pos.y].sprite, state->world_tiles[pos.x][pos.y].color);
	}
}

void UpdateGameLog(log_list_t *game_log, const char *format, ...) {
	assert(game_log != NULL);

	// Cycle old log messages upwards.
	snprintf(game_log->line3, LOG_BUFFER_SIZE, game_log->line2);
	snprintf(game_log->line2, LOG_BUFFER_SIZE, game_log->line1);

	// Feed in the new log message to the front of the log list.
	va_list argp;
	va_start(argp, format);
	vsnprintf(game_log->line1, LOG_BUFFER_SIZE, format, argp);
	va_end(argp);
}

void EnemyCombatUpdate(game_state_t *state, entity_node_t *enemy_list) {
	assert(state != NULL);
	assert(enemy_list != NULL);

	entity_node_t *node = enemy_list;
	for (; node != NULL; node = node->next) {
		if (node->entity->pos.x == state->player.pos.x && node->entity->pos.y == state->player.pos.y) {
			state->player.stats.curr_health--;
			node->entity->curr_health--;
			if (node->entity->curr_health <= 0) {
				node->entity->is_alive = false;
				UpdateWorldTile(state->world_tiles, node->entity->pos, SPR_EMPTY, T_Empty, CLR_WHITE, NULL);
			}
			break;
		}
	}
}

void NextPlayerInput(game_state_t *state) {
	assert(state != NULL);

	int w = GEO_screen_width();
	int h = GEO_screen_height();

	player_t *player = &state->player;
	coord_t oldPos = player->pos;

	// Get a character code from standard input without waiting (but looped until a valid key is pressed).
	// This method of input processing allows for interrupt handling (dealing with resizes).
	bool valid_key_pressed = false;
	while (!valid_key_pressed)
	{
		int key = GEO_get_char();
		switch (key) {
		case KEY_UP:
			if (player->pos.y - 1 >= 0 + TOP_PANEL_OFFSET && state->world_tiles[player->pos.x][player->pos.y - 1].type != T_Solid) {
				player->pos.y--;
			}
			valid_key_pressed = true;
			break;
		case KEY_DOWN:
			if (player->pos.y + 1 < h - BOTTOM_PANEL_OFFSET && state->world_tiles[player->pos.x][player->pos.y + 1].type != T_Solid) {
				player->pos.y++;
			}
			valid_key_pressed = true;
			break;
		case KEY_LEFT:
			if (player->pos.x - 1 >= 0 && state->world_tiles[player->pos.x - 1][player->pos.y].type != T_Solid) {
				player->pos.x--;
			}
			valid_key_pressed = true;
			break;
		case KEY_RIGHT:
			if (player->pos.x + 1 < w && state->world_tiles[player->pos.x + 1][player->pos.y].type != T_Solid) {
				player->pos.x++;
			}
			valid_key_pressed = true;
			break;
		case 'h':
			DrawHelpScreen();
			valid_key_pressed = true;
			break;
		case 'i':
			DrawMenuScreen(state);
			valid_key_pressed = true;
			break;
		case 'f':
			state->fog_of_war = !state->fog_of_war;
			valid_key_pressed = true;
			break;
		case 'q':
			g_process_over = true;
			valid_key_pressed = true;
			break;
		case KEY_RESIZE:
			g_process_over = true;
			g_resize_error = true;
			valid_key_pressed = true;
		case '\n':
		case KEY_ENTER:
			InteractWithNPC(state);
			valid_key_pressed = true;
			break;
		default:
			break;
		}
	}

	// The player's turn is over if they moved on this turn.
	state->player_turn_over = false;
	if (player->pos.x != oldPos.x || player->pos.y != oldPos.y) {
		state->player_turn_over = true;
	}
}

void InteractWithNPC(game_state_t *state) {
	switch (state->current_target)
	{
	case SPR_MERCHANT:
		DrawMerchantScreen(state);
		return;
	default:
		return;
	}
}

void DrawHelpScreen() {
	int w = GEO_screen_width() - 1;
	int h = GEO_screen_height() - 1;

	GEO_clear_screen();

	GEO_draw_align_center(h / 2 - 4, "Asciiscape by George Delosa", CLR_WHITE);
	GEO_draw_align_center(h / 2 - 2, "up/down/left/right: movement keys", CLR_WHITE);
	GEO_draw_string(w / 2 - 11, h / 2 - 1, "h: show this help screen", CLR_WHITE);
	GEO_draw_string(w / 2 - 11, h / 2 - 0, "i: show player info screen", CLR_WHITE);
	GEO_draw_string(w / 2 - 11, h / 2 + 1, "f: toggle fog of war", CLR_WHITE);
	GEO_draw_string(w / 2 - 11, h / 2 + 2, "q: quit game", CLR_WHITE);
	GEO_draw_align_center(h / 2 + 4, "Press any key to continue...", CLR_WHITE);

	GEO_show_screen();
	GetAnyKeyInput();
}

int GetAnyKeyInput() {
	while (1) {
		int key = GEO_get_char();
		switch (key)
		{
		case KEY_RESIZE:
			g_resize_error = true;
			g_process_over = true;
			return KEY_RESIZE;
		case ERR:
			break;
		default:
			return key;
		}
	}
}

void DrawDeathScreen() {
	int h = GEO_screen_height() - 1;

	GEO_clear_screen();

	GEO_draw_align_center(h / 2, "YOU DIED", CLR_RED);

	GEO_show_screen();
	GetAnyKeyInput();
}

void DrawMerchantScreen(game_state_t * state) {
	assert(state != NULL);

	int w = GEO_screen_width() - 1;
	int h = GEO_screen_height() - 1;

	GEO_clear_screen();

	GEO_draw_line(0, 0, w, 0, '*', CLR_WHITE);
	GEO_draw_line(0, 0, 0, h, '*', CLR_WHITE);
	GEO_draw_line(w, 0, w, h, '*', CLR_WHITE);
	GEO_draw_line(0, h, w, h, '*', CLR_WHITE);

	int x = 3;
	int y = 2;

	GEO_draw_align_center(y++, "Trading with Merchant", CLR_YELLOW);
	y++;
	GEO_draw_string(x, y++, "Option    Item         Price (gold)", CLR_YELLOW);
	GEO_draw_string(x, y++, "(1)\t\t\t\t\t\tFood (+1)\t\t\t\t\t\t20", CLR_WHITE);
	GEO_draw_string(x, y++, "(2)\t\t\t\t\t\tFood (+2)\t\t\t\t\t\t35", CLR_WHITE);
	y++;
	GEO_draw_formatted(x, y++, CLR_WHITE, "Your gold: %d", state->player.stats.num_gold);
	y++;
	GEO_draw_string(x, y++, "Press an option to buy, or press ENTER to leave.", CLR_WHITE);

	GEO_show_screen();

	bool validKeyPress = false;
	while (!validKeyPress) {
		int key = GetAnyKeyInput();
		switch (key)
		{
		case '1':
			if (state->player.stats.num_gold >= 20) {
				AddHealth(&state->player, 1);
				state->player.stats.num_gold -= 20;
				UpdateGameLog(&state->game_log, "Merchant: \"Thank you for purchasing my Food (+1)!\"");
				validKeyPress = true;
			}
			break;
		case '2':
			if (state->player.stats.num_gold >= 35) {
				AddHealth(&state->player, 2);
				state->player.stats.num_gold -= 35;
				UpdateGameLog(&state->game_log, "Merchant: \"Thank you for purchasing my Food (+2)!\"");
				validKeyPress = true;
			}
			break;
		case '\n':
		case KEY_ENTER:
		case 'q':
			validKeyPress = true;
			break;
		case KEY_RESIZE:
			return;
		default:
			break;
		}
	}
}

void AddHealth(player_t *player, int amt) {
	assert(player != NULL);

	if (player->stats.curr_health + amt > player->stats.max_health) {
		amt = player->stats.max_health - player->stats.curr_health;
	}
	player->stats.curr_health += amt;
}

void DrawMenuScreen(const game_state_t * state) {
	assert(state != NULL);

	int w = GEO_screen_width() - 1;
	int h = GEO_screen_height() - 1;

	GEO_clear_screen();

	GEO_draw_line(0, 0, w, 0, '*', CLR_WHITE);
	GEO_draw_line(0, 0, 0, h, '*', CLR_WHITE);
	GEO_draw_line(w, 0, w, h, '*', CLR_WHITE);
	GEO_draw_line(0, h, w, h, '*', CLR_WHITE);

	int x = 3;
	int y = 2;

	GEO_draw_formatted_align_center(y++, CLR_YELLOW, "Hero,  Lvl. %d", state->player.stats.level);
	GEO_draw_formatted_align_center(y++, CLR_WHITE, "Current floor: %d", state->current_floor);
	y++;
	GEO_draw_formatted(x, y++, CLR_RED, "Health - %d/%d", state->player.stats.curr_health, state->player.stats.max_health);
	GEO_draw_formatted(x, y++, CLR_CYAN, "Mana - %d/%d", state->player.stats.curr_mana, state->player.stats.max_mana);
	y++;
	GEO_draw_formatted(x, y++, CLR_GREEN, "STR - %d", state->player.stats.s_STR);
	GEO_draw_formatted(x, y++, CLR_GREEN, "DEF - %d", state->player.stats.s_DEF);
	GEO_draw_formatted(x, y++, CLR_GREEN, "VIT - %d", state->player.stats.s_VIT);
	GEO_draw_formatted(x, y++, CLR_GREEN, "INT - %d", state->player.stats.s_INT);
	GEO_draw_formatted(x, y++, CLR_GREEN, "LCK - %d", state->player.stats.s_LCK);
	y++;
	GEO_draw_formatted(x, y++, CLR_WHITE, "Enemies slain - %d", state->player.stats.enemies_slain);
	GEO_draw_formatted(x, y++, CLR_WHITE, "Gold - %d", state->player.stats.num_gold);

	GEO_show_screen();
	GetAnyKeyInput();
}

void AddToEnemyList(entity_node_t **list, entity_t *entity) {
	assert(list != NULL);
	assert(entity != NULL);

	entity_node_t *node = malloc(sizeof(*node));
	assert(node != NULL);

	node->entity = entity;
	node->next = NULL;

	if (*list == NULL) {
		*list = node;
	}
	else {
		entity_node_t *searcher = *list;
		while (searcher->next != NULL) {
			searcher = searcher->next;
		}
		searcher->next = node;
	}
}
