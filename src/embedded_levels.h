#ifndef CHUCK_EMBEDDED_LEVELS_H
#define CHUCK_EMBEDDED_LEVELS_H

#include <stddef.h>

typedef struct
{
    const char *name;
    const char *data;
    size_t size;
} EmbeddedLevelData;

extern const EmbeddedLevelData EMBEDDED_LEVELS[];
extern const size_t EMBEDDED_LEVEL_COUNT;

#endif /* CHUCK_EMBEDDED_LEVELS_H */
