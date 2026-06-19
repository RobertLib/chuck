#include "editor.h"

#include "bbc.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * The chrome, drawn as a BBC Micro utility of the period would have drawn it:
 * a black screen, the system font in the eight colours, solid title bars, keys
 * as coloured blocks, and the highlight in inverse video. Everything goes
 * through bbc.h; nothing here knows what an RGB triple is.
 *
 * The colours keep the meanings the game's style guide gives them, so an
 * author reads the report the way a player reads the HUD: red is what the
 * tests would reject, yellow is a reservation and what is worth looking at,
 * cyan is the machine talking, green is clean, white is plain type and blue
 * is furniture — the rules between panels and the keys at rest.
 */

#define ED_FONT 8.0f

/* Title bars: the band a section of a panel opens with. */
#define ED_BAR_H 12.0f
#define ED_TITLE_BAR BBC_RED
#define ED_TITLE_INK BBC_YELLOW
/* The one-pixel rules between the panels and the canvas. */
#define ED_RULE BBC_BLUE

static const char *const ED_TOOL_NAMES[] = {
    "Brush", "Line", "Rect", "Box", "Fill", "Pick", "Select"};

static const char *const ED_TOOL_HINTS[] = {
    "1  paint single tiles; drag to draw freehand",
    "2  drag a straight run of tiles",
    "3  drag a hollow rectangle",
    "4  drag a filled rectangle",
    "5  flood the connected run of one character",
    "6  pick up the character under the pointer",
    "7  drag out a selection to copy, mirror or clear"};

/* Unsized above, so these measure the lists against the enum rather than
 * against themselves; a missing row would be a null `char *` drawn as a tool
 * name. See the note on `LEVEL_THEME_NAMES` in src/level.c. */
_Static_assert(sizeof(ED_TOOL_NAMES) / sizeof(ED_TOOL_NAMES[0]) ==
                   (size_t)ED_TOOL_COUNT,
               "every tool needs a name");
_Static_assert(sizeof(ED_TOOL_HINTS) / sizeof(ED_TOOL_HINTS[0]) ==
                   (size_t)ED_TOOL_COUNT,
               "every tool needs a hint");

void ed_layout(EditorApp *app)
{
    SDL_GetWindowSize(app->window, &app->win_w, &app->win_h);
    float right = (float)app->win_w - ED_RIGHT_W;
    app->left_panel = (SDL_FRect){0.0f, ED_TOP_H, ED_LEFT_W,
                                  (float)app->win_h - ED_TOP_H - ED_BOTTOM_H};
    app->right_panel = (SDL_FRect){right, ED_TOP_H, ED_RIGHT_W,
                                   (float)app->win_h - ED_TOP_H - ED_BOTTOM_H};
    app->canvas = (SDL_FRect){ED_LEFT_W, ED_TOP_H, right - ED_LEFT_W,
                              (float)app->win_h - ED_TOP_H - ED_BOTTOM_H};
    if (app->canvas.w < 64.0f)
        app->canvas.w = 64.0f;
}

float ed_text_width(const char *text, float scale)
{
    return bbc_text_width(text, scale);
}

void ed_text(EditorApp *app, float x, float y, float scale, BbcColour colour,
             const char *format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    SDL_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    bbc_text(app->renderer, x, y, scale, colour, buffer);
}

/*
 * Lay out `text` inside `width`, breaking on spaces, and draw it when `draw`
 * is set. Returns the height used either way: a row that is highlighted has
 * to know how tall it is before the bar behind it can be laid down.
 */
static float text_block_ex(EditorApp *app, float x, float y, float width,
                           float scale, BbcColour colour, int max_lines,
                           const char *text, bool draw)
{
    int per_line = (int)(width / (ED_FONT * scale));
    if (per_line < 8)
        per_line = 8;

    float line_height = ED_FONT * scale + 3.0f;
    int lines = 0;
    const char *at = text;
    while (*at != '\0' && lines < max_lines)
    {
        int length = 0;
        while (at[length] != '\0' && length < per_line)
            length++;
        int take = length;
        if (at[length] != '\0' && length == per_line)
        {
            int space = length;
            while (space > 0 && at[space] != ' ')
                space--;
            if (space > 0)
                take = space;
        }
        if (draw)
        {
            char line[160];
            int copy =
                take < (int)sizeof(line) - 1 ? take : (int)sizeof(line) - 1;
            memcpy(line, at, (size_t)copy);
            line[copy] = '\0';
            ed_text(app, x, y + (float)lines * line_height, scale, colour, "%s",
                    line);
        }
        lines++;
        at += take;
        while (*at == ' ')
            at++;
    }
    return (float)lines * line_height;
}

static float text_block(EditorApp *app, float x, float y, float width,
                        float scale, BbcColour colour, int max_lines,
                        const char *text)
{
    return text_block_ex(app, x, y, width, scale, colour, max_lines, text, true);
}

