#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ascii_game.h"
#include "../george_graphics.h"
#include "../enemies.h"
#include "minunit.h"

int tests_run = 0;
int assertions_run = 0;

int last_seed_used = -1;

static void Setup_Test_Env() {
	GEO_setup_screen();

	if (GEO_screen_width() < MIN_SCREEN_WIDTH || GEO_screen_height() < MIN_SCREEN_HEIGHT) {
		GEO_cleanup_screen();
		fprintf(stderr, "Terminal screen dimensions must be at least %dx%d to run tests. Exiting...\n", MIN_SCREEN_WIDTH, MIN_SCREEN_HEIGHT);
		exit(1);
	}
}

static void Cleanup_Test_Env() {
	GEO_cleanup_screen();
}

static game_state_t Setup_Test_GameState() {
	game_state_t state;
	Init_GameState(&state);
	last_seed_used = state.debug_seed;
	return state;
}

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

	Cleanup_GameState(&state);
	return 0;
}

int test_create_player_correct_values() {
	player_t player = Create_Player();

	mu_assert(__func__, player.sprite == SPR_PLAYER);
	mu_assert(__func__, CoordsEqual(player.pos, NewCoord(0, 0)));
	mu_assert(__func__, player.color == Clr_Cyan);
	mu_assert(__func__, player.current_target == ' ');
	for (int i = 0; i < INVENTORY_SIZE; i++) {
		mu_assert(__func__, player.inventory[i] == GetItem(I_None));
	}
	mu_assert(__func__, player.stats.level == 1);
	mu_assert(__func__, player.stats.max_health > 0);
	mu_assert(__func__, player.stats.curr_health == player.stats.max_health);
	mu_assert(__func__, player.stats.max_mana > 0);
	mu_assert(__func__, player.stats.curr_mana == player.stats.max_mana);
	mu_assert(__func__, player.stats.max_vision == PLAYER_MAX_VISION);
	mu_assert(__func__, player.stats.s_STR == 1);
	mu_assert(__func__, player.stats.s_DEF == 1);
	mu_assert(__func__, player.stats.s_VIT == 1);
	mu_assert(__func__, player.stats.s_INT == 1);
	mu_assert(__func__, player.stats.s_LCK == 1);
	mu_assert(__func__, player.stats.enemies_slain == 0);
	mu_assert(__func__, player.stats.num_gold == 0);

	return 0;
}

int test_init_create_enemy_correct_values() {
	coord_t pos = NewCoord(10, 10);
	enemy_t* test_enemy = InitCreate_Enemy(GetEnemyData(E_Zombie), pos);

	mu_assert(__func__, test_enemy != NULL);
	mu_assert(__func__, test_enemy->data == GetEnemyData(E_Zombie));
	mu_assert(__func__, test_enemy->curr_health == GetEnemyData(E_Zombie)->max_health);
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

// Test for c-asserts/exception failures.
int test_create_valid_dungeon_floor() {
	game_state_t state = Setup_Test_GameState();
	InitCreate_DungeonFloor(&state, 10, NULL);

	Cleanup_DungeonFloor(&state);
	Cleanup_GameState(&state);
	return 0;
}

int test_addto_player_health_normal() {
	game_state_t state = Setup_Test_GameState();
	state.player = Create_Player();
	state.player.stats.max_health = 10;
	state.player.stats.curr_health = 5;

	AddTo_Health(&state.player, 2);
	mu_assert(__func__, state.player.stats.curr_health == 7);

	AddTo_Health(&state.player, -4);
	mu_assert(__func__, state.player.stats.curr_health == 3);

	Cleanup_GameState(&state);
	return 0;
}

int test_addto_player_health_over_limit() {
	game_state_t state = Setup_Test_GameState();
	state.player = Create_Player();
	state.player.stats.max_health = 10;
	state.player.stats.curr_health = 5;

	AddTo_Health(&state.player, 20);
	mu_assert(__func__, state.player.stats.curr_health == 10);

	AddTo_Health(&state.player, -15);
	mu_assert(__func__, state.player.stats.curr_health == 0);

	Cleanup_GameState(&state);
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
			Update_WorldTile(state.world_tiles, NewCoord(x, y), SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
		}
	}

	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(rand() % world_screen_w, rand() % world_screen_h)) == true);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == true);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, world_screen_h - 1)) == true);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(world_screen_w - 1, 0)) == true);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(world_screen_w - 1, world_screen_h - 1)) == true);

	Cleanup_GameState(&state);
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
			Update_WorldTile(state.world_tiles, NewCoord(x, y), SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
		}
	}

	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(-1, 0)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, -1)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(-1, -1)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, world_screen_h)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(world_screen_w, 0)) == false);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(world_screen_w, world_screen_h)) == false);

	Cleanup_GameState(&state);
	return 0;
}

int test_set_player_pos_to_different_location_types() {
	game_state_t state = Setup_Test_GameState();
	state.player = Create_Player();

	Update_WorldTile(state.world_tiles, NewCoord(0, 0), SPR_EMPTY, TileType_Empty, Clr_White, NULL, NULL);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == true);

	Update_WorldTile(state.world_tiles, NewCoord(0, 0), SPR_EMPTY, TileType_Item, Clr_White, NULL, NULL);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == true);

	Update_WorldTile(state.world_tiles, NewCoord(0, 0), SPR_EMPTY, TileType_Npc, Clr_White, NULL, NULL);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == true);

	Update_WorldTile(state.world_tiles, NewCoord(0, 0), SPR_EMPTY, TileType_Special, Clr_White, NULL, NULL);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == true);

	Update_WorldTile(state.world_tiles, NewCoord(0, 0), SPR_EMPTY, TileType_Enemy, Clr_White, NULL, NULL);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == true);

	Update_WorldTile(state.world_tiles, NewCoord(0, 0), SPR_EMPTY, TileType_Solid, Clr_White, NULL, NULL);
	mu_assert(__func__, Try_SetPlayerPos(&state, NewCoord(0, 0)) == false);

	Cleanup_GameState(&state);
	return 0;
}

int run_all_tests() {
	mu_run_test(test_init_game_state_correct_values);
	mu_run_test(test_create_player_correct_values);
	mu_run_test(test_init_create_enemy_correct_values);
	mu_run_test(test_get_world_width_correct_value);
	mu_run_test(test_get_world_height_correct_value);
	mu_run_test(test_create_valid_dungeon_floor);
	mu_run_test(test_addto_player_health_normal);
	mu_run_test(test_addto_player_health_over_limit);
	mu_run_test(test_set_player_pos_bounds_valid);
	mu_run_test(test_set_player_pos_out_of_bounds);
	mu_run_test(test_set_player_pos_to_different_location_types);
	return 0;
}

int main(int argc, char **argv) {
	Setup_Test_Env();

	int result = run_all_tests();

	Cleanup_Test_Env();

	if (result != 0) {
		printf("TEST FAILED!\n\t%s\n", G_TEST_FAILED_BUF);
		printf("\tSeed used: %d\n", last_seed_used);
	} else {
		printf("ALL TESTS PASSED\n");
	}
	printf("\nTotal assertions: %d", assertions_run);
	printf("\nTotal tests: %d\n", tests_run);

	return result != 0;
}