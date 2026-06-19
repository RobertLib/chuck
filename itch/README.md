# The itch.io page

Everything the store page needs, and what to do with it. Nothing in this
repository uploads anything — every command writes a file into `dist/` and stops,
and you drag it into the dashboard yourself.

| What | Where it is |
| --- | --- |
| Description | `itch/page.html` — open it, select all, paste into the description editor |
| Install instructions | `itch/install-instructions.md` — paste as plain text into that field |
| Pictures | `make press` → `dist/press/` (screenshots, cover, GIFs, `MANIFEST.txt` with captions) |
| macOS build | `make mac` → `dist/Chuck-<v>-macos.zip` (needs a Mac and an Apple ID) |
| Windows build | `make win` → `dist/Chuck-<v>-windows-x64.zip` (works on a Mac) |
| Linux build | `make linux` on a Linux, or the **payloads** button on the Actions tab |
| Browser build | `make web` → `dist/Chuck-<v>-web.zip` (needs emscripten; works on a Mac) |

`itch/page.md` is the same description in markdown, and it is the copy to edit;
`page.html` is what the editor can swallow, since it has no markdown and no source
view. If you change one, change the other.

## Creating the page

| Field | Value |
| --- | --- |
| Title | `Chuck: Kessler Tower` |
| Project URL | `chuck` — **not** the title; changing it breaks every link already out there |
| Short description / tagline | `Forty floors, one way up, and thirty-eight minutes.` |
| Classification | Games |
| Kind of project | **HTML** — see *The browser build* below. Downloads still attach to an HTML project; the reverse is not true |
| Release status | Released |
| Pricing | **Donate** — the download stays free and a support button appears |
| Genre | Platformer |
| Tags | `action`, `stealth`, `pixel-art`, `2d`, `side-scroller`, `singleplayer`, `retro`, `difficult`, `open-source` |
| Made with | SDL |
| Average session | A few hours |
| Languages | English |
| Inputs | Keyboard, Gamepad (any) |
| Mobile friendly | **No.** There is no touch control scheme and the game is played with two hands on a keyboard; saying yes puts it in front of phone users who cannot play it |
| Accessibility | Configurable controls |
| Community | Comments on |
| Visibility | Draft until the builds are up, then Public |

The title carries the building and the project URL does not, and the two are
deliberately different things. `Chuck` alone is unsearchable — on itch it competes
with a radio show, a card-game designer and several people's first names, and a
visitor who half-remembers this game has nothing to type. `Kessler Tower` is the
one proper noun in the fiction that belongs only to it. The URL stays `chuck`
because it is already printed in downloads, links and anything anybody has
bookmarked, and a project URL is the one field on this page that cannot be
changed without cost.

**The store says `Chuck: Kessler Tower` and the title screen says `CHUCK`, and
that is a drift this file is choosing rather than missing.** The game's own name
is `CHUCK_APP_NAME` in [src/version.h](../src/version.h), and it is not a label:
it is the folder every player's settings and progress live in
(`~/Library/Application Support/rob/Chuck`). Renaming it orphans the saves of
everybody who already has the game, to buy a word on a screen — so the subtitle
is the shop's and the game keeps its name. If the logotype in `render_logo` ever
grows a second line, this paragraph is the thing to re-read.

Not `procedural-generation`, however tempting: the *art* is generated at runtime
and the levels are hand-drawn, and that tag means the opposite to somebody
browsing.

The tagline is fifty characters because a browse listing truncates around sixty,
and the half that was being cut was the half that sells: the genre is carried by
the tags and the cover, and `A 2D infiltration platformer up the inside of a
sealed tower` was never reaching a reader who had not already clicked. The
sentence it was cut down to is the one the checker holds — `check_docs.py`
derives both `forty` and `thirty-eight` from `BUILDING_FLOORS` and the night
clock, so shortening it any further means editing that check first.

`open-source` and `no-ai` are the last two of the ten itch allows, and both are
facts rather than reaches: the tree is MIT and every pixel in it is drawn by
code. `no-ai` was the fourth most used tag on itch in 2025, and it is the one
tag on this list whose audience is looking for exactly what this project is.

## The pictures

- **Cover image**: `cover-630x500@2x.png`. itch shows it at 630×500 and as small
  as 315×250, and it is the only picture most people will ever see of this game.
- **Screenshots**: the number in the filename is the *capture* order and not the
  gallery order — the first one in the gallery is the one that gets seen, and it
  should be the shot that shows what this game is rather than the shot that comes
  first in the campaign. Upload five, in this order: `02-alarm`, `03-climb`,
  `01-lobby`, `04-offices`, `05-drive`. The alarm is the whole game in one frame
  (red light, bodies where they fell, a way out) and the facade is the mechanic
  nothing else on the `stealth` + `platformer` tag looks like; the lobby is an
  honest picture of the first room and a weak picture of the game. Use the `@2x`
  files: itch scales down cleanly and up badly. **Not `19-roof`** — it is the last
  frame of the game. **Not `10-manual-crew`, `11-manual-quiet` or `15-options`**
  either, however well drawn they are: a store gallery is not the place for a
  menu, and a reader deciding in a second wants the game and not its settings.
  They are in the kit because a press kit is not only a store page.
