#include "tiles.h"

static const tile_data_t g_tile_data_database[] = {
	[TileSlug_VOID] = {.sprite = SPR_VOID, .type = TileType_EMPTY, .color = Clr_MAGENTA},
	[TileSlug_GROUND] = {.sprite = SPR_GROUND, .type = TileType_EMPTY, .color = Clr_MAGENTA},
	[TileSlug_WALL] = {.sprite = SPR_WALL, .type = TileType_SOLID, .color = Clr_WHITE},
	[TileSlug_GOLD] = {.sprite = SPR_GOLD, .type = TileType_ITEM, .color = Clr_GREEN},
	[TileSlug_BIGGOLD] = {.sprite = SPR_BIGGOLD, .type = TileType_ITEM, .color = Clr_GREEN},
	[TileSlug_STAIRCASE] = {.sprite = SPR_STAIRCASE, .type = TileType_SPECIAL, .color = Clr_YELLOW},
	[TileSlug_OPENING] = {.sprite = SPR_OPENING, .type = TileType_EMPTY, .color = Clr_WHITE},
	[TileSlug_MERCHANT] = {.sprite = SPR_MERCHANT, .type = TileType_NPC, .color = Clr_CYAN}
};

const tile_data_t* GetTileData(const tile_slug_en tile_slug) {
	return &g_tile_data_database[tile_slug];
}