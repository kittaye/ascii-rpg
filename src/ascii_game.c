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
#include "items.h"
#include "enemies.h"
#include "coord.h"
#include "ascii_game.h"

static bool g_resize_error = false;
static bool g_process_over = false;

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Run with: ./ascii_game [num_rooms] (OPTIONAL: [filename.txt])\n");
		exit(1);
	}

	// Get command line info.
	int num_rooms_specified = 0;
	int room_size_specified = 5;
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
	InitGameState(&game_state);

	DrawHelpScreen();
	UpdateGameLog(&game_state.game_log, LOGMSG_WELCOME);
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

	state->debug_seed = time(NULL);
	srand(state->debug_seed);

	state->game_turns = 0;
	state->num_rooms_created = 0;
	state->current_floor = 1;
	state->fog_of_war = true;
	state->player_turn_over = false;
	state->floor_complete = false;
	state->current_target = ' ';
	state->debug_rcs = 0;

	state->player = InitPlayer(SPR_PLAYER);
	state->enemy_list = (enemy_node_t*)NULL;

	const int world_screen_w = WorldScreenWidth();
	const int world_screen_h = WorldScreenHeight();

	state->world_tiles = malloc(sizeof(*state->world_tiles) * world_screen_w);
	assert(state->world_tiles != NULL);
	for (int i = 0; i < world_screen_w; i++) {
		state->world_tiles[i] = malloc(sizeof(*state->world_tiles[i]) * world_screen_h);
		assert(state->world_tiles[i] != NULL);
	}

	snprintf(state->game_log.line1, LOG_BUFFER_SIZE, LOGMSG_EMPTY_SPACE);
	snprintf(state->game_log.line2, LOG_BUFFER_SIZE, LOGMSG_EMPTY_SPACE);
	snprintf(state->game_log.line3, LOG_BUFFER_SIZE, LOGMSG_EMPTY_SPACE);
}

void Cleanup_GameState(game_state_t *state) {
	assert(state != NULL);

	const int world_screen_w = WorldScreenWidth();
	for (int i = 0; i < world_screen_w; i++) {
		free(state->world_tiles[i]);
	}
	free(state->world_tiles);
	free(state->rooms);
	FreeEnemyList(&state->enemy_list);
}

void ResetDungeonFloor(game_state_t *state) {
	assert(state != NULL);

	FreeEnemyList(&state->enemy_list);
	state->enemy_list = (enemy_node_t*)NULL;

	free(state->rooms);
	state->rooms = (room_t*)NULL;

	// Reset other floor-specific variables.
	state->fog_of_war = true;
	state->num_rooms_created = 0;
	state->debug_rcs = 0;
}

void CreateDungeonFloor(game_state_t *state, int num_rooms_specified, int room_size_specified, const char *filename_specified) {
	assert(state != NULL);

	const int world_screen_w = WorldScreenWidth();
	const int world_screen_h = WorldScreenHeight();
	const int HUB_MAP_FREQUENCY = 4;

	// Create empty space.
	for (int x = 0; x < world_screen_w; x++) {
		for (int y = 0; y < world_screen_h; y++) {
			UpdateWorldTile(state->world_tiles, NewCoord(x, y), SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
		}
	}

	state->rooms = malloc(sizeof(*state->rooms) * num_rooms_specified);
	assert(state->rooms != NULL);

	// Every few floors, the hub layout is created instead of a random dungeon layout.
	if (state->current_floor % HUB_MAP_FREQUENCY == 0) {
		filename_specified = HUB_FILENAME;
		state->player.stats.max_vision = PLAYER_MAX_VISION + 100;
	} else {
		state->player.stats.max_vision = PLAYER_MAX_VISION;
	}

	// Create the floor.
	if (filename_specified != NULL) {
		CreateRoomsFromFile(state, filename_specified);
	} else {
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
					enemy_t *enemy = NULL;
					if (val == 99) {
						enemy = InitAndCreateEnemy(GetEnemyData(E_Zombie), NewCoord(x, y));
					} else if (val == 100) {
						enemy = InitAndCreateEnemy(GetEnemyData(E_Werewolf), NewCoord(x, y));
					}
					UpdateWorldTile(state->world_tiles, enemy->pos, enemy->data->sprite, TileType_Enemy, Clr_Red, enemy, NULL);
					AddToEnemyList(&state->enemy_list, enemy);

				} else if (val <= 2) {
					UpdateWorldTile(state->world_tiles, NewCoord(x, y), SPR_BIGGOLD, TileType_Item, Clr_Green, NULL, NULL);
				} else if (val <= 4) {
					UpdateWorldTile(state->world_tiles, NewCoord(x, y), SPR_GOLD, TileType_Item, Clr_Green, NULL, NULL);
				} else if (val <= 5) {
					UpdateWorldTile(state->world_tiles, NewCoord(x, y), SPR_SMALLFOOD, TileType_Item, Clr_Green, NULL, GetItem(I_SmallFood));
				} else if (val <= 6) {
					UpdateWorldTile(state->world_tiles, NewCoord(x, y), SPR_BIGFOOD, TileType_Item, Clr_Green, NULL, GetItem(I_BigFood));
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
			UpdateWorldTile(state->world_tiles, pos, SPR_STAIRCASE, TileType_Special, Clr_Yellow, NULL, NULL);
		}
	}
}

