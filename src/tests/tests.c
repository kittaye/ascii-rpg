#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>

#include "../ascii_game.h"
#include "../george_graphics.h"
#include "../log_messages.h"
#include "minunit.h"

typedef struct floor_statistics_t {
	int floors_created;
	int average_rooms;
	int least_rooms;
	int most_rooms;
} floor_statistics_t;

int tests_run = 0;

int last_seed_used = -1;

// Private functions.

static void Setup_Test_Env();
static void Cleanup_Test_Env();
static game_state_t Setup_Test_GameStateAndPlayer();
static void Cleanup_Test_GameStateAndPlayer(game_state_t *state);
static game_state_t Setup_Test_GameStatePlayerAndDungeon();
static void Cleanup_Test_GameStatePlayerAndDungeon(game_state_t *state);

/*
Setup before any tests.
*/
static void Setup_Test_Env() {
	GEO_setup_screen();

	if (GEO_screen_width() < MIN_TERMINAL_WIDTH || GEO_screen_height() < MIN_TERMINAL_HEIGHT) {
		GEO_cleanup_screen();
		fprintf(stderr, "Terminal screen dimensions must be at least %dx%d to run tests. Exiting...\n", MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT);
		exit(1);
	}
}

/*
Cleanup after all tests are finished or a test fails.
*/
static void Cleanup_Test_Env() {
	GEO_cleanup_screen();
}

/*
Streamline tests that need game state and the player to be initialised and created.
*/
static game_state_t Setup_Test_GameStateAndPlayer() {
	game_state_t state;
	Init_GameState(&state);
	state.player = Create_Player();
	state.player.pos = New_Coord(0, 0);
	last_seed_used = state.debug_seed;
	return state;
}

/*
Frees all allocated memory from calling 'Setup_Test_GameStateAndPlayer'.
*/
static void Cleanup_Test_GameStateAndPlayer(game_state_t *state) {
	Cleanup_GameState(state);
}

/*
Streamline tests that need game state, player, and the dungeon floor to be initialised and created.
*/
static game_state_t Setup_Test_GameStatePlayerAndDungeon() {
	game_state_t state;
	Init_GameState(&state);
	last_seed_used = state.debug_seed;
	state.player = Create_Player();
	InitCreate_DungeonFloor(&state, 10, NULL);
	return state;
}

/*
Frees all allocated memory from calling 'Setup_Test_GameStatePlayerAndDungeon'.
*/
static void Cleanup_Test_GameStatePlayerAndDungeon(game_state_t *state) {
	Cleanup_DungeonFloor(state);
	Cleanup_GameState(state);
}

/*
Compare all values of a world tile at position 'pos_to_assert'.
*/
static bool WorldTile_IsEqualTo(game_state_t *state, coord_t pos_to_assert, const tile_data_t *tile_data) {
	return state->world_tiles[pos_to_assert.x][pos_to_assert.y].data == tile_data;
}

/*
Compare the world tile item occupier at position 'pos_to_assert'.
*/
static bool WorldTile_Item_IsEqualTo(game_state_t *state, coord_t pos_to_assert, const item_t *item_occupier) {
	return state->world_tiles[pos_to_assert.x][pos_to_assert.y].item_occupier == item_occupier;
}

/*
Compare the world tile enemy occupier at position 'pos_to_assert'.
*/
static bool WorldTile_Enemy_IsEqualTo(game_state_t *state, coord_t pos_to_assert, enemy_t *enemy_occupier) {
	return state->world_tiles[pos_to_assert.x][pos_to_assert.y].enemy_occupier == enemy_occupier;
}

// TESTS START HERE --------------------------------------------------------------------------------

