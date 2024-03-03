#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <complex.h>
#include <math.h>
#include <expat.h>
#include <raylib.h>
#include <raymath.h>

#define NOB_IMPLEMENTATION
#include "nob.h"
#include "guppy.h"

#define FONT_SIZE_DEBUG 20
#define FONT_SIZE 64

#define MAP_SCALE 3.0f
#define MAP_CELL_SIZE 16.0f
#define MAP_COLS 30
#define MAP_ROWS 24 
#define MAP_WIDTH (float)MAP_COLS * MAP_CELL_SIZE * MAP_SCALE
#define MAP_HEIGHT (float)MAP_ROWS * MAP_CELL_SIZE * MAP_SCALE

#define COLOR_BACKGROUND WHITE
#define WINDOW_INIT_WIDTH MAP_WIDTH
#define WINDOW_INIT_HEIGHT MAP_HEIGHT


#define PLANTS_SPRITE_SCALE 3.0f
// The "stride" is how wide a sprite is on the sprite sheet
#define PLANTS_SPRITE_SHEET_STRIDE 16.0f

#define CHICKEN_SPRITE_SHEET_STRIDE 16.0f
#define CHICKEN_WALKING_SPEED 50.0f

#define PLAYER_SPRITE_SCALE 3.0f
#define PLAYER_WIDTH 64.0f
#define PLAYER_HEIGHT 64.0f
// The "stride" is how wide a sprite is on the sprite sheet
#define PLAYER_SPRITE_SHEET_STRIDE 48.0f
#define PLAYER_SPRITE_SHEET_DOWN_ROW 0
#define PLAYER_SPRITE_SHEET_UP_ROW 1
#define PLAYER_SPRITE_SHEET_LEFT_ROW 2
#define PLAYER_SPRITE_SHEET_RIGHT_ROW 3
#define PLAYER_WALKING_SPEED 300.0f
#define PLAYER_RUNNING_SPEED 500.0f
#define PLAYER_WALKING_SPEED_ANIM_MILLIS 250
#define PLAYER_RUNNING_SPEED_ANIM_MILLIS 100

#define TOOL_ANIM_SPRITE_SHEET_STRIDE 16.0f

#define WHEAT_FLOAT_SPEED 50.0f
#define WHEAT_FADE_SPEED 100.0f

#define INVENTORY_CAPACITY 5

#define ITEM_SPRITE_SCALE 100.0f
// The "stride" is how wide a sprite is on the sprite sheet
#define ITEM_SPRITE_SHEET_STRIDE 16.0f
#define ITEM_ID_SEEDS 0
#define ITEM_ID_WATERING_CAN 1
#define ITEM_ID_SCYTHE 2

// XML ---------------------------------------------------------------------------------------------

void xml_start(void *data, const char *el, const char **attr) {
    Rectangle *collision = (Rectangle *)data;
    
    if (strcmp(el, "object") == 0) {
        attr += 2;
        collision->x = (float)atof(attr[1]) * MAP_SCALE;

        attr += 2;
        collision->y = (float)atof(attr[1]) * MAP_SCALE;

        attr += 2;
        collision->width = (float)atof(attr[1]) * MAP_SCALE;

        attr += 2;
        collision->height = (float)atof(attr[1]) * MAP_SCALE;
    }
}

void xml_end(void *data, const char *el) {
    // noop
}

void parse_collision(Rectangle *collision) {
    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetElementHandler(parser, xml_start, xml_end);
    XML_SetUserData(parser, collision);

    char *xml = gup_file_read_as_cstr("resources/tilesets/map2.tmx");

    int done = 1;
    if (XML_Parse(parser, xml, strlen(xml), done) == XML_STATUS_ERROR) {
        printf("Error: %s\n", XML_ErrorString(XML_GetErrorCode(parser)));
    }

    XML_ParserFree(parser);
    free(xml);
}


// CSS-like helpers --------------------------------------------------------------------------------

float vh(float vh) {
    return GetRenderHeight() * vh * 0.01;
}

float vw(float vw) {
    return GetRenderWidth() * vw * 0.01;
}

float x_center(float width) {
    return GetRenderWidth() / 2 - width / 2;
}

