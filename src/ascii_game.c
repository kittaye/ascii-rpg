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

bool g_resize_error = false;	// Global flag which is set when a terminal resize interrupt occurs.
bool g_process_over = false;	// Global flag which controls the main while loop of the application.

// Private functions.
static int Get_KeyInput(game_state_t *state);
static int Get_NextRoomRadius(void);
static void Define_Room(room_t *room, coord_t pos, int radius);
static void Generate_Room(tile_t **world_tiles, const room_t *room);
static void Generate_Corridor(tile_t **world_tiles, coord_t starting_room, int corridor_size, direction_en direction);
static void Create_Rooms(game_state_t *state, int num_rooms_specified);
static void Create_RoomRecursive(game_state_t *state, coord_t pos, int radius, int iterations, int max_rooms);
static void Create_RoomsFromFile(game_state_t *state, const char *filename);
static void Populate_Rooms(game_state_t *state);
static bool Check_CorridorCollision(const tile_t **world_tiles, coord_t starting_room, int corridor_size, direction_en direction);
static bool Check_RoomCollision(const tile_t **world_tiles, const room_t *room);
static bool Check_RoomOutOfWorldBounds(const room_t *room);

void Init_GameState(game_state_t *state) {
	assert(state != NULL);

	const int world_screen_w = Get_WorldScreenWidth();
	const int world_screen_h = Get_WorldScreenHeight();

	state->debug_seed = time(NULL);
	srand(state->debug_seed);

	state->game_turns = 0;
	state->num_rooms_created = 0;
	state->current_floor = 1;
	state->fog_of_war = true;
	state->player_turn_over = false;
	state->floor_complete = false;
	state->debug_rcs = 0;
	state->debug_injected_input_pos = 0;
	memset(state->debug_injected_inputs, '\0', sizeof(state->debug_injected_inputs));
	state->enemy_list = (enemy_node_t*)NULL;
	state->rooms = (room_t*)NULL;

	// Initialise empty world space.
	state->world_tiles = malloc(sizeof(*state->world_tiles) * world_screen_w);
	assert(state->world_tiles != NULL);
	for (int i = 0; i < world_screen_w; i++) {
		state->world_tiles[i] = malloc(sizeof(*state->world_tiles[i]) * world_screen_h);
		assert(state->world_tiles[i] != NULL);
	}

	// Create empty world space.
	for (int x = 0; x < world_screen_w; x++) {
		for (int y = 0; y < world_screen_h; y++) {
			coord_t coord = NewCoord(x, y);
			Update_WorldTile(state->world_tiles, coord, SPR_EMPTY, TileType_EMPTY, Clr_WHITE);
			Update_WorldTileItemOccupier(state->world_tiles, coord, NULL);
			Update_WorldTileEnemyOccupier(state->world_tiles, coord, NULL);
		}
	}

	snprintf(state->game_log.line1, LOG_BUFFER_SIZE, LOGMSG_EMPTY_SPACE);
	snprintf(state->game_log.line2, LOG_BUFFER_SIZE, LOGMSG_EMPTY_SPACE);
	snprintf(state->game_log.line3, LOG_BUFFER_SIZE, LOGMSG_EMPTY_SPACE);
}

void Cleanup_GameState(game_state_t *state) {
	assert(state != NULL);

	const int world_screen_w = Get_WorldScreenWidth();

	for (int i = 0; i < world_screen_w; i++) {
		free(state->world_tiles[i]);
	}
	free(state->world_tiles);
}

void Cleanup_DungeonFloor(game_state_t *state) {
	assert(state != NULL);

	FreeEnemyList(&state->enemy_list);
	state->enemy_list = (enemy_node_t*)NULL;

	free(state->rooms);
	state->rooms = (room_t*)NULL;
}

void InitCreate_DungeonFloor(game_state_t *state, unsigned int num_rooms_specified, const char *filename_specified) {
	assert(state != NULL);
	assert(num_rooms_specified >= MIN_ROOMS);
	assert(num_rooms_specified <= MAX_ROOMS);

	const int world_screen_w = Get_WorldScreenWidth();
	const int world_screen_h = Get_WorldScreenHeight();
	const int HUB_MAP_FREQUENCY = 4;

	// Reset dungeon floor values from the previous floor.
	if (state->current_floor > 1) {
		state->fog_of_war = true;
		state->num_rooms_created = 0;
		state->debug_rcs = 0;

		for (int x = 0; x < world_screen_w; x++) {
			for (int y = 0; y < world_screen_h; y++) {
				Update_WorldTile(state->world_tiles, NewCoord(x, y), SPR_EMPTY, TileType_EMPTY, Clr_WHITE);
			}
		}
	}

	// Every few floors, the hub layout is created instead of a random dungeon layout.
	if (state->current_floor % HUB_MAP_FREQUENCY == 0) {
		filename_specified = HUB_FILENAME;
		state->player.stats.max_vision = PLAYER_MAX_VISION + 100;
	} else {
		state->player.stats.max_vision = PLAYER_MAX_VISION;
	}

	state->rooms = malloc(sizeof(*state->rooms) * num_rooms_specified);
	assert(state->rooms != NULL);

	// Create the floor.
	if (filename_specified != NULL) {
		Create_RoomsFromFile(state, filename_specified);
	} else {
		Create_Rooms(state, num_rooms_specified);
		Populate_Rooms(state);
	}

	Update_GameLog(&state->game_log, LOGMSG_PLR_NEW_FLOOR, state->current_floor);
}

