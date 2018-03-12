#include "items.h"

#define SPR_EMPTY ' '

static const item_t g_item_database[] = {
	[ItmSlug_NONE] =		{.name = " ",						.sprite = SPR_EMPTY,			.item_slug = ItmSlug_NONE,			.value = 0},
	[ItmSlug_HP_POT_I] =	{.name = "HP potion I",				.sprite = SPR_HP_POT_I,			.item_slug = ItmSlug_HP_POT_I,		.value = 20},
	[ItmSlug_HP_POT_II] =	{.name = "HP potion II",			.sprite = SPR_HP_POT_II,		.item_slug = ItmSlug_HP_POT_II,		.value = 35}
};

const item_t* Get_Item(const item_slug_en item_slug) {
	return &g_item_database[item_slug];
}