#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <complex.h>
#include <math.h>

// #define TRACELOG_DEBUG 1
#include <raylib.h>
#include <raymath.h>

#define NOB_IMPLEMENTATION
#include "nob.h"
#include "guppy.h"

#define FONT_SIZE_DEBUG 20
#define FONT_SIZE 64

#define COLOR_BACKGROUND WHITE
#define WINDOW_INIT_FACTOR 100
#define WINDOW_INIT_WIDTH WINDOW_INIT_FACTOR * 16
#define WINDOW_INIT_HEIGHT WINDOW_INIT_FACTOR * 9

// TODO: don't actually use these? just messing around with these numbers. Really
// there should be some sort of relation between the camera and the scene or something.
#define MAP_WIDTH 1280.0f
#define MAP_HEIGHT 720.0f
// TODO: I don't really understand how the math comes out to 60 x 43
// ...........good enough for now ¯\_(ツ)_/¯
#define MAP_ROWS 34 
#define MAP_COLS 60

#define CAMERA_ROTATION 0.0f
#define CAMERA_ZOOM 1.0f

#define MAP_SCALE 3.0f
#define MAP_CELL_SIZE 16.0f

#define PLANTS_SPRITE_SCALE 3.0f
// The "stride" is how wide a sprite is on the sprite sheet
#define PLANTS_SPRITE_SHEET_STRIDE 16.0f

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

#define INVENTORY_CAPACITY 8

#define ITEM_SPRITE_SCALE 75.0f
// The "stride" is how wide a sprite is on the sprite sheet
#define ITEM_SPRITE_SHEET_STRIDE 16.0f
#define ITEM_ID_HOE 0
#define ITEM_ID_WATERING_CAN 1

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
} Character;

Vector2 get_player_pos(Character player) {
    return (Vector2) {
        player.rect.x + (player.rect.width / 2),
        player.rect.y + player.rect.height - (player.rect.height / 4),
    };
}

Rectangle get_player_cell(Character player) {
    return (Rectangle) {
        get_player_pos(player).x - (float)((int)get_player_pos(player).x % (int)(MAP_CELL_SIZE * MAP_SCALE)),
        get_player_pos(player).y - (float)((int)get_player_pos(player).y % (int)(MAP_CELL_SIZE * MAP_SCALE)),
        MAP_CELL_SIZE * MAP_SCALE,
        MAP_CELL_SIZE * MAP_SCALE,
    };
}