static void Populate_Rooms(game_state_t *state) {
	assert(state != NULL);
	assert(state->num_rooms_created >= MIN_ROOMS);

	// Choose a random room for the player spawn (except the last room created which is reserved for staircase room).
	const int player_spawn_room_index = rand() % (state->num_rooms_created - 1);

	for (int i = 0; i < state->num_rooms_created - 1; i++) {
		if (i == player_spawn_room_index) {
			coord_t pos = NewCoord(
				state->rooms[i].TL_corner.x + ((state->rooms[i].TR_corner.x - state->rooms[i].TL_corner.x) / 2),
				state->rooms[i].TR_corner.y + ((state->rooms[i].BR_corner.y - state->rooms[i].TR_corner.y) / 2)
			);
			Try_SetPlayerPos(state, pos);
			continue;
		}

		// Create gold, food, and enemies in rooms.
		for (int x = state->rooms[i].TL_corner.x + 1; x < state->rooms[i].TR_corner.x; x++) {
			for (int y = state->rooms[i].TL_corner.y + 1; y < state->rooms[i].BL_corner.y; y++) {
				int val = (rand() % 100) + 1;

				switch (val) {
					case 1:
					case 2:;
						enemy_t *enemy = NULL;
						if (val == 1) {
							enemy = InitCreate_Enemy(GetEnemyData(EnmySlug_ZOMBIE), NewCoord(x, y));
						} else /*if (val == 2)*/ {
							enemy = InitCreate_Enemy(GetEnemyData(EnmySlug_WEREWOLF), NewCoord(x, y));
						}
						Update_WorldTileEnemyOccupier(state->world_tiles, enemy->pos, enemy);
						AddToEnemyList(&state->enemy_list, enemy);
						break;
					case 3:
					case 4:
						Update_WorldTile(state->world_tiles, NewCoord(x, y), SPR_BIGGOLD, TileType_ITEM, Clr_GREEN);
						break;
					case 5: 
					case 6:
						Update_WorldTile(state->world_tiles, NewCoord(x, y), SPR_GOLD, TileType_ITEM, Clr_GREEN);
						break;
					case 7:
						Update_WorldTileItemOccupier(state->world_tiles, NewCoord(x, y), GetItem(ItmSlug_SMALLFOOD));
						break;
					case 8:
						Update_WorldTileItemOccupier(state->world_tiles, NewCoord(x, y), GetItem(ItmSlug_BIGFOOD));
						break;
					default:
						break;
				}
			}
		}
	}

	// Spawn staircase in the last room created (tends to be near center of map due to dungeon creation algorithm).
	const int last_room = state->num_rooms_created - 1;
	coord_t pos = NewCoord(
		state->rooms[last_room].TL_corner.x + ((state->rooms[last_room].TR_corner.x - state->rooms[last_room].TL_corner.x) / 2),
		state->rooms[last_room].TR_corner.y + ((state->rooms[last_room].BR_corner.y - state->rooms[last_room].TR_corner.y) / 2)
	);
	// TODO: staircase may spawn ontop of enemy, causing strange behaviour. Find fix.
	Update_WorldTile(state->world_tiles, pos, SPR_STAIRCASE, TileType_SPECIAL, Clr_YELLOW);
}

enemy_t* InitCreate_Enemy(const enemy_data_t *enemy_data, coord_t pos) {
	assert(enemy_data != NULL);

	enemy_t *enemy = malloc(sizeof(*enemy));
	assert(enemy != NULL);

	enemy->data = enemy_data;
	enemy->curr_health = enemy->data->max_health;
	enemy->pos = pos;
	enemy->is_alive = true;
	if (rand() % 10 == 0) {
		enemy->loot = GetItem(ItmSlug_MAP);
	} else {
		enemy->loot = GetItem(ItmSlug_NONE);
	}

	return enemy;
}

player_t Create_Player(void) {
	player_t player;

	player.sprite = SPR_PLAYER;
	player.pos = NewCoord(0, 0);
	player.color = Clr_CYAN;
	player.current_npc_target = ' ';
	player.current_item_index_selected = -1;

	for (int i = 0; i < INVENTORY_SIZE; i++) {
		player.inventory[i] = GetItem(ItmSlug_NONE);
	}

	player.stats.level = 1;
	player.stats.max_health = 10;
	player.stats.curr_health = 10;
	player.stats.max_mana = 10;
	player.stats.curr_mana = 10;

	player.stats.max_vision = PLAYER_MAX_VISION;

	player.stats.s_str = 1;
	player.stats.s_def = 1;
	player.stats.s_vit = 1;
	player.stats.s_int = 1;
	player.stats.s_lck = 1;


	player.stats.enemies_slain = 0;
	player.stats.num_gold = 0;

	return player;
}

bool Try_SetPlayerPos(game_state_t *state, coord_t pos) {
	assert(state != NULL);

	if (!Check_OutOfWorldBounds(pos) && state->world_tiles[pos.x][pos.y].type != TileType_SOLID) {
		state->player.pos = pos;
		return true;
	}
	return false;
}

int Get_WorldScreenWidth() {
	return GEO_screen_width() - RIGHT_PANEL_OFFSET;
}

int Get_WorldScreenHeight() {
	return GEO_screen_height() - BOTTOM_PANEL_OFFSET;
}

