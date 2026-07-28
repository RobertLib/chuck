#include "editor.h"

#include "fx.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define ED_FONT 8.0f

static const char *const ED_TOOL_NAMES[ED_TOOL_COUNT] = {
    "Brush", "Line", "Rect", "Box", "Fill", "Pick", "Select"};

static const char *const ED_TOOL_HINTS[ED_TOOL_COUNT] = {
    "1  paint single tiles; drag to draw freehand",
    "2  drag a straight run of tiles",
    "3  drag a hollow rectangle",
    "4  drag a filled rectangle",
    "5  flood the connected run of one character",
    "6  pick up the character under the pointer",
    "7  drag out a selection to copy, mirror or clear"};

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
    return (float)SDL_strlen(text) * ED_FONT * scale;
}

void ed_text(EditorApp *app, float x, float y, float scale, SDL_Color color,
             const char *format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    SDL_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    SDL_SetRenderScale(app->renderer, scale, scale);
    SDL_SetRenderDrawColor(app->renderer, color.r, color.g, color.b, 255);
    SDL_RenderDebugText(app->renderer, x / scale, y / scale, buffer);
    SDL_SetRenderScale(app->renderer, 1.0f, 1.0f);
}

/* Draw `text` inside `width`, breaking on spaces. Returns the height used. */
static float text_block(EditorApp *app, float x, float y, float width,
                        float scale, SDL_Color color, int max_lines,
                        const char *text)
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
        char line[160];
        int copy = take < (int)sizeof(line) - 1 ? take : (int)sizeof(line) - 1;
        memcpy(line, at, (size_t)copy);
        line[copy] = '\0';
        ed_text(app, x, y + (float)lines * line_height, scale, color, "%s",
                line);
        lines++;
        at += take;
        while (*at == ' ')
            at++;
    }
    return (float)lines * line_height;
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

bool ed_button(EditorApp *app, SDL_FRect rect, const char *label, bool active,
               const char *tooltip)
{
    bool hover = point_in(app, &rect, app->mouse_x, app->mouse_y);
    SDL_Color face = active ? FX_CYAN_DK : FX_BASE;
    if (hover)
        face = active ? FX_CYAN_DK : FX_MID;
    fx_rect(app->renderer, face, rect.x, rect.y, rect.w, rect.h);
    fx_rect(app->renderer, hover ? FX_CYAN : FX_STEEL_DK, rect.x, rect.y,
            rect.w, 1.0f);

    SDL_Color ink = active ? FX_CYAN : (hover ? FX_CREAM : FX_PALE);
    float text_w = ed_text_width(label, 1.0f);
    ed_text(app, rect.x + (rect.w - text_w) * 0.5f,
            rect.y + (rect.h - ED_FONT) * 0.5f, 1.0f, ink, "%s", label);

    if (hover && tooltip != NULL)
        SDL_snprintf(app->hint, sizeof(app->hint), "%s", tooltip);
    return hover && app->clicked;
}

/* ---- Top bar ----------------------------------------------------------- */