static bool inside(const SDL_FRect *rect, float x, float y)
{
    return x >= rect->x && x < rect->x + rect->w && y >= rect->y &&
           y < rect->y + rect->h;
}

/* A widget scrolled out of its panel is clipped away, and something you
 * cannot see must not be something you can press. */
static bool point_in(EditorApp *app, const SDL_FRect *rect, float x, float y)
{
    if (!inside(rect, x, y))
        return false;
    if (!SDL_RenderClipEnabled(app->renderer))
        return true;
    SDL_Rect clip;
    if (!SDL_GetRenderClipRect(app->renderer, &clip))
        return true;
    return x >= (float)clip.x && x < (float)(clip.x + clip.w) &&
           y >= (float)clip.y && y < (float)(clip.y + clip.h);
}

/*
 * A key: a solid block of colour with its label on it, the way the machine's
 * own function keys were a strip of red caps with the words on a card above
 * them. Blue at rest, so a screenful of them stays quiet; the label goes yellow
 * under the pointer; and the one that is *on* — the tool in the hand, the map
 * on screen, MODE FACADE — is inverse video, black on cyan, which is the
 * highlight every menu on the machine used.
 */
bool ed_button(EditorApp *app, SDL_FRect rect, const char *label, bool active,
               const char *tooltip)
{
    bool hover = point_in(app, &rect, app->mouse_x, app->mouse_y);
    BbcColour face = BBC_BLUE;
    BbcColour ink = hover ? BBC_YELLOW : BBC_WHITE;
    if (active)
    {
        face = hover ? BBC_WHITE : BBC_CYAN;
        ink = BBC_BLACK;
    }
    bbc_fill(app->renderer, face, rect.x, rect.y, rect.w, rect.h);

    float text_w = ed_text_width(label, 1.0f);
    ed_text(app, floorf(rect.x + (rect.w - text_w) * 0.5f),
            floorf(rect.y + (rect.h - ED_FONT) * 0.5f), 1.0f, ink, "%s", label);

    if (hover && tooltip != NULL)
        SDL_snprintf(app->hint, sizeof(app->hint), "%s", tooltip);
    return hover && app->clicked;
}

/* A band of colour across a panel with the section's name on it. */
static void title_bar(EditorApp *app, float x, float y, float w,
                      const char *text)
{
    bbc_fill(app->renderer, ED_TITLE_BAR, x, y, w, ED_BAR_H);
    ed_text(app, x + 4.0f, y + 2.0f, 1.0f, ED_TITLE_INK, "%s", text);
}

/* The width of a key that fits its label with a character's room either side. */
static float key_width(const char *label)
{
    return ed_text_width(label, 1.0f) + 2.0f * ED_FONT;
}

/* ---- Top bar ----------------------------------------------------------- */

