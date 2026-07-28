#include "editor_doc.h"

#include "rng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char editor_doc_fill_char(const EditorDoc *doc)
{
    return doc->grid.facade ? '.' : ' ';
}

static void grid_clear(EditorGrid *grid, char fill)
{
    for (int row = 0; row < MAX_LEVEL_HEIGHT; ++row)
    {
        for (int col = 0; col < MAX_LEVEL_WIDTH; ++col)
            grid->cells[row][col] = fill;
    }
}

static void history_reset(EditorDoc *doc)
{
    doc->undo_len = 0;
    doc->redo_len = 0;
}

/* Push `state` onto a stack, dropping the oldest entry when it is full. */
static void history_push(EditorGrid *stack, int *length,
                         const EditorGrid *state)
{
    if (*length >= ED_UNDO_DEPTH)
    {
        memmove(&stack[0], &stack[1], sizeof(stack[0]) * (ED_UNDO_DEPTH - 1));
        *length = ED_UNDO_DEPTH - 1;
    }
    stack[(*length)++] = *state;
}

void editor_doc_new(EditorDoc *doc, int width, int height, bool facade,
                    LevelTheme theme)
{
    memset(doc, 0, sizeof(*doc));
    if (width < 4)
        width = 4;
    if (height < 4)
        height = 4;
    if (width > MAX_LEVEL_WIDTH)
        width = MAX_LEVEL_WIDTH;
    if (height > MAX_LEVEL_HEIGHT)
        height = MAX_LEVEL_HEIGHT;

    doc->grid.width = width;
    doc->grid.height = height;
    doc->grid.facade = facade;
    doc->grid.has_theme = true;
    doc->grid.theme = theme;
    grid_clear(&doc->grid, editor_doc_fill_char(doc));

    /* A new interior starts as a sealed box with a floor to stand on, which is
     * the one shape every sector in the campaign shares. A new climb starts
     * blank: its masonry is drawn on, not carved out. */
    if (!facade)
    {
        for (int col = 0; col < width; ++col)
        {
            doc->grid.cells[0][col] = '#';
            doc->grid.cells[height - 1][col] = '#';
        }
        for (int row = 0; row < height; ++row)
        {
            doc->grid.cells[row][0] = '#';
            doc->grid.cells[row][width - 1] = '#';
        }
        doc->grid.cells[height - 2][2] = 'S';
        doc->grid.cells[height - 2][width - 3] = 'E';
    }
    else
    {
        doc->grid.cells[height - 2][width / 2] = 'S';
        doc->grid.cells[1][width / 2] = 'Y';
    }

    history_reset(doc);
}

static bool line_is(const char *line, size_t length, const char *literal)
{
    size_t want = strlen(literal);
    return length == want && strncmp(line, literal, want) == 0;
}

