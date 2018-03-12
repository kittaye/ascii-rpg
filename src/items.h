#ifndef ITEMS_H_
#define ITEMS_H_

#define SPR_HP_POT_I 'h'
#define SPR_HP_POT_II 'H'

typedef enum item_slug_en {
	ItmSlug_NONE,
	ItmSlug_HP_POT_I,
	ItmSlug_HP_POT_II
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
const item_t* Get_Item(item_slug_en item_slug);

#endif /* ITEMS_H_ */