static void draw_top_bar(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    fx_rect(r, FX_SHADOW, 0.0f, 0.0f, (float)app->win_w, ED_TOP_H);
    fx_rect(r, FX_STEEL_DK, 0.0f, ED_TOP_H - 1.0f, (float)app->win_w, 1.0f);

    float x = 6.0f;
    for (int i = 0; i < app->file_count; ++i)
    {
        const EdFile *file = &app->files[i];
        float w = file->sublevel ? 46.0f : 26.0f;
        SDL_FRect rect = {x, 4.0f, w, 20.0f};
        bool active = i == app->current_file;
        char label[16];
        if (file->sublevel)
            SDL_snprintf(label, sizeof(label), "%.5s", file->label);
        else
            SDL_snprintf(label, sizeof(label), "%d", file->number);
        char tip[128];
        SDL_snprintf(tip, sizeof(tip), "Open %s", file->path);
        if (ed_button(app, rect, label, active, tip))
        {
            ed_open_file(app, i);
        }
        if (active && app->doc.dirty)
            fx_rect(r, FX_AMBER, rect.x, rect.y + rect.h - 2.0f, rect.w, 2.0f);
        x += w + 2.0f;
    }

    x += 12.0f;
    if (ed_button(app, (SDL_FRect){x, 4.0f, 60.0f, 20.0f}, "+ Sector", false,
                  "Ctrl+N  add the next campaign sector as an interior"))
    {
        ed_new_level(app, false);
    }
    x += 64.0f;
    if (ed_button(app, (SDL_FRect){x, 4.0f, 58.0f, 20.0f}, "+ Climb", false,
                  "Ctrl+Shift+N  add the next campaign sector as a facade"))
    {
        ed_new_level(app, true);
    }

    /* Second row: what the document is, and what happens to it. */
    const char *path = app->doc.path[0] != '\0' ? app->doc.path : "(unsaved)";
    ed_text(app, 8.0f, 34.0f, 1.0f, app->doc.dirty ? FX_AMBER : FX_PALE,
            "%s%s", path, app->doc.dirty ? " *" : "");

    float bx = (float)app->win_w - 6.0f;
    struct
    {
        const char *label;
        float width;
        const char *tip;
        int action;
    } actions[] = {
        {"Playtest F5", 84.0f, "F5  save, build and run the game in this sector", 0},
        {"Reload", 50.0f, "Ctrl+R  throw away changes and read the file again", 1},
        {"Save", 42.0f, "Ctrl+S  write the map back to its file", 2},
    };
    for (int i = 0; i < 3; ++i)
    {
        bx -= actions[i].width;
        SDL_FRect rect = {bx, 30.0f, actions[i].width, 20.0f};
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

    float mx = 300.0f;
    if (ed_button(app, (SDL_FRect){mx, 30.0f, 96.0f, 20.0f},
                  app->doc.grid.facade ? "MODE FACADE" : "MODE INTERIOR",
                  app->doc.grid.facade,
                  "F6  a facade is climbed four ways with no gravity"))
    {
        ed_toggle_facade(app);
    }
    mx += 100.0f;
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
    if (ed_button(app, (SDL_FRect){mx + 18.0f, 30.0f, 150.0f, 20.0f}, theme,
                  false, "F8 / F7  the palette, the backdrop and the score"))
    {
        ed_cycle_theme(app, 1);
    }
    if (ed_button(app, (SDL_FRect){mx + 170.0f, 30.0f, 16.0f, 20.0f}, ">",
                  false, "F8  next theme"))
    {
        ed_cycle_theme(app, 1);
    }
}

/* ---- Left panel: tools, brush, palette, geometry ----------------------- */

static void draw_left_panel(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    SDL_FRect panel = app->left_panel;
    fx_rect(r, FX_NIGHT, panel.x, panel.y, panel.w, panel.h);
    fx_rect(r, FX_STEEL_DK, panel.x + panel.w - 1.0f, panel.y, 1.0f, panel.h);

    SDL_Rect clip = {(int)panel.x, (int)panel.y, (int)panel.w, (int)panel.h};
    SDL_SetRenderClipRect(r, &clip);

    float y = panel.y + 6.0f - app->palette_scroll;
    ed_text(app, panel.x + 8.0f, y, 1.0f, FX_STEEL_LT, "TOOLS");
    y += 14.0f;
    for (int i = 0; i < ED_TOOL_COUNT; ++i)
    {
        float w = 68.0f;
        SDL_FRect rect = {panel.x + 8.0f + (float)(i % 3) * (w + 4.0f),
                          y + (float)(i / 3) * 22.0f, w, 19.0f};
        char label[24];
        SDL_snprintf(label, sizeof(label), "%d %s", i + 1, ED_TOOL_NAMES[i]);
        if (ed_button(app, rect, label, app->tool == (EdTool)i,
                      ED_TOOL_HINTS[i]))
        {
            app->tool = (EdTool)i;
        }
    }
    y += 3.0f * 22.0f + 6.0f;

    /* What the brush is, and what the legend says about it. */
    const EdSymbol *brush = editor_symbol(app->brush);
    fx_rect(r, FX_SHADOW, panel.x + 8.0f, y, panel.w - 16.0f, 52.0f);
    ed_draw_symbol(r, app->brush, panel.x + 12.0f, y + 4.0f, 32.0f, app->time);
    ed_text(app, panel.x + 50.0f, y + 5.0f, 1.25f, FX_CREAM, "%c  %s",
            app->brush == ' ' ? '_' : app->brush,
            brush != NULL ? brush->name : "unknown");
    if (brush != NULL)
    {
        text_block(app, panel.x + 50.0f, y + 21.0f, panel.w - 60.0f, 1.0f,
                   FX_STEEL_LT, 3, brush->detail);
    }
    y += 58.0f;

    ed_text(app, panel.x + 8.0f, y, 1.0f, FX_STEEL_LT, "PALETTE");
    y += 14.0f;

    float cell = 28.0f;
    int per_row = (int)((panel.w - 16.0f) / cell);
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
                y += cell;
                in_row = 0;
            }
            y += 4.0f;
            ed_text(app, panel.x + 8.0f, y, 1.0f, FX_STEEL, "%s",
                    ED_GROUP_NAMES[group]);
            y += 12.0f;
        }

        SDL_FRect rect = {panel.x + 8.0f + (float)in_row * cell, y, cell - 2.0f,
                          cell - 2.0f};
        bool hover = point_in(app, &rect, app->mouse_x, app->mouse_y);
        bool active = app->brush == symbol->symbol;
        fx_rect(r, active ? FX_CYAN_DK : (hover ? FX_MID : FX_BASE), rect.x,
                rect.y, rect.w, rect.h);
        ed_draw_symbol(r, symbol->symbol, rect.x + 2.0f, rect.y + 2.0f,
                       rect.w - 4.0f, app->time);
        if (active)
            fx_rect(r, FX_CYAN, rect.x, rect.y, rect.w, 2.0f);
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
            y += cell;
        }
    }
    if (in_row != 0)
        y += cell;
    y += 8.0f;

    ed_text(app, panel.x + 8.0f, y, 1.0f, FX_STEEL_LT, "GEOMETRY");
    y += 14.0f;
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
        SDL_FRect rect = {panel.x + 8.0f + (float)(i % 2) * 102.0f,
                          y + (float)(i / 2) * 21.0f, 98.0f, 18.0f};
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
    y += 5.0f * 21.0f;

    /* How far the panel can scroll before its last button is on screen. */
    float overflow = y - (panel.y + panel.h) + 8.0f;
    app->palette_overflow = overflow > 0.0f ? overflow : 0.0f;
    if (app->palette_scroll > app->palette_overflow)
        app->palette_scroll = app->palette_overflow;

    SDL_SetRenderClipRect(r, NULL);
}

