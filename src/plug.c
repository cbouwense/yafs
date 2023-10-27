#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <complex.h>

#include "plug.h"
#include "ffmpeg.h"
#define NOB_IMPLEMENTATION
#include "nob.h"

#include <raylib.h>
#include <rlgl.h>

#define _WINDOWS_
#include "miniaudio.h"

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

static Plug *p = NULL;
static Schmungo_State *state = NULL;

typedef enum {
    BS_NONE      = 0, // 00
    BS_HOVEROVER = 1, // 01
    BS_CLICKED   = 2, // 10
} Button_State;

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

static void sch_DrawMainMenu(void)
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

void sch_init(void)
{
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
}

void sch_update(void)
{
    BeginDrawing();
    ClearBackground(COLOR_BACKGROUND);

    sch_DrawMainMenu();
    // Draw all units
    for (int i = 0; i < state->unit_count; i++) {
        sch_DrawUnit(&state->units[i]);
    }

    if (IsKeyPressed(KEY_SPACE)) {
        // Add a unit
        state->unit_count++;
        state->units = realloc(state->units, state->unit_count * sizeof(*state->units));
        assert(state->units != NULL && "Buy more RAM lol");
        memset(&state->units[state->unit_count - 1], 0, sizeof(*state->units));

        state->units[state->unit_count - 1].color = RED;
        state->units[state->unit_count - 1].pos = (Vector2) { 
            .x = sch_x_center(sch_vw(5)) - (500 - ((state->unit_count - 1) * 100)),
            .y = sch_y_center(sch_vw(5))
        };
    }
    
    EndDrawing();
}

void sch_cleanup() {
    free(state->units);
    free(state);
    free(p);
}