float y_center(float height) {
    return GetRenderHeight() / 2 - height / 2;
}

float top() {
    return 0;
}

float right() {
    return GetRenderWidth();
}

float bottom() {
    return GetRenderHeight();
}

float left() {
    return 0;
}

typedef struct GameState {
    bool debug_mode;
    bool paused;
} GameState;

typedef struct Cell {
    int x; // col
    int y; // row
    double plantedAt;
    double wettedAt;
} Cell;

typedef struct Item {
    int id;
    char name[256];
    Vector2 sprite_sheet_pos;
} Item;

typedef struct Inventory {
    Item items[INVENTORY_CAPACITY];
    int selected_idx;
    Rectangle rect;
} Inventory;

typedef enum {
    UP = 0,
    DOWN,
    LEFT,
    RIGHT
} Direction;

typedef struct Character {
    Rectangle rect;
    Direction dir;
    double wheat_harvested_at;
    double swung_scythe_at;
} Character;

Vector2 get_character_pos(Character player) {
    return (Vector2) {
        player.rect.x + (player.rect.width / 2),
        // The feet are closer to the bottom of the sprite than the middle. So only chop off a
        // quarter of the sprite, not half like a first impression may elicit.
        player.rect.y + player.rect.height - (player.rect.height / 4),
    };
}

Rectangle get_character_cell_rect(Character player) {
    return (Rectangle) {
        get_character_pos(player).x - (float)((int)get_character_pos(player).x % (int)(MAP_CELL_SIZE * MAP_SCALE)),
        get_character_pos(player).y - (float)((int)get_character_pos(player).y % (int)(MAP_CELL_SIZE * MAP_SCALE)),
        MAP_CELL_SIZE * MAP_SCALE,
        MAP_CELL_SIZE * MAP_SCALE,
    };
}

Rectangle get_cell_rect_character_is_facing(Character player) {
    Rectangle result = get_character_cell_rect(player);
    
    switch (player.dir) {
        case UP: {
            result.y -= (MAP_CELL_SIZE * MAP_SCALE);      
            break;
        }
        case DOWN: {
            result.y += (MAP_CELL_SIZE * MAP_SCALE);  
            break;
        }
        case LEFT: {
            result.x -= (MAP_CELL_SIZE * MAP_SCALE);  
            break;
        }
        case RIGHT: {
            result.x += (MAP_CELL_SIZE * MAP_SCALE);  
            break;
        }
    }

    return result;
}

void print_cell(const Cell cell) {
    TraceLog(LOG_DEBUG, TextFormat("cell: (%d, %d)", cell.x, cell.y));
}

Cell get_cell_player_is_facing(Character player) {
    const Rectangle facing_cell_rect = get_cell_rect_character_is_facing(player);
    const Cell result = {
        (int)facing_cell_rect.x / (int)(MAP_CELL_SIZE * MAP_SCALE),
        (int)facing_cell_rect.y / (int)(MAP_CELL_SIZE * MAP_SCALE)
    };
    return result;
}

int get_cell_id_player_is_facing(Character player) {
    const Cell facing_cell = get_cell_player_is_facing(player);
    return facing_cell.x + (facing_cell.y * MAP_COLS);
}

bool is_cell_in_area(Cell needle, Cell haystack, int cols, int rows) {
    if (cols == 0 || rows == 0) return false;
    if (needle.x < haystack.x || needle.y < haystack.y) return false;
    if (needle.x >= haystack.x + cols) return false;
    if (needle.y >= haystack.y + rows) return false;
    
    return true;
}

static Cell cells[MAP_COLS * MAP_ROWS];

Cell cell_id_to_cell(const int cell_id) {
    // TODO: cells has to be moved to static and it's real bad
    return cells[cell_id];
}

int cell_to_cell_id(const Cell cell) {
    return cell.x + (cell.y * MAP_COLS);
}

