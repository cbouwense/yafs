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
#define RENDER_FACTOR 100
#define RENDER_WIDTH (16*RENDER_FACTOR)
#define RENDER_HEIGHT (9*RENDER_FACTOR)

#define PLAYER_SPRITE_SCALE 2.0f
// The "stride" is how wide a sprite is on the sprite sheet
#define PLAYER_SPRITE_SHEET_STRIDE 48.0f
#define PLAYER_SPRITE_SHEET_DOWN_ROW 0
#define PLAYER_SPRITE_SHEET_UP_ROW 1
#define PLAYER_SPRITE_SHEET_LEFT_ROW 2
#define PLAYER_SPRITE_SHEET_RIGHT_ROW 3

#define PLAYER_WALKING_SPEED 200.0f
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
    Vector2   pos;
    int       speed;

    Rectangle rect;
} Character;

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    size_t factor = 80;
    InitWindow(factor*16, factor*9, "YAFS");
    SetTargetFPS(144);
    SetExitKey(KEY_ESCAPE);
    InitAudioDevice();

    GameState game_state;
    Character player;
    Texture2D player_sprite_sheet;
    Texture2D map;
    int sprite_sheet_row, sprite_sheet_col;

    { // Initialization
        game_state = (GameState) {
            .debug_mode = false,
            .paused = false,
        };

        const float player_size = 150.0f;
        player = (Character) {
            .pos = (Vector2) { .x = vw(50), .y = vh(50) },
            .speed = PLAYER_WALKING_SPEED,
            .rect = (Rectangle) {
                .x = vw(50) - (player_size/2.0f),
                .y = vh(50) - (player_size/2.0f),
                .width = player_size,
                .height = player_size,
            }
        };

        player_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Characters/basic-character-spritesheet.png");
        sprite_sheet_row = 0;
        sprite_sheet_col = 0;

        map = LoadTexture("resources/sprout-lands-sprites/Tilesets/map.png");
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
            player.pos = Vector2Add(player.pos, pos_diff);
            player.rect.x = player.pos.x;
            player.rect.y = player.pos.y;
        }

        { // Draw
draw:       BeginDrawing();
            ClearBackground(COLOR_BACKGROUND);

            // Draw map
            DrawTextureEx(map, (Vector2) { 0.0f, 0.0f }, 0.0f, 5.0f, WHITE);

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
                    player.pos.x,
                    player.pos.y,
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

            EndDrawing();
        }

    }
    
    UnloadTexture(player_sprite_sheet);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