static void draw_top_bar(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    bbc_fill(r, BBC_BLACK, 0.0f, 0.0f, (float)app->win_w, ED_TOP_H);
    bbc_fill(r, ED_RULE, 0.0f, ED_TOP_H - (float)BBC_PX_H, (float)app->win_w,
             (float)BBC_PX_H);

    /* The maps, one key each, on whole BBC pixels with one between them. */
    float x = 4.0f;
    for (int i = 0; i < app->file_count; ++i)
    {
        const EdFile *file = &app->files[i];
        float w = file->sublevel ? 48.0f : 28.0f;
        SDL_FRect rect = {x, 4.0f, w, 20.0f};
        bool active = i == app->current_file;
        char label[16];
        if (file->sublevel)
        {
            /* Every room is `restroom_<where>`, so the first five letters of
             * the name are the same five four times over; the part after the
             * last underscore is the part that tells them apart. */
            const char *name = SDL_strrchr(file->label, '_');
            name = name != NULL && name[1] != '\0' ? name + 1 : file->label;
            SDL_snprintf(label, sizeof(label), "%.5s", name);
        }
        else
            SDL_snprintf(label, sizeof(label), "%d", file->number);
        char tip[128];
        SDL_snprintf(tip, sizeof(tip), "Open %s", file->path);
        if (ed_button(app, rect, label, active, tip))
        {
            ed_open_file(app, i);
        }
        /* Unsaved changes: a yellow strip along the bottom of the key. */
        if (active && app->doc.dirty)
            bbc_fill(r, BBC_YELLOW, rect.x, rect.y + rect.h - 4.0f, rect.w,
                     4.0f);
        x += w + 4.0f;
    }

    x += 8.0f;
    const char *sector = "+ Sector";
    if (ed_button(app, (SDL_FRect){x, 4.0f, key_width(sector), 20.0f}, sector,
                  false, "Ctrl+N  add the next campaign sector as an interior"))
    {
        ed_new_level(app, false);
    }
    x += key_width(sector) + 4.0f;
    const char *climb = "+ Climb";
    if (ed_button(app, (SDL_FRect){x, 4.0f, key_width(climb), 20.0f}, climb,
                  false, "Ctrl+Shift+N  add the next campaign sector as a facade"))
    {
        ed_new_level(app, true);
    }
    x += key_width(climb);

    /* The program's name in the corner, on a title bar like every other
     * section's — and only while the row of maps leaves room for it. */
    const char *title = "CHUCK LEVEL EDITOR";
    float title_w = key_width(title);
    float title_x = (float)app->win_w - 4.0f - title_w;
    if (title_x > x + 16.0f)
    {
        bbc_fill(r, ED_TITLE_BAR, title_x, 4.0f, title_w, 20.0f);
        ed_text(app, title_x + ED_FONT, 10.0f, 1.0f, ED_TITLE_INK, "%s", title);
    }

    /* Second row: what the document is, and what happens to it. */
    const char *path = app->doc.path[0] != '\0' ? app->doc.path : "(unsaved)";
    ed_text(app, 8.0f, 36.0f, 1.0f, app->doc.dirty ? BBC_YELLOW : BBC_WHITE,
            "%s%s", path, app->doc.dirty ? " *" : "");

    float bx = (float)app->win_w - 4.0f;
    struct
    {
        const char *label;
        const char *tip;
        int action;
    } actions[] = {
        {"Playtest F5", "F5  save, build and run the game in this sector", 0},
        {"Reload", "Ctrl+R  throw away changes and read the file again", 1},
        {"Save", "Ctrl+S  write the map back to its file", 2},
    };
    for (int i = 0; i < 3; ++i)
    {
        float width = key_width(actions[i].label);
        bx -= width;
        SDL_FRect rect = {bx, 30.0f, width, 20.0f};
        if (ed_button(app, rect, actions[i].label, false, actions[i].tip))
        {
            if (actions[i].action == 0)
                ed_playtest(app);
            else if (actions[i].action == 1)
                ed_reload(app);
            else
                ed_save(app);
        }
        bx -= 4.0f;
    }

    /* Clear of the longest path in the tree, the restroom maps under
     * levels/sublevels/, so the name of the file is never under a key. */
    float mx = 352.0f;
    const char *mode = app->doc.grid.facade ? "MODE FACADE" : "MODE INTERIOR";
    if (ed_button(app, (SDL_FRect){mx, 30.0f, 120.0f, 20.0f}, mode,
                  app->doc.grid.facade,
                  "F6  a facade is climbed four ways with no gravity"))
    {
        ed_toggle_facade(app);
    }
    mx += 124.0f;
    if (ed_button(app, (SDL_FRect){mx, 30.0f, 16.0f, 20.0f}, "<", false,
                  "F7  previous theme"))
    {
        ed_cycle_theme(app, -1);
    }
    char theme[48];
    SDL_snprintf(theme, sizeof(theme), "THEME %s",
                 app->doc.grid.has_theme
                     ? level_theme_name(app->doc.grid.theme)
                     : "(default)");
    if (ed_button(app, (SDL_FRect){mx + 20.0f, 30.0f, 164.0f, 20.0f}, theme,
                  false, "F8 / F7  the palette, the backdrop and the score"))
    {
        ed_cycle_theme(app, 1);
    }
    if (ed_button(app, (SDL_FRect){mx + 188.0f, 30.0f, 16.0f, 20.0f}, ">",
                  false, "F8  next theme"))
    {
        ed_cycle_theme(app, 1);
    }
}

/* ---- Left panel: tools, brush, palette, geometry ----------------------- */

/* A palette swatch is one tile at the size the game draws it, eight BBC
 * pixels by sixteen, with one BBC pixel of black between neighbours — which
 * is exactly where the frame round the swatch under the pointer goes. */
#define ED_SWATCH 32.0f
#define ED_SWATCH_PITCH_X 36.0f
#define ED_SWATCH_PITCH_Y 34.0f

