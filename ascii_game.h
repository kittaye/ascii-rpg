// Colours.
#define CLR_WHITE (0)
#define CLR_YELLOW (1)
#define CLR_RED (2)
#define CLR_BLUE (3)
#define CLR_MAGENTA (4)
#define CLR_CYAN (5)
#define CLR_GREEN (6)

// Sprites.
#define SPR_EMPTY ' '
#define SPR_PLAYER '@'
#define SPR_GOLD 'g'
#define SPR_BIGGOLD 'G'
#define SPR_WALL '#'
#define SPR_STAIRCASE '^'
#define SPR_FOOD 'f'
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

// NOTE: These min values should be the largest txt dimension size of a dungeon layout from a file at compile time!
#define MIN_SCREEN_WIDTH 85	
#define MIN_SCREEN_HEIGHT 56

typedef enum tile_type_en {
	T_Npc,
	T_Enemy,
	T_Solid,
	T_Special,
	T_Item,
	T_Empty
} tile_type_en;

typedef enum direction_en {
	D_Up = 0,
	D_Down = 1,
	D_Left = 2,
	D_Right = 3
} direction_en;

typedef enum item_slug_en {
	I_None,
	I_Map,
	I_Food1,
	I_Food2
} item_slug_en;

typedef struct item_t {
	char *name;
	item_slug_en item_slug;
	int value;
} item_t;

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
	item_t inventory[INVENTORY_SIZE];
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

typedef struct item_node_t {
	item_t *item;
	struct item_node_t *next;
} item_node_t;

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

	item_t item_empty;				// Global data for an empty item.
	item_t item_food1;				// Global data for a small food item.
	item_t item_food2;				// Global data for a big food item.

	entity_node_t *enemy_list;		// Linked list of all enemies created in game.
	item_node_t *item_list;			// Linked list of all items created in game.

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
void AddToItemList(item_node_t**, item_t*);
void EnemyCombatUpdate(game_state_t*, entity_node_t*);
void DrawHelpScreen();
void DrawPlayerInfoScreen(const game_state_t*);
void DrawDeathScreen();
void DrawMerchantScreen(game_state_t *);
void SetPlayerPos(player_t*, coord_t);
coord_t NewCoord(int, int);
item_t NewItem(char*, item_slug_en, int);
bool FContainsChar(FILE*, char);
void InteractWithNPC(game_state_t*, char);
int GetKeyInput();
void AddHealth(player_t*, int);
bool AddToInventory(player_t*, const item_t*);

void Cleanup_GameState(game_state_t*);
void FreeEnemyList(entity_node_t**);
void FreeItemList(item_node_t**);
