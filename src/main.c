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
// #include "guppy.h"

#define GLSL_VERSION 330

#define N (1<<13)
#define FONT_SIZE_DEBUG 20
#define FONT_SIZE 64

#define COLOR_BACKGROUND WHITE
#define WINDOW_INIT_FACTOR 100
#define WINDOW_INIT_WIDTH WINDOW_INIT_FACTOR * 16
#define WINDOW_INIT_HEIGHT WINDOW_INIT_FACTOR * 9

// TODO: don't actually use these? just testing
#define MAP_WIDTH 1280.0f
#define MAP_HEIGHT 720.0f

#define CAMERA_ROTATION 0.0f
#define CAMERA_ZOOM 1.0f

#define MAP_SCALE 3.0f
#define MAP_CELL_SIZE 16.0f

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

Rectangle get_player_cell_is_facing(Character player) {
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

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_INIT_WIDTH, WINDOW_INIT_HEIGHT, "YAFS");
    SetTargetFPS(144);
    SetExitKey(KEY_ESCAPE);
    InitAudioDevice();
    SetTraceLogLevel(LOG_ALL);

    GameState game_state;
    Camera2D camera;

    Character player;
    Texture2D player_sprite_sheet;
    int sprite_sheet_row, sprite_sheet_col;
    
    Inventory inventory;
    Texture2D item_sprite_sheet;

    Texture2D map;

    { // Initialization
        item_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Objects/Basic_tools_and_materials.png");
        map = LoadTexture("resources/tilesets/map.png");
        player_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Characters/basic-character-spritesheet.png");

        sprite_sheet_row = 0;
        sprite_sheet_col = 0;

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
                if (IsKeyDown(KEY_LEFT)) pos_diff.x--;
                if (IsKeyDown(KEY_RIGHT)) pos_diff.x++;
                if (IsKeyDown(KEY_UP)) pos_diff.y--;
                if (IsKeyDown(KEY_DOWN)) pos_diff.y++;

                pos_diff_normalized = Vector2Normalize(pos_diff);
            }

            const bool is_idle = Vector2Length(pos_diff_normalized) == 0.0f;
            const bool is_running = !is_idle && IsKeyDown(KEY_LEFT_SHIFT);
            const float speed = is_running ? PLAYER_RUNNING_SPEED : PLAYER_WALKING_SPEED;

            Vector2 new_pos = Vector2Scale(pos_diff_normalized, speed * GetFrameTime());
            player.rect.x += new_pos.x;
            player.rect.y += new_pos.y;
            
            // TODO: what the fuck
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
                            TraceLog(LOG_DEBUG, "used hoe!");
                            break;
                        }
                        case ITEM_ID_WATERING_CAN: {
                            TraceLog(LOG_DEBUG, "used watering can!");
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
                    sprite_sheet_row = PLAYER_SPRITE_SHEET_UP_ROW;
                    player.dir = UP;
                } else if (IsKeyDown(KEY_DOWN) || (IsKeyDown(KEY_LEFT) && IsKeyDown(KEY_RIGHT))) {
                    sprite_sheet_row = PLAYER_SPRITE_SHEET_DOWN_ROW;
                    player.dir = DOWN;
                } else if (IsKeyDown(KEY_LEFT)) {
                    sprite_sheet_row = PLAYER_SPRITE_SHEET_LEFT_ROW;
                    player.dir = LEFT;
                } else if (IsKeyDown(KEY_RIGHT)) {
                    sprite_sheet_row = PLAYER_SPRITE_SHEET_RIGHT_ROW;
                    player.dir = RIGHT;
                }

                // Movement animation frames
                const int now_in_millis = (int)(GetTime() * 1000.0f);
                const int anim_millis = is_running ? PLAYER_RUNNING_SPEED_ANIM_MILLIS : PLAYER_WALKING_SPEED_ANIM_MILLIS;
                if (is_idle) {
                    if (now_in_millis % 2000 < 1500) {
                        sprite_sheet_col = 0;
                    } else {
                        sprite_sheet_col = 1;
                    }
                } else {
                    if (now_in_millis % (anim_millis*2) < anim_millis) {
                        sprite_sheet_col = 2;
                    } else {
                        sprite_sheet_col = 3;
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

                // Draw player
                DrawTexturePro(
                    player_sprite_sheet,
                    (Rectangle) {
                        sprite_sheet_col * PLAYER_SPRITE_SHEET_STRIDE,
                        sprite_sheet_row * PLAYER_SPRITE_SHEET_STRIDE,
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

                // Draw cell that player is on
                // DrawRectangleLinesEx()

                // Draw game objects debug info
                if (game_state.debug_mode) {
                    for (int i = 0; i < MAP_WIDTH * MAP_SCALE; i += MAP_CELL_SIZE * MAP_SCALE) {
                        DrawLine(i, 0, i+1, MAP_HEIGHT * MAP_SCALE, PINK);
                        DrawLine(0, i, MAP_WIDTH * MAP_SCALE, i+1, PINK);
                    }
                    // Draw cell player is standing in
                    DrawRectangleRec(get_player_cell(player), (Color) { 230, 41, 55, 64 });
                    // Draw cell player is looking at
                    DrawRectangleRec(get_player_cell_is_facing(player), (Color) { 55, 41, 230, 64 });
                    DrawRectangleLinesEx(player.rect, 1.0f, ORANGE);
                }

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
    
    UnloadTexture(player_sprite_sheet);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