- **Captions** are in `dist/press/MANIFEST.txt`, one per file.
- **A GIF** can go at the top of the gallery. Of the five in the kit,
  `31-loop-drive.gif` reads best at thumbnail size; anything over about 3 MB stops
  autoplaying, which is why the sizes are printed. Know what you are uploading,
  though: **not one of these five shows the player doing anything**, for the
  reason written at the bottom of this file — a headless capture receives no
  input, so what moves in all of them is the building. For a store page that is
  the wrong GIF, and it is the single biggest gap in this kit: the thing that
  sells a stealth platformer is a man being seen and getting away with it. Twenty
  minutes with a screen recorder beats every animation here until `make press` can
  replay a recorded input.
- **Page banner**: `banner-960x360.png`, under **Edit theme** → Banner. Read this
  before uploading it: **the banner replaces the page's title.** itch draws it
  instead of the words `Chuck: Kessler Tower`, not above them — so a banner
  without the wordmark in it takes the game's name off the top of its own page.
  This one is the cover cut wide and it has `CHUCK` and the tagline in it, which
  is the whole reason it comes from that frame. It displays at 960 wide; the
  field takes up to 400 tall and this is 360, for the reason written up in
  [docs/tooling.md](../docs/tooling.md#the-press-kit). Under 3 MB (it is ~150 kB).
  Worth knowing what it does *not* solve: the title in the banner is the page
  title, and the **cover** is still what a browse listing shows, so both need the
  name in them and they are two different pictures.
- **Page background**: `wallpaper-1920x1080.png`, under Edit theme, if you want it.

## The browser build

**This is the one most people will play**, and it is the reason *Kind of project*
is HTML rather than Downloadable. Most of itch.io is played in the page: a
visitor who has to download a binary, unblock it on Windows or right-click-open
it on macOS, and then find it in a Downloads folder, is a visitor who has already
gone. The downloads still matter and still attach — an HTML project takes them —
but they stop being the only door.

Upload `dist/Chuck-<v>-web.zip` like any other file and then **tick *This file
will be played in the browser***. That checkbox is the whole difference; without
it the zip sits there as a download nobody can use. itch then reveals an
**Embed options** block, and these are its settings:

| Field | Value |
| --- | --- |
| Viewport dimensions | `800` x `552` |
| Fullscreen button | **On** |
| Enable scrollbars | Off |
| Automatically start on page load | **On** |
| Mobile friendly | Off |

The viewport is the game's own frame, `VIEW_W` x `VIEW_H` in
[src/game_config.h](../src/game_config.h), and at exactly this size the game is
drawn 1:1 — every pixel on the page is a pixel the game drew, with no scaling
anywhere in the path. Ask for a different size and it still works: the canvas
fills whatever frame it is given and SDL letterboxes into it, which is the same
thing the downloads do when their window is resized. Fullscreen is on for the
same reason, and it is the one case where a bigger viewport is genuinely better.

**Automatically start on page load is on, and it is worth knowing why, because
the obvious answer is the wrong one.** itch's own click-to-launch shows a
placeholder and loads the game on a click, which is right for a heavy game and
wrong for this one: the whole thing is 604 KB, and the page has a PLAY button of
its own that must be pressed regardless. A browser silently discards audio until
the page it is on has been clicked, and a click on *itch's* page does not count
for the iframe the game is in — different origin, different document — so the
game holds itself back until somebody presses PLAY inside it, and the first frame
it draws is the first frame it is allowed to make a sound on. Leaving itch's
launcher on as well would make that two clicks to start one game.

Nothing else needs configuring. There are no threads in this build, which is not
an accident: threads on the web need COOP/COEP headers from the server, itch does
not send them, and a build that wants them shows a blank page rather than a slow
one.

## The uploads

One archive per platform, four of them. Drag each in and tick the platform;
leave *This file will be downloaded* selected on the three downloads and tick
*This file will be played in the browser* on the web one. Put the web build
first — the order on the page is the order they are listed in, and the thing a
visitor can do without leaving belongs at the top.

`make mac` also cuts a DMG, for a GitHub release or a link in an email.
Offering a visitor a DMG *and* a zip for macOS is a choice they have no way to
make, so upload one file per platform and no more.

## Before it goes public

- [ ] The macOS build is **notarized and stapled**, not just signed — otherwise
      every Mac but yours says "Apple could not verify". `make mac` does the
      whole thing in order, which is the point of it being one target: the zip is
      a snapshot of the bundle, so it has to be cut after the ticket is stapled
      in and not before.
- [ ] Each of the three downloads has been unpacked and started on the platform it
      is for — the Windows one especially, since it is cross-built and nothing on
      the build machine can run it.
- [ ] The browser build has been **played on the page itself**, not just locally:
      PLAY starts it, the sound arrives, the arrow keys move Chuck instead of
      scrolling itch's page, and fullscreen works. Then reload the tab and check
      the title screen offers to resume — that is the one thing about this build
      that fails silently, because saving works perfectly right up until the tab
      closes and takes the filesystem with it.
- [ ] The web zip has `index.html` at its **root**. `unzip -l` it. This is the
      one archive here that must not be nested, and nested it is a page that does
      not start and does not say why.
- [ ] `make test`, `make lint` and `make soak` are green on the commit the
      archives were built from.
- [ ] The version in `src/version.h` matches what the page says.
- [ ] The cover reads at 315×250. Look at it that small before believing it.

## Not in here: a trailer

`make press` photographs the game and it cannot play it — a headless capture
receives no input, so what moves in those GIFs is the building rather than Chuck.
A real trailer wants a recording of somebody playing with the sound on, which is
twenty minutes with any screen recorder.