static void draw_left_panel(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    SDL_FRect panel = app->left_panel;
    LevelTheme paint = ed_theme(app);
    float inner_w = panel.w - (float)BBC_PX_W;
    bbc_fill(r, BBC_BLACK, panel.x, panel.y, panel.w, panel.h);
    bbc_fill(r, ED_RULE, panel.x + inner_w, panel.y, (float)BBC_PX_W, panel.h);

    /* Clipped to the whole panel rather than inside its rule, so the frame
     * round a swatch in the last column can be laid over the rule instead of
     * losing a side to it. */
    SDL_Rect clip = {(int)panel.x, (int)panel.y, (int)panel.w, (int)panel.h};
    SDL_SetRenderClipRect(r, &clip);

    float y = panel.y + 4.0f - app->palette_scroll;
    title_bar(app, panel.x, y, inner_w, "TOOLS");
    y += ED_BAR_H + 4.0f;
    for (int i = 0; i < ED_TOOL_COUNT; ++i)
    {
        SDL_FRect rect = {panel.x + 8.0f + (float)(i % 3) * 68.0f,
                          y + (float)(i / 3) * 22.0f, 64.0f, 20.0f};
        char label[24];
        SDL_snprintf(label, sizeof(label), "%d %s", i + 1, ED_TOOL_NAMES[i]);
        if (ed_button(app, rect, label, app->tool == (EdTool)i,
                      ED_TOOL_HINTS[i]))
        {
            app->tool = (EdTool)i;
        }
    }
    y += 3.0f * 22.0f + 4.0f;

    /* What the brush is, and what the legend says about it. */
    const EdSymbol *brush = editor_symbol(app->brush);
    bbc_frame(r, ED_RULE, panel.x + 4.0f, y, inner_w - 8.0f, 54.0f);
    ed_draw_symbol(r, paint, app->brush, panel.x + 12.0f, y + 12.0f,
                   ED_SWATCH, app->time);
    ed_text(app, panel.x + 52.0f, y + 6.0f, 1.0f, BBC_YELLOW, "%c  %s",
            app->brush == ' ' ? '_' : app->brush,
            brush != NULL ? brush->name : "unknown");
    if (brush != NULL)
    {
        text_block(app, panel.x + 52.0f, y + 19.0f, inner_w - 60.0f, 1.0f,
                   BBC_WHITE, 3, brush->detail);
    }
    y += 58.0f;

    title_bar(app, panel.x, y, inner_w, "PALETTE");
    y += ED_BAR_H + 2.0f;

    int per_row = (int)(inner_w / ED_SWATCH_PITCH_X);
    if (per_row < 1)
        per_row = 1;
    EdGroup group = ED_GROUP_COUNT;
    int in_row = 0;
    for (int i = 0; i < ED_SYMBOL_COUNT; ++i)
    {
        const EdSymbol *symbol = &ED_SYMBOLS[i];
        /* A climb has no ladders or guards on it, and an interior has no
         * throwers; hiding what does not apply keeps the palette honest. */
        if (app->doc.grid.facade && symbol->interior_only)
            continue;
        if (!app->doc.grid.facade && symbol->facade_only)
            continue;

        if (symbol->group != group)
        {
            group = symbol->group;
            if (in_row != 0)
            {
                y += ED_SWATCH_PITCH_Y;
                in_row = 0;
            }
            y += 2.0f;
            ed_text(app, panel.x + 8.0f, y, 1.0f, BBC_CYAN, "%s",
                    ED_GROUP_NAMES[group]);
            y += 12.0f;
        }

        SDL_FRect rect = {panel.x + 4.0f + (float)in_row * ED_SWATCH_PITCH_X, y,
                          ED_SWATCH, ED_SWATCH};
        bool hover = point_in(app, &rect, app->mouse_x, app->mouse_y);
        bool active = app->brush == symbol->symbol;
        /* Air is nothing at all, which is not something a swatch can show;
         * it gets the outline of an empty tile instead. */
        if (symbol->symbol == ' ')
            bbc_frame(r, ED_RULE, rect.x, rect.y, rect.w, rect.h);
        else if (symbol->symbol == '.')
        {
            bbc_frame(r, ED_RULE, rect.x, rect.y, rect.w, rect.h);
            bbc_block(r, BBC_CYAN, rect.x + 3.0f * BBC_PX_W,
                      rect.y + 7.0f * BBC_PX_H, 2, 2);
        }
        else
            ed_draw_symbol(r, paint, symbol->symbol, rect.x, rect.y, rect.w,
                           app->time);
        /* The brush in hand flashes yellow and blue in the gap round its
         * swatch; the one under the pointer is framed in white. */
        if (active || hover)
        {
            BbcColour ring = hover ? BBC_WHITE : bbc_flash(BBC_YELLOW);
            bbc_frame(r, ring, rect.x - (float)BBC_PX_W, rect.y - (float)BBC_PX_H,
                      rect.w + 2.0f * BBC_PX_W, rect.h + 2.0f * BBC_PX_H);
        }
        if (hover)
        {
            SDL_snprintf(app->hint, sizeof(app->hint), "%c  %s - %s",
                         symbol->symbol == ' ' ? '_' : symbol->symbol,
                         symbol->name, symbol->detail);
            if (app->clicked)
                app->brush = symbol->symbol;
        }

        in_row++;
        if (in_row >= per_row)
        {
            in_row = 0;
            y += ED_SWATCH_PITCH_Y;
        }
    }
    if (in_row != 0)
        y += ED_SWATCH_PITCH_Y;
    y += 6.0f;

    title_bar(app, panel.x, y, inner_w, "GEOMETRY");
    y += ED_BAR_H + 4.0f;
    struct
    {
        const char *label;
        const char *tip;
        int action;
    } geometry[] = {
        {"row +", "Ctrl+Up  insert a row at the cursor", 0},
        {"row -", "Ctrl+Down  delete the row under the cursor", 1},
        {"col +", "Ctrl+Left  insert a column at the cursor", 2},
        {"col -", "Ctrl+Right  delete the column under the cursor", 3},
        {"wider", "grow the map by one column on the right", 4},
        {"narrower", "crop one column off the right", 5},
        {"taller", "grow the map by one row at the bottom", 6},
        {"shorter", "crop one row off the bottom", 7},
        {"mirror |", "Ctrl+H  mirror the selection, or the map, left to right", 8},
        {"mirror -", "Ctrl+J  mirror the selection, or the map, top to bottom", 9},
    };
    for (int i = 0; i < 10; ++i)
    {
        SDL_FRect rect = {panel.x + 8.0f + (float)(i % 2) * 104.0f,
                          y + (float)(i / 2) * 22.0f, 100.0f, 20.0f};
        if (!ed_button(app, rect, geometry[i].label, false, geometry[i].tip))
            continue;
        switch (geometry[i].action)
        {
        case 0:
            ed_insert_row(app, true);
            break;
        case 1:
            ed_insert_row(app, false);
            break;
        case 2:
            ed_insert_col(app, true);
            break;
        case 3:
            ed_insert_col(app, false);
            break;
        case 4:
            ed_resize(app, 1, 0);
            break;
        case 5:
            ed_resize(app, -1, 0);
            break;
        case 6:
            ed_resize(app, 0, 1);
            break;
        case 7:
            ed_resize(app, 0, -1);
            break;
        case 8:
            ed_mirror(app, true);
            break;
        default:
            ed_mirror(app, false);
            break;
        }
    }
    y += 5.0f * 22.0f;

    /* How far the panel can scroll before its last button is on screen.
     * Measured from the unscrolled top: `y` already has the scroll taken off
     * it, and a ceiling that shrinks as the panel scrolls stops the wheel half
     * way, with the geometry keys still under the status bar. */
    float overflow = y + app->palette_scroll - (panel.y + panel.h) + 8.0f;
    app->palette_overflow = overflow > 0.0f ? overflow : 0.0f;
    if (app->palette_scroll > app->palette_overflow)
        app->palette_scroll = app->palette_overflow;

    SDL_SetRenderClipRect(r, NULL);
}

