#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ascii_game.h"
#include "../george_graphics.h"
#include "../enemies.h"
#include "../log_messages.h"
#include "minunit.h"

int tests_run = 0;
int assertions_run = 0;

int last_seed_used = -1;

// Private functions.

static void Setup_Test_Env();
static void Cleanup_Test_Env();
static game_state_t Setup_Test_GameState();
static void Cleanup_Test_GameState(game_state_t *state);
static game_state_t Setup_Test_GameStatePlayerAndDungeon();
static void Cleanup_Test_GameStatePlayerAndDungeon(game_state_t *state);

/*
	Setup before any tests.
*/
static void Setup_Test_Env() {
	GEO_setup_screen();

	if (GEO_screen_width() < MIN_SCREEN_WIDTH || GEO_screen_height() < MIN_SCREEN_HEIGHT) {
		GEO_cleanup_screen();
		fprintf(stderr, "Terminal screen dimensions must be at least %dx%d to run tests. Exiting...\n", MIN_SCREEN_WIDTH, MIN_SCREEN_HEIGHT);
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
	Streamline tests that need game state to be initialised.
*/
static game_state_t Setup_Test_GameState() {
	game_state_t state;
	Init_GameState(&state);
	last_seed_used = state.debug_seed;
	return state;
}

/*
	Frees all allocated memory from calling 'Setup_Test_GameState'.
*/
static void Cleanup_Test_GameState(game_state_t *state) {
	Cleanup_GameState(state);
}

/*
	Streamline tests that need game state, player, and the dungeon floor to be initialised.
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
static bool Worldtile_Values_Equal(game_state_t *state, coord_t pos_to_assert, char sprite, tile_type_en type, colour_en colour, enemy_t *enemy_occupier, const item_t *item_occupier) {
	if (state->world_tiles[pos_to_assert.x][pos_to_assert.y].sprite != sprite) return false;
	else if (state->world_tiles[pos_to_assert.x][pos_to_assert.y].type != type) return false;
	else if (state->world_tiles[pos_to_assert.x][pos_to_assert.y].color != colour) return false;
	else if (state->world_tiles[pos_to_assert.x][pos_to_assert.y].enemy_occupier != enemy_occupier) return false;
	else if (state->world_tiles[pos_to_assert.x][pos_to_assert.y].item_occupier != item_occupier) return false;
	else return true;
}

// TESTS START HERE --------------------------------------------------------------------------------

int test_init_game_state_correct_values() {
	game_state_t state = Setup_Test_GameState();

	mu_assert(__func__, state.game_turns == 0);
	mu_assert(__func__, state.num_rooms_created == 0);
	mu_assert(__func__, state.current_floor == 1);
	mu_assert(__func__, state.fog_of_war == true);
	mu_assert(__func__, state.player_turn_over == false);
	mu_assert(__func__, state.floor_complete == false);
	mu_assert(__func__, state.debug_rcs == 0);
	mu_assert(__func__, state.debug_seed > -1);
	mu_assert(__func__, state.enemy_list == (enemy_node_t*)NULL);
	mu_assert(__func__, state.rooms == (room_t*)NULL);

	mu_assert(__func__, state.world_tiles != NULL);
	for (int i = 0; i < Get_WorldScreenWidth(); i++) {
		mu_assert(__func__, state.world_tiles[i] != NULL);
	}

	mu_assert(__func__, strcmp(state.game_log.line1, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line2, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line3, " ") == 0);

	Cleanup_Test_GameState(&state);
	return 0;
}

int test_create_player_correct_values() {
	player_t player = Create_Player();

	mu_assert(__func__, player.sprite == SPR_PLAYER);
	mu_assert(__func__, CoordsEqual(player.pos, NewCoord(-1, -1)));
	mu_assert(__func__, player.color == Clr_CYAN);
	mu_assert(__func__, player.current_npc_target == ' ');
	mu_assert(__func__, player.current_item_index_selected == -1);
	for (int i = 0; i < INVENTORY_SIZE; i++) {
		mu_assert(__func__, player.inventory[i] == GetItem(ItmSlug_NONE));
	}
	mu_assert(__func__, player.stats.level == 1);
	mu_assert(__func__, player.stats.max_health > 0);
	mu_assert(__func__, player.stats.curr_health == player.stats.max_health);
	mu_assert(__func__, player.stats.max_mana > 0);
	mu_assert(__func__, player.stats.curr_mana == player.stats.max_mana);
	mu_assert(__func__, player.stats.max_vision == PLAYER_MAX_VISION);
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
	coord_t pos = NewCoord(10, 10);
	enemy_t* test_enemy = InitCreate_Enemy(GetEnemyData(EnmySlug_ZOMBIE), pos);

	mu_assert(__func__, test_enemy != NULL);
	mu_assert(__func__, test_enemy->data == GetEnemyData(EnmySlug_ZOMBIE));
	mu_assert(__func__, test_enemy->curr_health == GetEnemyData(EnmySlug_ZOMBIE)->max_health);
	mu_assert(__func__, CoordsEqual(test_enemy->pos, pos));
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
			if (state.world_tiles[x][y].sprite == SPR_STAIRCASE) {
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

	mu_assert(__func__, CoordsEqual(state.player.pos, NewCoord(-1, -1)) == false);

	Cleanup_Test_GameStatePlayerAndDungeon();
	return 0;
}

int test_addto_player_health_correct_return_values() {
	game_state_t state = Setup_Test_GameState();
	state.player = Create_Player();
	state.player.stats.max_health = 10;
	state.player.stats.curr_health = 5;

	mu_assert(__func__, AddTo_Health(&state.player, 2) == 2);		// current health is now 7
	mu_assert(__func__, AddTo_Health(&state.player, 5) == 3);		// current health is now 10
	mu_assert(__func__, AddTo_Health(&state.player, -7) == -7);		// current health is now 3
	mu_assert(__func__, AddTo_Health(&state.player, -10) == -3);	// current health is now 0

	Cleanup_Test_GameState(&state);
	return 0;
}

int test_addto_player_health_correct_current_health() {
	game_state_t state = Setup_Test_GameState();
	state.player = Create_Player();
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

	Cleanup_Test_GameState(&state);
	return 0;
}

int test_set_player_pos_bounds_valid() {
	game_state_t state = Setup_Test_GameState();
	state.player = Create_Player();
	const int world_screen_w = Get_WorldScreenWidth();
	const int world_screen_h = Get_WorldScreenHeight();

	// Create empty space.
	for (int x = 0; x < world_screen_w; x++) {
		for (int y = 0; y < world_screen_h; y++) {
			Update_WorldTile(state.world_tiles, NewCoord(x, y), SPR_EMPTY, TileType_EMPTY, Clr_WHITE, NULL, NULL);
		}
	}

	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(rand() % world_screen_w, rand() % world_screen_h)) == true);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == true);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, world_screen_h - 1)) == true);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(world_screen_w - 1, 0)) == true);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(world_screen_w - 1, world_screen_h - 1)) == true);

	Cleanup_Test_GameState(&state);
	return 0;
}

int test_set_player_pos_out_of_bounds() {
	game_state_t state = Setup_Test_GameState();
	state.player = Create_Player();
	const int world_screen_w = Get_WorldScreenWidth();
	const int world_screen_h = Get_WorldScreenHeight();

	// Create empty space.
	for (int x = 0; x < world_screen_w; x++) {
		for (int y = 0; y < world_screen_h; y++) {
			Update_WorldTile(state.world_tiles, NewCoord(x, y), SPR_EMPTY, TileType_EMPTY, Clr_WHITE, NULL, NULL);
		}
	}

	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(-1, 0)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, -1)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(-1, -1)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, world_screen_h)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(world_screen_w, 0)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(world_screen_w, world_screen_h)) == false);

	Cleanup_Test_GameState(&state);
	return 0;
}

int test_set_player_pos_to_different_location_types() {
	game_state_t state = Setup_Test_GameState();
	state.player = Create_Player();

	Update_WorldTile(state.world_tiles, NewCoord(0, 0), SPR_EMPTY, TileType_EMPTY, Clr_WHITE, NULL, NULL);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == true);

	Update_WorldTile(state.world_tiles, NewCoord(0, 0), SPR_EMPTY, TileType_ITEM, Clr_WHITE, NULL, NULL);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == true);

	Update_WorldTile(state.world_tiles, NewCoord(0, 0), SPR_EMPTY, TileType_NPC, Clr_WHITE, NULL, NULL);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == true);

	Update_WorldTile(state.world_tiles, NewCoord(0, 0), SPR_EMPTY, TileType_SPECIAL, Clr_WHITE, NULL, NULL);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == true);

	Update_WorldTile(state.world_tiles, NewCoord(0, 0), SPR_EMPTY, TileType_ENEMY, Clr_WHITE, NULL, NULL);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == true);

	Update_WorldTile(state.world_tiles, NewCoord(0, 0), SPR_EMPTY, TileType_SOLID, Clr_WHITE, NULL, NULL);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == false);

	Cleanup_Test_GameState(&state);
	return 0;
}

int test_update_game_log_normal_behaviour() {
	game_state_t state = Setup_Test_GameState();

	Update_GameLog(&state.game_log, "Hello");
	mu_assert(__func__, strcmp(state.game_log.line3, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line2, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line1, "Hello") == 0);

	Update_GameLog(&state.game_log, "World");
	mu_assert(__func__, strcmp(state.game_log.line3, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line2, "Hello") == 0);
	mu_assert(__func__, strcmp(state.game_log.line1, "World") == 0);

	Update_GameLog(&state.game_log, " ");
	mu_assert(__func__, strcmp(state.game_log.line3, "Hello") == 0);
	mu_assert(__func__, strcmp(state.game_log.line2, "World") == 0);
	mu_assert(__func__, strcmp(state.game_log.line1, " ") == 0);

	Update_GameLog(&state.game_log, "Test");
	Update_GameLog(&state.game_log, " ");
	mu_assert(__func__, strcmp(state.game_log.line3, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line2, "Test") == 0);
	mu_assert(__func__, strcmp(state.game_log.line1, " ") == 0);

	Cleanup_Test_GameState(&state);
	return 0;
}

int test_update_game_log_buffer_overflow_clamped() {
	game_state_t state = Setup_Test_GameState();
	char long_string[LOG_BUFFER_SIZE + 123];
	memset(long_string, 'a', LOG_BUFFER_SIZE + 123);

	Update_GameLog(&state.game_log, long_string);

	mu_assert(__func__, strlen(state.game_log.line1) == LOG_BUFFER_SIZE - 1);

	Cleanup_Test_GameState(&state);
	return 0;
}

int test_drop_an_item_successfully() {
	game_state_t state = Setup_Test_GameStatePlayerAndDungeon();
	const item_t *item = GetItem(ItmSlug_SMALLFOOD);

	state.player.inventory[0] = item;
	state.player.current_item_index_selected = 0;
	Interact_CurrentlySelectedItem(&state, 'd');

	char expected_result[LOG_BUFFER_SIZE];
	snprintf(expected_result, LOG_BUFFER_SIZE, LOGMSG_PLR_DROP_ITEM, item->name);

	mu_assert(__func__, strcmp(state.game_log.line1, expected_result) == 0);
	mu_assert(__func__, Worldtile_Values_Equal(&state, state.player.pos, item->sprite, TileType_ITEM, Clr_GREEN, NULL, item) == true);

	Cleanup_Test_GameStatePlayerAndDungeon(&state);
	return 0;
}

int test_cant_drop_item_onto_another() {
	game_state_t state = Setup_Test_GameStatePlayerAndDungeon();

	state.player.inventory[0] = GetItem(ItmSlug_SMALLFOOD);
	state.player.inventory[1] = GetItem(ItmSlug_SMALLFOOD);
	state.player.current_item_index_selected = 0;
	Interact_CurrentlySelectedItem(&state, ItmCtrl_DROP);
	state.player.current_item_index_selected = 1;
	Interact_CurrentlySelectedItem(&state, ItmCtrl_DROP);

	mu_assert(__func__, strcmp(state.game_log.line1, LOGMSG_PLR_CANT_DROP_ITEM) == 0);

	Cleanup_Test_GameStatePlayerAndDungeon(&state);
	return 0;
}

int test_item_examine_correct_value() {
	game_state_t state = Setup_Test_GameStatePlayerAndDungeon();
	state.player.inventory[0] = GetItem(ItmSlug_SMALLFOOD);
	state.player.inventory[1] = GetItem(ItmSlug_BIGFOOD);

	state.player.current_item_index_selected = 0;
	Interact_CurrentlySelectedItem(&state, ItmCtrl_EXAMINE);
	mu_assert(__func__, strcmp(state.game_log.line1, LOGMSG_EXAMINE_SMALL_FOOD) == 0);

	state.player.current_item_index_selected = 1;
	Interact_CurrentlySelectedItem(&state, ItmCtrl_EXAMINE);
	mu_assert(__func__, strcmp(state.game_log.line1, LOGMSG_EXAMINE_BIG_FOOD) == 0);

	Cleanup_Test_GameStatePlayerAndDungeon(&state);
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
	mu_run_test(test_set_player_pos_to_different_location_types);

	mu_run_test(test_update_game_log_normal_behaviour);
	mu_run_test(test_update_game_log_buffer_overflow_clamped);

	mu_run_test(test_drop_an_item_successfully);
	mu_run_test(test_cant_drop_item_onto_another);

	mu_run_test(test_item_examine_correct_value);
	return 0;
}

int main(int argc, char **argv) {
	Setup_Test_Env();

	int result = run_all_tests();

	Cleanup_Test_Env();

	if (result != 0) {
		printf("TEST FAILED!\n\t%s\n", G_TEST_FAILED_BUF);
		printf("\tLast seed used: %d\n", last_seed_used);
	} else {
		printf("ALL TESTS PASSED\n");
	}
	printf("\nTotal assertions: %d", assertions_run);
	printf("\nTotal tests: %d\n", tests_run);

	return result != 0;
}