bool editor_doc_parse(EditorDoc *doc, const char *data, size_t size)
{
    if (data == NULL)
        return false;

    memset(doc, 0, sizeof(*doc));
    grid_clear(&doc->grid, ' ');

    /* Pass one: the grid, up to the blank line that ends it — the same rule
     * `level_load_data` uses. */
    size_t at = 0;
    int row = 0;
    int width = 0;
    while (at < size && row < MAX_LEVEL_HEIGHT)
    {
        size_t end = at;
        while (end < size && data[end] != '\n')
            end++;
        size_t length = end - at;
        if (length > 0 && data[at + length - 1] == '\r')
            length--;
        if (length == 0 && row > 0)
        {
            at = end < size ? end + 1 : size;
            break;
        }
        if (length > MAX_LEVEL_WIDTH)
            length = MAX_LEVEL_WIDTH;
        for (size_t col = 0; col < length; ++col)
            doc->grid.cells[row][col] = data[at + col];
        if ((int)length > width)
            width = (int)length;
        row++;
        if (end >= size)
        {
            at = size;
            break;
        }
        at = end + 1;
    }
    doc->grid.width = width > 0 ? width : 1;
    doc->grid.height = row > 0 ? row : 1;

    /* Pass two: the metadata lines after it. */
    while (at < size)
    {
        size_t end = at;
        while (end < size && data[end] != '\n')
            end++;
        size_t length = end - at;
        if (length > 0 && data[at + length - 1] == '\r')
            length--;
        const char *line = data + at;

        if (length == 0)
        {
            /* a blank separator between metadata lines */
        }
        else if (line_is(line, length, "MODE FACADE"))
        {
            doc->grid.facade = true;
        }
        else if (length > 6 && strncmp(line, "THEME ", 6) == 0)
        {
            if (!level_theme_from_name(line + 6, length - 6, &doc->grid.theme))
                return false;
            doc->grid.has_theme = true;
        }
        else if (length > 7 && strncmp(line, "SPAWNS ", 7) == 0)
        {
            doc->grid.has_spawns = true;
            doc->grid.spawn_count = 0;
            size_t scan = 7;
            while (scan < length && doc->grid.spawn_count < MAX_DOORS)
            {
                while (scan < length && (line[scan] == ' ' || line[scan] == '\t'))
                    scan++;
                if (scan >= length)
                    break;
                int value = 0;
                bool digits = false;
                while (scan < length && line[scan] >= '0' && line[scan] <= '9')
                {
                    value = value * 10 + (line[scan] - '0');
                    scan++;
                    digits = true;
                }
                if (!digits)
                    break;
                doc->grid.spawns[doc->grid.spawn_count++] = value;
            }
        }
        else if (doc->grid.extra_count < ED_MAX_EXTRA)
        {
            size_t copy = length < ED_EXTRA_LEN - 1 ? length : ED_EXTRA_LEN - 1;
            memcpy(doc->grid.extra[doc->grid.extra_count], line, copy);
            doc->grid.extra[doc->grid.extra_count][copy] = '\0';
            doc->grid.extra_count++;
        }

        if (end >= size)
            break;
        at = end + 1;
    }

    history_reset(doc);
    return true;
}

bool editor_doc_load(EditorDoc *doc, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return false;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return false;
    }

    char *data = (char *)malloc((size_t)size + 1);
    if (data == NULL)
    {
        fclose(file);
        return false;
    }
    size_t read = fread(data, 1, (size_t)size, file);
    fclose(file);
    data[read] = '\0';

    bool ok = editor_doc_parse(doc, data, read);
    free(data);
    if (ok)
    {
        snprintf(doc->path, sizeof(doc->path), "%s", path);
        doc->dirty = false;
    }
    return ok;
}

/* A tiny append-only cursor so serialising can measure and write with one
 * body of code, whether or not the caller's buffer is big enough. */
typedef struct
{
    char *out;
    size_t capacity;
    size_t length;
} TextSink;

static void sink_char(TextSink *sink, char c)
{
    if (sink->out != NULL && sink->length + 1 < sink->capacity)
        sink->out[sink->length] = c;
    sink->length++;
}

static void sink_text(TextSink *sink, const char *text)
{
    for (const char *c = text; *c != '\0'; ++c)
        sink_char(sink, *c);
}

static void sink_int(TextSink *sink, int value)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", value);
    sink_text(sink, buffer);
}

size_t editor_doc_serialize(const EditorDoc *doc, char *out, size_t capacity)
{
    TextSink sink = {out, capacity, 0};
    char fill = editor_doc_fill_char(doc);

    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int col = 0; col < doc->grid.width; ++col)
        {
            char cell = doc->grid.cells[row][col];
            sink_char(&sink, cell == '\0' ? fill : cell);
        }
        sink_char(&sink, '\n');
    }

    /* The blank line exists to end the grid section. A map with nothing to say
     * after its grid — the restroom, whose theme its fittings imply — ends at
     * the last row, and adding a separator to it would rewrite the file. */
    bool has_metadata = doc->grid.facade || doc->grid.has_theme ||
                        doc->grid.extra_count > 0 ||
                        (doc->grid.has_spawns && doc->grid.spawn_count > 0);
    if (!has_metadata)
    {
        if (out != NULL && capacity > 0)
            out[sink.length < capacity ? sink.length : capacity - 1] = '\0';
        return sink.length;
    }
    sink_char(&sink, '\n');

    if (doc->grid.facade)
        sink_text(&sink, "MODE FACADE\n");
    if (doc->grid.has_spawns && doc->grid.spawn_count > 0)
    {
        sink_text(&sink, "SPAWNS");
        for (int i = 0; i < doc->grid.spawn_count; ++i)
        {
            sink_char(&sink, ' ');
            sink_int(&sink, doc->grid.spawns[i]);
        }
        sink_char(&sink, '\n');
    }
    if (doc->grid.has_theme)
    {
        sink_text(&sink, "THEME ");
        sink_text(&sink, level_theme_name(doc->grid.theme));
        sink_char(&sink, '\n');
    }
    for (int i = 0; i < doc->grid.extra_count; ++i)
    {
        sink_text(&sink, doc->grid.extra[i]);
        sink_char(&sink, '\n');
    }

    if (out != NULL && capacity > 0)
        out[sink.length < capacity ? sink.length : capacity - 1] = '\0';
    return sink.length;
}

