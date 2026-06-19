CC := cc
CPPFLAGS :=
BASE_CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -O2
CFLAGS := $(BASE_CFLAGS) $(shell pkg-config --cflags sdl3)
DEPFLAGS := -MMD -MP
LDFLAGS := $(shell pkg-config --libs sdl3) -lm
TEST_CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -O2 -Isrc -Ieditor
SANITIZER_FLAGS := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
DEBUG_CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -O0 -g3 $(shell pkg-config --cflags sdl3)
DEBUG_TARGET := build/debug/chuck-debug

SRC_DIR := src
BUILD_DIR := build
TARGET := chuck
LEVEL_GENERATOR := tools/embed_levels.py
LEVEL_FILES := $(wildcard levels/level*.txt)
SUBLEVEL_FILES := $(wildcard levels/sublevels/*.txt)
EMBEDDED_LEVELS_SOURCE := $(BUILD_DIR)/embedded_levels.c
EMBEDDED_LEVELS_OBJECT := $(BUILD_DIR)/embedded_levels.o
TEST_TARGET := $(BUILD_DIR)/core_tests
TEST_SOURCES := tests/test_main.c \
	src/camera.c src/chase.c src/credits.c src/crew.c src/enemy.c src/game_event.c src/gameplay_ai.c src/gameplay_combat.c \
	src/gameplay_climb.c src/gameplay_interaction.c src/gameplay_physics.c src/gameplay_world.c \
	src/gameplay_state.c src/intel.c src/level.c src/level_route.c src/manual_pages.c \
	src/pause_sheet.c src/run_tally.c src/sector_tally.c \
	src/player.c src/rng.c \
	src/keybind.c src/pad_hint.c src/progress.c src/settings.c \
	editor/editor_doc.c editor/editor_legend.c editor/editor_validate.c

# The suite is one compile rather than a set of objects, so it produces no
# depfiles and a header change was invisible to it: editing `game_config.h` and
# running `make test` rebuilt nothing, re-ran the *previous* binary and reported
# it green. That is the worst shape a gate can have — it passes precisely when
# you have just changed the thing it is meant to be checking — and it is how a
# raised `MAX_ENEMIES` looked verified when the check for it had never been
# compiled. Every header is a prerequisite, found by wildcard rather than
# listed, because a list is the thing that stays at the old contents. The whole
# binary is two seconds, so the coarseness costs nothing.
#
# `EDITOR_DIR` is therefore declared above rather than beside the editor's own
# variables, and that placement is the whole of a bug this line had for as long
# as it existed: `:=` expands immediately, so with the definition twenty lines
# below this one the editor wildcard read `$(wildcard /*.h)` and matched
# nothing. The suite links three editor translation units, so touching
# `editor_validate.h` re-ran the previous binary and reported it green — the
# exact failure the paragraph above describes, still live for the headers it
# names.
EDITOR_DIR := editor
TEST_HEADERS := $(wildcard tests/*.h) $(wildcard $(SRC_DIR)/*.h) \
	$(wildcard $(EDITOR_DIR)/*.h)

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES)) \
	$(EMBEDDED_LEVELS_OBJECT)
DEPENDENCIES := $(OBJECTS:.o=.d)

# The level editor is its own binary, but not its own idea of what a level is:
# it links the game's parser, art direction and route model so what it draws
# and what it reports cannot drift from the game and the tests.
EDITOR_TARGET := chuck-editor
#
# `manual_pages.c` is in the list for one integer, and that is the point of it:
# `CAMPAIGN_CLIMB_SECTOR_COUNT` is the campaign's shape, held against the
# embedded maps by the suite, and the editor used to carry its own copy of it —
# stale at four climbs and fifteen sectors, telling every author who opened a
# shipped map that the campaign disagreed with the tests. A number written down
# twice is checked or it is two numbers.
EDITOR_SOURCES := $(wildcard $(EDITOR_DIR)/*.c) \
	$(SRC_DIR)/level.c $(SRC_DIR)/level_art.c $(SRC_DIR)/level_route.c \
	$(SRC_DIR)/manual_pages.c $(SRC_DIR)/rng.c
EDITOR_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/editor/%.o,$(notdir $(EDITOR_SOURCES)))
EDITOR_DEPENDENCIES := $(EDITOR_OBJECTS:.o=.d)

.PHONY: all debug run run-debug run-editor editor test lint \
	sanitize soak coverage coverage-shell clean sdl3 app \
	press mac win linux web

all: $(TARGET)

debug:
	$(MAKE) BUILD_DIR=build/debug TARGET=$(DEBUG_TARGET) \
		CPPFLAGS="$(CPPFLAGS) -DCHUCK_DEBUG" CFLAGS="$(DEBUG_CFLAGS)" all

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(EMBEDDED_LEVELS_SOURCE): $(LEVEL_GENERATOR) $(LEVEL_FILES) $(SUBLEVEL_FILES) | $(BUILD_DIR)
	python3 $(LEVEL_GENERATOR) $@ $(LEVEL_FILES) --sublevels $(SUBLEVEL_FILES)

$(EMBEDDED_LEVELS_OBJECT): $(EMBEDDED_LEVELS_SOURCE) $(SRC_DIR)/embedded_levels.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: all
	./$(TARGET)

run-debug: debug
	./$(DEBUG_TARGET)

editor: $(EDITOR_TARGET)

$(EDITOR_TARGET): $(EDITOR_OBJECTS)
	$(CC) $(EDITOR_OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/editor/%.o: $(EDITOR_DIR)/%.c | $(BUILD_DIR)/editor
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -I$(SRC_DIR) -I$(EDITOR_DIR) -c $< -o $@

$(BUILD_DIR)/editor/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)/editor
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -I$(SRC_DIR) -I$(EDITOR_DIR) -c $< -o $@

$(BUILD_DIR)/editor:
	mkdir -p $(BUILD_DIR)/editor

run-editor: $(EDITOR_TARGET)
	./$(EDITOR_TARGET)

$(TEST_TARGET): $(TEST_SOURCES) $(TEST_HEADERS) $(EMBEDDED_LEVELS_SOURCE) \
		| $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $(TEST_SOURCES) $(EMBEDDED_LEVELS_SOURCE) -o $@ -lm

test: lint $(TEST_TARGET)
	./$(TEST_TARGET)

# The rules the suite structurally cannot reach, because they are about source
# text rather than behaviour: a colour literal that reproduces an fx.h value is
# that constant misspelt, and docs/art-and-audio.md has said so for a long time
# with nothing behind it. `test` depends on this so the two are one gate.
#
# `check_docs.py` is the second of them and answers the other direction. The
# suite holds every table *in code* to the maps — `INTEL_ARC_SECTORS` against the
# sectors that show a report, the manual's mission sheet against
# `CAMPAIGN_SECTORS` — and reaches no sentence at all, so the pages that state
# which sectors carry a camera, a heavy or two medkits went on describing the
# fifteen-sector campaign. One of them named a sector that had become a facade
# and therefore had no men on it to describe.
#
# `check_lists.py` is the third and answers a question neither of the others can:
# a **list** written down more than once. The map legend is in three files, the
# soak sweep's screen names in two, and main.c's own switches in four places —
# all of them in agreement, none of them held there by anything. The screens are
# the one that matters most, because that copy going stale is silent in the worst
# direction: a screen the game knows and the script does not is sanitizer-compiled,
# never sanitizer-executed, and `make sanitize` still reports a clean sweep.
lint:
	python3 tools/check_palette.py
	python3 tools/check_docs.py
	python3 tools/check_lists.py

# Walk a built game across the title screen and every sector, headless, and fail
# on anything a sanitizer or the loader says. `SOAK_BINARY` is what to run and
# `SOAK_SECONDS` how long to hold each one; the defaults soak the ordinary build,
# which is what makes this useful outside `sanitize` as a plain smoke test.
#
# It is a target of its own so that `make sanitize` can point it at the sanitized
# binary, which is the whole reason it exists — see tools/soak.sh.
SOAK_BINARY ?= $(TARGET)
SOAK_SECONDS ?= 2
# The editor is soaked by the same sweep and is a second binary, so it is named
# separately: `make sanitize` builds the two under different names in different
# directories, and guessing one from the other inside the script would work for
# the ordinary layout and quietly skip the editor for the sanitized one — which
# is the layout the sweep exists for.
SOAK_EDITOR ?= $(EDITOR_TARGET)
# Which of the two questions this run is asking. `full` is the coverage sweep and
# is what `make sanitize` wants; `smoke` skips the timed sequences, which is what
# the macOS CI job wants, because a cutscene beat drawn on arm64 is a beat already
# drawn under ASan on the other job and the hold is most of the sweep's clock.
# See the head of tools/soak.sh.
SOAK_MODE ?= full

soak: $(SOAK_BINARY)
	SOAK_MODE=$(SOAK_MODE) SOAK_EDITOR=$(SOAK_EDITOR) \
		tools/soak.sh $(SOAK_BINARY) $(SOAK_SECONDS)

# What the suite never executes, counted rather than believed.
#
# The renderers were measured once — the sweep in AGENTS.md that took them from
# forty-two never-executed functions to six — and the *core* never was, which is
# the half this project calls testable and the half a reader would assume was
# covered. It was not: fourteen functions in the SDL-free tree had never run,
# among them both platform updaters (`P` and `F` are on six shipped floors),
# half of how a dog reads a hole in the floor, and `release_body_bit`,
# which is the fallback that keeps a reinforcement from deleting the corpse the
# quiet route is played around.
#
# Two of them are worth knowing about as a class. `level_update_moving_platforms`
# and `level_update_falling_platforms` live in `level.c`, on the core side, and
# their only caller is `update_playing` in game.c, on the shell side — so a suite
# that links every gameplay module and drives it directly still never called
# them. A function is not reached because its file is; it is reached because
# something in the test binary calls it.
#
# This is a target rather than a gate on purpose. A percentage in CI is a number
# people learn to move; what is worth failing over is a *function nobody runs*,
# and the honest way to keep that at nought is to look at the list this prints
# after adding one. `make test` is the gate.
#
# **And then this target had the defect it exists to find, one floor down.** It
# printed functions whose *region* coverage is nought — never entered at all —
# reported `none`, and was believed. Underneath that answer sat eleven hundred
# lines of the SDL-free tree compiled and never run, because a function is
# reported here only if it is never entered and not if the mechanic inside it
# never fires. `player_update` was called about two hundred thousand times by the
# suite without one of those calls ever holding `down` on a floor, so the whole
# crawl — the posture a gas canister is shot from and one of the two ways of
# being hard to see — was in the covered column. So were the forty lines that
# pair two guards into a conversation, the twenty-five that walk a console's
# reinforcements out of a door, the animal half of body discovery, and the whole
# of the man with the mop.
#
# That is exactly the shape of the sweep AGENTS.md records for the renderers —
# "reached but never acted on" — arriving on the half of the tree this project
# calls testable, and it arrived here because the check for it was reading the
# wrong column. So the second list is lines rather than functions, with the
# longest unexecuted run in each file, because a run of twenty-five is a mechanic
# and a scatter of ones is a file's worth of `return false` guards. It is a list
# to read, not a number to move: `none` is not the target and never was.
#
# Needs clang's instrumentation, which is what `cc` already is on macOS; on a
# GNU toolchain say `make coverage CC=clang`. `xcrun` is how macOS finds
# `llvm-profdata` and `llvm-cov`, which elsewhere are simply on PATH — asked
# rather than assumed, because a Makefile that only works on the machine it was
# written on is the shape of thing this repository keeps finding.
COVERAGE_DIR := build/coverage
COVERAGE_FLAGS := -O0 -g -fprofile-instr-generate -fcoverage-mapping
LLVM_PREFIX := $(shell command -v xcrun >/dev/null 2>&1 && echo xcrun)
LLVM_PROFDATA ?= $(LLVM_PREFIX) llvm-profdata
LLVM_COV ?= $(LLVM_PREFIX) llvm-cov

# The instrumented suite and its profile, as a file rather than as steps inside
# one recipe: `coverage-shell` below needs the same profile to work out which
# functions the suite already reaches, and two copies of these four lines is how
# the two halves would come to measure slightly different things.
$(COVERAGE_DIR)/core_tests.profdata: $(TEST_SOURCES) $(TEST_HEADERS) \
		$(EMBEDDED_LEVELS_SOURCE)
	@mkdir -p $(COVERAGE_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $(COVERAGE_FLAGS) \
		$(TEST_SOURCES) $(EMBEDDED_LEVELS_SOURCE) \
		-o $(COVERAGE_DIR)/core_tests -lm
	@LLVM_PROFILE_FILE=$(COVERAGE_DIR)/core_tests.profraw \
		$(COVERAGE_DIR)/core_tests > /dev/null 2>&1
	@$(LLVM_PROFDATA) merge -sparse $(COVERAGE_DIR)/core_tests.profraw \
		-o $@

coverage: $(COVERAGE_DIR)/core_tests.profdata
	@$(LLVM_COV) report $(COVERAGE_DIR)/core_tests \
		-instr-profile=$(COVERAGE_DIR)/core_tests.profdata \
		$(TEST_SOURCES) 2>/dev/null | tail -n 3
	@echo
	@echo "functions the suite never executes:"
	@for source in $(TEST_SOURCES); do \
		$(LLVM_COV) report $(COVERAGE_DIR)/core_tests \
			-instr-profile=$(COVERAGE_DIR)/core_tests.profdata \
			-show-functions -sources $$source 2>/dev/null; \
	done | awk '$$4 == "0.00%" { print "  " $$1; found = 1 } \
		END { if (!found) print "  none" }' | sort -u
	@echo
	@echo "code the suite never executes, by file:"
	@: > $(COVERAGE_DIR)/dead.txt
	@for source in $(TEST_SOURCES); do \
		case $$source in tests/*) continue ;; esac; \
		$(LLVM_COV) show $(COVERAGE_DIR)/core_tests \
			-instr-profile=$(COVERAGE_DIR)/core_tests.profdata \
			$$source 2>/dev/null \
		| awk -F'|' -v name="$$source" \
			'{ line = $$1 + 0; count = $$2; gsub(/ /, "", count); \
			   if (count != "0") next; \
			   dead++; \
			   if (line != previous + 1) { start = line; run = 0 } \
			   run++; previous = line; \
			   if (run > longest) { longest = run; at = start } } \
			 END { if (dead) print name, dead, longest, at }' \
			>> $(COVERAGE_DIR)/dead.txt; \
	done
	@sort -k2,2rn $(COVERAGE_DIR)/dead.txt \
	| awk '{ total += $$2; \
		 printf "  %-30s %4d line(s), longest run %d from line %d\n", \
			$$1, $$2, $$3, $$4 } \
		END { if (total) \
			printf "\n  %d line(s) compiled and never run\n", total; \
		      else print "  none" }'

# The other half of the same question, and the half nobody had asked.
#
# `coverage` above measures the SDL-free tree against the suite. It cannot say a
# word about the shell — `game.c`, `game_input.c`, the renderers — because the
# test binary does not link any of it, and the reader of a target called
# `coverage` reporting `none` has every reason to think the answer covers the
# tree. It covered half of it.
#
# So this builds the *game* instrumented, walks it with the same sweep
# `make soak` uses, and prints the functions **neither gate executes**: never
# entered by the suite and never entered by the sweep. That intersection is the
# honest list, and the first time it was taken it held **42 functions** where
# AGENTS.md wrote down 14 — the pause and options row handlers plus
# `audio_toggle_mute` and `audio_stop_music`. The 28 nobody had named were the
# whole gamepad path in `game_input.c` (20 of them, including `turn_manual_page`
# and `toggle_fullscreen`) and eight more in `game.c`: `finish_player_death`,
# `continue_game`, `game_save_progress`, `leave_restroom`,
# `game_apply_assist_everywhere`, `game_resume_campaign`, `game_set_fullscreen`
# and `settings_current_row` — which is to say the death, the continue, the write
# to the player's disk and the way out of a restroom, all of them in the area
# where AGENTS.md documents a *shipped* bug found by hand.
#
# The renderers came out clean the same day: 394 functions across
# `game_render.c`, `level_art.c`, `cutscene.c`, `manual.c`, `intro.c`,
# `render_figures.c`, `particle.c` and `chase_render.c`, every one of them
# executed by the sweep. That is the claim the `--screen` work in AGENTS.md
# makes, measured rather than believed.
#
# Not a gate, for the same reason `coverage` is not one: what is worth reading is
# the list. It needs SDL and it needs the clock — the sweep on an -O0
# instrumented build is minutes rather than seconds — which is the other reason
# it is a target of its own rather than part of `coverage`, since that one is
# meant to work on a machine with no SDL at all.
coverage-shell: $(COVERAGE_DIR)/core_tests.profdata
	@mkdir -p $(COVERAGE_DIR)/profraw
	@find $(COVERAGE_DIR)/profraw -name '*.profraw' -delete
	$(MAKE) BUILD_DIR=$(COVERAGE_DIR)/game \
		TARGET=$(COVERAGE_DIR)/chuck-cov \
		CFLAGS="$(CFLAGS) $(COVERAGE_FLAGS)" \
		LDFLAGS="$(LDFLAGS) -fprofile-instr-generate" all
# Through a file rather than a pipe, and that is not tidiness: `soak.sh | tail`
# reports tail's exit status, so a sweep that failed halfway would be swallowed by
# the very command printing its summary — a check reporting success it does not
# have, in the recipe written to stop exactly that.
	@LLVM_PROFILE_FILE=$(COVERAGE_DIR)/profraw/chuck-%p.profraw \
		SOAK_MODE=$(SOAK_MODE) SOAK_EDITOR= \
		tools/soak.sh $(COVERAGE_DIR)/chuck-cov $(SOAK_SECONDS) \
		> $(COVERAGE_DIR)/sweep.log 2>&1 \
		|| { cat $(COVERAGE_DIR)/sweep.log; exit 1; }
	@tail -n 1 $(COVERAGE_DIR)/sweep.log
	@$(LLVM_PROFDATA) merge -sparse \
		$$(find $(COVERAGE_DIR)/profraw -name '*.profraw') \
		-o $(COVERAGE_DIR)/chuck.profdata
	@for source in $(wildcard $(SRC_DIR)/*.c); do \
		$(LLVM_COV) report $(COVERAGE_DIR)/chuck-cov \
			-instr-profile=$(COVERAGE_DIR)/chuck.profdata \
			-show-functions -sources $$source 2>/dev/null \
		| awk -v name="$$source" \
			'$$1 != "Filename" && $$1 != "TOTAL" && $$4 == "0.00%" \
				{ print name, $$1 }'; \
	done | sort -u > $(COVERAGE_DIR)/sweep_zero.txt
	@for source in $(TEST_SOURCES); do \
		case $$source in tests/*) continue ;; esac; \
		$(LLVM_COV) report $(COVERAGE_DIR)/core_tests \
			-instr-profile=$(COVERAGE_DIR)/core_tests.profdata \
			-show-functions -sources $$source 2>/dev/null \
		| awk -v name="$$source" \
			'$$1 != "Filename" && $$1 != "TOTAL" && $$4 != "0.00%" \
				{ print name, $$1 }'; \
	done | sort -u > $(COVERAGE_DIR)/suite_run.txt
	@echo
	@echo "functions the sweep never executes:"
	@awk '{ printf "  %s %s\n", $$1, $$2 }' $(COVERAGE_DIR)/sweep_zero.txt \
	| sort | awk 'END { if (NR == 0) print "  none" } { print }'
	@echo
	@echo "functions NEITHER gate executes (the honest list):"
	@comm -23 $(COVERAGE_DIR)/sweep_zero.txt $(COVERAGE_DIR)/suite_run.txt \
	| awk '{ printf "  %-24s %s\n", $$1, $$2; total++ } \
	       END { if (total) printf "\n  %d function(s) no gate runs\n", total; \
		     else print "  none" }'

# `all test` built the sanitized game and then ran only the suite, which links no
# SDL — so the renderers, the level art, the audio synth and the cutscenes were
# compiled with ASan and UBSan and never executed by either. More than half the
# tree, under the one target whose name promises otherwise. `soak` is what closes
# that: it runs `build/chuck-sanitize` over every sector on the dummy video
# driver, which really does rasterize.
#
# **And the editor is built here too**, which it was not for as long as this
# target existed. `all` is the game, so `editor_app.c`, `editor_render.c` and
# `editor_ui.c` were never compiled under a sanitizer at all — never mind run —
# while the three editor translation units the suite *does* link are precisely
# the ones that touch no SDL. The sweep runs it now; see tools/soak.sh.
#
# The soak is last, because it is the slow half and a failure in the suite is the
# cheaper one to be told about first.
#
# **And "last" was only true while nothing was built in parallel.** The goals
# were one list — `all editor test soak` — and goals on one command line are
# exactly what `make -j` runs at the same time: the sweep would start on a game
# that was still linking and on an editor that did not exist yet, and the second
# of those is silent, because tools/soak.sh *skips* an editor it cannot find
# rather than failing on it. A sanitized run of the one binary the sweep was
# written to reach would have reported `soak: ok` having never opened it. Two
# invocations is the ordering, stated rather than assumed, and it is what makes
# `make -j$(nproc) sanitize` safe — which is the difference between this job
# taking five minutes and fifteen on a CI runner.
#
# The variables are one list for the same reason: the second `$(MAKE)` has to
# agree with the first about what it built, or a binary that looks a second out
# of date is relinked by it — with no sanitizer in it — and soaked in that state.
SANITIZE_VARS := BUILD_DIR=build/sanitize TARGET=build/chuck-sanitize \
	EDITOR_TARGET=build/chuck-editor-sanitize \
	SOAK_BINARY=build/chuck-sanitize \
	SOAK_EDITOR=build/chuck-editor-sanitize \
	CFLAGS="$(CFLAGS) $(SANITIZER_FLAGS)" \
	LDFLAGS="$(LDFLAGS) $(SANITIZER_FLAGS)" \
	TEST_CFLAGS="$(TEST_CFLAGS) $(SANITIZER_FLAGS)"

sanitize:
	$(MAKE) $(SANITIZE_VARS) all editor test
	$(MAKE) $(SANITIZE_VARS) soak

# ---- The four releases ----------------------------------------------------
#
# One target per platform, one script per platform, and that is the whole of it:
#
#   make mac     dist/Chuck-<v>-macos.zip          (and the .app and the .dmg)
#   make win     dist/Chuck-<v>-windows-x64.zip
#   make linux   dist/Chuck-<v>-linux-x86_64.tar.gz
#   make web     dist/Chuck-<v>-web.zip            (played in the browser)
#
# Each script goes from this tree to one archive in dist/ and stops there, in the
# same four steps: the library, the game, the payload, the archive. Nothing here
# uploads anything and nothing here knows an account exists — publishing is a
# decision, not a build step.
#
# Which machine can make which is a fact rather than a taste. macOS needs a Mac:
# two slices, a Developer ID signature, and Apple's notary service on the other
# end of the network. Linux needs a Linux, because its SDL is compiled against
# that userland. Windows cross-builds anywhere mingw-w64 runs, a Mac included,
# which is why it is also this tree's cheapest second opinion on the C.
# .github/workflows/payloads.yml exists to hand back the one a Mac cannot make.
#
# The web is the fourth and the only one that is not a download: itch.io runs an
# HTML5 build in an iframe on the game's own page, which is how most of that
# site is played. It cross-builds anywhere emscripten does, a Mac included, and
# it is the one archive whose index.html has to sit at the *root* rather than
# inside a named folder — see the note at the top of packaging/build_web.sh.
#
# It was six targets and four scripts, and macOS had three of the targets and two
# of the scripts to itself — `app`, `notarize` and an `itch-macos` named after a
# shop, plus a `release-macos` whose entire job was to name the order the other
# three had to run in. That split is what shipped an archive nobody could open:
# the target that cut the zip depended on the one that deletes and re-signs the
# bundle, so it threw away the notarization ticket and packed the result. **A
# platform whose release is one target and one script cannot be run in the wrong
# order**, which is worth more than being able to stop halfway.
mac:
	packaging/build_macos.sh

win:
	packaging/build_windows.sh

linux:
	packaging/build_linux.sh

web:
	packaging/build_web.sh

# Not one of the three: the bundle built and signed without going near Apple,
# which is what the macOS CI job checks on every push. No signing identity is
# needed — build_macos.sh falls back to an ad-hoc signature — and it still proves
# the bundle assembles, links the vendored framework and verifies. Anything you
# would hand to a player comes out of `make mac`.
app:
	MACOS_BUNDLE_ONLY=1 packaging/build_macos.sh

# vendor/ is a verified download and survives a clean; this refetches it.
sdl3:
	packaging/fetch_sdl3.sh vendor

# `press` is not one of the three and is not a build. Every pixel of this game is
# drawn at runtime, so the store page's screenshots are captured from the built
# game rather than kept as files — which means they can be *rebuilt* after a
# change to the art instead of being re-photographed by hand and quietly left a
# version behind. See tools/press_kit.sh, and `--shot` in src/main.c.
press: $(TARGET)
	PRESS_BINARY=$(CURDIR)/$(TARGET) tools/press_kit.sh

# vendor/ is a verified download and survives a clean; `make sdl3` refetches it.
clean:
	rm -rf $(BUILD_DIR) dist $(TARGET) $(EDITOR_TARGET)

-include $(DEPENDENCIES)
-include $(EDITOR_DEPENDENCIES)
