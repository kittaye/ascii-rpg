#include "items.h"

#define SPR_EMPTY ' '

static const item_t g_item_database[] = {
	[I_None] = {.name = "Empty", .sprite = SPR_EMPTY, .item_slug = I_None, .value = 0},
	[I_Map] = {.name = "Map", .sprite = SPR_MAP, .item_slug = I_Map, .value = 0},
	[I_SmallFood] = {.name = "Small food", .sprite = SPR_SMALLFOOD, .item_slug = I_SmallFood, .value = 20},
	[I_BigFood] = {.name = "Big food", .sprite = SPR_BIGFOOD, .item_slug = I_BigFood, .value = 35}
};

const item_t* GetItem(const item_slug_en item_slug) {
	return &g_item_database[item_slug];
}