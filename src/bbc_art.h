#ifndef CHUCK_BBC_ART_H
#define CHUCK_BBC_ART_H

/*
 * The half of the BBC Micro look that is data rather than drawing.
 *
 * Every picture in this game is drawn as if by an Acorn BBC Micro in MODE 2:
 * eight colours, fat pixels twice as wide as they are tall, and nothing in
 * between — no alpha, no gradients, no light. [bbc.h](bbc.h) is the drawing
 * side of that and links SDL. This file is the part that does not need to: the
 * eight colours, the shape of a sprite, and the system font. It is SDL-free and
 * in `TEST_SOURCES` for the reason every other table of this kind is — a
 * sprite with a ragged row or a glyph missing from the font is a fact about a
 * table, and a table on the shell side is a table nothing measures.
 *
 * See [docs/art-and-audio.md](../docs/art-and-audio.md) for the rules the
 * renderers draw by.
 */

#include <stdbool.h>

/*
 * The eight physical colours, in the order the hardware numbers them: bit 0 is
 * red, bit 1 green, bit 2 blue. That ordering is not nostalgia — it is what
 * makes `bbc_colour_from_rgb` a three-comparison function and what makes the
 * palette exactly the corners of the RGB cube, so "the nearest BBC colour" is
 * never a judgement call.
 */
typedef enum
{
    BBC_BLACK = 0,
    BBC_RED = 1,
    BBC_GREEN = 2,
    BBC_YELLOW = 3,
    BBC_BLUE = 4,
    BBC_MAGENTA = 5,
    BBC_CYAN = 6,
    BBC_WHITE = 7,
    BBC_COLOUR_COUNT
} BbcColour;

/* The one place the eight colours are spelled as numbers. Full intensity on
 * every gun, because that is what the machine put out and what a television of
 * the period showed: there is no "dark red" on a BBC, and a renderer that wants
 * one is asking for a dither (see `bbc_dither` in bbc.h). */
extern const unsigned char BBC_RGB[BBC_COLOUR_COUNT][3];

/* The colour whose guns are on where the channel is at least half way. Since
 * the palette is the eight corners of the RGB cube this is also the nearest
 * colour, exactly. */
BbcColour bbc_colour_from_rgb(unsigned char r, unsigned char g,
                              unsigned char b);

/*
 * The BBC's logical colours 8 to 15: a pair the hardware alternates between at
 * the flash rate. Each one is its colour and that colour's complement, which is
 * exactly the pairing the machine used — and the reason a flashing alarm on a
 * BBC reads red-cyan rather than red-black.
 */
BbcColour bbc_complement(BbcColour c);

/* ---- Sprites ---------------------------------------------------------- */

/*
 * A sprite is rows of characters, one character to one BBC pixel, which is how
 * every one of them was designed on squared paper in 1983 and is still the
 * only form in which a 7x16 figure can be read and edited by eye.
 *
 *   '.'  transparent
 *   'K'  black (opaque — a pupil, a gap that must hide what is behind it)
 *   'R' 'G' 'Y' 'B' 'M' 'C' 'W'  the other seven colours
 *   '1' '2' '3' '4'  a slot, filled at draw time from a four-colour list, so
 *        one drawing serves a guard and a heavy, or a car in five paints
 *
 * Every row is the same length. `bbc_sprite_valid` is what holds that, and the
 * suite runs it over every shared sprite.
 */
typedef struct
{
    const char *const *rows;
    int h;
} BbcSprite;

#define BBC_SPRITE_SLOTS 4

/* Declares `name` as a sprite from the string literals that follow, one per
 * row. Works at file scope and as a static local. */
#define BBC_SPRITE(name, ...)                                                \
    static const char *const name##_rows[] = {__VA_ARGS__};                  \
    static const BbcSprite name = {                                           \
        name##_rows, (int)(sizeof(name##_rows) / sizeof(name##_rows[0]))}

/* The same, with external linkage, for the shared tables in bbc_cast.c. */
#define BBC_SPRITE_EXTERN(name, ...)                                         \
    static const char *const name##_rows[] = {__VA_ARGS__};                  \
    const BbcSprite name = {                                                  \
        name##_rows, (int)(sizeof(name##_rows) / sizeof(name##_rows[0]))}

/* Width in BBC pixels: the length of the first row. */
int bbc_sprite_w(const BbcSprite *sprite);

/* What one sprite character means: a colour 0..7, a slot 8..11, or -1 for
 * transparent. -2 for a character that is none of those, which is a typo. */
int bbc_sprite_code(char c);

#define BBC_CODE_TRANSPARENT (-1)
#define BBC_CODE_INVALID (-2)
#define BBC_CODE_SLOT0 8

/* Every row the same length, at least one row, and nothing but legal
 * characters. */
bool bbc_sprite_valid(const BbcSprite *sprite);

/* ---- The system font -------------------------------------------------- */

/*
 * An 8x8 bitmap face in the manner of the Acorn MOS character set: verticals
 * two pixels thick, horizontals one, the ink in the middle six columns so two
 * letters set side by side keep a two-pixel gap, and row seven left for
 * descenders. It covers printable ASCII, 32 to 126; anything else draws the
 * solid block the machine printed for a character it had no glyph for.
 *
 * The cell is exactly the cell `SDL_RenderDebugText` had, eight by eight, which
 * is what lets every fit check in the suite — they all measure a line as
 * characters times eight — stay true of the new face without a single one of
 * them changing.
 */
#define BBC_FONT_CELL 8
#define BBC_FONT_FIRST 32
#define BBC_FONT_LAST 126

/* Whether the pixel at (col, row) of `ch`'s glyph is ink. */
bool bbc_font_pixel(unsigned char ch, int col, int row);

#endif /* CHUCK_BBC_ART_H */
