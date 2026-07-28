CC := cc
CPPFLAGS :=
CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -O2 $(shell pkg-config --cflags sdl3)
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
	src/camera.c src/chase.c src/enemy.c src/game_event.c src/gameplay_ai.c src/gameplay_combat.c \
	src/gameplay_climb.c src/gameplay_interaction.c src/gameplay_physics.c src/gameplay_world.c \
	src/gameplay_state.c src/level.c src/level_route.c src/player.c src/rng.c \
	editor/editor_doc.c editor/editor_legend.c editor/editor_validate.c

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES)) \
	$(EMBEDDED_LEVELS_OBJECT)
DEPENDENCIES := $(OBJECTS:.o=.d)

# The level editor is its own binary, but not its own idea of what a level is:
# it links the game's parser, art direction and route model so what it draws
# and what it reports cannot drift from the game and the tests.
EDITOR_DIR := editor
EDITOR_TARGET := chuck-editor
EDITOR_SOURCES := $(wildcard $(EDITOR_DIR)/*.c) \
	$(SRC_DIR)/level.c $(SRC_DIR)/level_art.c $(SRC_DIR)/level_route.c \
	$(SRC_DIR)/rng.c
EDITOR_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/editor/%.o,$(notdir $(EDITOR_SOURCES)))
EDITOR_DEPENDENCIES := $(EDITOR_OBJECTS:.o=.d)

.PHONY: all release debug run run-debug run-editor editor test sanitize clean

all: $(TARGET)

release: all

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

$(TEST_TARGET): $(TEST_SOURCES) $(EMBEDDED_LEVELS_SOURCE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $(TEST_SOURCES) $(EMBEDDED_LEVELS_SOURCE) -o $@ -lm

test: $(TEST_TARGET)
	./$(TEST_TARGET)

sanitize:
	$(MAKE) BUILD_DIR=build/sanitize TARGET=build/chuck-sanitize \
		CFLAGS="$(CFLAGS) $(SANITIZER_FLAGS)" \
		LDFLAGS="$(LDFLAGS) $(SANITIZER_FLAGS)" \
		TEST_CFLAGS="$(TEST_CFLAGS) $(SANITIZER_FLAGS)" all test

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(EDITOR_TARGET)

-include $(DEPENDENCIES)
-include $(EDITOR_DEPENDENCIES)
