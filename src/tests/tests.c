#include <stdio.h>
#include <stdlib.h>

#include "ascii_game.h"
#include "george_graphics.h"
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

int run_all_tests() {
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