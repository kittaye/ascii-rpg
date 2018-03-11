#include <stdlib.h>
#include <assert.h>
#include "enemies.h"
#include "ascii_game.h"

static const enemy_data_t g_enemy_data_database[] = {
	[EnmySlug_ZOMBIE] =		{.name = "zombie",		.enemy_slug = EnmySlug_ZOMBIE,		.max_health = 5, .sprite = SPR_ZOMBIE},
	[EnmySlug_WEREWOLF] =	{.name = "werewolf",	.enemy_slug = EnmySlug_WEREWOLF,	.max_health = 3, .sprite = SPR_WEREWOLF}
};

const enemy_data_t* Get_EnemyData(const enemy_slug_en enemy_slug) {
	return &g_enemy_data_database[enemy_slug];
}

void AddTo_EnemyList(enemy_node_t **list, enemy_t *enemy) {
	assert(list != NULL);
	assert(enemy != NULL);

	enemy_node_t *node = malloc(sizeof(*node));
	assert(node != NULL);

	node->enemy = enemy;
	node->next = NULL;

	if (*list == NULL) {
		*list = node;
	} else {
		enemy_node_t *searcher = *list;
		while (searcher->next != NULL) {
			searcher = searcher->next;
		}
		searcher->next = node;
	}
}

void Cleanup_EnemyList(enemy_node_t **list) {
	assert(list != NULL);

	enemy_node_t *tmp;
	while ((*list) != NULL) {
		tmp = (*list);
		(*list) = (*list)->next;
		free(tmp->enemy);
		free(tmp);
	}
}