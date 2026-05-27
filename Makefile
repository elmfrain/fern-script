CC = gcc
CFLAGS = -Wall -Werror -std=c99 -ggdb
LDFLAGS =
BUILD_DIR = build
SOURCES = main.c errors.c memarena.c strings.c arrays.c
TARGET := $(BUILD_DIR)/fernc

OBJECTS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SOURCES))

all: $(TARGET)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)
	@echo "Built target $(TARGET)"

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build

.PHONY: clean
