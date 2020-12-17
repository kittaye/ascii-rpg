#include <stdlib.h>
#include <assert.h>
#include "enemies.h"
#include "ascii_game.h"

static const enemy_data_t g_enemy_data_database[] = {
	[EnmySlug_ZOMBIE] =	{
		.name = "zombie",		
		.enemy_slug = EnmySlug_ZOMBIE,		
		.max_health = 5, 
		.sprite = SPR_ZOMBIE},

	[EnmySlug_WEREWOLF] = {
		.name = "werewolf",	
		.enemy_slug = EnmySlug_WEREWOLF,	
		.max_health = 3, 
		.sprite = SPR_WEREWOLF}
};

const enemy_data_t* Get_EnemyData(const enemy_slug_en enemy_slug) {
	return &g_enemy_data_database[enemy_slug];
}

enemy_t* InitCreate_Enemy(const enemy_data_t *enemy_data, coord_t pos) {
	assert(enemy_data != NULL);

	enemy_t *enemy = malloc(sizeof(*enemy));
	assert(enemy != NULL);

	enemy->data = enemy_data;
	enemy->curr_health = enemy->data->max_health;
	enemy->pos = pos;
	enemy->is_alive = true;
	enemy->loot = Get_Item(ItmSlug_NONE);

	return enemy;
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