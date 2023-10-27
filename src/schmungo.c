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

#define _WINDOWS_

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
#define HUD_TIMER_SECS 1.0f
#define HUD_BUTTON_SIZE 50
#define HUD_BUTTON_MARGIN 50
#define HUD_ICON_SCALE 0.5

typedef struct {
    Font font;
} Plug;

typedef struct {
    Color color;
    Vector2 pos;
} Unit_State;

typedef struct {
    int unit_count;
    Unit_State *units;
} Schmungo_State;

static float sch_vh(float vh) {
    return GetRenderHeight() * vh * 0.01;
}

static float sch_vw(float vw) {
    return GetRenderWidth() * vw * 0.01;
}

static float sch_x_center(float width) {
    return GetRenderWidth() / 2 - width / 2;
}

static float sch_y_center(float height) {
    return GetRenderHeight() / 2 - height / 2;
}

static float sch_top(float height) {
    return height;
}

static void sch_DrawMainMenu(Plug *p)
{
    int w = GetRenderWidth();

    const char *label = "Schmungo";
    Color color = WHITE;
    Vector2 size = MeasureTextEx(p->font, label, p->font.baseSize, 0);
    Vector2 pos = {
        w/2 - size.x/2,
        sch_top(size.y)
    };
    DrawTextEx(p->font, label, pos, p->font.baseSize, 0, color);
}

static void sch_DrawUnitGround(Vector2 pos, Vector2 size)
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

// TODO: maybe make this a pure function.
static void sch_UpdateUnit(Unit_State *state)
{
    if (IsKeyDown(KEY_A)) {
        state->pos.x -= 1;
    }
    if (IsKeyDown(KEY_D)) {
        state->pos.x += 1;
    }
    if (IsKeyDown(KEY_W)) {
        state->pos.y -= 1;
    }
    if (IsKeyDown(KEY_S)) {
        state->pos.y += 1;
    }
}

static void sch_DrawUnit(Unit_State *state)
{
    Vector2 size = { sch_vw(5), sch_vw(5) };

    sch_UpdateUnit(state);
    Vector2 adjusted_pos = {
        state->pos.x - size.x/2,
        state->pos.y - size.y,
    };

    sch_DrawUnitGround(adjusted_pos, size);
    DrawRectangleV(adjusted_pos, size, state->color);
}

Schmungo_State *sch_init(Schmungo_State *state, Plug *p)
{
    // TODO: call this something other than plug.
    { // Initialize plug state
        p = malloc(sizeof(*p));
        assert(p != NULL && "Buy more RAM lol");
        memset(p, 0, sizeof(*p));

        p->font = LoadFontEx("./resources/fonts/Alegreya-Regular.ttf", FONT_SIZE, NULL, 0);
    }
    
    { // Initialize schmungo state
        // Allocate space for the containing struct
        state = malloc(sizeof(*state));
        assert(state != NULL && "Buy more RAM lol");
        memset(state, 0, sizeof(*state));
        
        // Allocate space for units
        state->unit_count = 10;
        state->units = malloc(state->unit_count * sizeof(*state->units));
        for (int i = 0; i < state->unit_count; i++) {
            state->units[i].color = RED;
            state->units[i].pos = (Vector2) { 
                .x = sch_x_center(sch_vw(5)) - (500 - (i * 100)),
                .y = sch_y_center(sch_vw(5))
            };
        }
    }

    SetMasterVolume(0.5);

    return state;
}

Schmungo_State *sch_update(Schmungo_State *old_state)
{
    Schmungo_State *new_state = malloc(sizeof(*new_state));
    assert(new_state != NULL && "Buy more RAM lol");
    memset(new_state, 0, sizeof(*new_state));

    // Add a unit
    if (IsKeyPressed(KEY_SPACE)) {
        new_state->unit_count = old_state->unit_count + 1;
        new_state->units = malloc(new_state->unit_count * sizeof(*new_state->units));
        assert(new_state->units != NULL && "Buy more RAM lol");
        
        // Copy the old units
        for (int i = 0; i < old_state->unit_count; i++) {
            new_state->units[i] = old_state->units[i];
        }

        new_state->units[new_state->unit_count - 1].color = RED;
        new_state->units[new_state->unit_count - 1].pos = (Vector2) { 
            .x = sch_x_center(sch_vw(5)) - (500 - ((new_state->unit_count - 1) * 100)),
            .y = sch_y_center(sch_vw(5))
        };
    }

    return new_state;
}

void sch_draw(Schmungo_State *state, Plug *p)
{
    BeginDrawing();
    ClearBackground(COLOR_BACKGROUND);

    sch_DrawMainMenu(p);

    // Draw all units
    for (int i = 0; i < state->unit_count; i++) {
        sch_DrawUnit(&state->units[i]);
    }
    
    EndDrawing();
}

void sch_cleanup(Schmungo_State *state, Plug *p) {
    free(state->units);
    free(state);
    free(p);
}

Plug *p = NULL;
Schmungo_State *state = NULL;

int main(void)
{
#ifndef _WIN32
    // NOTE: This is needed because if the pipe between Musializer and FFmpeg breaks
    // Musializer will receive SIGPIPE on trying to write into it. While such behavior
    // makes sense for command line utilities, Musializer is a relatively friendly GUI
    // application that is trying to recover from such situations.
    struct sigaction act = {0};
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
#endif // _WIN32

    Image logo = LoadImage("./resources/logo/logo-256.png");
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    size_t factor = 80;
    InitWindow(factor*16, factor*9, "Musializer");
    SetWindowIcon(logo);
    SetTargetFPS(144);
    SetExitKey(KEY_ESCAPE);
    InitAudioDevice();

    state = sch_init(state, p);

    while (!WindowShouldClose()) {
        // Reset the state
        if (IsKeyPressed(KEY_R)) {
            state = sch_init(state, p);
        }

        DrawFPS(10, 10);
        // Update the game state
        Schmungo_State *new_state = sch_update(state);

        // Draw the game state
        sch_draw(new_state, p);
    }

    sch_cleanup(state, p);

    CloseAudioDevice();
    CloseWindow();
    UnloadImage(logo);

    return 0;
}