/* ---- Right panel: the report ------------------------------------------- */

static SDL_Color severity_color(EdSeverity severity)
{
    switch (severity)
    {
    case ED_SEV_ERROR:
        return FX_RED;
    case ED_SEV_WARN:
        return FX_AMBER;
    default:
        return FX_CYAN;
    }
}

static void draw_info_block(EditorApp *app, float x, float y, float width)
{
    const EdReport *report = &app->report;
    ed_text(app, x, y, 1.0f, FX_STEEL_LT, "MAP");
    y += 13.0f;
    ed_text(app, x, y, 1.0f, FX_PALE, "%d x %d tiles  %s  budget %d",
            app->doc.grid.width, app->doc.grid.height,
            app->doc.grid.facade ? "climb" : "interior", report->budget);
    y += 12.0f;

    int guards = report->counts['M'] + report->counts['W'];
    ed_text(app, x, y, 1.0f, FX_PALE, "%d guards  %d dogs  %d mines", guards,
            report->counts['W'], report->counts['X']);
    y += 12.0f;
    ed_text(app, x, y, 1.0f, FX_PALE, "%d spikes  %d fans  %d crates",
            report->counts['^'], report->counts['O'], report->counts['B']);
    y += 12.0f;
    ed_text(app, x, y, 1.0f, FX_PALE, "%d cards  %d terminals  %d alarms",
            report->counts['C'], report->counts['T'], report->counts['A']);
    y += 12.0f;
    ed_text(app, x, y, 1.0f, FX_PALE, "%d doors  %d throwers  %d birds",
            report->counts['D'], report->counts['r'], report->counts['v']);
    y += 14.0f;

    /* The campaign's rising pressure, with this sector in place. */
    int number = editor_path_level_number(app->doc.path);
    if (number <= 0)
        return;
    ed_text(app, x, y, 1.0f, FX_STEEL_LT, "CAMPAIGN PRESSURE");
    y += 13.0f;
    float bar_x = x;
    float bar_w = width;
    int highest = 1;
    for (int i = 0; i < app->campaign.count; ++i)
    {
        int budget = i + 1 == number ? report->budget
                                     : app->campaign.levels[i].budget;
        if (budget > highest)
            highest = budget;
    }
    float step = bar_w / (float)(app->campaign.count > 0 ? app->campaign.count : 1);
    for (int i = 0; i < app->campaign.count; ++i)
    {
        const EdCampaignLevel *level = &app->campaign.levels[i];
        if (!level->loaded)
            continue;
        int budget = i + 1 == number ? report->budget : level->budget;
        float h = 34.0f * (float)budget / (float)highest;
        SDL_Color color = level->facade ? FX_CYAN_DK : FX_STEEL;
        if (i + 1 == number)
            color = level->facade ? FX_CYAN : FX_AMBER;
        fx_rect(app->renderer, color, bar_x + (float)i * step, y + 34.0f - h,
                step - 2.0f, h);
    }
    y += 36.0f;
    ed_text(app, x, y, 1.0f, FX_STEEL, "sector 1 to %d, left to right",
            app->campaign.count);
}

