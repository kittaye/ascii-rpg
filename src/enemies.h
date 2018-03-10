#ifndef ENEMIES_H_
#define ENEMIES_H_

#include <stdbool.h>
#include "items.h"
#include "coord.h"

#define SPR_ZOMBIE 'Z'
#define SPR_WEREWOLF 'W'

typedef enum enemy_slug_en {
	EnmySlug_ZOMBIE,
	EnmySlug_WEREWOLF,
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

/*
	Returns a pointer to enemy data from the global enemy data database (defined in enemies.c) that matches the 'enemy_slug' arg.
*/
const enemy_data_t* Get_EnemyData(enemy_slug_en enemy_slug);

/*
	Adds an enemy to a linked list of enemy nodes.
*/
void AddTo_EnemyList(enemy_node_t **list, enemy_t *enemy);

/*
	Frees the memory allocated to a linked list of enemy nodes.
*/
void Cleanup_EnemyList(enemy_node_t **list);

#endif /* ENEMIES_H_ */