int test_init_game_state_correct_values() {
	game_state_t state;
	Init_GameState(&state);

	const int world_screen_w = Get_WorldScreenWidth();
	const int world_screen_h = Get_WorldScreenHeight();

	mu_assert(__func__, state.game_turns == 0);
	mu_assert(__func__, state.num_rooms_created == 0);
	mu_assert(__func__, state.current_floor == 1);
	mu_assert(__func__, state.fog_of_war == true);
	mu_assert(__func__, state.player_turn_over == false);
	mu_assert(__func__, state.floor_complete == false);
	mu_assert(__func__, state.debug_rcs == 0);
	mu_assert(__func__, state.debug_seed > -1);
	mu_assert(__func__, state.debug_injected_input_pos == 0);
	mu_assert(__func__, state.enemy_list == (enemy_node_t*)NULL);
	mu_assert(__func__, state.rooms == (room_t*)NULL);

	for (int i = 0; i < DEBUG_INJECTED_INPUT_LIMIT + 1; i++) {
		mu_assert(__func__, state.debug_injected_inputs[i] == '\0');
	}

	// Assert initialised world space exists.
	mu_assert(__func__, state.world_tiles != NULL);
	for (int i = 0; i < world_screen_w; i++) {
		mu_assert(__func__, state.world_tiles[i] != NULL);
	}

	// Assert empty world space exists.
	for (int x = 0; x < world_screen_w; x++) {
		for (int y = 0; y < world_screen_h; y++) {
			coord_t coord = New_Coord(x, y);
			mu_assert(__func__, WorldTile_IsEqualTo(&state, coord, Get_TileData(TileSlug_VOID)) == true);
			mu_assert(__func__, WorldTile_Item_IsEqualTo(&state, coord, NULL) == true);
			mu_assert(__func__, WorldTile_Enemy_IsEqualTo(&state, coord, NULL) == true);
		}
	}

	mu_assert(__func__, strcmp(state.game_log.line1, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line2, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line3, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line4, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line5, " ") == 0);

	Cleanup_GameState(&state);
	return 0;
}

int test_create_player_correct_values() {
	player_t player = Create_Player();

	mu_assert(__func__, player.sprite == SPR_PLAYER);
	mu_assert(__func__, Check_CoordsEqual(player.pos, New_Coord(0, 0)));
	mu_assert(__func__, player.color == Clr_CYAN);
	mu_assert(__func__, player.current_npc_target == ' ');
	mu_assert(__func__, player.current_item_index_selected == -1);
	for (int i = 0; i < INVENTORY_SIZE; i++) {
		mu_assert(__func__, player.inventory[i] == Get_Item(ItmSlug_NONE));
	}
	mu_assert(__func__, player.stats.level == 1);
	mu_assert(__func__, player.stats.max_health > 0);
	mu_assert(__func__, player.stats.curr_health == player.stats.max_health);
	mu_assert(__func__, player.stats.max_mana > 0);
	mu_assert(__func__, player.stats.curr_mana == player.stats.max_mana);
	mu_assert(__func__, player.stats.s_str == 1);
	mu_assert(__func__, player.stats.s_def == 1);
	mu_assert(__func__, player.stats.s_vit == 1);
	mu_assert(__func__, player.stats.s_int == 1);
	mu_assert(__func__, player.stats.s_lck == 1);
	mu_assert(__func__, player.stats.enemies_slain == 0);
	mu_assert(__func__, player.stats.num_gold == 0);

	return 0;
}

int test_init_create_enemy_correct_values() {
	coord_t pos = New_Coord(10, 10);
	enemy_t* test_enemy = InitCreate_Enemy(Get_EnemyData(EnmySlug_ZOMBIE), pos);

	mu_assert(__func__, test_enemy != NULL);
	mu_assert(__func__, test_enemy->data == Get_EnemyData(EnmySlug_ZOMBIE));
	mu_assert(__func__, test_enemy->curr_health == Get_EnemyData(EnmySlug_ZOMBIE)->max_health);
	mu_assert(__func__, Check_CoordsEqual(test_enemy->pos, pos));
	mu_assert(__func__, test_enemy->is_alive == true);
	mu_assert(__func__, test_enemy->loot != NULL);

	free(test_enemy);
	return 0;
}

int test_get_world_width_correct_value() {
	int width = Get_WorldScreenWidth();

	mu_assert(__func__, width == GEO_screen_width() - RIGHT_PANEL_OFFSET);

	return 0;
}

int test_get_world_height_correct_value() {
	int height = Get_WorldScreenHeight();

	mu_assert(__func__, height == GEO_screen_height() - BOTTOM_PANEL_OFFSET);

	return 0;
}