/* ---- Right panel: the report ------------------------------------------- */

static BbcColour severity_colour(EdSeverity severity)
{
    switch (severity)
    {
    case ED_SEV_ERROR:
        return BBC_RED;
    case ED_SEV_WARN:
        return BBC_YELLOW;
    default:
        return BBC_CYAN;
    }
}

/* Write `text` at (x, y) and return where the next piece starts. */
static float text_run(EditorApp *app, float x, float y, BbcColour colour,
                      const char *text)
{
    ed_text(app, x, y, 1.0f, colour, "%s", text);
    return x + ed_text_width(text, 1.0f);
}

static void draw_info_block(EditorApp *app, float x, float y, float width)
{
    const EdReport *report = &app->report;
    float bar_x = app->right_panel.x + (float)BBC_PX_W;
    float bar_w = app->right_panel.w - (float)BBC_PX_W;
    title_bar(app, bar_x, y, bar_w, "MAP");
    y += ED_BAR_H + 3.0f;
    ed_text(app, x, y, 1.0f, BBC_WHITE, "%d x %d tiles  %s  budget %d",
            app->doc.grid.width, app->doc.grid.height,
            app->doc.grid.facade ? "climb" : "interior", report->budget);
    y += 12.0f;

    int guards = report->counts['M'] + report->counts['W'];
    ed_text(app, x, y, 1.0f, BBC_WHITE, "%d guards  %d dogs  %d mines", guards,
            report->counts['W'], report->counts['X']);
    y += 12.0f;
    ed_text(app, x, y, 1.0f, BBC_WHITE, "%d spikes  %d fans  %d crates",
            report->counts['^'], report->counts['O'], report->counts['B']);
    y += 12.0f;
    ed_text(app, x, y, 1.0f, BBC_WHITE, "%d cards  %d terminals  %d alarms",
            report->counts['C'], report->counts['T'], report->counts['A']);
    y += 12.0f;
    ed_text(app, x, y, 1.0f, BBC_WHITE, "%d doors  %d throwers  %d birds",
            report->counts['D'], report->counts['r'], report->counts['v']);
    y += 14.0f;

    /* The campaign's rising pressure, with this sector in place. */
    int number = editor_path_level_number(app->doc.path);
    if (number <= 0)
        return;
    title_bar(app, bar_x, y, bar_w, "CAMPAIGN PRESSURE");
    y += ED_BAR_H + 3.0f;
    int highest = 1;
    for (int i = 0; i < app->campaign.count; ++i)
    {
        int budget = i + 1 == number ? report->budget
                                     : app->campaign.levels[i].budget;
        if (budget > highest)
            highest = budget;
    }
    /* A whole number of BBC pixels a sector, so every bar is the same width
     * on the glass rather than alternating between two. */
    int count = app->campaign.count > 0 ? app->campaign.count : 1;
    int cells = (int)(width / (float)BBC_PX_W) / count;
    if (cells < 2)
        cells = 2;
    float step = (float)(cells * BBC_PX_W);
    float bar_h = 34.0f;
    /* The floor the bars stand on, so a sector with nothing on it still has a
     * place in the row. */
    bbc_fill(app->renderer, ED_RULE, x, y + bar_h, step * (float)count,
             (float)BBC_PX_H);
    for (int i = 0; i < app->campaign.count; ++i)
    {
        const EdCampaignLevel *level = &app->campaign.levels[i];
        if (!level->loaded)
            continue;
        int budget = i + 1 == number ? report->budget : level->budget;
        int rows = (int)ceilf(bar_h / (float)BBC_PX_H * (float)budget /
                              (float)highest);
        float h = (float)(rows * BBC_PX_H);
        BbcColour colour = level->facade ? BBC_CYAN : BBC_BLUE;
        if (i + 1 == number)
            colour = level->facade ? BBC_WHITE : BBC_YELLOW;
        bbc_fill(app->renderer, colour, x + (float)i * step, y + bar_h - h,
                 step - (float)BBC_PX_W, h);
    }
    y += bar_h + 4.0f;
    ed_text(app, x, y, 1.0f, BBC_CYAN, "sector 1 to %d, left to right",
            app->campaign.count);
}

