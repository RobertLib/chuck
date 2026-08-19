# Install instructions

Paste this into the page's **Install instructions** field (Edit game → below the
uploads). It is a separate field from the description and it is the one people
read after downloading, which is the moment a Gatekeeper dialog or a missing
execute bit turns a free game into a shrug.

Keep it short. This is not the place for the story.

---

**macOS** — Unzip and drag `Chuck.app` wherever you keep applications. It is
signed and notarized, so it opens by double-clicking; nothing else is needed and
nothing is installed. Needs macOS 11 or newer, Intel or Apple silicon.

**Windows** — Unzip the folder somewhere and run `chuck.exe`. Keep `SDL3.dll`
beside it. Needs 64-bit Windows 10 or newer. The build is not signed with an
Authenticode certificate, so Windows may say the publisher is unknown — *More
info* → *Run anyway*, or build it yourself from the source.

**Linux** — Unpack the tarball and run `./chuck`. The `lib/` folder next to it
holds the SDL3 this build was made against, and the binary looks for it there, so
keep the two together and install nothing. Built on Ubuntu 24.04 for x86_64
(glibc 2.39); on an older distribution, building from source is a `make` away.

**Everywhere** — Settings and progress live in one folder and nowhere else:
`~/Library/Application Support/rob/Chuck` on macOS, `%APPDATA%\rob\Chuck` on
Windows, `~/.local/share/rob/Chuck` on Linux. Deleting it starts the tower again.
The game does not touch the network.

**Controls** are on their own page of the options sheet and every one of them
rebinds. If you have a gamepad, plug it in at any time — the prompts respell
themselves for whatever is in your hands.
