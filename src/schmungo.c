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

#define COLOR_BACKGROUND BLACK
#define RENDER_FACTOR 100
#define RENDER_WIDTH (16*RENDER_FACTOR)
#define RENDER_HEIGHT (9*RENDER_FACTOR)

typedef struct GameState {
    Font font;

    bool debug_mode;
    bool paused;

    int player_one_score;
    int player_two_score;
} GameState;

typedef struct Ball {
    Vector2 position;
    Vector2 velocity;
    Color color;

    Rectangle collision;
} Ball;

typedef struct Paddle {
    Vector2 position;
    Vector2 size;
    Color color;

    Rectangle collision;
} Paddle;

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

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    size_t factor = 80;
    InitWindow(factor*16, factor*9, "Pong");
    SetTargetFPS(144);
    SetExitKey(KEY_ESCAPE);
    InitAudioDevice();

    GameState *game_state;
    Ball      *ball;
    Paddle    *paddle_one;
    Paddle    *paddle_two;

    { // Initialize the game state
        game_state = malloc(sizeof(*game_state));
        assert(game_state != NULL && "Buy more RAM lol");
        memset(game_state, 0, sizeof(*game_state));

        game_state->font = LoadFontEx("resources/fonts/Alegreya-Regular.ttf", FONT_SIZE, 0, 0);
        game_state->debug_mode = false;
        game_state->player_one_score = 0;
        game_state->player_two_score = 0;
    }

    { // Initialize ball
        ball = malloc(sizeof(*ball));
        assert(ball != NULL && "Buy more RAM lol");
        memset(ball, 0, sizeof(*ball));

        ball->position = (Vector2) { x_center(25), y_center(25) };
        ball->velocity = (Vector2) { 1, 0 };
        ball->collision = (Rectangle) { ball->position.x, ball->position.y, 25, 25 };
    }

    { // Initialize paddles
        paddle_one = malloc(sizeof(*paddle_one));
        assert(paddle_one != NULL && "Buy more RAM lol");
        memset(paddle_one, 0, sizeof(*paddle_one));

        paddle_one->position = (Vector2) { vw(10), vh(50) };
        paddle_one->collision = (Rectangle) { paddle_one->position.x - 10, paddle_one->position.y - 50, 20, 100 };

        paddle_two = malloc(sizeof(*paddle_two));
        assert(paddle_two != NULL && "Buy more RAM lol");
        memset(paddle_two, 0, sizeof(*paddle_two));

        paddle_two->position = (Vector2) { vw(90), vh(50) };
        paddle_two->collision = (Rectangle) { paddle_two->position.x - 10, paddle_two->position.y - 50, 20, 100 };
    }

    while (!WindowShouldClose()) {
        { // Update
            if (!game_state->paused) {
                { // Update ball
                    ball->position.x += ball->velocity.x;
                    ball->position.y += ball->velocity.y;
                    ball->collision.x = ball->position.x;
                    ball->collision.y = ball->position.y;
                }

                { // Update paddles
                    paddle_one->position.y = GetMousePosition().y;
                    paddle_one->collision.y = paddle_one->position.y - 50;

                    paddle_two->position.y = GetMousePosition().y;
                    paddle_two->collision.y = paddle_two->position.y - 50;
                }

                { // Check for collisions
                    // Check for collisions with the top and bottom of the screen
                    if (ball->position.y <= 0 || ball->position.y >= GetRenderHeight()) {
                        ball->velocity.y *= -1;
                    }

                    // Check for collisions with the left paddle
                    if (CheckCollisionRecs(ball->collision, paddle_one->collision)) {
                        ball->velocity.x *= -1;
                    }

                    // Check for collisions with the right paddle
                    if (CheckCollisionRecs(ball->collision, paddle_two->collision)) {
                        ball->velocity.x *= -1;
                    }

                    // Check for collisions with the left edge of screen
                    if (ball->collision.x <= 0) {
                        game_state->player_two_score++;
                        ball->position = (Vector2) { x_center(25), y_center(25) };
                        ball->velocity = (Vector2) { 1, 0 };
                    }

                    // Check for collisions with the right edge of screen
                    if (ball->collision.x + ball->collision.width >= GetRenderWidth()) {
                        game_state->player_one_score++;
                        ball->position = (Vector2) { x_center(25), y_center(25) };
                        ball->velocity = (Vector2) { -1, 0 };
                    }
                }
            }

            { // Update game state
                if (IsKeyPressed(KEY_F1)) {
                    game_state->debug_mode = !game_state->debug_mode;
                }

                if (IsKeyPressed(KEY_SPACE)) {
                    game_state->paused = !game_state->paused;
                }
            }
        }

        { // Draw
            BeginDrawing();
            ClearBackground(COLOR_BACKGROUND);

            const Vector2 score_one_position = (Vector2) { vw(40), vh(5) };
            const Vector2 score_two_position = (Vector2) { vw(60), vh(5) };
            const char *score_one_text = TextFormat("%i", game_state->player_one_score);
            const char *score_two_text = TextFormat("%i", game_state->player_two_score);
        
            { // Draw scores
                DrawTextEx(game_state->font, score_one_text, score_one_position, FONT_SIZE, 0, WHITE);
                DrawTextEx(game_state->font, score_two_text, score_two_position, FONT_SIZE, 0, WHITE);
            }

            { // Draw ball
                DrawRectangleV(ball->position, (Vector2) { 25, 25 }, WHITE);
            }

            { // Draw paddles
                DrawRectangle(paddle_one->position.x - 10, paddle_one->position.y - 50, 20, 100, WHITE);
                DrawRectangle(paddle_two->position.x - 10, paddle_two->position.y - 50, 20, 100, WHITE);
            }
        
            // Draw debug info
            if (game_state->debug_mode) { 
                { // Draw score outlines
                    Vector2 score_one_text_size = MeasureTextEx(game_state->font, score_one_text, FONT_SIZE, 0);
                    DrawRectangleLines(score_one_position.x, score_one_position.y, score_one_text_size.x, score_one_text_size.y, ORANGE);

                    Vector2 score_two_text_size = MeasureTextEx(game_state->font, score_two_text, FONT_SIZE, 0);
                    DrawRectangleLines(score_two_position.x, score_two_position.y, score_two_text_size.x, score_two_text_size.y, ORANGE);
                }

                // Draw ball outline
                DrawRectangleLinesEx(ball->collision, 1, ORANGE);

                // Draw paddle outlines
                DrawRectangleLinesEx(paddle_one->collision, 1, ORANGE);
                

                // Draw grid
                for (int i = 0; i < 20; i++) {
                    DrawRectangle(vw(5 * i), 0, 1, vh(100), GREEN);
                    DrawRectangle(0, vh(5 * i), vw(100), 1, GREEN);
                }
            }

            EndDrawing();
        }
    }

    CloseAudioDevice();
    CloseWindow();

    { // Cleanup
        UnloadFont(game_state->font);
        free(game_state);
        free(ball);
        free(paddle_one);
        free(paddle_two);
    }

    return 0;
}
