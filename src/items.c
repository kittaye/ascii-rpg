#include "items.h"

#define SPR_EMPTY ' '

static const item_t g_item_database[] = {
	[ItmSlug_NONE] = {.name = "Empty", .sprite = SPR_EMPTY, .item_slug = ItmSlug_NONE, .value = 0},
	[ItmSlug_MAP] = {.name = "Map", .sprite = SPR_MAP, .item_slug = ItmSlug_MAP, .value = 0},
	[ItmSlug_SMALLFOOD] = {.name = "Small food", .sprite = SPR_SMALLFOOD, .item_slug = ItmSlug_SMALLFOOD, .value = 20},
	[ItmSlug_BIGFOOD] = {.name = "Big food", .sprite = SPR_BIGFOOD, .item_slug = ItmSlug_BIGFOOD, .value = 35}
};

const item_t* GetItem(const item_slug_en item_slug) {
	return &g_item_database[item_slug];
}