static void draw_right_panel(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    SDL_FRect panel = app->right_panel;
    fx_rect(r, FX_NIGHT, panel.x, panel.y, panel.w, panel.h);
    fx_rect(r, FX_STEEL_DK, panel.x, panel.y, 1.0f, panel.h);

    const EdReport *report = &app->report;
    float x = panel.x + 8.0f;
    float y = panel.y + 6.0f;
    float width = panel.w - 16.0f;

    SDL_Color verdict = report->errors > 0
                            ? FX_RED
                            : (report->warnings > 0 ? FX_AMBER : FX_GREEN);
    const char *headline =
        report->errors > 0
            ? "THE TESTS WOULD REJECT THIS MAP"
            : (report->warnings > 0 ? "IT LOADS, WITH RESERVATIONS"
                                    : "CLEAN");
    fx_rect(r, fx_dim(verdict, 0.28f), panel.x, panel.y, panel.w, 20.0f);
    ed_text(app, x, y + 1.0f, 1.0f, verdict, "%s", headline);
    y += 22.0f;
    ed_text(app, x, y, 1.0f, FX_STEEL_LT, "%d errors  %d warnings  %d notes",
            report->errors, report->warnings, report->notes);
    y += 14.0f;

    float info_height = 186.0f;
    float list_bottom = panel.y + panel.h - info_height;

    for (int i = app->findings_scroll; i < report->count; ++i)
    {
        const EdFinding *finding = &report->findings[i];
        float height = 0.0f;
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
            ed_text(app, x, y, 1.0f, FX_STEEL, "... %d more, scroll the panel",
                    report->count - i);
            break;
        }

        fx_rect(r, severity_color(finding->severity), x - 4.0f, y + 1.0f, 3.0f,
                7.0f);
        height = text_block(app, x + 4.0f, y, width - 8.0f, 1.0f, FX_PALE, 3,
                            text);

        /* The row is however tall the wrapped text turned out to be, so a
         * click on its second line still means that finding. */
        SDL_FRect row = {panel.x + 2.0f, y - 2.0f, panel.w - 4.0f,
                         height + 4.0f};
        if (point_in(app, &row, app->mouse_x, app->mouse_y))
        {
            fx_rect_a(r, FX_CYAN, 22, row.x, row.y, row.w, row.h);
            if (app->clicked && finding->col >= 0)
                ed_focus_tile(app, finding->col, finding->row);
        }
        y += height + 5.0f;
    }
    if (report->count == 0)
    {
        text_block(app, x, y, width, 1.0f, FX_GREEN, 3,
                   "Nothing to report: this map builds, it loads, and the route "
                   "model can finish it.");
    }
    if (report->dropped > 0)
    {
        ed_text(app, x, list_bottom - 12.0f, 1.0f, FX_STEEL,
                "%d findings not listed", report->dropped);
    }

    fx_rect(r, FX_STEEL_DK, panel.x, list_bottom, panel.w, 1.0f);
    draw_info_block(app, x, list_bottom + 6.0f, width);
}