enemy_t* InitAndCreateEnemy(const enemy_data_t *enemy_data, coord_t pos) {
	assert(enemy_data != NULL);

	enemy_t *enemy = malloc(sizeof(*enemy));
	assert(enemy != NULL);

	enemy->data = enemy_data;
	enemy->curr_health = enemy->data->max_health;
	enemy->pos = pos;
	enemy->is_alive = true;
	if (rand() % 10 == 0) {
		enemy->loot = GetItem(I_Map);
	} else {
		enemy->loot = GetItem(I_None);
	}

	return enemy;
}

player_t InitPlayer(char sprite) {
	player_t player;

	player.sprite = sprite;
	player.pos = NewCoord(0, 0);
	player.color = Clr_Cyan;
	for (int i = 0; i < INVENTORY_SIZE; i++) {
		player.inventory[i] = GetItem(I_None);
	}

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

int WorldScreenWidth() {
	return GEO_screen_width() - RIGHT_PANEL_OFFSET;
}

int WorldScreenHeight() {
	return GEO_screen_height() - BOTTOM_PANEL_OFFSET;
}

void Process(game_state_t *state) {
	//const int terminal_w = GEO_screen_width();
	const int terminal_h = GEO_screen_height();

	const int world_screen_w = WorldScreenWidth();
	const int world_screen_h = WorldScreenHeight();

	// Clear drawn elements from screen.
	GEO_clear_screen();

	// Draw all world tiles.
	for (int x = 0; x < world_screen_w; x++) {
		for (int y = 0; y < world_screen_h; y++) {
			ApplyVision(state, NewCoord(x, y));
		}
	}

	// Draw player.
	GEO_draw_char(state->player.pos.x, state->player.pos.y, state->player.color, state->player.sprite);

	// Draw bottom panel line.
	GEO_draw_line(0, terminal_h - 2, world_screen_w, terminal_h - 2, Clr_Magenta, '_');

	// Draw last 3 game log lines.
	GEO_draw_formatted(0, terminal_h - 6, Clr_White, "* %s", state->game_log.line3);
	GEO_draw_formatted(0, terminal_h - 5, Clr_White, "* %s", state->game_log.line2);
	GEO_draw_formatted(0, terminal_h - 4, Clr_White, "* %s", state->game_log.line1);

	// Draw DEBUG label.
	GEO_draw_formatted(0, terminal_h - 1, Clr_Magenta, "   xy: (%d, %d)   rc(s): %d   seed: %d   rooms: %d   turns: %d",
		state->player.pos.x, state->player.pos.y, state->debug_rcs, (int)state->debug_seed, state->num_rooms_created, state->game_turns);

	// Draw right panel line.
	GEO_draw_line(world_screen_w, 0, world_screen_w, terminal_h - 2, Clr_Magenta, '|');

	// Draw basic player info.
	//GEO_draw_formatted(world_screen_w + 1, 1, Clr_Cyan, "Health - %d/%d", state->player.stats.curr_health, state->player.stats.max_health);
	//GEO_draw_formatted(world_screen_w + 1, 2, Clr_Cyan, "Mana - %d/%d", state->player.stats.curr_mana, state->player.stats.max_mana);
	//GEO_draw_formatted(world_screen_w + 1, 3, Clr_Cyan, "Gold - %d", state->player.stats.num_gold);

	// Draw basic player info.
	GEO_draw_formatted_align_center(RIGHT_PANEL_OFFSET, terminal_h - 3, Clr_Cyan, "Health - %d/%d   Mana - %d/%d   Gold - %d",
		state->player.stats.curr_health, state->player.stats.max_health, state->player.stats.curr_mana, state->player.stats.max_mana, state->player.stats.num_gold);

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

void PerformWorldLogic(game_state_t *state, const tile_t *curr_world_tile, coord_t player_old_pos) {
	assert(state != NULL);
	assert(curr_world_tile != NULL);

	state->current_target = ' ';

	switch (curr_world_tile->type) {
		case TileType_Item:
			// Special case for items which are gold.
			if (curr_world_tile->sprite == SPR_GOLD || curr_world_tile->sprite == SPR_BIGGOLD) {
				int amt = 0;

				if (curr_world_tile->sprite == SPR_GOLD) {
					amt = (rand() % 4) + 1;
				} else if (curr_world_tile->sprite == SPR_BIGGOLD) {
					amt = (rand() % 5) + 5;
				}

				state->player.stats.num_gold += amt;
				if (amt == 1) {
					UpdateGameLog(&state->game_log, LOGMSG_PLR_GET_GOLD_SINGLE, amt);
				} else {
					UpdateGameLog(&state->game_log, LOGMSG_PLR_GET_GOLD_PLURAL, amt);
				}
				UpdateWorldTile(state->world_tiles, state->player.pos, SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
				return;
			}

			// All other items are "picked up" (removed from world) if the player has room in their inventory.
			if (AddToInventory(&state->player, curr_world_tile->item_occupier)) {
				UpdateGameLog(&state->game_log, LOGMSG_PLR_GET_ITEM, curr_world_tile->item_occupier->name);
				UpdateWorldTile(state->world_tiles, state->player.pos, SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
			} else {
				UpdateGameLog(&state->game_log, LOGMSG_PLR_INVENTORY_FULL);
			}
			break;
		case TileType_Enemy:;
			enemy_t *attackedEnemy = curr_world_tile->enemy_occupier;
			attackedEnemy->curr_health--;
			UpdateGameLog(&state->game_log, LOGMSG_PLR_DMG_ENEMY, attackedEnemy->data->name, 1);

			if (attackedEnemy->curr_health <= 0) {
				UpdateGameLog(&state->game_log, LOGMSG_PLR_KILL_ENEMY, attackedEnemy->data->name);
				attackedEnemy->is_alive = false;
				state->player.stats.enemies_slain++;
				UpdateWorldTile(state->world_tiles, attackedEnemy->pos, SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);

				if (attackedEnemy->loot == GetItem(I_Map)) {
					UpdateWorldTile(state->world_tiles, attackedEnemy->pos, SPR_MAP, TileType_Special, Clr_Yellow, NULL, NULL);
				}
			} else {
				state->player.stats.curr_health--;
				UpdateGameLog(&state->game_log, LOGMSG_ENEMY_DMG_PLR, attackedEnemy->data->name, 1);
			}

			// Moving into any enemy results in no movement from the player.
			state->player.pos = player_old_pos;
			break;
		case TileType_Special:
			if (curr_world_tile->sprite == SPR_STAIRCASE) {
				UpdateGameLog(&state->game_log, LOGMSG_PLR_INTERACT_STAIRCASE);
				state->floor_complete = true;
			} else if (curr_world_tile->sprite == SPR_MAP) {
				UpdateGameLog(&state->game_log, LOGMSG_PLR_USE_MAP);
				state->fog_of_war = false;
				UpdateWorldTile(state->world_tiles, state->player.pos, SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
			}

			// Moving into a special object results in no movement from the player.
			state->player.pos = player_old_pos;
			break;
		case TileType_Npc:
			if (curr_world_tile->sprite == SPR_MERCHANT) {
				UpdateGameLog(&state->game_log, LOGMSG_PLR_INTERACT_MERCHANT);
				state->current_target = SPR_MERCHANT;
			}
			// Moving into an NPC results in no movement from the player.
			state->player.pos = player_old_pos;
			break;
		default:
			if (state->game_log.line1[0] != ' ' || state->game_log.line2[0] != ' ' || state->game_log.line3[0] != ' ') {
				UpdateGameLog(&state->game_log, LOGMSG_EMPTY_SPACE);
			}
			break;
	}
}

bool CheckRoomCollision(const tile_t **world_tiles, const room_t *room) {
	assert(world_tiles != NULL);
	assert(room != NULL);

	if (CheckRoomWorldBounds(room)) {
		return true;
	}

	const int room_width = room->TR_corner.x - room->TL_corner.x;
	const int room_height = room->BL_corner.y - room->TL_corner.y;

	for (int y = 0; y <= room_height; y++) {
		for (int x = 0; x <= room_width; x++) {
			if (world_tiles[room->TL_corner.x + x][room->TL_corner.y + y].type == TileType_Solid) {
				return true;
			}
		}
	}
	return false;
}

bool CheckRoomWorldBounds(const room_t *room) {
	assert(room != NULL);

	if (CheckWorldBounds(room->TL_corner) || CheckWorldBounds(room->TR_corner) || CheckWorldBounds(room->BL_corner)) {
		return true;
	}
	return false;
}

bool CheckWorldBounds(coord_t coord) {
	if (coord.x >= WorldScreenWidth() || coord.x < 0 || coord.y < 0 + TOP_PANEL_OFFSET || coord.y >= WorldScreenHeight()) {
		return true;
	}
	return false;
}

void CreateRoomsFromFile(game_state_t *state, const char *filename) {
	assert(state != NULL);
	assert(filename != NULL);

	// Attempt to open the file.
	FILE *fp = NULL;
	fp = fopen(filename, "r");
	if (fp == NULL) {
		return;
	} else {
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
		coord_t anchor_centered_map_offset = NewCoord((WorldScreenWidth() / 2) - (lineWidth / 2), ((WorldScreenHeight() / 2) - (lineNum / 2)));
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
				tile_type_en type = TileType_Empty;
				int colour = Clr_White;
				coord_t pos = NewCoord(anchor_centered_map_offset.x + i, anchor_centered_map_offset.y + lineNum);
				bool isEnemy = false;
				bool isItem = false;
				enemy_t *enemy = NULL;
				const item_t *item = NULL;

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
						type = TileType_Solid;
						break;
					case SPR_SMALLFOOD:
						isItem = true;
						item = GetItem(I_SmallFood);
						break;
					case SPR_BIGFOOD:
						isItem = true;
						item = GetItem(I_BigFood);
						break;
					case SPR_GOLD:
					case SPR_BIGGOLD:
						isItem = true;
						break;
					case SPR_ZOMBIE:
						isEnemy = true;
						enemy = InitAndCreateEnemy(GetEnemyData(E_Zombie), pos);
						break;
					case SPR_WEREWOLF:
						isEnemy = true;
						enemy = InitAndCreateEnemy(GetEnemyData(E_Werewolf), pos);
						break;
					case SPR_STAIRCASE:
						type = TileType_Special;
						colour = Clr_Yellow;
						break;
					case SPR_MERCHANT:
						type = TileType_Npc;
						colour = Clr_Magenta;
						break;
					default:
						break;
				}

				if (isEnemy) {
					type = TileType_Enemy;
					colour = Clr_Red;
					AddToEnemyList(&state->enemy_list, enemy);
				} else if (isItem) {
					type = TileType_Item;
					colour = Clr_Green;
				}

				UpdateWorldTile(state->world_tiles, pos, line[i], type, colour, enemy, item);
			}
			line = NULL;
			lineNum++;
		}
		free(line);
		fclose(fp);
	}
}

void CreateOpenRooms(game_state_t *state, int num_rooms_specified, int room_size) {
	assert(state != NULL);

	const int EXTENDED_BOUNDS_OFFSET = 2;

	// If room size is larger than the smallest terminal dimension, clamp it to that dimension size.
	if (room_size > fmin(WorldScreenWidth() - EXTENDED_BOUNDS_OFFSET, WorldScreenHeight() - EXTENDED_BOUNDS_OFFSET - TOP_PANEL_OFFSET)) {
		room_size = fmin(WorldScreenWidth() - EXTENDED_BOUNDS_OFFSET, WorldScreenHeight() - EXTENDED_BOUNDS_OFFSET - TOP_PANEL_OFFSET);
	}

	for (int i = 0; i < num_rooms_specified; i++) {
		bool valid_room;
		do {
			valid_room = true;

			// Define a new room.
			DefineOpenRoom(&state->rooms[i], room_size);

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
		UpdateWorldTile(state->world_tiles, opening, '?', TileType_Empty, Clr_White, NULL, NULL);

		//Connect corners with walls and convert marked opening to a real opening.
		GenerateRoom(state->world_tiles, &state->rooms[i]);

		state->num_rooms_created++;
	}
}

void DefineOpenRoom(room_t *room, int room_size) {
	assert(room != NULL);

	const int world_screen_w = WorldScreenWidth();
	const int world_screen_h = WorldScreenHeight();

	// TOP LEFT CORNER
	do {
		room->TL_corner.x = rand() % world_screen_w;
		room->TL_corner.y = (rand() % (world_screen_h - TOP_PANEL_OFFSET)) + TOP_PANEL_OFFSET;
	} while (room->TL_corner.x + room_size - 1 >= world_screen_w || room->TL_corner.y + room_size - 1 >= world_screen_h);

	// TOP RIGHT CORNER
	room->TR_corner.x = room->TL_corner.x + room_size - 1;
	room->TR_corner.y = room->TL_corner.y;

	// BOTTOM LEFT CORNER
	room->BL_corner.x = room->TL_corner.x;
	room->BL_corner.y = room->TL_corner.y + room_size - 1;

	// BOTTOM RIGHT CORNER
	room->BR_corner.x = room->TL_corner.x + room_size - 1;
	room->BR_corner.y = room->TL_corner.y + room_size - 1;
}

coord_t GetRandRoomOpeningPos(const room_t *room) {
	assert(room != NULL);

	coord_t opening;
	// Choose a random x position.
	if (rand() % 2 == 0) {
		opening.x = room->TL_corner.x + 1 + (rand() % (room->TR_corner.x - room->TL_corner.x - 1));
		// Choose between upper or lower wall.
		if (rand() % 2 == 0) {
			opening.y = room->TL_corner.y;
		} else {
			opening.y = room->BL_corner.y;
		}
	// Or choose a random y position.
	} else {
		opening.y = room->TL_corner.y + 1 + (rand() % (room->BR_corner.y - room->TL_corner.y - 1));
		// Choose between left or right wall.
		if (rand() % 2 == 0) {
			opening.x = room->TL_corner.x;
		} else {
			opening.x = room->TR_corner.x;
		}
	}

	return opening;
}

void CreateClosedRooms(game_state_t *state, int num_rooms_specified, int room_size) {
	assert(state != NULL);

	int radius = (room_size - 1) / 2;

	// Ensure there is enough space to define the first room at the center of the map. Reduce radius if room is too big.
	coord_t pos = NewCoord(WorldScreenWidth() / 2, WorldScreenHeight() / 2);
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

	// Begin creating rooms.
	const int NUM_INITIAL_ITERATIONS = 100;
	InstantiateClosedRoomRecursive(state, pos, radius, NUM_INITIAL_ITERATIONS, num_rooms_specified);
}

void InstantiateClosedRoomRecursive(game_state_t *state, coord_t pos, int radius, int iterations, int max_rooms) {
	assert(state != NULL);

	// Create the latest defined room.
	GenerateRoom(state->world_tiles, &state->rooms[state->num_rooms_created]);
	state->num_rooms_created++;

	coord_t oldPos = pos;
	int randDirection = rand() % 4;
	int nextRadius = GetNextRoomRadius();

	// Try to initialise new rooms and draw corridors to them, until all retry iterations are used up OR max rooms are reached.
	for (int i = 0; i < iterations; i++) {
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
			case Dir_Up:
				newPos.y = oldPos.y - (radius * 2) - nextRadius;
				break;
			case Dir_Down:
				newPos.y = oldPos.y + (radius * 2) + nextRadius;
				break;
			case Dir_Left:
				newPos.x = oldPos.x - (radius * 2) - nextRadius;
				break;
			case Dir_Right:
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

int GetNextRoomRadius(void) {
	return (rand() % 6) + 2;
}

void GenerateCorridor(tile_t **world_tiles, coord_t starting_room, int corridor_size, direction_en direction) {
	assert(world_tiles != NULL);

	switch (direction) {

		case Dir_Up:
			// Create opening for THIS room; create marked opening for NEXT room.
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x, starting_room.y - corridor_size), SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x, starting_room.y - (corridor_size * 2)), '?', TileType_Empty, Clr_White, NULL, NULL);

			// Connect rooms with the corridor.
			for (int i = 0; i < corridor_size; i++) {
				UpdateWorldTile(world_tiles, NewCoord(starting_room.x - 1, starting_room.y - corridor_size - (i + 1)), SPR_WALL, TileType_Solid, Clr_White, NULL, NULL);
				UpdateWorldTile(world_tiles, NewCoord(starting_room.x + 1, starting_room.y - corridor_size - (i + 1)), SPR_WALL, TileType_Solid, Clr_White, NULL, NULL);
			}
			break;
		case Dir_Down:
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x, starting_room.y + corridor_size), SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x, starting_room.y + (corridor_size * 2)), '?', TileType_Empty, Clr_White, NULL, NULL);

			for (int i = 0; i < corridor_size; i++) {
				UpdateWorldTile(world_tiles, NewCoord(starting_room.x - 1, starting_room.y + corridor_size + (i + 1)), SPR_WALL, TileType_Solid, Clr_White, NULL, NULL);
				UpdateWorldTile(world_tiles, NewCoord(starting_room.x + 1, starting_room.y + corridor_size + (i + 1)), SPR_WALL, TileType_Solid, Clr_White, NULL, NULL);
			}
			break;
		case Dir_Left:
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x - corridor_size, starting_room.y), SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x - (corridor_size * 2), starting_room.y), '?', TileType_Empty, Clr_White, NULL, NULL);

			for (int i = 0; i < corridor_size; i++) {
				UpdateWorldTile(world_tiles, NewCoord(starting_room.x - corridor_size - (i + 1), starting_room.y - 1), SPR_WALL, TileType_Solid, Clr_White, NULL, NULL);
				UpdateWorldTile(world_tiles, NewCoord(starting_room.x - corridor_size - (i + 1), starting_room.y + 1), SPR_WALL, TileType_Solid, Clr_White, NULL, NULL);
			}
			break;
		case Dir_Right:
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x + corridor_size, starting_room.y), SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
			UpdateWorldTile(world_tiles, NewCoord(starting_room.x + (corridor_size * 2), starting_room.y), '?', TileType_Empty, Clr_White, NULL, NULL);

			for (int i = 0; i < corridor_size; i++) {
				UpdateWorldTile(world_tiles, NewCoord(starting_room.x + corridor_size + (i + 1), starting_room.y - 1), SPR_WALL, TileType_Solid, Clr_White, NULL, NULL);
				UpdateWorldTile(world_tiles, NewCoord(starting_room.x + corridor_size + (i + 1), starting_room.y + 1), SPR_WALL, TileType_Solid, Clr_White, NULL, NULL);
			}
			break;
		default:
			break;
	}
}

