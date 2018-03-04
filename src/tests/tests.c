#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ascii_game.h"
#include "../george_graphics.h"
#include "../enemies.h"
#include "minunit.h"

int tests_run = 0;

static void Setup() {
	GEO_setup_screen();

	if (GEO_screen_width() < MIN_SCREEN_WIDTH || GEO_screen_height() < MIN_SCREEN_HEIGHT) {
		GEO_cleanup_screen();
		fprintf(stderr, "Terminal screen dimensions must be at least %dx%d to run tests. Exiting...\n", MIN_SCREEN_WIDTH, MIN_SCREEN_HEIGHT);
		exit(1);
	}
}

static void Cleanup() {
	GEO_cleanup_screen();
}

int test_init_game_state_correct_values() {
	game_state_t state;
	Init_GameState(&state);

	mu_assert(__func__, state.game_turns == 0);
	mu_assert(__func__, state.num_rooms_created == 0);
	mu_assert(__func__, state.current_floor == 1);
	mu_assert(__func__, state.fog_of_war == true);
	mu_assert(__func__, state.player_turn_over == false);
	mu_assert(__func__, state.floor_complete == false);
	mu_assert(__func__, state.debug_rcs == 0);
	mu_assert(__func__, state.enemy_list == (enemy_node_t*)NULL);
	mu_assert(__func__, state.rooms == (room_t*)NULL);

	mu_assert(__func__, state.world_tiles != NULL);
	for (int i = 0; i < Get_WorldScreenWidth(); i++) {
		mu_assert(__func__, state.world_tiles[i] != NULL);
		free(state.world_tiles[i]);
	}
	free(state.world_tiles);

	mu_assert(__func__, strcmp(state.game_log.line1, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line2, " ") == 0);
	mu_assert(__func__, strcmp(state.game_log.line3, " ") == 0);
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
	mu_assert(__func__, player.stats.max_health == 10);
	mu_assert(__func__, player.stats.curr_health == 10);
	mu_assert(__func__, player.stats.max_mana == 10);
	mu_assert(__func__, player.stats.curr_mana == 10);
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


int run_all_tests() {
	mu_run_test(test_init_game_state_correct_values);
	mu_run_test(test_create_player_correct_values);
	mu_run_test(test_init_create_enemy_correct_values);
	mu_run_test(test_get_world_width_correct_value);
	mu_run_test(test_get_world_height_correct_value);
	return 0;
}

int main(int argc, char **argv) {
	Setup();

	int result = run_all_tests();

	Cleanup();

	if (result != 0) {
		printf("TEST FAILED!\n\t%s\n", G_TEST_FAILED_BUF);
	} else {
		printf("ALL TESTS PASSED\n");
	}
	printf("\nTests run: %d\n", tests_run);

	return result != 0;
}