void Process(game_state_t *state) {
	//const int terminal_w = GEO_screen_width();
	const int terminal_h = GEO_screen_height();

	const int world_screen_w = Get_WorldScreenWidth();
	const int world_screen_h = Get_WorldScreenHeight();

	// Clear drawn elements from screen.
	GEO_clear_screen();

	// Draw all world tiles.
	for (int x = 0; x < world_screen_w; x++) {
		for (int y = 0; y < world_screen_h; y++) {
			Apply_Vision(state, NewCoord(x, y));
		}
	}

	// Draw player.
	GEO_draw_char(state->player.pos.x, state->player.pos.y, state->player.color, state->player.sprite);

	// Draw bottom panel line.
	GEO_draw_line(0, terminal_h - 2, world_screen_w, terminal_h - 2, Clr_MAGENTA, '_');

	// Draw last 3 game log lines.
	GEO_draw_printf(0, terminal_h - 6, Clr_WHITE, "* %s", state->game_log.line3);
	GEO_draw_printf(0, terminal_h - 5, Clr_WHITE, "* %s", state->game_log.line2);
	GEO_draw_printf(0, terminal_h - 4, Clr_WHITE, "* %s", state->game_log.line1);

	// Draw basic player info.
	GEO_draw_printf_align_center(-RIGHT_PANEL_OFFSET, terminal_h - 3, Clr_CYAN, "Health - %d/%d   Mana - %d/%d   Gold - %d",
		state->player.stats.curr_health, state->player.stats.max_health, state->player.stats.curr_mana, state->player.stats.max_mana, state->player.stats.num_gold);

	// Draw DEBUG label.
	GEO_draw_printf(0, terminal_h - 1, Clr_MAGENTA, "   xy: (%d, %d)   rc(s): %d   seed: %d   rooms: %d   turns: %d",
		state->player.pos.x, state->player.pos.y, state->debug_rcs, (int)state->debug_seed, state->num_rooms_created, state->game_turns);

	// Draw right-hand panel info.
	{
		GEO_draw_line(world_screen_w, 0, world_screen_w, terminal_h - 2, Clr_MAGENTA, '|');

		int x = world_screen_w + 2;
		int y = 2;

		GEO_draw_printf_align_center(world_screen_w, y++, Clr_CYAN, "Hero,  Lvl. %d", state->player.stats.level);
		GEO_draw_printf_align_center(world_screen_w, y++, Clr_WHITE, "Current floor: %d", state->current_floor);
		y++;
		GEO_draw_printf(x, y++, Clr_CYAN, "Inventory");
		for (int i = 0; i < INVENTORY_SIZE; i++) {
			GEO_draw_printf(x, y++, Clr_YELLOW, "(%d) %s", i + 1, state->player.inventory[i]->name);
		}
		y++;
		GEO_draw_printf(x, y++, Clr_CYAN, "Stats");
		GEO_draw_printf(x, y++, Clr_YELLOW, "STR - %d", state->player.stats.s_str);
		GEO_draw_printf(x, y++, Clr_YELLOW, "DEF - %d", state->player.stats.s_def);
		GEO_draw_printf(x, y++, Clr_YELLOW, "VIT - %d", state->player.stats.s_vit);
		GEO_draw_printf(x, y++, Clr_YELLOW, "INT - %d", state->player.stats.s_int);
		GEO_draw_printf(x, y++, Clr_YELLOW, "LCK - %d", state->player.stats.s_lck);
		y++;

		if (state->player.current_item_index_selected != -1) {
			y++;
			GEO_draw_printf(x, y++, Clr_CYAN, "Selected item: %s", state->player.inventory[state->player.current_item_index_selected]->name);
			GEO_draw_printf(x, y++, Clr_YELLOW, "Press 'e' to use");
			GEO_draw_printf(x, y++, Clr_YELLOW, "Press 'd' to drop");
			GEO_draw_printf(x, y++, Clr_YELLOW, "Press 'x' to examine");
		}
	}

	// Display drawn elements to screen.
	GEO_show_screen();

	// Remember player's current position before a move is made.
	coord_t oldPos = state->player.pos;

	// Wait for next player input, and determine whether it is considered to be ending their turn.
	state->player_turn_over = Perform_PlayerLogic(state);

	// If the player made a move that ends their turn, do world logic in response and finish this game turn.
	if (state->player_turn_over) {
		Perform_WorldLogic(state, oldPos);
		if (state->player.stats.curr_health <= 0) {
			// GAME OVER.
			Draw_DeathScreen(state);
			g_process_over = true;
		}
		state->player_turn_over = false;
		state->game_turns++;
	}
}

void Perform_WorldLogic(game_state_t *state, coord_t player_old_pos) {
	assert(state != NULL);

	state->player.current_npc_target = ' ';

	tile_t *curr_world_tile = &state->world_tiles[state->player.pos.x][state->player.pos.y];
	switch (Get_TileForegroundType(curr_world_tile)) {
		case TileType_ITEM:
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
					Update_GameLog(&state->game_log, LOGMSG_PLR_GET_GOLD_SINGLE, amt);
				} else {
					Update_GameLog(&state->game_log, LOGMSG_PLR_GET_GOLD_PLURAL, amt);
				}
				Update_WorldTile(state->world_tiles, state->player.pos, SPR_EMPTY, TileType_EMPTY, Clr_WHITE);
				return;
			}

			// All other items are "picked up" (removed from world) if the player has room in their inventory.
			if (AddTo_Inventory(&state->player, curr_world_tile->item_occupier)) {
				Update_GameLog(&state->game_log, LOGMSG_PLR_GET_ITEM, curr_world_tile->item_occupier->name);
				Update_WorldTileItemOccupier(state->world_tiles, state->player.pos, NULL);
			} else {
				Update_GameLog(&state->game_log, LOGMSG_PLR_INVENTORY_FULL);
			}
			break;
		case TileType_ENEMY:;
			enemy_t *attackedEnemy = curr_world_tile->enemy_occupier;
			attackedEnemy->curr_health--;
			Update_GameLog(&state->game_log, LOGMSG_PLR_DMG_ENEMY, attackedEnemy->data->name, 1);

			if (attackedEnemy->curr_health <= 0) {
				Update_GameLog(&state->game_log, LOGMSG_PLR_KILL_ENEMY, attackedEnemy->data->name);
				attackedEnemy->is_alive = false;
				state->player.stats.enemies_slain++;
				Update_WorldTileEnemyOccupier(state->world_tiles, attackedEnemy->pos, NULL);

				if (attackedEnemy->loot == GetItem(ItmSlug_MAP)) {
					Update_WorldTile(state->world_tiles, attackedEnemy->pos, SPR_MAP, TileType_SPECIAL, Clr_YELLOW);
				}
			} else {
				state->player.stats.curr_health--;
				Update_GameLog(&state->game_log, LOGMSG_ENEMY_DMG_PLR, attackedEnemy->data->name, 1);
			}

			// Moving into any enemy results in no movement from the player.
			state->player.pos = player_old_pos;
			break;
		case TileType_SPECIAL:
			if (curr_world_tile->sprite == SPR_STAIRCASE) {
				Update_GameLog(&state->game_log, LOGMSG_PLR_INTERACT_STAIRCASE);
				state->floor_complete = true;
			} else if (curr_world_tile->sprite == SPR_MAP) {
				Update_GameLog(&state->game_log, LOGMSG_PLR_USE_MAP);
				state->fog_of_war = false;
				Update_WorldTile(state->world_tiles, state->player.pos, SPR_EMPTY, TileType_EMPTY, Clr_WHITE);
			}

			// Moving into a special object results in no movement from the player.
			state->player.pos = player_old_pos;
			break;
		case TileType_NPC:
			if (curr_world_tile->sprite == SPR_MERCHANT) {
				Update_GameLog(&state->game_log, LOGMSG_PLR_INTERACT_MERCHANT);
				state->player.current_npc_target = SPR_MERCHANT;
			}
			// Moving into an NPC results in no movement from the player.
			state->player.pos = player_old_pos;
			break;
		default:
			if (state->game_log.line1[0] != ' ' || state->game_log.line2[0] != ' ' || state->game_log.line3[0] != ' ') {
				Update_GameLog(&state->game_log, LOGMSG_EMPTY_SPACE);
			}
			break;
	}
}

