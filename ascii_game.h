#ifndef ASCII_GAME_H_
#define ASCII_GAME_H_

#include "items.h"

// Sprites.
#define SPR_EMPTY ' '
#define SPR_PLAYER '@'
#define SPR_GOLD 'g'
#define SPR_BIGGOLD 'G'
#define SPR_WALL '#'
#define SPR_STAIRCASE '^'
#define SPR_SMALLFOOD 'f'
#define SPR_BIGFOOD 'F'
#define SPR_ZOMBIE 'Z'
#define SPR_WEREWOLF 'W'
#define SPR_MERCHANT '1'

// Other.
#define PLAYER_MAX_VISION 100
#define MAX_ENEMIES 1000
#define BOTTOM_PANEL_OFFSET 6			
#define TOP_PANEL_OFFSET 0		
#define DEBUG_RCS_LIMIT 100000	// Room collision limit.
#define LOG_BUFFER_SIZE 250
#define MIN_ROOMS 2
#define MIN_ROOM_SIZE 5
#define INVENTORY_SIZE 10

// NOTE: These min values should be the largest txt dimension size of a dungeon layout from a file!
#define MIN_SCREEN_WIDTH 85	
#define MIN_SCREEN_HEIGHT 56

typedef enum colour_en {
	Clr_White = 0,
	Clr_Yellow = 1,
	Clr_Red = 2,
	Clr_Blue = 3,
	Clr_Magenta = 4,
	Clr_Cyan = 5,
	Clr_Green = 6
} colour_en;

typedef enum tile_type_en {
	T_Npc,
	T_Enemy,
	T_Solid,
	T_Special,
	T_Item,
	T_Empty
} tile_type_en;

typedef enum direction_en {
	D_Up,
	D_Down,
	D_Left,
	D_Right
} direction_en;

typedef struct player_stats {
	int level;
	int max_health;
	int curr_health;
	int max_mana;
	int curr_mana;
	int max_vision;

	int s_STR;
	int s_DEF;
	int s_VIT;
	int s_INT;
	int s_LCK;

	int enemies_slain;
	int num_gold;
} stats_t;

typedef struct log_list_t {
	char line1[LOG_BUFFER_SIZE];	// First message that is shown in the game log.
	char line2[LOG_BUFFER_SIZE];	// Second message that is shown in the game log.
	char line3[LOG_BUFFER_SIZE];	// Third message that is shown in the game log.
} log_list_t;

typedef struct coord_t {
	int x;
	int y;
} coord_t;

typedef struct room_t {
	coord_t TL_corner;
	coord_t TR_corner;
	coord_t BL_corner;
	coord_t BR_corner;
} room_t;

typedef struct player_t {
	stats_t stats;
	coord_t pos;
	item_t *inventory[INVENTORY_SIZE];
	char sprite;
	int color;
} player_t;

typedef struct entity_data_t {
	char *name;
	tile_type_en type;
	int max_health;
	char sprite;
	int color;
} entity_data_t;

typedef struct entity_t {
	entity_data_t *data;
	int curr_health;
	bool is_alive;
	item_slug_en loot;
	coord_t pos;
} entity_t;

typedef struct entity_node_t {
	entity_t *entity;
	struct entity_node_t *next;
} entity_node_t;

typedef struct tile_t {
	char sprite;
	tile_type_en type;
	int color;
	entity_t *entity_occupier;
	item_t *item_occupier;
} tile_t;

typedef struct game_state_t {
	int game_turns;					// Current number of game turns since game started.
	int num_rooms_created;			// Number of rooms created in game (may not always == num_rooms_specified in command line).
	bool fog_of_war;				// Toggle whether all world tiles are shown or only those within range of the player.
	bool player_turn_over;			// Determines when the player has finished their turn.
	bool floor_complete;			// Determines when the player has completed the dungeon floor.
	int current_floor;				
	char current_target;			// The player-targetted sprite, used to interact with NPCs.

	player_t player;				
	tile_t **world_tiles;			// Stores information about every (x, y) coordinate in the terminal, for use in the game.
	room_t *rooms;					// Array of all created rooms after CreateDungeonRooms returns.
	log_list_t game_log;			

	entity_data_t data_zombie;		// Global data for all zombies.
	entity_data_t data_werewolf;	// Global data for all werewolves.

	entity_node_t *enemy_list;		// Linked list of all enemies created in game.

	int debug_rcs;					// Room collisions during room creation.
	double debug_seed;				// RNG seed used to create this game.
} game_state_t;


// Function initialization.
void InitGameState(game_state_t*);
void ResetDungeonFloor(game_state_t*);
void CreateDungeonFloor(game_state_t*, int, int, char*);
player_t InitPlayer(const game_state_t*, char);
entity_t* InitAndCreateEnemy(entity_data_t*, coord_t);
void Process(game_state_t*);

int GetNextRoomRadius();
void CreateOpenRooms(game_state_t*, int, int);
void DefineOpenRoom(room_t*, int);
void CreateClosedRooms(game_state_t*, int, int);
void DefineClosedRoom(room_t*, coord_t, int);
void InstantiateClosedRoomRecursive(game_state_t*, coord_t, int, int, int);
void GenerateRoom(tile_t**, const room_t*);
void GenerateCorridor(tile_t**, coord_t, int, direction_en);
void CreateRoomsFromFile(game_state_t*, const char*);
void PopulateRooms(game_state_t*);
coord_t GetRandRoomOpeningPos(const room_t*);
bool CheckRoomCollision(const tile_t**, const room_t*);
bool CheckRoomMapBounds(const room_t*);
bool CheckMapBounds(coord_t);
bool CheckCorridorCollision(const tile_t**, coord_t, int, direction_en);

void UpdateWorldTile(tile_t**, coord_t, char, tile_type_en, int, entity_t*, item_t*);
void PerformWorldLogic(game_state_t*, const tile_t*, coord_t);
void UpdateGameLog(log_list_t*, const char*, ...);
void NextPlayerInput(game_state_t*);
void ApplyVision(const game_state_t*, coord_t);
void AddToEnemyList(entity_node_t**, entity_t*);
void EnemyCombatUpdate(game_state_t*, entity_node_t*);
void DrawHelpScreen();
void DrawPlayerInfoScreen(const game_state_t*);
void DrawDeathScreen();
void DrawMerchantScreen(game_state_t *);
void SetPlayerPos(player_t*, coord_t);
coord_t NewCoord(int, int);
bool FContainsChar(FILE*, char);
void InteractWithNPC(game_state_t*, char);
int GetKeyInput();
int AddHealth(player_t*, int);
bool AddToInventory(player_t*, item_t*);

void Cleanup_GameState(game_state_t*);
void FreeEnemyList(entity_node_t**);

#endif /* ASCII_GAME_H_ */
