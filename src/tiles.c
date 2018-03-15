#include "tiles.h"

static const tile_data_t g_tile_data_database[] = {
	[TileSlug_VOID] =			{.sprite = SPR_VOID,			.type = TileType_VOID,		.color = Clr_MAGENTA,	.is_structural = true	},
	[TileSlug_GROUND] =			{.sprite = SPR_GROUND,			.type = TileType_EMPTY,		.color = Clr_MAGENTA,	.is_structural = false	},
	[TileSlug_GENERIC_WALL] =	{.sprite = SPR_GENERIC_WALL,	.type = TileType_SOLID,		.color = Clr_WHITE,		.is_structural = true	},
	[TileSlug_VERT_WALL] =		{.sprite = SPR_VERT_WALL,		.type = TileType_SOLID,		.color = Clr_WHITE,		.is_structural = true	},
	[TileSlug_HORI_WALL] =		{.sprite = SPR_HORI_WALL,		.type = TileType_SOLID,		.color = Clr_WHITE,		.is_structural = true	},
	[TileSlug_GOLD] =			{.sprite = SPR_GOLD,			.type = TileType_ITEM,		.color = Clr_GREEN,		.is_structural = false	},
	[TileSlug_BIGGOLD] =		{.sprite = SPR_BIGGOLD,			.type = TileType_ITEM,		.color = Clr_GREEN,		.is_structural = false	},
	[TileSlug_STAIRCASE] =		{.sprite = SPR_STAIRCASE,		.type = TileType_SPECIAL,	.color = Clr_YELLOW,	.is_structural = false	},
	[TileSlug_MERCHANT] =		{.sprite = SPR_MERCHANT,		.type = TileType_NPC,		.color = Clr_CYAN,		.is_structural = false	},
	[TileSlug_DOOR] =			{.sprite = SPR_DOOR,			.type = TileType_EMPTY,		.color = Clr_WHITE,		.is_structural = true	}
};

const tile_data_t* Get_TileData(const tile_slug_en tile_slug) {
	return &g_tile_data_database[tile_slug];
}