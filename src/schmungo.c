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

enum LandplotState {
    LP_FALLOW,
    LP_PLOWED,
    LP_SEEDED,
    LP_GROWING,
    LP_GROWN,
};

typedef struct {
    int id;
    Vector2 pos;
    enum LandplotState state;
} Landplot;

typedef struct {
    Font font;
    bool debug_mode;

    int landplot_count;
    Landplot *landplots;

    int turn;
    int actions_remaining;
} Schmungo;

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

// Component properties ----------------------------------------------------------------------------

Rectangle rec_area(const Vector2 pos, const Vector2 size)
{
    return (Rectangle) { pos.x, pos.y, size.x, size.y };
}

Vector2 sch_LandplotSize()
{
    return (Vector2) { sch_vw(5), sch_vw(5) };
}

Color sch_landplot_color(const enum LandplotState state)
{
    switch (state) {
        case LP_FALLOW:
            return GRAY;

        case LP_PLOWED:
            return DARKBROWN;

        case LP_SEEDED:
            return LIME;

        case LP_GROWING:
            return GREEN;

        case LP_GROWN:
            return BROWN;

        default:
            return RED;
    }
}

Vector2 sch_NextTurnSize()
{
    return (Vector2) { sch_vw(15), sch_vw(5) };
}

Vector2 sch_NextTurnButtonPos()
{
    const Vector2 size = sch_NextTurnSize();
    Vector2 margin = { sch_vw(2), sch_vw(2) };
    return (Vector2) {
        sch_right() - size.x - margin.x,
        sch_bottom() - size.y - margin.y
    };
}

// Draw components ---------------------------------------------------------------------------------

void sch_DrawLandplotGround(const Vector2 pos, const Vector2 size)
{
    Color color = GREEN;

    DrawEllipse(
        pos.x + size.x/2,
        pos.y + size.y,
        size.x,
        size.x/3,
        color
    );
}

void sch_DrawLandplot(const Landplot *lp)
{
    Vector2 size = { sch_vw(5), sch_vw(5) };
    Color c = sch_landplot_color(lp->state);
    
    sch_DrawLandplotGround(lp->pos, size);
    DrawRectangleV(lp->pos, size, c);
}

void sch_DrawLandplots(const Schmungo *state)
{
    for (int i = 0; i < state->landplot_count; i++) {
        sch_DrawLandplot(&state->landplots[i]);
    }
}

void sch_DrawNextTurnButton(const Schmungo *state)
{
    const Vector2 pos = sch_NextTurnButtonPos();
    const Vector2 size = sch_NextTurnSize();

    DrawRectangleV(pos, size, ORANGE);
    DrawText("Next Turn", pos.x + 10, pos.y + 10, 30, WHITE);
}

// Draw debug components ---------------------------------------------------------------------------

void sch_DrawGameState(const Schmungo *state)
{
    int y = 30;
    if (!state->debug_mode) return;

    // Draw state of all landplots
    DrawText(TextFormat("Landplot count: %d", state->landplot_count), 10, y, 20, WHITE);
    for (int i = 0; i < state->landplot_count; i++) {
        const Landplot landplot = state->landplots[i];
        y += 20;

        DrawText(
            TextFormat("Landplot %d: (%.1f, %.1f)", landplot.id, landplot.pos.x, landplot.pos.x),
            10,
            y,
            20,
            sch_landplot_color(landplot.state)
        );
    }

    // Draw turn
    y += 20;
    DrawText(TextFormat("Turn: %d", state->turn), 10, y, 20, WHITE);

    // Draw actions remaining
    y += 20;
    DrawText(TextFormat("Actions remaining: %d", state->actions_remaining), 10, y, 20, WHITE);
}

void sch_DrawDebugNextTurnButton()
{
    const Vector2 pos = sch_NextTurnButtonPos();
    const Vector2 size = sch_NextTurnSize();
    const Rectangle area = { pos.x, pos.y, size.x, size.y };

    double currentTime = GetTime();
    int currentSecond = (int)currentTime % 60;
    const Color c = (currentSecond % 2 == 0) ? BLACK : WHITE;

    DrawRectangleLinesEx(area, 2, c);
}

// Update Components -------------------------------------------------------------------------------

// TODO: this mutates the state. Probably shouldn't do that in a real game, but it's fine for now.
Landplot *sch_UpdateLandplot(const int i, Schmungo *state)
{
    Landplot *new_lp = malloc(sizeof(*new_lp));
    Landplot old_lp = state->landplots[i];

    new_lp->id = old_lp.id;
    new_lp->pos = old_lp.pos;
    new_lp->state = old_lp.state;

    if (state->actions_remaining <= 0) return new_lp;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse_pos = GetMousePosition();

        if (CheckCollisionPointRec(mouse_pos, rec_area(new_lp->pos, sch_LandplotSize()))) {
            new_lp->state = (new_lp->state + 1) % 5;
            
            // NOTE: mutating the actions remaining instead of returning a new state.
            state->actions_remaining -= 1;
            printf("Actions remaining: %d\n", state->actions_remaining);

            printf("Landplot %d: %d\n", new_lp->id, new_lp->state);
        }
    }

    return new_lp;
}