bool player_is_facing_farmable_cell(Character player) {
    const int cell_id = get_cell_id_player_is_facing(player);

    // TODO: hardcoding these for now. Ideally we could somehow parse this from the tilemap,
    // or do literally anything smarter than this.
    return (
        cell_id == 280 || cell_id == 281 || cell_id == 282 || cell_id == 283 ||
        cell_id == 310 || cell_id == 311 || cell_id == 312 || cell_id == 313 ||
        cell_id == 340 || cell_id == 341 || cell_id == 342 || cell_id == 343 ||
        cell_id == 370 || cell_id == 371 || cell_id == 372 || cell_id == 373 ||

        cell_id == 286 || cell_id == 287 || cell_id == 288 || cell_id == 289 ||
        cell_id == 316 || cell_id == 317 || cell_id == 318 || cell_id == 319 ||
        cell_id == 346 || cell_id == 347 || cell_id == 348 || cell_id == 349 ||
        cell_id == 376 || cell_id == 377 || cell_id == 378 || cell_id == 379 ||

        cell_id == 460 || cell_id == 461 || cell_id == 462 || cell_id == 463 ||
        cell_id == 490 || cell_id == 491 || cell_id == 492 || cell_id == 493 ||
        cell_id == 520 || cell_id == 521 || cell_id == 522 || cell_id == 523 ||
        cell_id == 550 || cell_id == 551 || cell_id == 552 || cell_id == 553 ||

        cell_id == 466 || cell_id == 467 || cell_id == 468 || cell_id == 469 ||
        cell_id == 496 || cell_id == 497 || cell_id == 498 || cell_id == 499 ||
        cell_id == 526 || cell_id == 527 || cell_id == 528 || cell_id == 529 ||
        cell_id == 556 || cell_id == 557 || cell_id == 558 || cell_id == 559
    );
}

bool is_cell_full_grown(Cell cell) {
    return GetTime() - cell.plantedAt > 3.0;
}

