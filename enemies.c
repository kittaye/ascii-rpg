#include "enemies.h"
#include "ascii_game.h"

static const enemy_data_t g_enemy_data_database[] = {
	[E_Zombie] = {.name = "Zombie", .enemy_slug = E_Zombie, .max_health = 5, .sprite = SPR_ZOMBIE},
	[E_Werewolf] = {.name = "Werewolf", .enemy_slug = E_Werewolf, .max_health = 3, .sprite = SPR_WEREWOLF}
};

const enemy_data_t* GetEnemyData(const enemy_slug_en enemy_slug) {
	return &g_enemy_data_database[enemy_slug];
}