#ifndef CHUCK_VERSION_H
#define CHUCK_VERSION_H

/* The version and the bundle identifier are written here once and read from
 * here twice: the binary hands them to SDL_SetAppMetadata, and
 * packaging/build_app.sh greps them out of this header for the .app's
 * Info.plist. A shipped build whose About box and whose bundle disagree about
 * what it is has no way of being diagnosed from the outside, so neither is
 * allowed its own copy of the answer. */
#define CHUCK_VERSION "1.0.0"
#define CHUCK_APP_ID "cz.rob.chuck"
#define CHUCK_APP_NAME "Chuck"

#endif /* CHUCK_VERSION_H */