int test_created_dungeon_floor_contains_staircase() {
	game_state_t state = Setup_Test_GameStatePlayerAndDungeon();

	bool contains_staircase = false;

	// Ensure a staircase exists in the last generated room.
	room_t staircase_room = state.rooms[state.num_rooms_created - 1];
	for (int x = staircase_room.TL_corner.x + 1; x < staircase_room.TR_corner.x; x++) {
		for (int y = staircase_room.TL_corner.y + 1; y < staircase_room.BL_corner.y; y++) {
			if (state.world_tiles[x][y].data->sprite == SPR_STAIRCASE) {
				contains_staircase = true;
				break;
			}
		}
	}

	mu_assert(__func__, contains_staircase == true);

	Cleanup_Test_GameStatePlayerAndDungeon(&state);
	return 0;
}

int test_created_dungeon_floor_contains_player() {
	game_state_t state = Setup_Test_GameStatePlayerAndDungeon();

	mu_assert(__func__, Check_CoordsEqual(state.player.pos, New_Coord(0, 0)) == false);

	Cleanup_Test_GameStatePlayerAndDungeon(&state);
	return 0;
}

int test_addto_player_health_correct_return_values() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	state.player.stats.max_health = 10;
	state.player.stats.curr_health = 5;

	mu_assert(__func__, AddTo_Health(&state.player, 2) == 2);		// current health is now 7
	mu_assert(__func__, AddTo_Health(&state.player, 5) == 3);		// current health is now 10
	mu_assert(__func__, AddTo_Health(&state.player, -7) == -7);		// current health is now 3
	mu_assert(__func__, AddTo_Health(&state.player, -10) == -3);	// current health is now 0

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_addto_player_health_correct_current_health() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	state.player.stats.max_health = 10;
	state.player.stats.curr_health = 5;

	AddTo_Health(&state.player, 2);
	mu_assert(__func__, state.player.stats.curr_health == 7);

	AddTo_Health(&state.player, -4);
	mu_assert(__func__, state.player.stats.curr_health == 3);

	AddTo_Health(&state.player, 20);
	mu_assert(__func__, state.player.stats.curr_health == 10);

	AddTo_Health(&state.player, -15);
	mu_assert(__func__, state.player.stats.curr_health == 0);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_set_player_pos_bounds_valid() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	const int world_screen_w = Get_WorldScreenWidth();
	const int world_screen_h = Get_WorldScreenHeight();

	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(rand() % world_screen_w, rand() % world_screen_h)) == true);
	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(0, 0)) == true);
	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(0, world_screen_h - 1)) == true);
	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(world_screen_w - 1, 0)) == true);
	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(world_screen_w - 1, world_screen_h - 1)) == true);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_set_player_pos_out_of_bounds() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	const int world_screen_w = Get_WorldScreenWidth();
	const int world_screen_h = Get_WorldScreenHeight();

	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(-1, 0)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(0, -1)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(-1, -1)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(0, world_screen_h)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(world_screen_w, 0)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(world_screen_w, world_screen_h)) == false);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_set_player_pos_into_solid_fails() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	Update_WorldTile(state.world_tiles, New_Coord(0, 0), Get_TileData(TileSlug_GENERIC_WALL));
	mu_assert(__func__, state.world_tiles[0][0].data->type == TileType_SOLID);
	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(0, 0)) == false);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_set_player_pos_into_door() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	Update_WorldTile(state.world_tiles, New_Coord(0, 0), Get_TileData(TileSlug_DOOR));
	mu_assert(__func__, state.world_tiles[0][0].data->type == TileType_SOLID);
	mu_assert(__func__, Try_SetPlayerPos(&state, New_Coord(0, 0)) == true);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_update_game_log_normal_behaviour() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	Update_GameLog(&state.game_log, "Hello");
	mu_assert(__func__, strcmp(state.game_log.line5, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line4, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line3, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line2, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line1, "Hello") == 0);

	Update_GameLog(&state.game_log, "World");
	mu_assert(__func__, strcmp(state.game_log.line5, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line4, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line3, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line2, "Hello") == 0);
	mu_assert(__func__, strcmp(state.game_log.line1, "World") == 0);

	Update_GameLog(&state.game_log, " ");
	mu_assert(__func__, strcmp(state.game_log.line5, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line4, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line3, "Hello") == 0);
	mu_assert(__func__, strcmp(state.game_log.line2, "World") == 0);
	mu_assert(__func__, strcmp(state.game_log.line1, " ") == 0);

	Update_GameLog(&state.game_log, "Test");
	Update_GameLog(&state.game_log, " ");
	mu_assert(__func__, strcmp(state.game_log.line5, "Hello") == 0);
	mu_assert(__func__, strcmp(state.game_log.line4, "World") == 0);
	mu_assert(__func__, strcmp(state.game_log.line3, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line2, "Test") == 0);
	mu_assert(__func__, strcmp(state.game_log.line1, " ") == 0);

	Update_GameLog(&state.game_log, "abc");
	Update_GameLog(&state.game_log, "def");

	mu_assert(__func__, strcmp(state.game_log.line5, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line4, "Test") == 0);
	mu_assert(__func__, strcmp(state.game_log.line3, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line2, "abc") == 0);
	mu_assert(__func__, strcmp(state.game_log.line1, "def") == 0);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_update_game_log_buffer_overflow_clamped() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	char long_string[LOG_BUFFER_SIZE + 123];
	memset(long_string, 'a', LOG_BUFFER_SIZE + 123);

	Update_GameLog(&state.game_log, long_string);

	mu_assert(__func__, strlen(state.game_log.line1) == LOG_BUFFER_SIZE - 1);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_drop_an_item_successfully() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	const item_t *item = Get_Item(ItmSlug_HP_POT_I);
	state.player.inventory[0] = item;
	state.player.current_item_index_selected = 0;
	Interact_CurrentlySelectedItem(&state, ItmCtrl_DROP);

	char expected_result[LOG_BUFFER_SIZE];
	snprintf(expected_result, LOG_BUFFER_SIZE, LOGMSG_PLR_DROP_ITEM, item->name);

	mu_assert(__func__, strcmp(state.game_log.line1, expected_result) == 0);
	mu_assert(__func__, WorldTile_Item_IsEqualTo(&state, New_Coord(0, 0), item) == true);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_cant_drop_item_onto_another() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	state.player.inventory[0] = Get_Item(ItmSlug_HP_POT_II);
	state.player.inventory[1] = Get_Item(ItmSlug_HP_POT_I);

	state.player.current_item_index_selected = 0;
	Interact_CurrentlySelectedItem(&state, ItmCtrl_DROP);
	state.player.current_item_index_selected = 1;
	Interact_CurrentlySelectedItem(&state, ItmCtrl_DROP);

	mu_assert(__func__, strcmp(state.game_log.line1, LOGMSG_PLR_CANT_DROP_ITEM) == 0);
	mu_assert(__func__, state.player.inventory[0] == Get_Item(ItmSlug_NONE));
	mu_assert(__func__, state.player.inventory[1] == Get_Item(ItmSlug_HP_POT_I));

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_pickup_an_item_successfully() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	const item_t *item = Get_Item(ItmSlug_HP_POT_II);

	// Spawn item next to player.
	state.player.pos = New_Coord(0, 0);
	Update_WorldTileItemOccupier(state.world_tiles, New_Coord(0, 1), item);

	// Set player input to move into the item, then process one game turn.
	state.debug_injected_inputs[0] = KEY_DOWN;
	Process(&state);

	char expected_logmsg_result[LOG_BUFFER_SIZE];
	snprintf(expected_logmsg_result, LOG_BUFFER_SIZE, LOGMSG_PLR_GET_ITEM, item->name);

	// Assert item has been picked up and removed from world.
	mu_assert(__func__, state.player.inventory[0] == item);
	mu_assert(__func__, WorldTile_Item_IsEqualTo(&state, New_Coord(0, 1), NULL) == true);
	mu_assert(__func__, strcmp(state.game_log.line1, expected_logmsg_result) == 0);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_item_examine_correct_value() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	state.player.inventory[0] = Get_Item(ItmSlug_HP_POT_I);
	state.player.inventory[1] = Get_Item(ItmSlug_HP_POT_II);

	state.player.current_item_index_selected = 0;
	Interact_CurrentlySelectedItem(&state, ItmCtrl_EXAMINE);
	mu_assert(__func__, strcmp(state.game_log.line1, LOGMSG_EXAMINE_SMALL_HP_POT) == 0);

	state.player.current_item_index_selected = 1;
	Interact_CurrentlySelectedItem(&state, ItmCtrl_EXAMINE);
	mu_assert(__func__, strcmp(state.game_log.line1, LOGMSG_EXAMINE_MEDIUM_HP_POT) == 0);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_cant_pickup_another_item_because_inventory_full() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	const item_t  *item = Get_Item(ItmSlug_HP_POT_I);

	// Fill inventory.
	for (int i = 0; i < INVENTORY_SIZE; i++) {
		state.player.inventory[i] = item;
	}

	// Spawn item next to player.
	state.player.pos = New_Coord(0, 0);
	Update_WorldTileItemOccupier(state.world_tiles, New_Coord(0, 1), item);

	// Set player input to move into the item, then process one game turn.
	state.debug_injected_inputs[0] = KEY_DOWN;
	Process(&state);

	// Assert the player has tried but failed to pick up the item.
	mu_assert(__func__, strcmp(state.game_log.line1, LOGMSG_PLR_INVENTORY_FULL) == 0);
	mu_assert(__func__, WorldTile_Item_IsEqualTo(&state, New_Coord(0, 1), item) == true);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_get_tile_foreground_attributes_no_occupiers() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	const tile_data_t random_tile1 = {.sprite = 'X', .type = TileType_SOLID, .color = Clr_CYAN};
	const tile_data_t random_tile2 = {.sprite = 'Q', .type = TileType_SPECIAL, .color = Clr_MAGENTA};

	Update_WorldTile(state.world_tiles, New_Coord(0, 0), &random_tile1);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).sprite == 'X');
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).type == TileType_SOLID);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).color == Clr_CYAN);

	Update_WorldTile(state.world_tiles, New_Coord(0, 0), &random_tile2);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).sprite == 'Q');
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).type == TileType_SPECIAL);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).color == Clr_MAGENTA);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_get_tile_foreground_attributes_single_occupier() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	enemy_t *enemy = InitCreate_Enemy(Get_EnemyData(EnmySlug_WEREWOLF), New_Coord(1, 1));
	const item_t *item = Get_Item(ItmSlug_HP_POT_II);
	const tile_data_t random_tile1 = {.sprite = 'X', .type = TileType_EMPTY, .color = Clr_CYAN};

	Update_WorldTile(state.world_tiles, New_Coord(0, 0), &random_tile1);
	Update_WorldTileItemOccupier(state.world_tiles, New_Coord(0, 0), item);

	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).sprite == item->sprite);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).type == TileType_EMPTY);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).color == Clr_GREEN);

	Update_WorldTileItemOccupier(state.world_tiles, New_Coord(0, 0), NULL);
	Update_WorldTileEnemyOccupier(state.world_tiles, New_Coord(0, 0), enemy);

	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).sprite == enemy->data->sprite);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).type == TileType_EMPTY);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).color == Clr_RED);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int test_get_tile_foreground_attributes_full_occupiers() {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	enemy_t *enemy = InitCreate_Enemy(Get_EnemyData(EnmySlug_WEREWOLF), New_Coord(0, 0));
	const item_t *item = Get_Item(ItmSlug_HP_POT_II);
	const tile_data_t random_tile1 = {.sprite = 'X', .type = TileType_EMPTY, .color = Clr_CYAN};

	// Priority ordering: Enemy > Item > Tile.

	Update_WorldTile(state.world_tiles, New_Coord(0, 0), &random_tile1);
	Update_WorldTileEnemyOccupier(state.world_tiles, New_Coord(0, 0), enemy);
	Update_WorldTileItemOccupier(state.world_tiles, New_Coord(0, 0), item);

	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).sprite == enemy->data->sprite);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).type == TileType_EMPTY);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).color == Clr_RED);

	Update_WorldTileEnemyOccupier(state.world_tiles, New_Coord(0, 0), NULL);

	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).sprite == item->sprite);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).type == TileType_EMPTY);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).color == Clr_GREEN);

	Update_WorldTileItemOccupier(state.world_tiles, New_Coord(0, 0), NULL);

	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).sprite == 'X');
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).type == TileType_EMPTY);
	mu_assert(__func__, Get_ForegroundTileData(&state.world_tiles[0][0]).color == Clr_CYAN);

	Cleanup_Test_GameStateAndPlayer(&state);
	return 0;
}