bool editor_doc_save(EditorDoc *doc, const char *path)
{
    size_t needed = editor_doc_serialize(doc, NULL, 0);
    char *text = (char *)malloc(needed + 1);
    if (text == NULL)
        return false;
    editor_doc_serialize(doc, text, needed + 1);

    FILE *file = fopen(path, "wb");
    if (file == NULL)
    {
        free(text);
        return false;
    }
    size_t written = fwrite(text, 1, needed, file);
    bool ok = fclose(file) == 0 && written == needed;
    free(text);

    if (ok)
    {
        if (path != doc->path)
            snprintf(doc->path, sizeof(doc->path), "%s", path);
        doc->dirty = false;
    }
    return ok;
}

char editor_doc_get(const EditorDoc *doc, int col, int row)
{
    if (col < 0 || row < 0 || col >= doc->grid.width || row >= doc->grid.height)
        return '\0';
    char cell = doc->grid.cells[row][col];
    return cell == '\0' ? ' ' : cell;
}

bool editor_doc_set(EditorDoc *doc, int col, int row, char value)
{
    if (col < 0 || row < 0 || col >= doc->grid.width || row >= doc->grid.height)
        return false;
    if (doc->grid.cells[row][col] == value)
        return false;
    doc->grid.cells[row][col] = value;
    doc->dirty = true;
    return true;
}

void editor_doc_checkpoint(EditorDoc *doc)
{
    /* An edit that starts from the state already on top of the stack would
     * only cost the author an undo press that does nothing. */
    if (doc->undo_len > 0 &&
        memcmp(&doc->undo[doc->undo_len - 1], &doc->grid, sizeof(doc->grid)) == 0)
    {
        return;
    }
    history_push(doc->undo, &doc->undo_len, &doc->grid);
    /* Anything the author had redone away is gone the moment they edit. */
    doc->redo_len = 0;
}

bool editor_doc_can_undo(const EditorDoc *doc)
{
    return doc->undo_len > 0;
}

bool editor_doc_can_redo(const EditorDoc *doc)
{
    return doc->redo_len > 0;
}

bool editor_doc_undo(EditorDoc *doc)
{
    if (!editor_doc_can_undo(doc))
        return false;
    history_push(doc->redo, &doc->redo_len, &doc->grid);
    doc->grid = doc->undo[--doc->undo_len];
    doc->dirty = true;
    return true;
}

bool editor_doc_redo(EditorDoc *doc)
{
    if (!editor_doc_can_redo(doc))
        return false;
    history_push(doc->undo, &doc->undo_len, &doc->grid);
    doc->grid = doc->redo[--doc->redo_len];
    doc->dirty = true;
    return true;
}

bool editor_doc_resize(EditorDoc *doc, int width, int height)
{
    if (width < 2)
        width = 2;
    if (height < 2)
        height = 2;
    if (width > MAX_LEVEL_WIDTH)
        width = MAX_LEVEL_WIDTH;
    if (height > MAX_LEVEL_HEIGHT)
        height = MAX_LEVEL_HEIGHT;
    if (width == doc->grid.width && height == doc->grid.height)
        return false;

    /* Anything outside the old map is new ground, whatever character happens
     * to be left in the buffer there from an earlier document. */
    char fill = editor_doc_fill_char(doc);
    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            if (row >= doc->grid.height || col >= doc->grid.width)
                doc->grid.cells[row][col] = fill;
            else if (doc->grid.cells[row][col] == '\0')
                doc->grid.cells[row][col] = fill;
        }
    }
    doc->grid.width = width;
    doc->grid.height = height;
    doc->dirty = true;
    return true;
}

