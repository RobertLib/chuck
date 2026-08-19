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

`itch/page.md` is the same description in markdown, and it is the copy to edit;
`page.html` is what the editor can swallow, since it has no markdown and no source
view. If you change one, change the other.

## Creating the page

| Field | Value |
| --- | --- |
| Title | `Chuck` |
| Project URL | `chuck` |
| Short description / tagline | `A 2D infiltration platformer up the inside of a sealed tower. Forty floors, one way up, and thirty-eight minutes.` |
| Classification | Games |
| Kind of project | Downloadable |
| Release status | Released |
| Pricing | **Donate** — the download stays free and a support button appears |
| Genre | Platformer |
| Tags | `action`, `stealth`, `pixel-art`, `2d`, `side-scroller`, `singleplayer`, `retro`, `difficult` |
| Made with | SDL |
| Average session | A few hours |
| Languages | English |
| Inputs | Keyboard, Gamepad (any) |
| Accessibility | Configurable controls |
| Community | Comments on |
| Visibility | Draft until the builds are up, then Public |

Not `procedural-generation`, however tempting: the *art* is generated at runtime
and the levels are hand-drawn, and that tag means the opposite to somebody
browsing.

## The pictures

- **Cover image**: `cover-630x500@2x.png`. itch shows it at 630×500 and as small
  as 315×250, and it is the only picture most people will ever see of this game.
- **Screenshots**: the number in the filename is the order — `01` is the one that
  gets seen. Five is plenty; the recommended five are `01-lobby`, `02-alarm`,
  `03-climb`, `04-offices`, `05-drive`. Use the `@2x` files: itch scales down
  cleanly and up badly. **Not `19-roof`** — it is the last frame of the game.
- **Captions** are in `dist/press/MANIFEST.txt`, one per file.
- **A GIF** can go at the top of the gallery if you want one. `31-loop-drive.gif`
  reads best at thumbnail size; anything over about 3 MB stops autoplaying, which
  is why the sizes are printed.
- **Page background**: `wallpaper-1920x1080.png`, under Edit theme, if you want it.

## The uploads

One archive per platform. Drag each in, tick the platform, and leave *This file
will be downloaded* selected — there is no web build.

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
- [ ] `make test`, `make lint` and `make soak` are green on the commit the
      archives were built from.
- [ ] The version in `src/version.h` matches what the page says.
- [ ] The cover reads at 315×250. Look at it that small before believing it.

## Not in here: a trailer

`make press` photographs the game and it cannot play it — a headless capture
receives no input, so what moves in those GIFs is the building rather than Chuck.
A real trailer wants a recording of somebody playing with the sound on, which is
twenty minutes with any screen recorder.
