#ifndef ASCII_GAME_H_
#define ASCII_GAME_H_

#include <stdio.h>
#include "items.h"
#include "enemies.h"
#include "coord.h"
#include "tiles.h"
#include "colours.h"


#define CLAMP(x, min_val, max_val) (((x) < (min_val)) ? (min_val) : (((x) > (max_val)) ? (max_val) : (x)))

// Sprites.
#define SPR_EMPTY ' '
#define SPR_PLAYER '@'

// Other.
#define HUB_FILENAME "maps/hub.txt"
#define HUB_MAP_FREQUENCY 4
#define PLAYER_MAX_VISION 100
#define RIGHT_PANEL_OFFSET 36
#define BOTTOM_PANEL_OFFSET 6			
#define TOP_PANEL_OFFSET 0		
#define DEBUG_RCS_LIMIT 100000			// Room collision limit.
#define DEBUG_INJECTED_INPUT_LIMIT 256	// Injected user input limit (used for testing).
#define LOG_BUFFER_SIZE 175
#define MIN_ROOMS 2
#define MAX_ROOMS 100
#define MIN_ROOM_SIZE 5
#define INVENTORY_SIZE 9
#define _UNITY_VOID_SPRITE ';'		// Used by the Unity img-to-ascii converter for representing the void. NOTE: This value must be unique - no other sprite may use it.

typedef enum direction_en {
	Dir_UP,
	Dir_DOWN,
	Dir_LEFT,
	Dir_RIGHT
} direction_en;

typedef enum item_select_control_en {
	ItmCtrl_USE = 'e',
	ItmCtrl_DROP = 'd',
	ItmCtrl_EXAMINE = 'x'
} item_select_control_en;

typedef struct player_stats {
	int level;
	int max_health;
	int curr_health;
	int max_mana;
	int curr_mana;

	int max_vision;

	int s_str;	
	int s_def;
	int s_vit;
	int s_int;
	int s_lck;

	int enemies_slain;
	int num_gold;
} stats_t;

typedef struct log_list_t {
	char line1[LOG_BUFFER_SIZE];	// First message that is shown in the game log.
	char line2[LOG_BUFFER_SIZE];	// Second message that is shown in the game log.
	char line3[LOG_BUFFER_SIZE];	// Third message that is shown in the game log.
	char line4[LOG_BUFFER_SIZE];	// Fourth message that is shown in the game log.
	char line5[LOG_BUFFER_SIZE];	// Final message that is shown in the game log.
} log_list_t;

typedef struct room_t {
	coord_t TL_corner;
	coord_t TR_corner;
	coord_t BL_corner;
	coord_t BR_corner;
} room_t;

typedef struct player_t {
	stats_t stats;
	coord_t pos;
	const item_t *inventory[INVENTORY_SIZE];
	char sprite;
	colour_en color;
	char current_npc_target;						// The currently targeted sprite, used to interact with NPCs.
	int current_item_index_selected;				// The currently selected item index from the player's inventory, used to interact with the item.
} player_t;

typedef struct game_state_t {
	int game_turns;						// Current number of game turns since game started.
	int num_rooms_created;				// Number of rooms created in game (may not always == num_rooms_specified in command line).
	bool fog_of_war;					// Toggle whether all world tiles are shown or only those within range of the player.
	bool player_turn_over;				// Determines when the player has finished their turn.
	bool floor_complete;				// Determines when the player has completed the dungeon floor.
	int current_floor;			

	player_t player;				
	tile_t **world_tiles;				// Stores information about every (x, y) coordinate in the world map, for use in the game.
	room_t *rooms;						// Array of all created rooms after dungeon generation.
	log_list_t game_log;			
	enemy_node_t *enemy_list;			// Linked list of all enemies created in a dungeon.

	int debug_rcs;						// Room collisions during room creation.
	double debug_seed;					// RNG seed used to create this game.

	// This debug field is used to simulate a sequence of player inputs and inject them into unit tests. +1 to ensure a NUL-terminating byte.
	int debug_injected_inputs[DEBUG_INJECTED_INPUT_LIMIT + 1];
	int debug_injected_input_pos;
} game_state_t;


extern bool g_resize_error;			
extern bool g_process_over;			


/* 
	Initialises a game state struct to it's default values, including dynamic allocations.
*/
void Init_GameState(game_state_t *state);