int run_all_tests() {
	mu_run_test(test_init_game_state_correct_values);
	mu_run_test(test_create_player_correct_values);
	mu_run_test(test_init_create_enemy_correct_values);

	mu_run_test(test_get_world_width_correct_value);
	mu_run_test(test_get_world_height_correct_value);

	mu_run_test(test_created_dungeon_floor_contains_staircase);
	mu_run_test(test_created_dungeon_floor_contains_player);

	mu_run_test(test_addto_player_health_correct_return_values);
	mu_run_test(test_addto_player_health_correct_current_health);

	mu_run_test(test_set_player_pos_bounds_valid);
	mu_run_test(test_set_player_pos_out_of_bounds);
	mu_run_test(test_set_player_pos_into_solid_fails);
	mu_run_test(test_set_player_pos_into_door);

	mu_run_test(test_update_game_log_normal_behaviour);
	mu_run_test(test_update_game_log_buffer_overflow_clamped);

	mu_run_test(test_drop_an_item_successfully);
	mu_run_test(test_cant_drop_item_onto_another);

	mu_run_test(test_pickup_an_item_successfully);
	mu_run_test(test_cant_pickup_another_item_because_inventory_full);

	mu_run_test(test_item_examine_correct_value);

	mu_run_test(test_get_tile_foreground_attributes_no_occupiers);
	mu_run_test(test_get_tile_foreground_attributes_single_occupier);
	mu_run_test(test_get_tile_foreground_attributes_full_occupiers);
	return 0;
}