bool editor_doc_insert_row(EditorDoc *doc, int row)
{
    if (doc->grid.height >= MAX_LEVEL_HEIGHT)
        return false;
    if (row < 0)
        row = 0;
    if (row > doc->grid.height)
        row = doc->grid.height;

    for (int r = doc->grid.height; r > row; --r)
        memcpy(doc->grid.cells[r], doc->grid.cells[r - 1], MAX_LEVEL_WIDTH);
    memset(doc->grid.cells[row], editor_doc_fill_char(doc), MAX_LEVEL_WIDTH);
    doc->grid.height++;
    doc->dirty = true;
    return true;
}

bool editor_doc_delete_row(EditorDoc *doc, int row)
{
    if (doc->grid.height <= 2 || row < 0 || row >= doc->grid.height)
        return false;
    for (int r = row; r < doc->grid.height - 1; ++r)
        memcpy(doc->grid.cells[r], doc->grid.cells[r + 1], MAX_LEVEL_WIDTH);
    memset(doc->grid.cells[doc->grid.height - 1], editor_doc_fill_char(doc),
           MAX_LEVEL_WIDTH);
    doc->grid.height--;
    doc->dirty = true;
    return true;
}

bool editor_doc_insert_col(EditorDoc *doc, int col)
{
    if (doc->grid.width >= MAX_LEVEL_WIDTH)
        return false;
    if (col < 0)
        col = 0;
    if (col > doc->grid.width)
        col = doc->grid.width;

    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int c = doc->grid.width; c > col; --c)
            doc->grid.cells[row][c] = doc->grid.cells[row][c - 1];
        doc->grid.cells[row][col] = editor_doc_fill_char(doc);
    }
    doc->grid.width++;
    doc->dirty = true;
    return true;
}

bool editor_doc_delete_col(EditorDoc *doc, int col)
{
    if (doc->grid.width <= 2 || col < 0 || col >= doc->grid.width)
        return false;
    for (int row = 0; row < doc->grid.height; ++row)
    {
        for (int c = col; c < doc->grid.width - 1; ++c)
            doc->grid.cells[row][c] = doc->grid.cells[row][c + 1];
        doc->grid.cells[row][doc->grid.width - 1] = editor_doc_fill_char(doc);
    }
    doc->grid.width--;
    doc->dirty = true;
    return true;
}

void editor_doc_mirror(EditorDoc *doc, int col0, int row0, int col1, int row1,
                       bool horizontal)
{
    if (col0 > col1)
    {
        int swap = col0;
        col0 = col1;
        col1 = swap;
    }
    if (row0 > row1)
    {
        int swap = row0;
        row0 = row1;
        row1 = swap;
    }
    if (col0 < 0)
        col0 = 0;
    if (row0 < 0)
        row0 = 0;
    if (col1 >= doc->grid.width)
        col1 = doc->grid.width - 1;
    if (row1 >= doc->grid.height)
        row1 = doc->grid.height - 1;

    if (horizontal)
    {
        for (int row = row0; row <= row1; ++row)
        {
            for (int a = col0, b = col1; a < b; ++a, --b)
            {
                char swap = doc->grid.cells[row][a];
                doc->grid.cells[row][a] = doc->grid.cells[row][b];
                doc->grid.cells[row][b] = swap;
            }
        }
    }
    else
    {
        for (int col = col0; col <= col1; ++col)
        {
            for (int a = row0, b = row1; a < b; ++a, --b)
            {
                char swap = doc->grid.cells[a][col];
                doc->grid.cells[a][col] = doc->grid.cells[b][col];
                doc->grid.cells[b][col] = swap;
            }
        }
    }
    doc->dirty = true;
}

bool editor_doc_build_level(const EditorDoc *doc, Level *level, uint64_t seed)
{
    size_t needed = editor_doc_serialize(doc, NULL, 0);
    char *text = (char *)malloc(needed + 1);
    if (text == NULL)
        return false;
    editor_doc_serialize(doc, text, needed + 1);

    Rng rng;
    rng_seed(&rng, seed);
    const char *name = doc->path[0] != '\0' ? doc->path : "untitled";
    bool ok = level_load_data(level, name, text, needed, &rng);
    free(text);
    return ok;
}

int editor_path_level_number(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *file = slash != NULL ? slash + 1 : path;
    if (strncmp(file, "level", 5) != 0)
        return 0;

    const char *digits = file + 5;
    int number = 0;
    if (*digits < '0' || *digits > '9')
        return 0;
    while (*digits >= '0' && *digits <= '9')
        number = number * 10 + (*digits++ - '0');
    return strcmp(digits, ".txt") == 0 ? number : 0;
}