static bool Check_RoomCollision(const tile_t **world_tiles, const room_t *room) {
	assert(world_tiles != NULL);
	assert(room != NULL);

	if (Check_RoomOutOfWorldBounds(room)) {
		return true;
	}

	const int room_width = room->TR_corner.x - room->TL_corner.x;
	const int room_height = room->BL_corner.y - room->TL_corner.y;

	for (int y = 0; y <= room_height; y++) {
		for (int x = 0; x <= room_width; x++) {
			if (world_tiles[room->TL_corner.x + x][room->TL_corner.y + y].type == TileType_SOLID) {
				return true;
			}
		}
	}
	return false;
}

static bool Check_RoomOutOfWorldBounds(const room_t *room) {
	assert(room != NULL);

	if (Check_OutOfWorldBounds(room->TL_corner) || Check_OutOfWorldBounds(room->TR_corner) || Check_OutOfWorldBounds(room->BL_corner)) {
		return true;
	}
	return false;
}

bool Check_OutOfWorldBounds(coord_t coord) {
	if (coord.x >= Get_WorldScreenWidth() || coord.x < 0 || coord.y < 0 + TOP_PANEL_OFFSET || coord.y >= Get_WorldScreenHeight()) {
		return true;
	}
	return false;
}

static void Create_RoomsFromFile(game_state_t *state, const char *filename) {
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
		coord_t anchor_centered_map_offset = NewCoord((Get_WorldScreenWidth() / 2) - (lineWidth / 2), ((Get_WorldScreenHeight() / 2) - (lineNum / 2)));
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
				tile_type_en type = TileType_EMPTY;
				int colour = Clr_WHITE;
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
						Try_SetPlayerPos(state, pos);
						line[i] = SPR_EMPTY;
						break;
					case SPR_WALL:
						type = TileType_SOLID;
						break;
					case SPR_SMALLFOOD:
						isItem = true;
						item = GetItem(ItmSlug_SMALLFOOD);
						break;
					case SPR_BIGFOOD:
						isItem = true;
						item = GetItem(ItmSlug_BIGFOOD);
						break;
					case SPR_GOLD:
					case SPR_BIGGOLD:
						isItem = true;
						break;
					case SPR_ZOMBIE:
						isEnemy = true;
						enemy = InitCreate_Enemy(GetEnemyData(EnmySlug_ZOMBIE), pos);
						break;
					case SPR_WEREWOLF:
						isEnemy = true;
						enemy = InitCreate_Enemy(GetEnemyData(EnmySlug_WEREWOLF), pos);
						break;
					case SPR_STAIRCASE:
						type = TileType_SPECIAL;
						colour = Clr_YELLOW;
						break;
					case SPR_MERCHANT:
						type = TileType_NPC;
						colour = Clr_MAGENTA;
						break;
					default:
						break;
				}

				if (isEnemy) {
					type = TileType_ENEMY;
					colour = Clr_RED;
					AddToEnemyList(&state->enemy_list, enemy);
				} else if (isItem) {
					type = TileType_ITEM;
					colour = Clr_GREEN;
				}

				Update_WorldTile(state->world_tiles, pos, line[i], type, colour);
				Update_WorldTileItemOccupier(state->world_tiles, pos, item);
				Update_WorldTileEnemyOccupier(state->world_tiles, pos, enemy);
			}
			line = NULL;
			lineNum++;
		}
		free(line);
		fclose(fp);
	}
}

static void Create_Rooms(game_state_t *state, int num_rooms_specified) {
	assert(state != NULL);

	// Arbitrary starting room size.
	int radius = (5 - 1) / 2;

	// Ensure there is enough space to define the first room at the center of the map. Reduce radius if room is too big.
	coord_t pos = NewCoord(Get_WorldScreenWidth() / 2, Get_WorldScreenHeight() / 2);
	bool isValid;
	do {
		Define_Room(&state->rooms[0], pos, radius);
		isValid = true;
		if (Check_RoomCollision((const tile_t**)state->world_tiles, &state->rooms[0])) {
			isValid = false;
			state->debug_rcs++;
			radius--;
		}
	} while (!isValid);

	// Begin creating rooms.
	const int NUM_INITIAL_ITERATIONS = 100;
	Create_RoomRecursive(state, pos, radius, NUM_INITIAL_ITERATIONS, num_rooms_specified);
}