floor_statistics_t get_statistics_on_dungeon_floor_creation(int iterations) {
	game_state_t state = Setup_Test_GameStateAndPlayer();

	int total_rooms = 0;
	int least_rooms = MAX_ROOMS;
	int most_rooms = MIN_ROOMS;

	for (int i = 0; i < iterations; i++) {
		InitCreate_DungeonFloor(&state, MAX_ROOMS, NULL);
		total_rooms += state.num_rooms_created;
		state.current_floor++;

		if (state.num_rooms_created < least_rooms) least_rooms = state.num_rooms_created;
		if (state.num_rooms_created > most_rooms) most_rooms = state.num_rooms_created;

		Cleanup_DungeonFloor(&state);
	}

	int average_rooms = total_rooms / iterations;

	Cleanup_Test_GameStateAndPlayer(&state);

	floor_statistics_t stats = { .floors_created = iterations, .average_rooms = average_rooms, .least_rooms = least_rooms, .most_rooms = most_rooms};
	return stats;
}


int main(int argc, char **argv) {
	floor_statistics_t dungeon_statistics;

	Setup_Test_Env();

	int result = run_all_tests();
	if (result == 0) {
		dungeon_statistics = get_statistics_on_dungeon_floor_creation(10000);
	}

	Cleanup_Test_Env();

	if (result != 0) {
		printf("TEST FAILED!\n\t%s\n", G_TEST_FAILED_BUF);
		printf("\tLast seed used: %d\n", last_seed_used);
	} else {
		printf("ALL TESTS PASSED\n");

		printf("\nStatistics for seed: %d\n", last_seed_used);
		printf(" - Average number of rooms over %d dungeon floors: %d, least number of rooms in a floor was: %d, most was: %d\n"
			, dungeon_statistics.floors_created, dungeon_statistics.average_rooms, dungeon_statistics.least_rooms, dungeon_statistics.most_rooms);
	}
	printf("\nTotal tests: %d\n", tests_run);

	return result != 0;
}