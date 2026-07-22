CC := cc
CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -O2 $(shell pkg-config --cflags sdl3)
DEPFLAGS := -MMD -MP
LDFLAGS := $(shell pkg-config --libs sdl3) -lm
TEST_CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -O2 -Isrc
SANITIZER_FLAGS := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer

SRC_DIR := src
BUILD_DIR := build
TARGET := chuck
LEVEL_GENERATOR := tools/embed_levels.py
LEVEL_FILES := $(wildcard levels/level*.txt)
EMBEDDED_LEVELS_SOURCE := $(BUILD_DIR)/embedded_levels.c
EMBEDDED_LEVELS_OBJECT := $(BUILD_DIR)/embedded_levels.o
TEST_TARGET := $(BUILD_DIR)/core_tests
TEST_SOURCES := tests/test_main.c \
	src/enemy.c src/game_event.c src/gameplay_ai.c src/gameplay_combat.c \
	src/gameplay_interaction.c src/gameplay_physics.c src/gameplay_world.c \
	src/gameplay_state.c src/level.c src/player.c src/rng.c

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES)) \
	$(EMBEDDED_LEVELS_OBJECT)
DEPENDENCIES := $(OBJECTS:.o=.d)

.PHONY: all clean run test sanitize

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(EMBEDDED_LEVELS_SOURCE): $(LEVEL_GENERATOR) $(LEVEL_FILES) | $(BUILD_DIR)
	python3 $(LEVEL_GENERATOR) $@ $(LEVEL_FILES)

$(EMBEDDED_LEVELS_OBJECT): $(EMBEDDED_LEVELS_SOURCE) $(SRC_DIR)/embedded_levels.h
	$(CC) $(CFLAGS) $(DEPFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: all
	./$(TARGET)

$(TEST_TARGET): $(TEST_SOURCES) $(EMBEDDED_LEVELS_SOURCE) | $(BUILD_DIR)
	$(CC) $(TEST_CFLAGS) $(TEST_SOURCES) $(EMBEDDED_LEVELS_SOURCE) -o $@ -lm

test: $(TEST_TARGET)
	./$(TEST_TARGET)

sanitize:
	$(MAKE) BUILD_DIR=build/sanitize TARGET=build/chuck-sanitize \
		CFLAGS="$(CFLAGS) $(SANITIZER_FLAGS)" \
		LDFLAGS="$(LDFLAGS) $(SANITIZER_FLAGS)" \
		TEST_CFLAGS="$(TEST_CFLAGS) $(SANITIZER_FLAGS)" all test

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPENDENCIES)
