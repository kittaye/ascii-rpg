#ifndef TILES_H_
#define TILES_H_

#include <stdbool.h>
#include "items.h"
#include "enemies.h"
#include "colours.h"

#define SPR_VOID ' '		
#define SPR_GROUND '.'		
#define SPR_GENERIC_WALL '#'
#define SPR_HORI_WALL '-'
#define SPR_VERT_WALL '|'
#define SPR_STAIRCASE '^'
#define SPR_MERCHANT '1'
#define SPR_GOLD 'g'
#define SPR_BIGGOLD 'G'
#define SPR_DOOR '+'

typedef enum tile_slug_en {
	TileSlug_VOID,
	TileSlug_GROUND,
	TileSlug_GENERIC_WALL,
	TileSlug_VERT_WALL,
	TileSlug_HORI_WALL,
	TileSlug_GOLD,
	TileSlug_BIGGOLD,
	TileSlug_STAIRCASE,
	TileSlug_OPENING,
	TileSlug_MERCHANT,
	TileSlug_DOOR
} tile_slug_en;

typedef enum tile_type_en {
	TileType_SOLID,
	TileType_SPECIAL,
	TileType_EMPTY,
	TileType_VOID
} tile_type_en;

typedef struct tile_data_t {
	const char sprite;
	const tile_type_en type;
	const colour_en color;
} tile_data_t;

typedef struct tile_t {
	bool is_visible;
	const tile_data_t *data;
	enemy_t *enemy_occupier;
	const item_t *item_occupier;
} tile_t;

/*
	Returns a pointer to the tile data from the global tile data database (defined in tiles.c) that matches the 'tile_slug' arg.
*/
const tile_data_t* Get_TileData(tile_slug_en tile_slug);

#endif /* TILES_H_ */