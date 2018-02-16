#include "items.h"

static item_t g_item_database[] = {
	[I_None] = {.name = "Empty", .item_slug = I_None, .value = 0},
	[I_Map] = {.name = "Map", .item_slug = I_Map, .value = 0},
	[I_SmallFood] = {.name = "Small food", .item_slug = I_SmallFood, .value = 20},
	[I_BigFood] = {.name = "Big food", .item_slug = I_BigFood, .value = 35}
};

item_t* GetItem(item_slug_en item_slug) {
	return &g_item_database[item_slug];
}