static void draw_right_panel(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    SDL_FRect panel = app->right_panel;
    bbc_fill(r, BBC_BLACK, panel.x, panel.y, panel.w, panel.h);
    bbc_fill(r, ED_RULE, panel.x, panel.y, (float)BBC_PX_W, panel.h);

    const EdReport *report = &app->report;
    float x = panel.x + 12.0f;
    float y = panel.y;
    float width = panel.w - 20.0f;

    /* The verdict is the panel's title bar, in the colour of the worst thing
     * on the list; a map the tests would reject also gets a lamp that will
     * not stop flashing. */
    BbcColour verdict = report->errors > 0
                            ? BBC_RED
                            : (report->warnings > 0 ? BBC_YELLOW : BBC_GREEN);
    const char *headline =
        report->errors > 0
            ? "THE TESTS WOULD REJECT THIS MAP"
            : (report->warnings > 0 ? "IT LOADS, WITH RESERVATIONS"
                                    : "CLEAN");
    bbc_fill(r, verdict, panel.x + (float)BBC_PX_W, y,
             panel.w - (float)BBC_PX_W, 20.0f);
    ed_text(app, x, y + 6.0f, 1.0f,
            report->errors > 0 ? BBC_WHITE : BBC_BLACK, "%s", headline);
    if (report->errors > 0)
    {
        bbc_fill(r, BBC_BLACK, panel.x + panel.w - 24.0f, y + 4.0f, 16.0f,
                 12.0f);
        bbc_fill(r, bbc_flash(BBC_RED), panel.x + panel.w - 20.0f, y + 6.0f,
                 8.0f, 8.0f);
    }
    y += 26.0f;

    /* The tally, each count in its own severity's colour while it is not
     * nought — the eye finds the red figure before it reads the line. */
    char part[32];
    float tx = x;
    SDL_snprintf(part, sizeof(part), "%d errors", report->errors);
    tx = text_run(app, tx, y, report->errors > 0 ? BBC_RED : BBC_WHITE, part);
    SDL_snprintf(part, sizeof(part), "  %d warnings", report->warnings);
    tx = text_run(app, tx, y, report->warnings > 0 ? BBC_YELLOW : BBC_WHITE,
                  part);
    SDL_snprintf(part, sizeof(part), "  %d notes", report->notes);
    text_run(app, tx, y, report->notes > 0 ? BBC_CYAN : BBC_WHITE, part);
    y += 16.0f;

    float info_height = 186.0f;
    float list_bottom = panel.y + panel.h - info_height;

    for (int i = app->findings_scroll; i < report->count; ++i)
    {
        const EdFinding *finding = &report->findings[i];
        char text[ED_FINDING_LEN + 32];
        if (finding->col >= 0)
        {
            SDL_snprintf(text, sizeof(text), "%s  [%d,%d]", finding->text,
                         finding->col, finding->row);
        }
        else
        {
            SDL_snprintf(text, sizeof(text), "%s", finding->text);
        }

        if (y + 30.0f > list_bottom)
        {
            ed_text(app, x, y, 1.0f, BBC_CYAN, "... %d more, scroll the panel",
                    report->count - i);
            break;
        }

        float height = text_block_ex(app, x + 8.0f, y, width - 8.0f, 1.0f,
                                     BBC_WHITE, 3, text, false);

        /* The row is however tall the wrapped text turned out to be, so a
         * click on its second line still means that finding. Under the
         * pointer it is laid on a blue bar, the highlight that does not hide
         * the type. */
        SDL_FRect row = {panel.x + 2.0f, y - 2.0f, panel.w - 4.0f,
                         height + 4.0f};
        bool hover = point_in(app, &row, app->mouse_x, app->mouse_y);
        if (hover)
        {
            bbc_fill(r, BBC_BLUE, panel.x + (float)BBC_PX_W, y - 2.0f,
                     panel.w - (float)BBC_PX_W, height + 2.0f);
            if (app->clicked && finding->col >= 0)
                ed_focus_tile(app, finding->col, finding->row);
        }
        bbc_fill(r, severity_colour(finding->severity), x - 4.0f, y, 4.0f,
                 8.0f);
        text_block(app, x + 8.0f, y, width - 8.0f, 1.0f, BBC_WHITE, 3, text);
        y += height + 5.0f;
    }
    if (report->count == 0)
    {
        text_block(app, x, y, width, 1.0f, BBC_GREEN, 3,
                   "Nothing to report: this map builds, it loads, and the route "
                   "model can finish it.");
    }
    if (report->dropped > 0)
    {
        ed_text(app, x, list_bottom - 12.0f, 1.0f, BBC_CYAN,
                "%d findings not listed", report->dropped);
    }

    draw_info_block(app, x, list_bottom + 2.0f, width);
}