static void Create_RoomRecursive(game_state_t *state, coord_t pos, int radius, int iterations, int max_rooms) {
	assert(state != NULL);

	// Create the latest room.
	Generate_Room(state->world_tiles, &state->rooms[state->num_rooms_created]);
	state->num_rooms_created++;

	coord_t oldPos = pos;
	int randDirection = rand() % 4;
	int nextRadius = Get_NextRoomRadius();

	// Try to initialise new rooms and draw corridors to them, until all retry iterations are used up OR max rooms are reached.
	for (int i = 0; i < iterations; i++) {
		if (state->num_rooms_created >= max_rooms) {
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
			case Dir_UP:
				newPos.y = oldPos.y - (radius * 2) - nextRadius;
				break;
			case Dir_DOWN:
				newPos.y = oldPos.y + (radius * 2) + nextRadius;
				break;
			case Dir_LEFT:
				newPos.x = oldPos.x - (radius * 2) - nextRadius;
				break;
			case Dir_RIGHT:
				newPos.x = oldPos.x + (radius * 2) + nextRadius;
				break;
			default:
				break;
		}

		// Define the new room.
		Define_Room(&state->rooms[state->num_rooms_created], newPos, nextRadius);

		// Check that this new room doesnt collide with map boundaries or anything solid.
		if (Check_RoomCollision((const tile_t **)state->world_tiles, &state->rooms[state->num_rooms_created])) {
			state->debug_rcs++;
			continue;
		}

		// Check that the corridor that will connect the last created room with this new room doesnt collide with anything solid.
		if (Check_CorridorCollision((const tile_t**)state->world_tiles, oldPos, radius, randDirection)) {
			state->debug_rcs++;
			continue;
		}

		// Generate the corridor, connecting the last created room to the new one (new room's opening is marked with '?').
		Generate_Corridor(state->world_tiles, oldPos, radius, randDirection);

		// Instantiate the new conjoined room.
		int newMaxIterations = 100;
		Create_RoomRecursive(state, newPos, nextRadius, newMaxIterations, max_rooms);
	}
	return;
}

static int Get_NextRoomRadius(void) {
	return (rand() % 6) + 2;
}

static void Generate_Corridor(tile_t **world_tiles, coord_t starting_room, int corridor_size, direction_en direction) {
	assert(world_tiles != NULL);

	switch (direction) {

		case Dir_UP:
			// Create opening for THIS room; create marked opening for NEXT room.
			Update_WorldTile(world_tiles, NewCoord(starting_room.x, starting_room.y - corridor_size), SPR_EMPTY, TileType_EMPTY, Clr_WHITE);
			Update_WorldTile(world_tiles, NewCoord(starting_room.x, starting_room.y - (corridor_size * 2)), '?', TileType_EMPTY, Clr_WHITE);

			// Connect rooms with the corridor.
			for (int i = 0; i < corridor_size; i++) {
				Update_WorldTile(world_tiles, NewCoord(starting_room.x - 1, starting_room.y - corridor_size - (i + 1)), SPR_WALL, TileType_SOLID, Clr_WHITE);
				Update_WorldTile(world_tiles, NewCoord(starting_room.x + 1, starting_room.y - corridor_size - (i + 1)), SPR_WALL, TileType_SOLID, Clr_WHITE);
			}
			break;
		case Dir_DOWN:
			Update_WorldTile(world_tiles, NewCoord(starting_room.x, starting_room.y + corridor_size), SPR_EMPTY, TileType_EMPTY, Clr_WHITE);
			Update_WorldTile(world_tiles, NewCoord(starting_room.x, starting_room.y + (corridor_size * 2)), '?', TileType_EMPTY, Clr_WHITE);

			for (int i = 0; i < corridor_size; i++) {
				Update_WorldTile(world_tiles, NewCoord(starting_room.x - 1, starting_room.y + corridor_size + (i + 1)), SPR_WALL, TileType_SOLID, Clr_WHITE);
				Update_WorldTile(world_tiles, NewCoord(starting_room.x + 1, starting_room.y + corridor_size + (i + 1)), SPR_WALL, TileType_SOLID, Clr_WHITE);
			}
			break;
		case Dir_LEFT:
			Update_WorldTile(world_tiles, NewCoord(starting_room.x - corridor_size, starting_room.y), SPR_EMPTY, TileType_EMPTY, Clr_WHITE);
			Update_WorldTile(world_tiles, NewCoord(starting_room.x - (corridor_size * 2), starting_room.y), '?', TileType_EMPTY, Clr_WHITE);

			for (int i = 0; i < corridor_size; i++) {
				Update_WorldTile(world_tiles, NewCoord(starting_room.x - corridor_size - (i + 1), starting_room.y - 1), SPR_WALL, TileType_SOLID, Clr_WHITE);
				Update_WorldTile(world_tiles, NewCoord(starting_room.x - corridor_size - (i + 1), starting_room.y + 1), SPR_WALL, TileType_SOLID, Clr_WHITE);
			}
			break;
		case Dir_RIGHT:
			Update_WorldTile(world_tiles, NewCoord(starting_room.x + corridor_size, starting_room.y), SPR_EMPTY, TileType_EMPTY, Clr_WHITE);
			Update_WorldTile(world_tiles, NewCoord(starting_room.x + (corridor_size * 2), starting_room.y), '?', TileType_EMPTY, Clr_WHITE);

			for (int i = 0; i < corridor_size; i++) {
				Update_WorldTile(world_tiles, NewCoord(starting_room.x + corridor_size + (i + 1), starting_room.y - 1), SPR_WALL, TileType_SOLID, Clr_WHITE);
				Update_WorldTile(world_tiles, NewCoord(starting_room.x + corridor_size + (i + 1), starting_room.y + 1), SPR_WALL, TileType_SOLID, Clr_WHITE);
			}
			break;
		default:
			break;
	}
}