/* ---- Status bar and help ----------------------------------------------- */

static void draw_status_bar(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    float y = (float)app->win_h - ED_BOTTOM_H;
    fx_rect(r, FX_SHADOW, 0.0f, y, (float)app->win_w, ED_BOTTOM_H);
    fx_rect(r, FX_STEEL_DK, 0.0f, y, (float)app->win_w, 1.0f);

    char cell[8] = "-";
    if (app->cursor_col >= 0 && app->cursor_row >= 0)
    {
        char at = editor_doc_get(&app->doc, app->cursor_col, app->cursor_row);
        SDL_snprintf(cell, sizeof(cell), "%c", at == ' ' ? '_' : at);
    }
    ed_text(app, 8.0f, y + 9.0f, 1.0f, FX_STEEL_LT,
            "[%3d,%3d] %s   zoom %d%%   %s", app->cursor_col, app->cursor_row,
            cell, (int)(app->zoom * 100.0f), ED_TOOL_NAMES[app->tool]);

    if (app->status_timer > 0.0f)
    {
        ed_text(app, 300.0f, y + 9.0f, 1.0f,
                app->status_bad ? FX_RED : FX_GREEN, "%s", app->status);
    }
    else
    {
        ed_text(app, 300.0f, y + 9.0f, 1.0f, FX_STEEL, "%s", app->hint);
    }

    const char *hint = "F1 help";
    ed_text(app, (float)app->win_w - ed_text_width(hint, 1.0f) - 8.0f,
            y + 9.0f, 1.0f, FX_STEEL_LT, "%s", hint);
}

static void draw_help(EditorApp *app)
{
    SDL_Renderer *r = app->renderer;
    fx_rect_a(r, FX_INK, 235, 0.0f, 0.0f, (float)app->win_w,
              (float)app->win_h);
    float x = 60.0f;
    float y = 50.0f;
    ed_text(app, x, y, 2.0f, FX_CYAN, "CHUCK LEVEL EDITOR");
    y += 30.0f;

    static const char *const lines[] = {
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
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i)
    {
        const char *line = lines[i];
        bool header = line[0] != '\0' && line[0] != ' ';
        ed_text(app, x, y, 1.25f, header ? FX_AMBER : FX_PALE, "%s", line);
        y += 15.0f;
    }
    ed_text(app, x, (float)app->win_h - 40.0f, 1.25f, FX_STEEL_LT,
            "F1 or escape closes this");
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
