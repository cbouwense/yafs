#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <complex.h>
#include <math.h>

#include <raylib.h>

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
    InitWindow(factor*16, factor*9, "RPG Demo");
    SetTargetFPS(144);
    SetExitKey(KEY_ESCAPE);
    InitAudioDevice();

    GameState game_state;
    Character player;
    Texture2D player_sprite_sheet;

    { // Initialization
        game_state = (GameState) {
            .debug_mode = false,
            .paused = false,
        };

        const float player_size = 150.0f;
        player = (Character) {
            .pos = (Vector2) { .x = vw(50), .y = vh(50) },
            .speed = 10,
            .rect = (Rectangle) {
                .x = vw(50) - (player_size/2.0f),
                .y = vh(50) - (player_size/2.0f),
                .width = player_size,
                .height = player_size,
            }
        };
        player_sprite_sheet = LoadTexture("resources/sprout-lands-sprites/Characters/basic-character-spritesheet.png");
    }

    while (!WindowShouldClose()) {
        const float deltaTime = GetFrameTime();

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
        }

        { // Draw
draw:       BeginDrawing();
            ClearBackground(COLOR_BACKGROUND);
            DrawFPS(10, 10);

            const float scale = 3.0f;
            // Draw player
            DrawTexturePro(
                player_sprite_sheet, 
                 // TODO: updating the sprite tile in the draw method feels very very wrong.
                (Rectangle) {((int)GetTime() % 2 == 0 ? 0.0f : 48.0f), 0.0f, 48.0f, 48.0f},
                (Rectangle) {
                    player.rect.x,
                    player.rect.y,
                    player.rect.width * scale,
                    player.rect.height * scale,
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
            }

            EndDrawing();
        }

    }
    

    CloseAudioDevice();
    CloseWindow();

    return 0;
}