static bool Check_CorridorCollision(const tile_t **world_tiles, coord_t starting_room, int corridor_size, direction_en direction) {
	assert(world_tiles != NULL);

	switch (direction) {
		case Dir_UP:
			for (int i = 0; i < corridor_size; i++) {
				if (Check_OutOfWorldBounds(NewCoord(starting_room.x, starting_room.y - corridor_size - (i + 1)))) {
					return true;
				} else if (world_tiles[starting_room.x][starting_room.y - corridor_size - (i + 1)].type == TileType_SOLID
					|| world_tiles[starting_room.x - 1][starting_room.y - corridor_size - (i + 1)].type == TileType_SOLID
					|| world_tiles[starting_room.x + 1][starting_room.y - corridor_size - (i + 1)].type == TileType_SOLID) {
					return true;
				}
			}
			return false;
		case Dir_DOWN:
			for (int i = 0; i < corridor_size; i++) {
				if (Check_OutOfWorldBounds(NewCoord(starting_room.x, starting_room.y + corridor_size + (i + 1)))) {
					return true;
				} else if (world_tiles[starting_room.x][starting_room.y + corridor_size + (i + 1)].type == TileType_SOLID
					|| world_tiles[starting_room.x - 1][starting_room.y + corridor_size + (i + 1)].type == TileType_SOLID
					|| world_tiles[starting_room.x + 1][starting_room.y + corridor_size + (i + 1)].type == TileType_SOLID) {
					return true;
				}
			}
			return false;
		case Dir_LEFT:
			for (int i = 0; i < corridor_size; i++) {
				if (Check_OutOfWorldBounds(NewCoord(starting_room.x - corridor_size - (i + 1), starting_room.y))) {
					return true;
				} else if (world_tiles[starting_room.x - corridor_size - (i + 1)][starting_room.y].type == TileType_SOLID
					|| world_tiles[starting_room.x - corridor_size - (i + 1)][starting_room.y - 1].type == TileType_SOLID
					|| world_tiles[starting_room.x - corridor_size - (i + 1)][starting_room.y + 1].type == TileType_SOLID) {
					return true;
				}
			}
			return false;
		case Dir_RIGHT:
			for (int i = 0; i < corridor_size; i++) {
				if (Check_OutOfWorldBounds(NewCoord(starting_room.x + corridor_size + (i + 1), starting_room.y))) {
					return true;
				} else if (world_tiles[starting_room.x + corridor_size + (i + 1)][starting_room.y].type == TileType_SOLID
					|| world_tiles[starting_room.x + corridor_size + (i + 1)][starting_room.y - 1].type == TileType_SOLID
					|| world_tiles[starting_room.x + corridor_size + (i + 1)][starting_room.y + 1].type == TileType_SOLID) {
					return true;
				}
			}
			return false;
		default:
			return true;
	}
}