/* ---- Status bar and help ----------------------------------------------- */

static void draw_status_bar(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    float y = (float)app->win_h - ED_BOTTOM_H;
    bbc_fill(r, BBC_BLACK, 0.0f, y, (float)app->win_w, ED_BOTTOM_H);
    bbc_fill(r, ED_RULE, 0.0f, y, (float)app->win_w, (float)BBC_PX_H);

    char cell[8] = "-";
    if (app->cursor_col >= 0 && app->cursor_row >= 0)
    {
        char at = editor_doc_get(&app->doc, app->cursor_col, app->cursor_row);
        SDL_snprintf(cell, sizeof(cell), "%c", at == ' ' ? '_' : at);
    }
    ed_text(app, 8.0f, y + 10.0f, 1.0f, BBC_CYAN,
            "[%3d,%3d] %s   zoom %d%%   %s", app->cursor_col, app->cursor_row,
            cell, (int)(app->zoom * 100.0f), ED_TOOL_NAMES[app->tool]);

    if (app->status_timer > 0.0f)
    {
        /* A good outcome is green type; a bad one is inverse video, white on
         * red, because it is the one line on the screen that must be read. */
        if (app->status_bad)
            bbc_text_inverse(r, 300.0f, y + 10.0f, 1.0f, BBC_RED, BBC_WHITE,
                             app->status);
        else
            ed_text(app, 300.0f, y + 10.0f, 1.0f, BBC_GREEN, "%s", app->status);
    }
    else
    {
        ed_text(app, 300.0f, y + 10.0f, 1.0f, BBC_WHITE, "%s", app->hint);
    }

    const char *hint = "F1 help";
    float hint_x = (float)app->win_w - ed_text_width(hint, 1.0f) - 8.0f;
    bbc_text_inverse(r, hint_x, y + 10.0f, 1.0f, BBC_BLUE, BBC_YELLOW, hint);
}

static const char *const HELP_LINES[] = {
    "MOUSE",
    "  left drag        apply the current tool",
    "  right drag       erase back to air",
    "  middle drag      pan the canvas",
    "  wheel            zoom around the pointer",
    "  alt + left       pick up the character under the pointer",
    "",
    "TOOLS",
    "  1 - 7            brush, line, rect, box, fill, pick, select",
    "  any legend key   choose that character as the brush ('#', 'H', 'M' ...)",
    "  shift + click    with select: extend the selection to here",
    "",
    "FILE",
    "  ctrl+S           save        ctrl+R  reload from disk",
    "  ctrl+N           new sector  ctrl+shift+N  new climb",
    "  [ ]              previous / next map",
    "  F5               save, build and play this sector",
    "",
    "EDIT",
    "  ctrl+Z / ctrl+Y  undo / redo",
    "  ctrl+C / X / V   copy, cut and paste the selection",
    "  delete           clear the selection to air",
    "  ctrl+H / ctrl+J  mirror the selection, or the whole map",
    "  ctrl+arrows      insert or delete a row or column at the cursor",
    "  escape           drop the selection",
    "",
    "VIEW",
    "  F2  real art or schematic     F3  grid",
    "  F4  route overlay: green is reachable, red cannot get back out",
    "  F6  MODE FACADE               F7 / F8  theme",
    "  F9  rewrite the SPAWNS line for the doors the map now has",
    "  0                             fit the map to the window",
    "",
    "The report on the right is the same set of rules `make test` applies.",
    "Click a finding to jump to the tile it is about.",
};