/* 
	Initialises and creates a dungeon floor which consists of, at least, a player spawn room and a staircase room, with rooms connected inbetween filled with enemies and items. 
	Optionally specify a text file to use a custom layout for the dungeon floor.
*/
void InitCreate_DungeonFloor(game_state_t *state, unsigned int num_rooms_specified, const char *filename_specified);

/*
	Initialises a player struct to it's default values and returns it.
*/
player_t Create_Player(void);

/*
	Initalises and creates an enemy with it's default values set according to the enemy_data struct. It's current position is set to 'pos'. The created enemy is returned.
*/
enemy_t* InitCreate_Enemy(const enemy_data_t *enemy_data, coord_t pos);

/*
	Draws all elements to the screen, waits for user input, then performs world logic in response. Returning from this essentially finishes one game turn.
*/
void Process(game_state_t *state);

/*
	Draws the help screen. Waits for user input before returning.
*/
void Draw_HelpScreen(game_state_t *state);

/*
	Draws the player death screen. Waits for user input before returning.
*/
void Draw_DeathScreen(game_state_t *state);

/*
	Draws the Merchant trading screen. User input can buy a listed item or exit.
*/
void Draw_MerchantScreen(game_state_t *state);

/*
	Updates a world tile at position 'pos' with a new set of tile data.
*/
void Update_WorldTile(tile_t **world_tiles, coord_t pos, const tile_data_t *tile_data);

/*
	Updates a world tile at position 'pos' with a new item occupier.
*/
void Update_WorldTileItemOccupier(tile_t **world_tiles, coord_t pos, const item_t *item);

/*
	Updates a world tile at position 'pos' with a new enemy occupier.
*/
void Update_WorldTileEnemyOccupier(tile_t **world_tiles, coord_t pos, enemy_t *enemy);

/*
	Updates the game log consisting of 3 lines with a new formatted line of text, pushing the previous two lines of text upwards. The last line of text is removed.
*/
void Update_GameLog(log_list_t *game_log, const char *format, ...);

/*
	Loops through all enemies on the dungeon floor and computes their logic for the current game turn.
*/
void Update_AllEnemyCombat(game_state_t *state, enemy_node_t *enemy_list);

/*
	Gets the sprite that should be shown to the player when multiple things are at the same position. Foreground sprite is based on ordering rules.
*/
char Get_TileForegroundSprite(const tile_t *tile);

/*
	Gets the colour that should be shown to the player when multiple things are at the same position. Foreground colour is based on ordering rules.
*/
colour_en Get_TileForegroundColour(const tile_t *tile);

/*
	Gets the tile type that should be relevant to the player when multiple things are at the same position. Foreground type is based on ordering rules.
*/
tile_type_en Get_TileForegroundType(const tile_t *tile);

/*
	Loops through all world tiles on the dungeon floor and determines which should be shown to the user. 
	Game state variable 'fog_of_war' and player variable 'max_vision' are the determining factors.
*/
void Apply_Vision(const game_state_t *state, coord_t pos);

/*
	Interacts with the player's currently targeted NPC.
*/
void Interact_CurrentlyTargetedNPC(game_state_t *state);

/*
	Interacts with the player's currently selected item in a way specified by the key press.
*/
void Interact_CurrentlySelectedItem(game_state_t *state, item_select_control_en key_pressed);

/*
	Examines the item by displaying text to the game log.
*/
void Examine_Item(game_state_t *state, const item_t *item);

/*
	Adds to the player's current health. Negative values are used to deal damage to the player. Returns the difference in current health.
*/
int AddTo_Health(player_t *player, int amount);

/*
	Adds an item to the player's inventory. Returns true if the item is successfully added, false if the player's inventory is full.
*/
bool AddTo_Inventory(player_t *player, const item_t *item);

/*
	Tries to set the player's position to 'pos'. Returns true if successful, false if 'pos' is outside world bounds or on a world tile that is solid.
*/
bool Try_SetPlayerPos(game_state_t *state, coord_t pos);

/*
	Checks if 'coord' is outside of the world bounds. Returns true if it is, false otherwise.
*/
bool Check_OutOfWorldBounds(coord_t coord);

/*
	Returns the width of the world screen.
*/
int Get_WorldScreenWidth(void);

/*
	Returns the height of the world screen.
*/
int Get_WorldScreenHeight(void);

/*
	Frees all memory allocated from calling 'InitCreate_DungeonFloor'.
*/
void Cleanup_DungeonFloor(game_state_t *state);

/*
	Frees all memory allocated from calling 'Init_GameState'.
*/
void Cleanup_GameState(game_state_t *state);

#endif /* ASCII_GAME_H_ */