static void Define_Room(room_t *room, coord_t pos, int radius) {
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

static void Generate_Room(tile_t **world_tiles, const room_t *room) {
	assert(world_tiles != NULL);
	assert(room != NULL);

	//Connect corners with walls; marked tiles (?) become openings.

	// Bottom and top walls.
	for (int x = room->TL_corner.x; x <= room->TR_corner.x; x++) {
		if (world_tiles[x][room->TL_corner.y].sprite != '?') {
			Update_WorldTile(world_tiles, NewCoord(x, room->TL_corner.y), SPR_WALL, TileType_SOLID, Clr_WHITE);
		} else {
			Update_WorldTile(world_tiles, NewCoord(x, room->TL_corner.y), SPR_EMPTY, TileType_EMPTY, Clr_WHITE);
		}

		if (world_tiles[x][room->BL_corner.y].sprite != '?') {
			Update_WorldTile(world_tiles, NewCoord(x, room->BL_corner.y), SPR_WALL, TileType_SOLID, Clr_WHITE);
		} else {
			Update_WorldTile(world_tiles, NewCoord(x, room->BL_corner.y), SPR_EMPTY, TileType_EMPTY, Clr_WHITE);
		}
	}

	// Left and right walls.
	for (int y = room->TR_corner.y; y <= room->BR_corner.y; y++) {
		if (world_tiles[room->TR_corner.x][y].sprite != '?') {
			Update_WorldTile(world_tiles, NewCoord(room->TR_corner.x, y), SPR_WALL, TileType_SOLID, Clr_WHITE);
		} else {
			Update_WorldTile(world_tiles, NewCoord(room->TR_corner.x, y), SPR_EMPTY, TileType_EMPTY, Clr_WHITE);
		}
		if (world_tiles[room->TL_corner.x][y].sprite != '?') {
			Update_WorldTile(world_tiles, NewCoord(room->TL_corner.x, y), SPR_WALL, TileType_SOLID, Clr_WHITE);
		} else {
			Update_WorldTile(world_tiles, NewCoord(room->TL_corner.x, y), SPR_EMPTY, TileType_EMPTY, Clr_WHITE);
		}
	}
}

char Get_TileForegroundSprite(const tile_t *tile) {
	// Show enemy occupier's sprite, before an item's, before the tile's sprite itself.
	if (tile->enemy_occupier != NULL) {
		return tile->enemy_occupier->data->sprite;
	} else if (tile->item_occupier != NULL) {
		return tile->item_occupier->sprite;
	} else {
		return tile->sprite;
	}
}

colour_en Get_TileForegroundColour(const tile_t *tile) {
	// Show enemy occupier's colour, before an item's, before the tile's colour itself.
	if (tile->enemy_occupier != NULL) {
		return Clr_RED;
	} else if (tile->item_occupier != NULL) {
		return Clr_GREEN;
	} else {
		return tile->color;
	}
}

tile_type_en Get_TileForegroundType(const tile_t *tile) {
	// Show enemy occupier's type, before an item's, before the tile's type itself.
	if (tile->enemy_occupier != NULL) {
		return TileType_ENEMY;
	} else if (tile->item_occupier != NULL) {
		return TileType_ITEM;
	} else {
		return tile->type;
	}
}

void Update_WorldTile(tile_t **world_tiles, coord_t pos, char sprite, tile_type_en type, colour_en color) {
	assert(world_tiles != NULL);

	world_tiles[pos.x][pos.y].sprite = sprite;
	world_tiles[pos.x][pos.y].type = type;
	world_tiles[pos.x][pos.y].color = color;
}

void Update_WorldTileItemOccupier(tile_t **world_tiles, coord_t pos, const item_t *item) {
	world_tiles[pos.x][pos.y].item_occupier = item;
}

void Update_WorldTileEnemyOccupier(tile_t **world_tiles, coord_t pos, enemy_t *enemy) {
	world_tiles[pos.x][pos.y].enemy_occupier = enemy;
}

void Apply_Vision(const game_state_t *state, coord_t pos) {
	assert(state != NULL);
	assert(pos.x >= 0 && pos.x < Get_WorldScreenWidth());
	assert(pos.y >= 0 && pos.y < Get_WorldScreenHeight());

	const tile_t *tile = &state->world_tiles[pos.x][pos.y];

	if (state->fog_of_war) {
		const int vision_to_tile = abs((pos.x - state->player.pos.x) * (pos.x - state->player.pos.x)) + abs((pos.y - state->player.pos.y) * (pos.y - state->player.pos.y));
		if (vision_to_tile < state->player.stats.max_vision) {
			GEO_draw_char(pos.x, pos.y, Get_TileForegroundColour(tile), Get_TileForegroundSprite(tile));
		}
	} else {
		GEO_draw_char(pos.x, pos.y, Get_TileForegroundColour(tile), Get_TileForegroundSprite(tile));
	}
}

void Update_GameLog(log_list_t *game_log, const char *format, ...) {
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

void Update_AllEnemyCombat(game_state_t *state, enemy_node_t *enemy_list) {
	assert(state != NULL);
	assert(enemy_list != NULL);

	enemy_node_t *node = enemy_list;
	for (; node != NULL; node = node->next) {
		if (CoordsEqual(node->enemy->pos, state->player.pos)) {
			state->player.stats.curr_health--;
			node->enemy->curr_health--;
			if (node->enemy->curr_health <= 0) {
				node->enemy->is_alive = false;
				Update_WorldTileEnemyOccupier(state->world_tiles, node->enemy->pos, NULL);
			}
			break;
		}
	}
}

bool Perform_PlayerLogic(game_state_t *state) {
	assert(state != NULL);

	player_t *player = &state->player;

	while (true) {
		// Get a character code from standard input without waiting (but looped until any key is pressed).
		// This method of input processing allows for interrupt handling (dealing with terminal resizes).
		const int key = Get_KeyInput(state);

		switch (key) {
			case KEY_UP:
				return Try_SetPlayerPos(state, NewCoord(player->pos.x, player->pos.y - 1));
			case KEY_DOWN:
				return Try_SetPlayerPos(state, NewCoord(player->pos.x, player->pos.y + 1));
			case KEY_LEFT:
				return Try_SetPlayerPos(state, NewCoord(player->pos.x - 1, player->pos.y));
			case KEY_RIGHT:
				return Try_SetPlayerPos(state, NewCoord(player->pos.x + 1, player->pos.y));
			case 'h':
				Draw_HelpScreen(state);
				return false;
			case 'f':
				state->fog_of_war = !state->fog_of_war;
				return false;
			case 'q':
				g_process_over = true;
				return false;
			case 'e':
			case 'd':
			case 'x':
				if (player->current_item_index_selected != -1) {
					Interact_CurrentlySelectedItem(state, key);
					player->current_item_index_selected = -1;
					return false;
				}
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (player->inventory[(key - '1')] != GetItem(ItmSlug_NONE)) {
					player->current_item_index_selected = (key - '1');
					return false;
				}
				break;
			case KEY_RESIZE:
				return false;
			case '\n':
			case KEY_ENTER: // TODO: Interacting with NPCs should be considered as world logic, thus a game turn ending action.
				if (state->player.current_npc_target != ' ') {
					Interact_CurrentlyTargetedNPC(state);
					return false;
				}
				break;
			default:
				break;
		}
	}
}

void Interact_CurrentlySelectedItem(game_state_t *state, item_select_control_en key_pressed) {
	assert(state != NULL);

	const item_t *item_selected = state->player.inventory[state->player.current_item_index_selected];

	switch (key_pressed) {
		case ItmCtrl_USE:
			switch (item_selected->item_slug) {
				case ItmSlug_SMALLFOOD:
				case ItmSlug_BIGFOOD:;
					int health_to_add = (item_selected->item_slug == ItmSlug_SMALLFOOD) ? 1 : 2;
					int health_restored = AddTo_Health(&state->player, health_to_add);

					if (health_restored > 0) {
						Update_GameLog(&state->game_log, LOGMSG_PLR_USE_FOOD, health_restored);
					} else {
						Update_GameLog(&state->game_log, LOGMSG_PLR_USE_FOOD_FULL);
					}

					state->player.inventory[state->player.current_item_index_selected] = GetItem(ItmSlug_NONE);
					break;
				default:
					return;
			}
			break;
		case ItmCtrl_EXAMINE:
			Examine_Item(state, item_selected);
			break;
		case ItmCtrl_DROP:
			if (state->world_tiles[state->player.pos.x][state->player.pos.y].item_occupier == NULL) {
				Update_WorldTileItemOccupier(state->world_tiles, state->player.pos, item_selected);
				Update_GameLog(&state->game_log, LOGMSG_PLR_DROP_ITEM, item_selected->name);
				state->player.inventory[state->player.current_item_index_selected] = GetItem(ItmSlug_NONE);
			} else {
				Update_GameLog(&state->game_log, LOGMSG_PLR_CANT_DROP_ITEM, item_selected->name);
			}
			break;
		default:
			break;
	}
}

void Examine_Item(game_state_t *state, const item_t *item) {
	switch (item->item_slug) {
		case ItmSlug_SMALLFOOD:
			Update_GameLog(&state->game_log, LOGMSG_EXAMINE_SMALL_FOOD);
			break;
		case ItmSlug_BIGFOOD:
			Update_GameLog(&state->game_log, LOGMSG_EXAMINE_BIG_FOOD);
			break;
		default:
			Update_GameLog(&state->game_log, "There's nothing here...?");	// SHOULD NOT HAPPEN
			break;
	}
}

void Interact_CurrentlyTargetedNPC(game_state_t *state) {
	assert(state != NULL);

	switch (state->player.current_npc_target) {
		case SPR_MERCHANT:
			Draw_MerchantScreen(state);
			return;
		default:
			return;
	}
}

void Draw_HelpScreen(game_state_t *state) {
	const int terminal_w = GEO_screen_width() - 1;
	const int terminal_h = GEO_screen_height() - 1;

	GEO_clear_screen();

	GEO_draw_printf_align_center(0, terminal_h / 2 - 4, Clr_WHITE, "Asciiscape by George Delosa");
	GEO_draw_printf_align_center(0, terminal_h / 2 - 2, Clr_WHITE, "up/down/left/right: movement keys");
	GEO_draw_printf(terminal_w / 2 - 11, terminal_h / 2 - 1, Clr_WHITE, "h: show this help screen");
	GEO_draw_printf(terminal_w / 2 - 11, terminal_h / 2 + 0, Clr_WHITE, "f: toggle fog of war");
	GEO_draw_printf(terminal_w / 2 - 11, terminal_h / 2 + 1, Clr_WHITE, "q: quit game");
	GEO_draw_printf_align_center(0, terminal_h / 2 + 3, Clr_WHITE, "Press any key to continue...");

	GEO_show_screen();
	Get_KeyInput(state);
}

static int Get_KeyInput(game_state_t *state) {
	// USED TO INJECT INPUT FOR TESTING/DEBUGGING.
	if (state->debug_injected_inputs[state->debug_injected_input_pos] != '\0') {
		return state->debug_injected_inputs[state->debug_injected_input_pos++];
	}

	// Wait for the user's next key press.
	while (true) {
		const int key = GEO_get_char();
		switch (key) {
			case ERR:
				break;
			case KEY_RESIZE:
				g_resize_error = true;
				g_process_over = true;
			default:
				return key;
		}
	}
}

void Draw_DeathScreen(game_state_t *state) {
	GEO_clear_screen();

	GEO_draw_printf_align_center(0, GEO_screen_height() / 2, Clr_RED, "YOU DIED");

	GEO_show_screen();
	Get_KeyInput(state);
}

void Draw_MerchantScreen(game_state_t *state) {
	assert(state != NULL);

	const int terminal_w = GEO_screen_width() - 1;
	const int terminal_h = GEO_screen_height() - 1;

	GEO_clear_screen();

	// Draw screen border.
	GEO_draw_line(0, 0, terminal_w, 0, Clr_WHITE, '*');
	GEO_draw_line(0, 0, 0, terminal_h, Clr_WHITE, '*');
	GEO_draw_line(terminal_w, 0, terminal_w, terminal_h, Clr_WHITE, '*');
	GEO_draw_line(0, terminal_h, terminal_w, terminal_h, Clr_WHITE, '*');

	// Draw the merchant shop interface.
	int x = 3;
	int y = 2;
	GEO_draw_printf_align_center(0, y++, Clr_YELLOW, "Trading with Merchant");
	y++;
	GEO_draw_printf(x, y++, Clr_YELLOW, "Option     Item                    Price (gold)");
	GEO_draw_printf(x, y++, Clr_WHITE, "(1)        %-23s %d", GetItem(ItmSlug_SMALLFOOD)->name, GetItem(ItmSlug_SMALLFOOD)->value);
	GEO_draw_printf(x, y++, Clr_WHITE, "(2)        %-23s %d", GetItem(ItmSlug_BIGFOOD)->name, GetItem(ItmSlug_BIGFOOD)->value);
	y++;
	GEO_draw_printf(x, y++, Clr_WHITE, "Your gold: %d", state->player.stats.num_gold);
	y++;
	GEO_draw_printf(x, y++, Clr_WHITE, "Select an option to buy an item, or press ENTER to leave.");

	GEO_show_screen();

	// Get player input to buy something or leave.
	bool validKeyPress = false;
	item_slug_en chosen_item = ItmSlug_NONE;
	while (!validKeyPress) {
		const int key = Get_KeyInput(state);
		switch (key) {
			case '1':
				chosen_item = ItmSlug_SMALLFOOD;
				validKeyPress = true;
				break;
			case '2':
				chosen_item = ItmSlug_BIGFOOD;
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
	if (chosen_item != ItmSlug_NONE) {
		if (state->player.stats.num_gold >= GetItem(chosen_item)->value) {
			if (AddTo_Inventory(&state->player, GetItem(chosen_item))) {
				state->player.stats.num_gold -= GetItem(chosen_item)->value;
				Update_GameLog(&state->game_log, LOGMSG_PLR_BUY_MERCHANT, GetItem(chosen_item)->name);
			} else {
				Update_GameLog(&state->game_log, LOGMSG_PLR_BUY_FULL_MERCHANT);
			}
		} else {
			Update_GameLog(&state->game_log, LOGMSG_PLR_INSUFFICIENT_GOLD_MERCHANT);
		}
	}
}

int AddTo_Health(player_t *player, int amount) {
	assert(player != NULL);

	int old_health = player->stats.curr_health;
	player->stats.curr_health = CLAMP(player->stats.curr_health + amount, 0, player->stats.max_health);
	int health_difference = player->stats.curr_health - old_health;

	return health_difference;
}

bool AddTo_Inventory(player_t *player, const item_t *item) {
	assert(player != NULL);
	assert(item != NULL);

	for (int i = 0; i < INVENTORY_SIZE; i++) {
		if (player->inventory[i]->item_slug == ItmSlug_NONE) {
			player->inventory[i] = item;
			return true;
		}
	}
	return false;
}
