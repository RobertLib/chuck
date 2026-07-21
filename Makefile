CC := cc
CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -O2 $(shell pkg-config --cflags sdl3)
DEPFLAGS := -MMD -MP
LDFLAGS := $(shell pkg-config --libs sdl3) -lm

SRC_DIR := src
BUILD_DIR := build
TARGET := chuck
LEVEL_GENERATOR := tools/embed_levels.py
LEVEL_FILES := $(wildcard levels/level*.txt)
EMBEDDED_LEVELS_SOURCE := $(BUILD_DIR)/embedded_levels.c
EMBEDDED_LEVELS_OBJECT := $(BUILD_DIR)/embedded_levels.o

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES)) \
	$(EMBEDDED_LEVELS_OBJECT)
DEPENDENCIES := $(OBJECTS:.o=.d)

.PHONY: all clean run

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

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPENDENCIES)