// TODO: this mutates the state. Probably shouldn't do that in a real game, but it's fine for now.
const int sch_UpdateNextTurnButton(const int old_turn, Schmungo *state)
{
    int new_turn = old_turn;
    const Vector2 pos = sch_NextTurnButtonPos();
    const Vector2 size = sch_NextTurnSize();

    // TODO: this wold be better as a function.
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse_pos = GetMousePosition();
        Rectangle button_area = { pos.x, pos.y, size.x, size.y };

        if (CheckCollisionPointRec(mouse_pos, button_area)) {
            // TODO: mutating the actions remaining instead of returning a new state.
            state->actions_remaining = 2;

            new_turn = old_turn + 1;
            printf("Turn: %d\n", new_turn);
        }
    }

    return new_turn;
}

// Game loop core-----------------------------------------------------------------------------------

const Schmungo *sch_init(Schmungo *state)
{
    state = malloc(sizeof(*state));
    assert(state != NULL && "Buy more RAM lol");
    memset(state, 0, sizeof(*state));

    state->font = LoadFontEx("./resources/fonts/Alegreya-Regular.ttf", FONT_SIZE, NULL, 0);
    state->debug_mode = true;

    { // Land plots
        state->landplot_count = 9;
        state->landplots = malloc(state->landplot_count * sizeof(*state->landplots));
        for (int i = 0; i < state->landplot_count; i++) {
            state->landplots[i].state = LP_FALLOW;
            state->landplots[i].id = i;

            float x;
            switch (i) {
                case 0:
                case 3:
                case 6:
                    x = sch_x_center(sch_vw(5) - 250);
                    break;

                case 1:
                case 4:
                case 7:
                    x = sch_x_center(sch_vw(5));
                    break;

                case 2:
                case 5:
                case 8:
                    x = sch_x_center(sch_vw(5) + 250);
                    break;
            }
            
            float y;
            switch (i) {
                case 0:
                case 1:
                case 2:
                    y = sch_y_center(sch_vw(5) - 250);
                    break;

                case 3:
                case 4:
                case 5:
                    y = sch_y_center(sch_vw(5));
                    break;

                case 6:
                case 7:
                case 8:
                    y = sch_y_center(sch_vw(5) + 250);
                    break;
            }  
            
            state->landplots[i].pos = (Vector2) { x, y };
        }
    }

    state->turn = 1;
    state->actions_remaining = 2;

    SetMasterVolume(0.5);

    return state;
}

const Schmungo *sch_update(const Schmungo *old_state)
{
    // TODO: I kinda want to just have a local struct and then copy the struct over to the new state.
    Schmungo *new_state = malloc(sizeof(*new_state));
    assert(new_state != NULL && "Buy more RAM lol");
    new_state = memcpy(new_state, old_state, sizeof(*new_state));

    // Update land plots
    new_state->landplot_count = old_state->landplot_count;
    new_state->landplots = malloc(new_state->landplot_count * sizeof(*new_state->landplots));
    for (int i = 0; i < new_state->landplot_count; i++) {
        // TODO: I'm not sure about the implications of this. I'm allocating memory for each landplot
        // and then copying the old landplot over to the new one. I'm not sure if this is the best
        // way to do this.
        new_state->landplots[i] = old_state->landplots[i];
        new_state->landplots[i] = *sch_UpdateLandplot(i, new_state);
        // TODO: Freeing the old state causes a segfault but I don't know why.
        // free(&old_state->landplots[i]);
    }

    // Update turn
    new_state->turn = sch_UpdateNextTurnButton(old_state->turn, new_state);

    // Update actions remaining
    // new_state->actions_remaining = old_state->actions_remaining;

    // Update debug mode
    if (IsKeyPressed(KEY_F1)) {
        new_state->debug_mode = !old_state->debug_mode;
    } else {
        new_state->debug_mode = old_state->debug_mode;
    }

    return new_state;
}

void sch_draw(const Schmungo *state)
{
    BeginDrawing();
    ClearBackground(COLOR_BACKGROUND);

    sch_DrawGameState(state);
    sch_DrawLandplots(state);

    if (state->actions_remaining == 0) {
        sch_DrawNextTurnButton(state);
    }
    
    if (state->debug_mode) {
        DrawFPS(10, 10);
        sch_DrawDebugNextTurnButton();
    }

    EndDrawing();
}

void sch_cleanup(Schmungo *state) {
    free(state->landplots);
    free(state);
    // _gup_memory_print();
}

Schmungo *persistent_state = NULL;

int main(void)
{
    Image logo = LoadImage("./resources/logo/logo-256.png");
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    size_t factor = 80;
    InitWindow(factor*16, factor*9, "Schmungo");
    SetWindowIcon(logo);
    SetTargetFPS(144);
    SetExitKey(KEY_ESCAPE);
    InitAudioDevice();

    persistent_state = sch_init(persistent_state);

    while (!WindowShouldClose()) {
        // Reset the state
        if (IsKeyPressed(KEY_R)) {
            persistent_state = sch_init(persistent_state);
        }

        // Update the game state
        Schmungo *new_state = sch_update(persistent_state);

        // Draw the game state
        sch_draw(new_state);

        // Cleanup the old state
        sch_cleanup(persistent_state);

        // Update the persistent state to the new state
        persistent_state = new_state;
    }

    sch_cleanup(persistent_state);

    CloseAudioDevice();
    CloseWindow();
    UnloadImage(logo);

    return 0;
}
