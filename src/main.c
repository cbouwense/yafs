#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <complex.h>
#include <math.h>

#include <raylib.h>
#include <raymath.h>

#define NOB_IMPLEMENTATION
#include "nob.h"
// #include "guppy.h"

#define GLSL_VERSION 330

#define N (1<<13)
#define FONT_SIZE 64

#define COLOR_BACKGROUND WHITE
#define WINDOW_INIT_FACTOR 100
#define WINDOW_INIT_WIDTH WINDOW_INIT_FACTOR * 16
#define WINDOW_INIT_HEIGHT WINDOW_INIT_FACTOR * 9

#define MAP_SCALE 3.0f
#define PLAYER_SPRITE_SCALE 1.5f
#define PLAYER_SIZE 150.0f
// The "stride" is how wide a sprite is on the sprite sheet
#define PLAYER_SPRITE_SHEET_STRIDE 48.0f
#define PLAYER_SPRITE_SHEET_DOWN_ROW 0
#define PLAYER_SPRITE_SHEET_UP_ROW 1
#define PLAYER_SPRITE_SHEET_LEFT_ROW 2
#define PLAYER_SPRITE_SHEET_RIGHT_ROW 3

#define PLAYER_WALKING_SPEED 300.0f
#define PLAYER_WALKING_SPEED_ANIM_MILLIS 250

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

typedef struct Character {
    Rectangle rect;
} Character;

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_INIT_WIDTH, WINDOW_INIT_HEIGHT, "YAFS");
    SetTargetFPS(144);
    SetExitKey(KEY_ESCAPE);
    InitAudioDevice();

    GameState game_state;
    Camera2D camera;

    Character player;
    Texture2D player_sprite_sheet;
    int sprite_sheet_row, sprite_sheet_col;
    
    Texture2D map;

    { // Initialization
        game_state = (GameState) {
            .debug_mode = false,
            .paused = false,
        };

        player = (Character) {
            .rect = (Rectangle) {
                .x = vw(50),
                .y = vh(50),
                .width = PLAYER_SIZE,
                .height = PLAYER_SIZE,
            }
        };

        camera = (Camera2D) {
            .offset = { 0 },
            .rotation = 0.0f,
            .target = (Vector2) { player.rect.x, player.rect.y },
            .zoom = 1.0f,
        };

        player_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Characters/basic-character-spritesheet.png");
        sprite_sheet_row = 0;
        sprite_sheet_col = 0;

        map = LoadTexture("resources/tilesets/map.png");
        // tilled_dirt = LoadTexture("resources/sprout-lands-sprites/Tilesets/Tilled_Dirt.png");
        // water = LoadTexture("resources/sprout-lands-sprites/Tilesets/Water.png");
    }

    while (!WindowShouldClose()) {
        { // Update
            { // Can be done regardless of pause state. 
                if (IsKeyPressed(KEY_F1)) {
                    game_state.debug_mode = !game_state.debug_mode;
                }

                if (IsKeyPressed(KEY_SPACE)) {
                    game_state.paused = !game_state.paused;
                }
            }

            if (game_state.paused) goto draw;

            // Animation frames for different directions
            if (IsKeyDown(KEY_UP)) {
                sprite_sheet_row = PLAYER_SPRITE_SHEET_UP_ROW;
            } else if (IsKeyDown(KEY_DOWN) || (IsKeyDown(KEY_LEFT) && IsKeyDown(KEY_RIGHT))) {
                sprite_sheet_row = PLAYER_SPRITE_SHEET_DOWN_ROW;
            } else if (IsKeyDown(KEY_LEFT)) {
                sprite_sheet_row = PLAYER_SPRITE_SHEET_LEFT_ROW;
            } else if (IsKeyDown(KEY_RIGHT)) {
                sprite_sheet_row = PLAYER_SPRITE_SHEET_RIGHT_ROW;
            }
            
            Vector2 pos_diff = { 0.0f, 0.0f };
            if (IsKeyDown(KEY_LEFT)) pos_diff.x--;
            if (IsKeyDown(KEY_RIGHT)) pos_diff.x++;
            if (IsKeyDown(KEY_UP)) pos_diff.y--;
            if (IsKeyDown(KEY_DOWN)) pos_diff.y++;
            Vector2 pos_diff_normalized = Vector2Normalize(pos_diff);

            // Movement animation frames
            const int now_in_millis = (int)(GetTime() * 1000.0f);
            const bool is_idle = Vector2Length(pos_diff_normalized) == 0.0f;
            if (is_idle) {
                if (now_in_millis % 2000 < 1500) {
                    sprite_sheet_col = 0;
                } else {
                    sprite_sheet_col = 1;
                }
            } else {
                if (now_in_millis % (PLAYER_WALKING_SPEED_ANIM_MILLIS*2) < PLAYER_WALKING_SPEED_ANIM_MILLIS) {
                    sprite_sheet_col = 2;
                } else {
                    sprite_sheet_col = 3;
                }
            }

            pos_diff = Vector2Scale(pos_diff_normalized, PLAYER_WALKING_SPEED * GetFrameTime());

            player.rect.x += pos_diff.x;
            player.rect.y += pos_diff.y;

            camera.target = (Vector2) {
                player.rect.x - ((float)GetScreenWidth()/2.0f) + (player.rect.width/2.0f),
                player.rect.y - ((float)GetScreenHeight()/2.0f) + (player.rect.height/2.0f),
            };
        }

        { // Draw
draw:       BeginDrawing();
            ClearBackground(COLOR_BACKGROUND);
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
                    player.rect.width * PLAYER_SPRITE_SCALE,
                    player.rect.height * PLAYER_SPRITE_SCALE,
                },
                (Vector2) {
                    player.rect.width,
                    player.rect.height,
                },
                0.0f,
                WHITE
            );

            // Draw debug info
            if (game_state.debug_mode) { 
                // Draw grid
                for (int i = 0; i < 20; i++) {
                    DrawRectangle(vw(5 * i), 0, 1, vh(100), GREEN);
                    DrawRectangle(0, vh(5 * i), vw(100), 1, GREEN);
                }

                // Draw player rect
                DrawRectangleLinesEx(player.rect, 1.0f, ORANGE);

                DrawFPS(10, 10);
            }

            EndMode2D();
            EndDrawing();
        }

    }
    
    UnloadTexture(player_sprite_sheet);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