bool CheckCorridorCollision(const tile_t **world_tiles, coord_t starting_room, int corridor_size, direction_en direction) {
	assert(world_tiles != NULL);

	switch (direction) {
		case Dir_Up:
			for (int i = 0; i < corridor_size; i++) {
				if (CheckWorldBounds(NewCoord(starting_room.x, starting_room.y - corridor_size - (i + 1)))) {
					return true;
				} else if (world_tiles[starting_room.x][starting_room.y - corridor_size - (i + 1)].type == TileType_Solid
					|| world_tiles[starting_room.x - 1][starting_room.y - corridor_size - (i + 1)].type == TileType_Solid
					|| world_tiles[starting_room.x + 1][starting_room.y - corridor_size - (i + 1)].type == TileType_Solid) {
					return true;
				}
			}
			return false;
		case Dir_Down:
			for (int i = 0; i < corridor_size; i++) {
				if (CheckWorldBounds(NewCoord(starting_room.x, starting_room.y + corridor_size + (i + 1)))) {
					return true;
				} else if (world_tiles[starting_room.x][starting_room.y + corridor_size + (i + 1)].type == TileType_Solid
					|| world_tiles[starting_room.x - 1][starting_room.y + corridor_size + (i + 1)].type == TileType_Solid
					|| world_tiles[starting_room.x + 1][starting_room.y + corridor_size + (i + 1)].type == TileType_Solid) {
					return true;
				}
			}
			return false;
		case Dir_Left:
			for (int i = 0; i < corridor_size; i++) {
				if (CheckWorldBounds(NewCoord(starting_room.x - corridor_size - (i + 1), starting_room.y))) {
					return true;
				} else if (world_tiles[starting_room.x - corridor_size - (i + 1)][starting_room.y].type == TileType_Solid
					|| world_tiles[starting_room.x - corridor_size - (i + 1)][starting_room.y - 1].type == TileType_Solid
					|| world_tiles[starting_room.x - corridor_size - (i + 1)][starting_room.y + 1].type == TileType_Solid) {
					return true;
				}
			}
			return false;
		case Dir_Right:
			for (int i = 0; i < corridor_size; i++) {
				if (CheckWorldBounds(NewCoord(starting_room.x + corridor_size + (i + 1), starting_room.y))) {
					return true;
				} else if (world_tiles[starting_room.x + corridor_size + (i + 1)][starting_room.y].type == TileType_Solid
					|| world_tiles[starting_room.x + corridor_size + (i + 1)][starting_room.y - 1].type == TileType_Solid
					|| world_tiles[starting_room.x + corridor_size + (i + 1)][starting_room.y + 1].type == TileType_Solid) {
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
			UpdateWorldTile(world_tiles, NewCoord(x, room->TL_corner.y), SPR_WALL, TileType_Solid, Clr_White, NULL, NULL);
		} else {
			UpdateWorldTile(world_tiles, NewCoord(x, room->TL_corner.y), SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
		}

		if (world_tiles[x][room->BL_corner.y].sprite != '?') {
			UpdateWorldTile(world_tiles, NewCoord(x, room->BL_corner.y), SPR_WALL, TileType_Solid, Clr_White, NULL, NULL);
		} else {
			UpdateWorldTile(world_tiles, NewCoord(x, room->BL_corner.y), SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
		}
	}

	// Left and right walls.
	for (int y = room->TR_corner.y; y <= room->BR_corner.y; y++) {
		if (world_tiles[room->TR_corner.x][y].sprite != '?') {
			UpdateWorldTile(world_tiles, NewCoord(room->TR_corner.x, y), SPR_WALL, TileType_Solid, Clr_White, NULL, NULL);
		} else {
			UpdateWorldTile(world_tiles, NewCoord(room->TR_corner.x, y), SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
		}
		if (world_tiles[room->TL_corner.x][y].sprite != '?') {
			UpdateWorldTile(world_tiles, NewCoord(room->TL_corner.x, y), SPR_WALL, TileType_Solid, Clr_White, NULL, NULL);
		} else {
			UpdateWorldTile(world_tiles, NewCoord(room->TL_corner.x, y), SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
		}
	}
}

void UpdateWorldTile(tile_t **world_tiles, coord_t pos, char sprite, tile_type_en type, int color, enemy_t *enemy_occupier, const item_t *item_occupier) {
	assert(world_tiles != NULL);

	world_tiles[pos.x][pos.y].sprite = sprite;
	world_tiles[pos.x][pos.y].type = type;
	world_tiles[pos.x][pos.y].color = color;
	world_tiles[pos.x][pos.y].enemy_occupier = enemy_occupier;
	world_tiles[pos.x][pos.y].item_occupier = item_occupier;
}

void ApplyVision(const game_state_t *state, coord_t pos) {
	assert(state != NULL);

	if (state->fog_of_war) {
		const int vision_to_tile = abs((pos.x - state->player.pos.x) * (pos.x - state->player.pos.x)) + abs((pos.y - state->player.pos.y) * (pos.y - state->player.pos.y));
		if (vision_to_tile < state->player.stats.max_vision) {
			GEO_draw_char(pos.x, pos.y, state->world_tiles[pos.x][pos.y].color, state->world_tiles[pos.x][pos.y].sprite);
		}
	} else {
		GEO_draw_char(pos.x, pos.y, state->world_tiles[pos.x][pos.y].color, state->world_tiles[pos.x][pos.y].sprite);
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

void EnemyCombatUpdate(game_state_t *state, enemy_node_t *enemy_list) {
	assert(state != NULL);
	assert(enemy_list != NULL);

	enemy_node_t *node = enemy_list;
	for (; node != NULL; node = node->next) {
		if (CoordsEqual(node->enemy->pos, state->player.pos)) {
			state->player.stats.curr_health--;
			node->enemy->curr_health--;
			if (node->enemy->curr_health <= 0) {
				node->enemy->is_alive = false;
				UpdateWorldTile(state->world_tiles, node->enemy->pos, SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
			}
			break;
		}
	}
}

void NextPlayerInput(game_state_t *state) {
	assert(state != NULL);

	player_t *player = &state->player;
	coord_t oldPos = player->pos;

	// Get a character code from standard input without waiting (but looped until a valid key is pressed).
	// This method of input processing allows for interrupt handling (dealing with resizes).
	bool valid_key_pressed = false;
	while (!valid_key_pressed) {
		const int key = GetKeyInput();
		switch (key) {
			case KEY_UP:
				if (player->pos.y - 1 >= 0 + TOP_PANEL_OFFSET && state->world_tiles[player->pos.x][player->pos.y - 1].type != TileType_Solid) {
					player->pos.y--;
				}
				valid_key_pressed = true;
				break;
			case KEY_DOWN:
				if (player->pos.y + 1 < WorldScreenHeight() && state->world_tiles[player->pos.x][player->pos.y + 1].type != TileType_Solid) {
					player->pos.y++;
				}
				valid_key_pressed = true;
				break;
			case KEY_LEFT:
				if (player->pos.x - 1 >= 0 && state->world_tiles[player->pos.x - 1][player->pos.y].type != TileType_Solid) {
					player->pos.x--;
				}
				valid_key_pressed = true;
				break;
			case KEY_RIGHT:
				if (player->pos.x + 1 < WorldScreenWidth() && state->world_tiles[player->pos.x + 1][player->pos.y].type != TileType_Solid) {
					player->pos.x++;
				}
				valid_key_pressed = true;
				break;
			case 'h':
				DrawHelpScreen();
				valid_key_pressed = true;
				break;
			case 'i':
				DrawPlayerInfoScreen(state);
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
				valid_key_pressed = true;
			case '\n':
			case KEY_ENTER:
				if (state->current_target != ' ') {
					InteractWithNPC(state, state->current_target);
					valid_key_pressed = true;
				}
				break;
			default:
				break;
		}
	}

	// The player's turn is over if they moved on this turn.
	state->player_turn_over = false;
	if (!CoordsEqual(player->pos, oldPos)) {
		state->player_turn_over = true;
	}
}

void InteractWithNPC(game_state_t *state, char npc_target) {
	switch (npc_target) {
		case SPR_MERCHANT:
			DrawMerchantScreen(state);
			return;
		default:
			return;
	}
}

void DrawHelpScreen(void) {
	const int terminal_w = GEO_screen_width() - 1;
	const int terminal_h = GEO_screen_height() - 1;

	GEO_clear_screen();

	GEO_draw_align_center(0, terminal_h / 2 - 4, Clr_White, "Asciiscape by George Delosa");
	GEO_draw_align_center(0, terminal_h / 2 - 2, Clr_White, "up/down/left/right: movement keys");
	GEO_draw_string(terminal_w / 2 - 11, terminal_h / 2 - 1, Clr_White, "h: show this help screen");
	GEO_draw_string(terminal_w / 2 - 11, terminal_h / 2 - 0, Clr_White, "i: show player info screen");
	GEO_draw_string(terminal_w / 2 - 11, terminal_h / 2 + 1, Clr_White, "f: toggle fog of war");
	GEO_draw_string(terminal_w / 2 - 11, terminal_h / 2 + 2, Clr_White, "q: quit game");
	GEO_draw_align_center(0, terminal_h / 2 + 4, Clr_White, "Press any key to continue...");

	GEO_show_screen();
	GetKeyInput();
}

int GetKeyInput(void) {
	while (true) {
		const int key = GEO_get_char();
		switch (key) {
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

void DrawDeathScreen(void) {
	GEO_clear_screen();

	GEO_draw_align_center(0, GEO_screen_height() / 2, Clr_Red, "YOU DIED");

	GEO_show_screen();
	GetKeyInput();
}

void DrawMerchantScreen(game_state_t *state) {
	assert(state != NULL);

	const int terminal_w = GEO_screen_width() - 1;
	const int terminal_h = GEO_screen_height() - 1;

	GEO_clear_screen();

	// Draw screen border.
	GEO_draw_line(0, 0, terminal_w, 0, Clr_White, '*');
	GEO_draw_line(0, 0, 0, terminal_h, Clr_White, '*');
	GEO_draw_line(terminal_w, 0, terminal_w, terminal_h, Clr_White, '*');
	GEO_draw_line(0, terminal_h, terminal_w, terminal_h, Clr_White, '*');

	// Draw the merchant shop interface.
	int x = 3;
	int y = 2;
	GEO_draw_align_center(0, y++, Clr_Yellow, "Trading with Merchant");
	y++;
	GEO_draw_string(x, y++, Clr_Yellow, "Option     Item                    Price (gold)");
	GEO_draw_formatted(x, y++, Clr_White, "(1)        %-23s %d", GetItem(I_SmallFood)->name, GetItem(I_SmallFood)->value);
	GEO_draw_formatted(x, y++, Clr_White, "(2)        %-23s %d", GetItem(I_BigFood)->name, GetItem(I_BigFood)->value);
	y++;
	GEO_draw_formatted(x, y++, Clr_White, "Your gold: %d", state->player.stats.num_gold);
	y++;
	GEO_draw_string(x, y++, Clr_White, "Select an option to buy an item, or press ENTER to leave.");

	GEO_show_screen();

	// Get player input to buy something or leave.
	bool validKeyPress = false;
	item_slug_en chosen_item = I_None;
	while (!validKeyPress) {
		const int key = GetKeyInput();
		switch (key) {
			case '1':
				chosen_item = I_SmallFood;
				validKeyPress = true;
				break;
			case '2':
				chosen_item = I_BigFood;
				validKeyPress = true;
				break;
			case '\n':
			case KEY_ENTER:
			case 'q':
			case KEY_RESIZE:
				validKeyPress = true;
				break;
			default:
				break;
		}
	}

	// If player bought something...
	if (chosen_item != I_None) {
		if (state->player.stats.num_gold >= GetItem(chosen_item)->value) {
			if (AddToInventory(&state->player, GetItem(chosen_item))) {
				state->player.stats.num_gold -= GetItem(chosen_item)->value;
				UpdateGameLog(&state->game_log, LOGMSG_PLR_BUY_MERCHANT, GetItem(chosen_item)->name);
			} else {
				UpdateGameLog(&state->game_log, LOGMSG_PLR_BUY_FULL_MERCHANT);
			}
		} else {
			UpdateGameLog(&state->game_log, LOGMSG_PLR_INSUFFICIENT_GOLD_MERCHANT);
		}
	}
}

int AddHealth(player_t *player, int amount) {
	assert(player != NULL);

	if (player->stats.curr_health + amount > player->stats.max_health) {
		amount = player->stats.max_health - player->stats.curr_health;
	}
	player->stats.curr_health += amount;
	return amount;
}

void DrawPlayerInfoScreen(const game_state_t * state) {
	assert(state != NULL);

	const int terminal_w = GEO_screen_width() - 1;
	const int terminal_h = GEO_screen_height() - 1;

	GEO_clear_screen();

	GEO_draw_line(0, 0, terminal_w, 0, Clr_White, '*');
	GEO_draw_line(0, 0, 0, terminal_h, Clr_White, '*');
	GEO_draw_line(terminal_w, 0, terminal_w, terminal_h, Clr_White, '*');
	GEO_draw_line(0, terminal_h, terminal_w, terminal_h, Clr_White, '*');

	int x = 3;
	int y = 2;

	GEO_draw_formatted_align_center(0, y++, Clr_Cyan, "Hero,  Lvl. %d", state->player.stats.level);
	GEO_draw_formatted_align_center(0, y++, Clr_White, "Current floor: %d", state->current_floor);
	y++;
	GEO_draw_formatted_align_center(0, y++, Clr_White, "Health - %d/%d   Mana - %d/%d   Gold - %d",
		state->player.stats.curr_health, state->player.stats.max_health, state->player.stats.curr_mana, state->player.stats.max_mana, state->player.stats.num_gold);
	y++;
	GEO_draw_string(x, y++, Clr_Cyan, "Inventory");
	for (int i = 0; i < INVENTORY_SIZE; i++) {
		GEO_draw_formatted(x, y++, Clr_Yellow, "(%d) %s", i + 1, state->player.inventory[i]->name);
	}
	y++;
	GEO_draw_string(x, y++, Clr_Cyan, "Stats");
	GEO_draw_formatted(x, y++, Clr_Yellow, "STR - %d", state->player.stats.s_STR);
	GEO_draw_formatted(x, y++, Clr_Yellow, "DEF - %d", state->player.stats.s_DEF);
	GEO_draw_formatted(x, y++, Clr_Yellow, "VIT - %d", state->player.stats.s_VIT);
	GEO_draw_formatted(x, y++, Clr_Yellow, "INT - %d", state->player.stats.s_INT);
	GEO_draw_formatted(x, y++, Clr_Yellow, "LCK - %d", state->player.stats.s_LCK);
	y++;
	GEO_draw_formatted(x, y++, Clr_White, "Enemies slain - %d", state->player.stats.enemies_slain);

	GEO_show_screen();
	GetKeyInput();
}

bool AddToInventory(player_t *player, const item_t *item) {
	assert(player != NULL);
	assert(item != NULL);

	for (int i = 0; i < INVENTORY_SIZE; i++) {
		if (player->inventory[i]->item_slug == I_None) {
			player->inventory[i] = item;
			return true;
		}
	}
	return false;
}
