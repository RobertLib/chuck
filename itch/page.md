# Chuck

**Forty floors. One way up.**

A stealth platformer up the inside of a tower that has been sealed from within.

**Play it here in the browser**, or download it for macOS, Windows or Linux. It
is the same game either way, though a browser tab keeps its own progress rather
than sharing the download's.

## What you actually do

Seventeen sectors between the lobby and the roof, each one a small infiltration
route: find the key card or breach the security terminal that opens the way
deeper in, and decide floor by floor when to fight, when to walk past, and when
to let the building do it for you.

- **Guards that look, listen and remember.** They see through a forward cone that
  walls, floors and crates block, so they can spot you diagonally or up a ladder
  shaft and fire straight down it. Gunfire draws the nearest of them over to look.
  One who finds a body often sprints for a wall switch — and the fallen stay where
  they drop, so where you leave one is a decision as real as where you fought.
- **A quiet way through, and it is three things.** A knife takes a man down in one
  stroke from behind, provided nobody has seen him and the alarm is down. A pocket
  of bolts that never runs out makes a noise somewhere else and calm guards walk
  to the noise instead of to you. And a body can be dragged out of the room it
  fell in, at half speed, no ladders, no jumping.
- **An alarm that changes the floor rather than ending it.** Red light, every
  guard and dog moving, rounds led ahead of a runner, gaps hopped to keep up, a
  sweep around where you were last seen — and it switches itself off once you have
  been out of sight long enough.
- **Answers to being seen.** A flash charge kills nobody: it takes the sight out
  of every guard near it for a few seconds and makes any camera in reach forget
  what it had. Then they carry on exactly as they were. It buys seconds, and the
  seconds are for leaving.
- **Five sectors that are not walked at all.** The stair door is welded, the way
  on is an open window, and out there gravity and ladders are replaced by four-way
  movement across the brickwork. Cornices are the obstacle and the only cover, a
  building-wide wind howls before it shoves, men lean out of windows and shout
  before they throw, and birds cross at you. What you carry up is what you have
  inside the next floor.
- **The crew talk, and you can read it.** Twelve men run this night like a shift.
  A guard on his own calls in, two standing together chat instead, whoever reaches
  a switch shouts. Stand close enough and the line prints under the HUD with the
  speaker's callsign on it. The locks on the vault, the cordon outside, the clock
  and how many of them are still answering all get said out loud by the men doing
  it — none of it in a screen you have to stop playing to read.

## What it remembers

Three hearts a life, and one rule: what hits you costs hearts, what crushes you
or breaks your fall kills you. Progress inside a floor is banked, so a death
resumes at the last card, hack, door or medkit rather than at the stairs — and the
world keeps its dead guards and its opened walls. Running out of lives always
offers the same sector back; the game never sends you to the lobby uninvited.

The night gives every floor the same slot on the clock, and every second you hand
back is paid in points. Clear one without dying and it pays again. Across
sessions the game keeps three things: the furthest floor any run reached, the best
score any run finished on, and the quickest each sector has ever been cleared —
all seventeen readable on one sheet of the manual, so the par you are being paid
against is something you can measure yourself against wherever you are.

## Set up how you want to play it

- **Every one of the nine sector controls rebinds**, on the keyboard and on a pad,
  two keys and two buttons apiece.
- **Buttons are bound by the letter printed on them.** A pad is asked what it
  prints on each face the moment it is plugged in, so `A` is the button marked A on
  an Xbox pad *and* on a Nintendo one — and a PlayStation pad's prompts are spelled
  with its own shapes.
- **Three assist switches:** five hearts a life, guards and dogs at 80% speed, and
  deaths that never cost a life. Nothing is locked behind finishing.
- **Veteran** is the same lever the other way: a faster crew, one life, no
  continues.
- **Reduced motion** takes the shake out and holds every warning light at a steady
  glow instead of a strobe, without changing what any of them is telling you.
- The **CRT filter** can go off. Sound and music have separate levels.
- A **ten-sheet illustrated field manual** on the title screen explains the crew,
  the building, the floor plan, the guards, the quiet route, the wall and the HUD.

---

## The night

Ellen Ross wrote the access system for Kessler Tower and works nights as its duty
controller. At 00:12 she is three blocks from work with a coffee in her hand and
her husband twenty metres behind her on the pavement when an SUV comes up the
kerb lane with its lights off. Two men in maintenance coveralls put her in it.
Nobody comes, because eight minutes earlier every unit in the city was sent
somewhere else.

Chuck follows them across a cordon he does not yet understand and watches them
walk her in through the tower's own front door at 00:22. He goes in after them.

What is happening inside is not a kidnapping. Twelve men have been badged into
this building since March as its night maintenance contractor, and the flight
cases they wheeled through the goods entrance were never inspected. The demand
they broadcast at 00:04 is theatre: it put every unit in the city on a cordon
around this building and nobody at all inside it. The sub-vault opens on the
overnight settlement, and its seventh and last lock runs on a two-key rule — the
bank's key, and the building's duty controller, alive and present.

That is why they needed Ellen, and it is the only reason she still is. At 01:00
the bonds leave the roof.

---

## Under it

Chuck is one binary and no asset files. Every pixel is drawn procedurally at
runtime and every sound is synthesised at startup — there is no sprite sheet, no
tileset, no audio folder, and the levels are compiled into the executable. It is
C17 against SDL3, it opens in a window you can resize, and it does not touch the
network. The browser version is that same tree compiled to WebAssembly, which is
why it is six hundred kilobytes and starts in a second: a game with no asset
files has nothing to load.

The source is public and MIT licensed: **https://github.com/RobertLib/chuck**

It is free. If it was worth an evening to you, the donate button is there, and
thank you — but nothing here is waiting on it.
