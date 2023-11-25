#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <complex.h>
#include <math.h>

#include <raylib.h>

#ifndef _WIN32
#include <signal.h> // needed for sigaction()
#endif // _WIN32

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOB_IMPLEMENTATION
#include "nob.h"

#include <raylib.h>
#include <rlgl.h>

#define GLSL_VERSION 330

#define N (1<<13)
#define FONT_SIZE 64

#define RENDER_FPS 30
#define RENDER_FACTOR 100
#define RENDER_WIDTH (16*RENDER_FACTOR)
#define RENDER_HEIGHT (9*RENDER_FACTOR)

#define COLOR_ACCENT                  ColorFromHSV(225, 0.75, 0.8)
#define COLOR_BACKGROUND              GetColor(0x151515FF)
#define COLOR_TRACK_PANEL_BACKGROUND  ColorBrightness(COLOR_BACKGROUND, -0.1)
#define COLOR_TRACK_BUTTON_BACKGROUND ColorBrightness(COLOR_BACKGROUND, 0.15)
#define COLOR_TRACK_BUTTON_HOVEROVER  ColorBrightness(COLOR_TRACK_BUTTON_BACKGROUND, 0.15)
#define COLOR_TRACK_BUTTON_SELECTED   COLOR_ACCENT
#define COLOR_TIMELINE_CURSOR         COLOR_ACCENT
#define COLOR_TIMELINE_BACKGROUND     ColorBrightness(COLOR_BACKGROUND, -0.3)
#define COLOR_HUD_BUTTON_BACKGROUND   COLOR_TRACK_BUTTON_BACKGROUND
#define COLOR_HUD_BUTTON_HOVEROVER    COLOR_TRACK_BUTTON_HOVEROVER

#define HUD_TIMER_SECS    1.0f
#define HUD_BUTTON_SIZE   50
#define HUD_BUTTON_MARGIN 50
#define HUD_ICON_SCALE    0.5

typedef struct GameState {
    Font font;

    int player_one_score;
    int player_two_score;
} GameState;

typedef struct Ball {
    Vector2 position;
    Vector2 velocity;
    Vector2 size;
    Color color;
} Ball;

typedef struct Paddle {
    Vector2 position;
    Vector2 size;
    Color color;
} Paddle;

// CSS-like helpers --------------------------------------------------------------------------------

float sch_vh(float vh) {
    return GetRenderHeight() * vh * 0.01;
}

float sch_vw(float vw) {
    return GetRenderWidth() * vw * 0.01;
}

float sch_x_center(float width) {
    return GetRenderWidth() / 2 - width / 2;
}

float sch_y_center(float height) {
    return GetRenderHeight() / 2 - height / 2;
}

float sch_top() {
    return 0;
}

float sch_right() {
    return GetRenderWidth();
}

float sch_bottom() {
    return GetRenderHeight();
}

float sch_left() {
    return 0;
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    size_t factor = 80;
    InitWindow(factor*16, factor*9, "Pong");
    SetTargetFPS(144);
    SetExitKey(KEY_ESCAPE);
    InitAudioDevice();

    GameState *game_state;
    Ball *ball;
    Paddle *paddle_one;
    Paddle *paddle_two;

    { // Initialize the game state
        game_state = malloc(sizeof(*game_state));
        assert(game_state != NULL && "Buy more RAM lol");
        memset(game_state, 0, sizeof(*game_state));

        game_state->font = LoadFontEx("resources/fonts/Alegreya-Regular.ttf", FONT_SIZE, 0, 0);
        game_state->player_one_score = 0;
        game_state->player_two_score = 0;
    }

    { // Initialize ball
        ball = malloc(sizeof(*ball));
        assert(ball != NULL && "Buy more RAM lol");
        memset(ball, 0, sizeof(*ball));

        ball->position = (Vector2) { sch_x_center(25), sch_y_center(25) };
        ball->velocity = (Vector2) { 1, 0 };
    }

    { // Initialize paddles
        paddle_one = malloc(sizeof(*paddle_one));
        assert(paddle_one != NULL && "Buy more RAM lol");
        memset(paddle_one, 0, sizeof(*paddle_one));

        paddle_one->position = (Vector2) { sch_vw(10), sch_vh(50) };

        paddle_two = malloc(sizeof(*paddle_two));
        assert(paddle_two != NULL && "Buy more RAM lol");
        memset(paddle_two, 0, sizeof(*paddle_two));

        paddle_two->position = (Vector2) { sch_vw(90), sch_vh(50) };
    }

    while (!WindowShouldClose()) {
        { // Update
            { // Update ball
                ball->position.x += ball->velocity.x;
                ball->position.y += ball->velocity.y;
            }

            { // Update paddles
                paddle_one->position.y = GetMousePosition().y;
                paddle_two->position.y = GetMousePosition().y;
            }

            { // Check for collisions
                // Check for collisions with the top and bottom of the screen
                if (ball->position.y <= 0 || ball->position.y >= GetRenderHeight()) {
                    ball->velocity.y *= -1;
                }

                // Check for collisions with the paddles
                if (CheckCollisionCircleRec(ball->position, 25, (Rectangle) { paddle_one->position.x - 10, paddle_one->position.y - 50, 20, 100 })) {
                    ball->velocity.x *= -1;
                }

                if (CheckCollisionCircleRec(ball->position, 25, (Rectangle) { paddle_two->position.x - 10, paddle_two->position.y - 50, 20, 100 })) {
                    ball->velocity.x *= -1;
                }

                // Check for collisions with the left and right of the screen
                if (ball->position.x <= 0) {
                    game_state->player_two_score++;
                    ball->position = (Vector2) { sch_x_center(25), sch_y_center(25) };
                    ball->velocity = (Vector2) { 0, 0 };
                }

                if (ball->position.x >= GetRenderWidth()) {
                    game_state->player_one_score++;
                    ball->position = (Vector2) { sch_x_center(25), sch_y_center(25) };
                    ball->velocity = (Vector2) { 0, 0 };
                }
            }
        }

        { // Draw
            BeginDrawing();
            ClearBackground(COLOR_BACKGROUND);

            // { // Draw grid
            //     for (int i = 0; i < 20; i++) {
            //         DrawRectangle(sch_vw(5 * i), 0, 2, sch_vh(100), GREEN);
            //         DrawRectangle(0, sch_vh(5 * i), sch_vw(100), 2, GREEN);
            //     }
            // }

            // Draw scores
            DrawTextEx(game_state->font, TextFormat("%i", game_state->player_one_score), (Vector2){ sch_vw(40), sch_vh(5) }, FONT_SIZE, 0, WHITE);
            DrawTextEx(game_state->font, TextFormat("%i", game_state->player_two_score), (Vector2){ sch_vw(60), sch_vh(5) }, FONT_SIZE, 0, WHITE);

            // Draw ball
            DrawRectangleV(ball->position, (Vector2) { 25, 25 }, WHITE);

            // Draw paddles
            DrawRectangle(paddle_one->position.x - 10, paddle_one->position.y - 50, 20, 100, WHITE);
            DrawRectangle(paddle_two->position.x - 10, paddle_two->position.y - 50, 20, 100, WHITE);
        
            EndDrawing();
        }
    }

    CloseAudioDevice();
    CloseWindow();

    return 0;
}
