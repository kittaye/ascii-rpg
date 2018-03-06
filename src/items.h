#ifndef ITEMS_H_
#define ITEMS_H_

#define SPR_GOLD 'g'
#define SPR_BIGGOLD 'G'
#define SPR_SMALLFOOD 'f'
#define SPR_BIGFOOD 'F'
#define SPR_MAP 'm'

typedef enum item_slug_en {
	I_None,
	I_Map,
	I_SmallFood,
	I_BigFood
} item_slug_en;

typedef struct item_t {
	const char *const name;
	const char sprite;
	const item_slug_en item_slug;
	const int value;
} item_t;

/*
	Returns a pointer to the item from the global item database (defined in items.c) that matches the 'item_slug' arg.
*/
const item_t* GetItem(item_slug_en item_slug);

#endif /* ITEMS_H_ */