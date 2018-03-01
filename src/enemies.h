#ifndef ENEMIES_H_
#define ENEMIES_H_

#include <stdbool.h>
#include "items.h"
#include "coord.h"

typedef enum enemy_slug_en {
	E_Zombie,
	E_Werewolf,
} enemy_slug_en;

typedef struct enemy_data_t {
	const char *const name;
	const enemy_slug_en enemy_slug;
	const int max_health;
	const char sprite;
} enemy_data_t;

typedef struct enemy_t {
	const enemy_data_t *data;
	int curr_health;
	bool is_alive;
	const item_t *loot;
	coord_t pos;
} enemy_t;

typedef struct enemy_node_t {
	enemy_t *enemy;
	struct enemy_node_t *next;
} enemy_node_t;

// Returns a pointer to the enemy data from the global enemy data database (defined in enemies.c) that matches the enemy_slug arg.
const enemy_data_t* GetEnemyData(enemy_slug_en enemy_slug);

void AddToEnemyList(enemy_node_t **list, enemy_t *enemy);

void FreeEnemyList(enemy_node_t **list);

#endif /* ENEMIES_H_ */