Rectangle get_cell_rect_player_is_facing(Character player) {
    Rectangle result = get_player_cell(player);
    
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
    const Rectangle facing_cell_rect = get_cell_rect_player_is_facing(player);
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

bool is_cell_in_grid(Cell needle, Cell haystack, int cols, int rows) {
    if (cols == 0 || rows == 0) return false;
    if (needle.x < haystack.x || needle.y < haystack.y) return false;
    if (needle.x >= haystack.x + cols) return false;
    if (needle.y >= haystack.y + rows) return false;
    
    return true;
}

Cell cell_id_to_cell(const int cell_id) {
    return (Cell) { cell_id % MAP_COLS, cell_id / MAP_COLS };
}

int cell_to_cell_id(const Cell cell) {
    return cell.x + (cell.y * MAP_COLS);
}

bool player_is_facing_farmable_cell(Character player) {
    const Cell cell = get_cell_player_is_facing(player);

    // TODO: hardcoding these for now. Ideally we could somehow parse this from the tilemap.
    return (
        is_cell_in_grid(cell, cell_id_to_cell(793), 4, 4)  ||
        is_cell_in_grid(cell, cell_id_to_cell(799), 4, 4)  ||
        is_cell_in_grid(cell, cell_id_to_cell(805), 4, 4)  ||
        is_cell_in_grid(cell, cell_id_to_cell(1153), 4, 4) ||
        is_cell_in_grid(cell, cell_id_to_cell(1159), 4, 4) ||
        is_cell_in_grid(cell, cell_id_to_cell(1165), 4, 4) ||
        is_cell_in_grid(cell, cell_id_to_cell(1513), 4, 4) ||
        is_cell_in_grid(cell, cell_id_to_cell(1519), 4, 4) ||
        is_cell_in_grid(cell, cell_id_to_cell(1525), 4, 4)
    );
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_INIT_WIDTH, WINDOW_INIT_HEIGHT, "YAFS");
    SetTargetFPS(144);
    SetExitKey(KEY_ESCAPE);
    InitAudioDevice();
    SetTraceLogLevel(LOG_DEBUG);

    GameState game_state;
    Camera2D camera;

    Character player;
    Texture2D player_sprite_sheet;
    int player_sprite_sheet_row, player_sprite_sheet_col;
    
    Inventory inventory;
    Texture2D item_sprite_sheet;

    Texture2D map;
    Texture2D plants_sprite_sheet;
    GupArrayInt wet_cells = gup_array_int();
    GupArrayInt tilled_cells = gup_array_int();
    GupArrayDouble tilled_cell_planted_at = gup_array_double();

    { // Initialization
        item_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Objects/Basic_tools_and_materials.png");
        map = LoadTexture("resources/tilesets/map.png");
        plants_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Objects/Basic_Plants.png");
        player_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Characters/basic-character-spritesheet.png");

        player_sprite_sheet_row = 0;
        player_sprite_sheet_col = 0;

        game_state = (GameState) {
            .debug_mode = false,
            .paused = false,
        };

        player = (Character) {
            .rect = (Rectangle) {
                .x = vw(50),
                .y = vh(50),
                .width = PLAYER_WIDTH,
                .height = PLAYER_HEIGHT,
            }
        };

        Item hoe = { 
            .id = ITEM_ID_HOE,
            .name = "Hoe",
            .sprite_sheet_pos = (Vector2) { 32.0f, 0.0f },
        };
        Item watering_can = { 
            .id = ITEM_ID_WATERING_CAN,
            .name = "Watering Can",
            .sprite_sheet_pos = (Vector2) { 0.0f, 0.0f },
        };

        inventory = (Inventory) {
            .items = { hoe, watering_can },
            .selected_idx = 0,
            .rect = (Rectangle) {
                .x = vw(25.0f),
                .y = vh(85.0f),
                .width = vw(50.0f),
                .height = vh(10.0f),
            },
        };

        camera = (Camera2D) {
            .offset = { 0 },
            .rotation = CAMERA_ROTATION,
            .target = (Vector2) { player.rect.x, player.rect.y },
            .zoom = CAMERA_ZOOM,
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

            Vector2 new_pos = Vector2Scale(pos_diff_normalized, speed * GetFrameTime());
            player.rect.x += new_pos.x;
            player.rect.y += new_pos.y;
            
            // TODO: what the fuck is this, redo this man
            float new_cam_x = player.rect.x - ((float)GetScreenWidth()/2.0f) + (player.rect.width/2.0f);
            float new_cam_y = player.rect.y - ((float)GetScreenHeight()/2.0f) + (player.rect.height/2.0f);
            camera.target = (Vector2) { Clamp(new_cam_x, 0.0f, MAP_WIDTH), Clamp(new_cam_y, 0.0f, MAP_HEIGHT) };

            { // Items
                if (IsKeyPressed(KEY_ONE)) inventory.selected_idx   = 0;
                if (IsKeyPressed(KEY_TWO)) inventory.selected_idx   = 1;
                if (IsKeyPressed(KEY_THREE)) inventory.selected_idx = 2;
                if (IsKeyPressed(KEY_FOUR)) inventory.selected_idx  = 3;
                if (IsKeyPressed(KEY_FIVE)) inventory.selected_idx  = 4;

                if (IsKeyPressed(KEY_SPACE)) {
                    switch (inventory.items[inventory.selected_idx].id) {
                        case ITEM_ID_HOE: {
                            // TODO: animation

                            const int cell_id = get_cell_id_player_is_facing(player);
                            if (!player_is_facing_farmable_cell(player)) break;
                            if (gup_array_int_has(tilled_cells, cell_id)) break;

                            gup_array_int_append(&tilled_cells, cell_id);
                            gup_array_double_append(&tilled_cell_planted_at, GetTime());
                            if (game_state.debug_mode) {
                                gup_array_int_print(tilled_cells);
                                gup_array_double_print(tilled_cell_planted_at);
                            }

                            break;
                        }
                        case ITEM_ID_WATERING_CAN: {
                            // TODO: animation
                            const int cell_id = get_cell_id_player_is_facing(player);

                            if (!player_is_facing_farmable_cell(player)) break;
                            if (gup_array_int_has(wet_cells, cell_id)) break;

                            gup_array_int_append(&wet_cells, get_cell_id_player_is_facing(player));
                            if (game_state.debug_mode) gup_array_int_print(wet_cells);
                            break;
                        }
                        default: {
                            TraceLog(LOG_ERROR, "tried to use a non-existent item");
                            break;
                        }
                    }
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
                BeginMode2D(camera);

                // Draw map
                DrawTextureEx(map, (Vector2) { 0.0f, 0.0f }, 0.0f, MAP_SCALE, WHITE);

                // Draw wet cells
                for (int i = 0; i < wet_cells.count; i++) {
                    const float x = (wet_cells.data[i] % MAP_COLS) * (MAP_CELL_SIZE * MAP_SCALE);
                    const float y = (wet_cells.data[i] / MAP_COLS) * (MAP_CELL_SIZE * MAP_SCALE);
                    DrawRectangle(
                        x,
                        y,
                        MAP_CELL_SIZE * MAP_SCALE,
                        MAP_CELL_SIZE * MAP_SCALE,
                        (Color) { 0, 0, 64, 32 }
                    );
                }
                // Draw tilled cells
                for (int i = 0; i < tilled_cells.count; i++) {
                    const float x = (tilled_cells.data[i] % MAP_COLS) * (MAP_CELL_SIZE * MAP_SCALE);
                    const float y = (tilled_cells.data[i] / MAP_COLS) * (MAP_CELL_SIZE * MAP_SCALE);
                    float plant_sprite_sheet_x = 16.0f;
                    if (GetTime() - tilled_cell_planted_at.data[i] > 5.0) {
                        plant_sprite_sheet_x = 32.0f;
                    }
                    if (GetTime() - tilled_cell_planted_at.data[i] > 10.0) {
                        plant_sprite_sheet_x = 48.0f;
                    }
                    if (GetTime() - tilled_cell_planted_at.data[i] > 15.0) {
                        plant_sprite_sheet_x = 64.0f;
                    }
                    DrawTexturePro(
                        plants_sprite_sheet,
                        (Rectangle) { plant_sprite_sheet_x, 0.0f, PLANTS_SPRITE_SHEET_STRIDE, PLANTS_SPRITE_SHEET_STRIDE },
                        (Rectangle) { x, y, MAP_CELL_SIZE * MAP_SCALE, MAP_CELL_SIZE * MAP_SCALE },
                        (Vector2) { 0, 0 },
                        0.0f,
                        WHITE
                    );
                }

                // Draw game objects debug info
                if (game_state.debug_mode) {
                    // Draw world grid
                    for (int i = 0; i < MAP_WIDTH * MAP_SCALE; i += MAP_CELL_SIZE * MAP_SCALE) {
                        DrawLine(i, 0, i+1, MAP_HEIGHT * MAP_SCALE, PINK);
                        DrawLine(0, i, MAP_WIDTH * MAP_SCALE, i+1, PINK);
                    }

                    // Draw cell player is standing in
                    DrawRectangleRec(get_player_cell(player), (Color) { 230, 41, 55, 64 });
                    DrawRectangleLinesEx(player.rect, 1.0f, ORANGE);
                }

                // Draw player
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

                // Draw cell player is looking at
                DrawRectangleRec(get_cell_rect_player_is_facing(player), (Color) { 55, 41, 230, 64 });

                EndMode2D();
            }

            { // Draw UI
                { // Draw inventory
                    DrawTexturePro(
                        item_sprite_sheet,
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
                    DrawText(TextFormat("Player pos: (%d, %d)", (int)get_player_pos(player).x, (int)get_player_pos(player).y), 10, 30, FONT_SIZE_DEBUG, WHITE);
                    DrawRectangleLinesEx(inventory.rect, 1.0f, ORANGE);
                }
            }

            EndDrawing();
        }

    }
    
    gup_array_int_free(wet_cells);

    UnloadTexture(player_sprite_sheet);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
