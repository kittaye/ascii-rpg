#ifndef ASCII_GAME_H_
#define ASCII_GAME_H_

#include <stdio.h>
#include "items.h"
#include "enemies.h"
#include "coord.h"

// Sprites.
#define SPR_EMPTY ' '
#define SPR_PLAYER '@'
#define SPR_GOLD 'g'
#define SPR_BIGGOLD 'G'
#define SPR_WALL '#'
#define SPR_STAIRCASE '^'
#define SPR_SMALLFOOD 'f'
#define SPR_BIGFOOD 'F'
#define SPR_MAP 'm'
#define SPR_ZOMBIE 'Z'
#define SPR_WEREWOLF 'W'
#define SPR_MERCHANT '1'

// Other.
#define HUB_FILENAME "maps/hub.txt"
#define PLAYER_MAX_VISION 100
#define MAX_ENEMIES 1000
#define RIGHT_PANEL_OFFSET 36
#define BOTTOM_PANEL_OFFSET 6			
#define TOP_PANEL_OFFSET 0		
#define DEBUG_RCS_LIMIT 100000	// Room collision limit.
#define LOG_BUFFER_SIZE 250
#define MIN_ROOMS 2
#define MAX_ROOMS 1000
#define MIN_ROOM_SIZE 5
#define INVENTORY_SIZE 10

// NOTE: These min values should be the largest txt dimension size of a dungeon layout from a file, + the panel offsets!
#define MIN_SCREEN_WIDTH 85 + 36	
#define MIN_SCREEN_HEIGHT 50 + 6

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
	TileType_Npc,
	TileType_Enemy,
	TileType_Solid,
	TileType_Special,
	TileType_Item,
	TileType_Empty
} tile_type_en;

typedef enum direction_en {
	Dir_Up,
	Dir_Down,
	Dir_Left,
	Dir_Right
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
	char line1[LOG_BUFFER_SIZE + 1];	// First message that is shown in the game log.
	char line2[LOG_BUFFER_SIZE + 1];	// Second message that is shown in the game log.
	char line3[LOG_BUFFER_SIZE + 1];	// Third message that is shown in the game log.
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
	int color;
	char current_target;			// The player-targetted sprite, used to interact with NPCs.
} player_t;

typedef struct tile_t {
	char sprite;
	tile_type_en type;
	int color;
	enemy_t *enemy_occupier;
	const item_t *item_occupier;
} tile_t;

typedef struct game_state_t {
	int game_turns;					// Current number of game turns since game started.
	int num_rooms_created;			// Number of rooms created in game (may not always == num_rooms_specified in command line).
	bool fog_of_war;				// Toggle whether all world tiles are shown or only those within range of the player.
	bool player_turn_over;			// Determines when the player has finished their turn.
	bool floor_complete;			// Determines when the player has completed the dungeon floor.
	int current_floor;				

	player_t player;				
	tile_t **world_tiles;			// Stores information about every (x, y) coordinate in the terminal, for use in the game.
	room_t *rooms;					// Array of all created rooms after CreateDungeonRooms returns.
	log_list_t game_log;			
	enemy_node_t *enemy_list;		// Linked list of all enemies created in game.

	int debug_rcs;					// Room collisions during room creation.
	double debug_seed;				// RNG seed used to create this game.
} game_state_t;

// Global variables.
extern bool g_resize_error;
extern bool g_process_over;

// Function declarations.
void Init_GameState(game_state_t *state);
void InitCreate_DungeonFloor(game_state_t *state, int num_rooms_specified, int room_size_specified, const char *filename_specified);
player_t Create_Player(void);
enemy_t* InitCreate_Enemy(const enemy_data_t *enemy_data, coord_t pos);
void Process(game_state_t *state);

int GetNextRoomRadius(void);
void Create_OpenRooms(game_state_t *state, int num_rooms_specified, int room_size);
void Create_ClosedRooms(game_state_t *state, int num_rooms_specified, int room_size);
void Create_RoomsFromFile(game_state_t *state, const char *filename);
void DefineOpenRoom(room_t *room, int room_size);
void DefineClosedRoom(room_t *room, coord_t pos, int radius);
void InstantiateClosedRoomRecursive(game_state_t *state, coord_t pos, int radius, int iterations, int max_rooms);
void GenerateRoom(tile_t **world_tiles, const room_t *room);
void GenerateCorridor(tile_t **world_tiles, coord_t starting_room, int corridor_size, direction_en direction);
void PopulateRooms(game_state_t *state);
coord_t GetRandRoomOpeningPos(const room_t *room);
bool CheckRoomCollision(const tile_t **world_tiles, const room_t *room);
bool CheckRoomWorldBounds(const room_t *room);
bool CheckWorldBounds(coord_t coord);
bool CheckCorridorCollision(const tile_t **world_tiles, coord_t starting_room, int corridor_size, direction_en direction);

void UpdateWorldTile(tile_t **world_tiles, coord_t pos, char sprite, tile_type_en type, int color, enemy_t *enemy_occupier, const item_t *item_occupier);
void PerformWorldLogic(game_state_t *state, const tile_t *curr_world_tile, coord_t player_old_pos);
void UpdateGameLog(log_list_t *game_log, const char *format, ...);
void NextPlayerInput(game_state_t *state);
void ApplyVision(const game_state_t *state, coord_t pos);
void EnemyCombatUpdate(game_state_t *state, enemy_node_t *enemy_list);
void DrawHelpScreen(void);
void DrawPlayerInfoScreen(const game_state_t *state);
void DrawDeathScreen(void);
void DrawMerchantScreen(game_state_t *state);
void SetPlayerPos(player_t *player, coord_t pos);
void InteractWithNPC(game_state_t *state, char npc_target);
int GetKeyInput(void);
int AddHealth(player_t *player, int amount);
bool AddToInventory(player_t *player, const item_t *item);

int WorldScreenWidth(void);
int WorldScreenHeight(void);

void Cleanup_DungeonFloor(game_state_t *state);
void Cleanup_GameState(game_state_t *state);

#endif /* ASCII_GAME_H_ */