/* A heading is a line in capitals; everything else on the sheet is prose. */
static bool help_heading(const char *line)
{
    if (line[0] == '\0' || line[0] == ' ')
        return false;
    for (const char *at = line; *at != '\0'; ++at)
    {
        if (*at >= 'a' && *at <= 'z')
            return false;
    }
    return true;
}

/* An indented line is a key and what it does; the key, up to the first gap of
 * two spaces, is drawn in cyan so the column of keys reads down the sheet. */
static void help_line(EditorApp *app, float x, float y, const char *line)
{
    if (help_heading(line))
    {
        ed_text(app, x, y, 1.0f, BBC_YELLOW, "%s", line);
        return;
    }
    if (line[0] != ' ')
    {
        ed_text(app, x, y, 1.0f, BBC_WHITE, "%s", line);
        return;
    }
    const char *key = line;
    while (*key == ' ')
        key++;
    const char *end = SDL_strstr(key, "  ");
    if (end == NULL)
    {
        ed_text(app, x, y, 1.0f, BBC_CYAN, "%s", line);
        return;
    }
    int key_len = (int)(end - line);
    ed_text(app, x, y, 1.0f, BBC_CYAN, "%.*s", key_len, line);
    ed_text(app, x + (float)key_len * ED_FONT, y, 1.0f, BBC_WHITE, "%s", end);
}

static void draw_help(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    const int count = (int)(sizeof(HELP_LINES) / sizeof(HELP_LINES[0]));
    float longest = 0.0f;
    for (int i = 0; i < count; ++i)
        longest = SDL_max(longest, ed_text_width(HELP_LINES[i], 1.0f));

    float line_h = 12.0f;
    float plate_w = bbc_snap_x(longest + 64.0f);
    float plate_h = bbc_snap_y((float)count * line_h + 96.0f);
    float px = bbc_snap_x(SDL_max(4.0f, ((float)app->win_w - plate_w) * 0.5f));
    float py = bbc_snap_y(SDL_max(4.0f, ((float)app->win_h - plate_h) * 0.5f));

    /* The editor behind the sheet is shaded the BBC's way, with a black
     * checkerboard laid over it, and the sheet itself is cleared to black. */
    float ww = (float)app->win_w, wh = (float)app->win_h;
    bbc_stipple(r, BBC_BLACK, BBC_DITHER_CHECK, 0.0f, 0.0f, ww, py);
    bbc_stipple(r, BBC_BLACK, BBC_DITHER_CHECK, 0.0f, py + plate_h, ww,
                wh - py - plate_h);
    bbc_stipple(r, BBC_BLACK, BBC_DITHER_CHECK, 0.0f, py, px, plate_h);
    bbc_stipple(r, BBC_BLACK, BBC_DITHER_CHECK, px + plate_w, py,
                ww - px - plate_w, plate_h);

    /* A sheet that matters gets its border doubled. */
    bbc_fill(r, BBC_BLACK, px, py, plate_w, plate_h);
    bbc_frame(r, BBC_CYAN, px, py, plate_w, plate_h);
    bbc_frame(r, BBC_CYAN, px + 2.0f * BBC_PX_W, py + 2.0f * BBC_PX_H,
              plate_w - 4.0f * BBC_PX_W, plate_h - 4.0f * BBC_PX_H);

    float x = px + 32.0f;
    float y = py + 12.0f;
    bbc_fill(r, ED_TITLE_BAR, px + 16.0f, y, plate_w - 32.0f, 24.0f);
    ed_text(app, x, y + 4.0f, 2.0f, ED_TITLE_INK, "CHUCK LEVEL EDITOR");
    y += 36.0f;

    for (int i = 0; i < count; ++i)
    {
        help_line(app, x, y, HELP_LINES[i]);
        y += line_h;
    }
    const char *footer = "F1 or escape closes this";
    bbc_text_inverse(r, px + plate_w - 32.0f - ed_text_width(footer, 1.0f),
                     py + plate_h - 24.0f, 1.0f, BBC_BLUE, BBC_YELLOW, footer);
}

void ed_draw_chrome(EditorApp *app)
{
    /* The hint belongs to whatever the pointer is over this frame; a message
     * from an action lives in `status` and outlives it. */
    app->hint[0] = '\0';

    draw_left_panel(app);
    draw_right_panel(app);
    draw_top_bar(app);
    draw_status_bar(app);
    if (app->show_help)
        draw_help(app);
}