int main(void) {
    // SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_INIT_WIDTH, WINDOW_INIT_HEIGHT, "YAFS");
    SetTargetFPS(144);
    SetExitKey(KEY_ESCAPE);
    InitAudioDevice();
    SetTraceLogLevel(LOG_DEBUG);

    GameState game_state;

    Character chicken;
    Character player;
    Texture2D player_sprite_sheet;
    int player_sprite_sheet_row, player_sprite_sheet_col;
    
    Inventory inventory;
    Texture2D item_sprite_sheet;

    Texture2D map;
    Rectangle collision;

    Texture2D plants_sprite_sheet;
    Texture2D chicken_sprite_sheet;
    Texture2D tool_anim_sprite_sheet;

    { // Initialization
        parse_collision(&collision);
        TraceLog(LOG_DEBUG, TextFormat("rect: {.x = %f, .y = %f, .width = %f, .height = %f }\n", collision.x, collision.y, collision.width, collision.height));

        item_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Objects/Basic_tools_and_materials.png");
        map = LoadTexture("resources/tilesets/map2.png");
        plants_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Objects/Basic_Plants.png");
        player_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Characters/basic-character-spritesheet.png");
        tool_anim_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Characters/Tools.png");
        chicken_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Characters/free-chicken-sprites.png");

        player_sprite_sheet_row = 0;
        player_sprite_sheet_col = 0;

        game_state = (GameState) {
            .debug_mode = false,
            .paused = false,
        };

        for (int i = 0; i < MAP_COLS * MAP_ROWS; i++) {
            cells[i] = (Cell) {
                .x = (i % MAP_COLS) * MAP_CELL_SIZE * MAP_SCALE,
                .y = (i / MAP_COLS) * MAP_CELL_SIZE * MAP_SCALE,
                .plantedAt = 0,
                .wettedAt = 0,
            };
        }

        player = (Character) {
            .rect = (Rectangle) {
                .x = vw(50),
                .y = vh(50),
                .width = PLAYER_WIDTH,
                .height = PLAYER_HEIGHT,
            },
            .wheat_harvested_at = 0,
        };

        chicken = (Character) {
            .rect = (Rectangle) {
                .x = vw(25),
                .y = vh(25),
                .width = PLAYER_WIDTH,
                .height = PLAYER_HEIGHT,
            }
        };

        Item seeds = { 
            .id = ITEM_ID_SEEDS,
            .name = "Seeds",
            .sprite_sheet_pos = (Vector2) { 0.0f, 0.0f },
        };
        Item watering_can = { 
            .id = ITEM_ID_WATERING_CAN,
            .name = "Watering Can",
            .sprite_sheet_pos = (Vector2) { 0.0f, 0.0f },
        };
        Item scythe = { 
            .id = ITEM_ID_SCYTHE,
            .name = "Scythe",
            .sprite_sheet_pos = (Vector2) { 32.0f, 0.0f },
        };

        inventory = (Inventory) {
            .items = { seeds, watering_can, scythe },
            .selected_idx = 0,
            .rect = (Rectangle) {
                .x = vw(25.0f),
                .y = vh(85.0f),
                .width = vw(50.0f),
                .height = vh(10.0f),
            },
        };
    }

    while (!WindowShouldClose()) {
        { // Update
            { // Stuff that should be done regardless of pause state. 
                if (IsKeyPressed(KEY_F1)) {
                    game_state.debug_mode = !game_state.debug_mode;
                }

                if (IsKeyPressed(KEY_TAB)) {
                    game_state.paused = !game_state.paused;
                }
            }

            // The rest of the update should not happen if the game is paused.
            // So, if we're paused at this point just jump to drawing.
            if (game_state.paused) goto draw;

            Vector2 pos_diff_normalized = { 0 };
            { // Movement
                Vector2 pos_diff = { 0 };
                if (IsKeyDown(KEY_LEFT))  pos_diff.x--;
                if (IsKeyDown(KEY_RIGHT)) pos_diff.x++;
                if (IsKeyDown(KEY_UP))    pos_diff.y--;
                if (IsKeyDown(KEY_DOWN))  pos_diff.y++;

                pos_diff_normalized = Vector2Normalize(pos_diff);
            }

            const bool is_idle = Vector2Length(pos_diff_normalized) == 0.0f;
            const bool is_running = !is_idle && IsKeyDown(KEY_LEFT_SHIFT);
            const float speed = is_running ? PLAYER_RUNNING_SPEED : PLAYER_WALKING_SPEED;

            Vector2 scaled_pos_diff = Vector2Scale(pos_diff_normalized, speed * GetFrameTime());

            // Check what the new player rect would be if we were to move as much as
            // the new position would like us to. We will then check if this is possible.
            Rectangle hypothetical_player_rect = {
                .x = player.rect.x + scaled_pos_diff.x,
                .y = player.rect.y + scaled_pos_diff.y,
                .width = player.rect.width,
                .height = player.rect.height,
            };

            if (!CheckCollisionRecs(hypothetical_player_rect, collision)) {
                // TODO: this probably isn't right i guess id need to cehck both axes?
                player.rect = hypothetical_player_rect;
            }

            { // Items
                if (IsKeyPressed(KEY_ONE)) inventory.selected_idx   = 0;
                if (IsKeyPressed(KEY_TWO)) inventory.selected_idx   = 1;
                if (IsKeyPressed(KEY_THREE)) inventory.selected_idx = 2;
                if (IsKeyPressed(KEY_FOUR)) inventory.selected_idx  = 3;
                if (IsKeyPressed(KEY_FIVE)) inventory.selected_idx  = 4;

                if (IsKeyPressed(KEY_SPACE)) {
                    switch (inventory.items[inventory.selected_idx].id) {
                        case ITEM_ID_SEEDS: {
                            const int id = get_cell_id_player_is_facing(player);
                            if (!player_is_facing_farmable_cell(player)) break;
                            if (cells[id].plantedAt > 0) break;

                            cells[id].plantedAt = GetTime();
                            break;
                        }
                        case ITEM_ID_WATERING_CAN: {
                            // TODO: animation

                            const int id = get_cell_id_player_is_facing(player);
                            if (!player_is_facing_farmable_cell(player)) break;
                            if (cells[id].wettedAt > 0) break;

                            cells[id].wettedAt = GetTime();
                            break;
                        }
                        case ITEM_ID_SCYTHE: {
                            // Play animation every time.
                            player.swung_scythe_at = GetTime();
                            
                            const int id = get_cell_id_player_is_facing(player);
                            if (cells[id].plantedAt == 0) break;

                            if (is_cell_full_grown(cells[id])) {
                                player.wheat_harvested_at = GetTime();
                                cells[id].plantedAt = 0;
                            }
                            
                            break;
                        }
                        default: {
                            TraceLog(LOG_ERROR, "tried to use a non-existent item");
                            break;
                        }
                    }
                }
            }

            { // Timers
                if (GetTime() - player.wheat_harvested_at > 1) {
                    player.wheat_harvested_at = 0;
                }

                if ((GetTime() - player.swung_scythe_at) * 1000 > 500) {
                    player.swung_scythe_at = 0;
                }
            }

            { // Animation
                if (IsKeyDown(KEY_UP)) {
                    player_sprite_sheet_row = PLAYER_SPRITE_SHEET_UP_ROW;
                    player.dir = UP;
                } else if (IsKeyDown(KEY_DOWN) || (IsKeyDown(KEY_LEFT) && IsKeyDown(KEY_RIGHT))) {
                    player_sprite_sheet_row = PLAYER_SPRITE_SHEET_DOWN_ROW;
                    player.dir = DOWN;
                } else if (IsKeyDown(KEY_LEFT)) {
                    player_sprite_sheet_row = PLAYER_SPRITE_SHEET_LEFT_ROW;
                    player.dir = LEFT;
                } else if (IsKeyDown(KEY_RIGHT)) {
                    player_sprite_sheet_row = PLAYER_SPRITE_SHEET_RIGHT_ROW;
                    player.dir = RIGHT;
                }

                // Movement animation frames
                const int now_in_millis = (int)(GetTime() * 1000.0f);
                const int anim_millis = is_running ? PLAYER_RUNNING_SPEED_ANIM_MILLIS : PLAYER_WALKING_SPEED_ANIM_MILLIS;
                if (is_idle) {
                    if (now_in_millis % 2000 < 1500) {
                        player_sprite_sheet_col = 0;
                    } else {
                        player_sprite_sheet_col = 1;
                    }
                } else {
                    if (now_in_millis % (anim_millis*2) < anim_millis) {
                        player_sprite_sheet_col = 2;
                    } else {
                        player_sprite_sheet_col = 3;
                    }
                }
            }
        }

        { // Draw
draw:       BeginDrawing();
            ClearBackground(COLOR_BACKGROUND);

            { // Draw world objects
                // Draw map
                DrawTextureEx(map, (Vector2) { 0.0f, 0.0f }, 0.0f, MAP_SCALE, WHITE);

                // Draw additions to cells
                // TODO: this is relatively slow because we have to go through all of the cells
                // every frame. What would be better is if we could know beforehand which cells
                // needed to be drawn.
                for (int i = 0; i < MAP_COLS * MAP_ROWS; i++) {
                    // Draw planted cells
                    if (cells[i].plantedAt > 0) {
                        float plant_sprite_sheet_x = 16.0f;
                        if (GetTime() - cells[i].plantedAt > 1.0) {
                            plant_sprite_sheet_x = 32.0f;
                        }
                        if (GetTime() - cells[i].plantedAt > 2.0) {
                            plant_sprite_sheet_x = 48.0f;
                        }
                        if (GetTime() - cells[i].plantedAt > 3.0) {
                            plant_sprite_sheet_x = 64.0f;
                        }
                        DrawTexturePro(
                            plants_sprite_sheet,
                            (Rectangle) { plant_sprite_sheet_x, 0.0f, PLANTS_SPRITE_SHEET_STRIDE, PLANTS_SPRITE_SHEET_STRIDE },
                            (Rectangle) { cells[i].x, cells[i].y, MAP_CELL_SIZE * MAP_SCALE, MAP_CELL_SIZE * MAP_SCALE },
                            (Vector2) { 0, 0 },
                            0.0f,
                            WHITE
                        );
                    }

                    if (cells[i].wettedAt > 0) {
                        DrawRectangle(
                            cells[i].x,
                            cells[i].y,
                            MAP_CELL_SIZE * MAP_SCALE,
                            MAP_CELL_SIZE * MAP_SCALE,
                            (Color) { 0, 0, 64, 32 }
                        );
                    }
                }

                // Draw game objects debug info
                if (game_state.debug_mode) {
                    // Draw world grid
                    for (int i = 0; i < MAP_WIDTH * MAP_SCALE; i += MAP_CELL_SIZE * MAP_SCALE) {
                        DrawLine(i, 0, i+1, MAP_HEIGHT * MAP_SCALE, PINK);
                        DrawLine(0, i, MAP_WIDTH * MAP_SCALE, i+1, PINK);
                    }

                    // Draw cell player is standing in
                    DrawRectangleRec(get_character_cell_rect(player), (Color) { 230, 41, 55, 64 });
                    DrawRectangleLinesEx(player.rect, 1.0f, ORANGE);
                }

                { // Draw player
                    DrawTexturePro(
                        player_sprite_sheet,
                        (Rectangle) {
                            player_sprite_sheet_col * PLAYER_SPRITE_SHEET_STRIDE,
                            player_sprite_sheet_row * PLAYER_SPRITE_SHEET_STRIDE,
                            PLAYER_SPRITE_SHEET_STRIDE,
                            PLAYER_SPRITE_SHEET_STRIDE,
                        },
                        (Rectangle) {
                            player.rect.x,
                            player.rect.y,
                            PLAYER_WIDTH * PLAYER_SPRITE_SCALE,
                            PLAYER_HEIGHT * PLAYER_SPRITE_SCALE,
                        },
                        (Vector2) { PLAYER_WIDTH, PLAYER_HEIGHT },
                        0.0f,
                        WHITE
                    );

                    // Draw wheat above head if you just harvested some.
                    if (GetTime() > 1 && GetTime() - player.wheat_harvested_at < 1.0) {
                        const float wheatTimeAlive = (GetTime() - player.wheat_harvested_at) * WHEAT_FLOAT_SPEED;
                        const unsigned char wheatAlpha = (int)(wheatTimeAlive * wheatTimeAlive / 5) <= 255
                            ? (unsigned char)(wheatTimeAlive * wheatTimeAlive / 5) 
                            : 255;

                        DrawTexturePro(
                            plants_sprite_sheet,
                            (Rectangle) {
                                5 * PLANTS_SPRITE_SHEET_STRIDE,
                                0,
                                PLANTS_SPRITE_SHEET_STRIDE,
                                PLANTS_SPRITE_SHEET_STRIDE,
                            },
                            (Rectangle) {
                                player.rect.x,
                                player.rect.y - (player.rect.height / 2) - wheatTimeAlive,
                                player.rect.width,
                                player.rect.height,
                            },
                            (Vector2) { 0.0f, 0.0f },
                            0.0f,
                            (Color) { 255, 255, 255, 255 - wheatAlpha }
                        );
                    }
                }

                // Draw tool in hand
                const int now_in_millis = (int)(GetTime() * 1000.0f);
                float tool_anim_sprite_sheet_col = 0.0f;
                // TODO: lots of magic numbers here. Potential to DRY this up.
                switch (player.dir) {
                    case UP: {
                        break;
                    }

                    case DOWN: {
                        // Intentional fall through.
                    }

                    case LEFT: {
                        if (now_in_millis % 500 < 125) {
                            tool_anim_sprite_sheet_col = 2.0f;
                        } else if (now_in_millis % 500 < 250) {
                            tool_anim_sprite_sheet_col = 1.0f;
                        } else {
                            tool_anim_sprite_sheet_col = 0.0f;
                        }
                        break;
                    }

                    case RIGHT: {
                        if (now_in_millis % 500 < 125) {
                            tool_anim_sprite_sheet_col = 3.0f;
                        } else if (now_in_millis % 500 < 250) {
                            tool_anim_sprite_sheet_col = 4.0f;
                        } else {
                            tool_anim_sprite_sheet_col = 5.0f;
                        }
                        break;
                    }
                }
                if (player.dir != UP && inventory.selected_idx == ITEM_ID_SCYTHE && player.swung_scythe_at != 0) {
                    DrawTexturePro(
                        tool_anim_sprite_sheet,
                        (Rectangle) {
                            tool_anim_sprite_sheet_col * TOOL_ANIM_SPRITE_SHEET_STRIDE,
                            5 * TOOL_ANIM_SPRITE_SHEET_STRIDE,
                            TOOL_ANIM_SPRITE_SHEET_STRIDE,
                            TOOL_ANIM_SPRITE_SHEET_STRIDE,
                        },
                        player.rect,
                        (Vector2) { 0 },
                        0.0f,
                        WHITE
                    );
                }

                // Draw cell player is looking at
                DrawRectangleRec(get_cell_rect_character_is_facing(player), (Color) { 55, 41, 230, 64 });
            
                { // Draw chicken
                    DrawTexturePro(
                        chicken_sprite_sheet,
                        (Rectangle) {
                            CHICKEN_SPRITE_SHEET_STRIDE,
                            CHICKEN_SPRITE_SHEET_STRIDE,
                            CHICKEN_SPRITE_SHEET_STRIDE,
                            CHICKEN_SPRITE_SHEET_STRIDE,
                        },
                        (Rectangle) {
                            chicken.rect.x,
                            chicken.rect.y,
                            PLAYER_WIDTH,
                            PLAYER_HEIGHT,
                        },
                        (Vector2) { 0.0f, 0.0f },
                        0.0f,
                        WHITE
                    );
                }
            }

            { // Draw UI
                { // Draw inventory
                    DrawTexturePro(
                        plants_sprite_sheet,
                        (Rectangle) {
                            inventory.items[0].sprite_sheet_pos.x,
                            inventory.items[0].sprite_sheet_pos.y,
                            ITEM_SPRITE_SHEET_STRIDE,
                            ITEM_SPRITE_SHEET_STRIDE,
                        },
                        (Rectangle) {
                            inventory.rect.x,
                            inventory.rect.y,
                            ITEM_SPRITE_SCALE,
                            ITEM_SPRITE_SCALE,
                        },
                        (Vector2) { 0 },
                        0.0f,
                        WHITE
                    );

                    // Draw watering can in inventory
                    DrawTexturePro(
                        item_sprite_sheet,
                        (Rectangle) {
                            inventory.items[1].sprite_sheet_pos.x,
                            inventory.items[1].sprite_sheet_pos.y,
                            ITEM_SPRITE_SHEET_STRIDE,
                            ITEM_SPRITE_SHEET_STRIDE,
                        },
                        (Rectangle) {
                            inventory.rect.x + (inventory.rect.width / INVENTORY_CAPACITY),
                            inventory.rect.y,
                            ITEM_SPRITE_SCALE,
                            ITEM_SPRITE_SCALE,
                        },
                        (Vector2) { 0 },
                        0.0f,
                        WHITE
                    );

                    // Draw scythe
                    DrawTexturePro(
                        item_sprite_sheet,
                        (Rectangle) {
                            inventory.items[2].sprite_sheet_pos.x,
                            inventory.items[2].sprite_sheet_pos.y,
                            ITEM_SPRITE_SHEET_STRIDE,
                            ITEM_SPRITE_SHEET_STRIDE,
                        },
                        (Rectangle) {
                            inventory.rect.x + (inventory.rect.width / INVENTORY_CAPACITY * 2),
                            inventory.rect.y,
                            ITEM_SPRITE_SCALE,
                            ITEM_SPRITE_SCALE,
                        },
                        (Vector2) { 0 },
                        0.0f,
                        WHITE
                    );
                    
                    // Draw selected item in inventory
                    DrawRectangleLinesEx(
                        (Rectangle) {
                            inventory.rect.x + ((inventory.rect.width / INVENTORY_CAPACITY) * inventory.selected_idx),
                            inventory.rect.y,
                            inventory.rect.height,
                            inventory.rect.height,
                        },
                        4.0f,
                        WHITE
                    );
                }

                // Draw UI debug stuff
                if (game_state.debug_mode) { 
                    DrawFPS(10, 10);
                    DrawText(TextFormat("Player pos: (%d, %d)", (int)get_character_pos(player).x, (int)get_character_pos(player).y), 10, 30, FONT_SIZE_DEBUG, WHITE);
                    DrawRectangleLinesEx(inventory.rect, 1.0f, ORANGE);
                    DrawRectangleLinesEx(collision, 1.0f, ORANGE);
                }
            }

            EndDrawing();
        }

    }
    
    UnloadTexture(player_sprite_sheet);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
