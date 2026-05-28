CC = gcc
FLAGS = -fsanitize=address
CFLAGS = -Wall -Werror -std=c99 -ggdb $(FLAGS)
LDFLAGS = $(FLAGS)
BUILD_DIR = build
SOURCES = main.c errors.c memarena.c strings.c arrays.c lexer.c parser.c
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
