# Flying Toasters - SDL2 version (Wayland, Raspberry Pi compatible)
# Dependencies: libsdl2-dev (sudo apt install libsdl2-dev)
#
# On Raspberry Pi with Wayland, SDL2 will use Wayland automatically.
# To force Wayland: SDL_VIDEODRIVER=wayland ./bin/flying-toasters
# To run windowed: ./bin/flying-toasters -windowed

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra
SDL_CFLAGS = $(shell pkg-config --cflags sdl2 2>/dev/null || sdl2-config --cflags 2>/dev/null)
SDL_LIBS = $(shell pkg-config --libs sdl2 2>/dev/null || sdl2-config --libs 2>/dev/null)

SRCS = src/flying-toasters.c src/xpm.c
TARGET = bin/flying-toasters

.PHONY: build clean init run all

build: init clean
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $(TARGET) $(SRCS) $(SDL_LIBS)

clean:
	rm -f $(TARGET)

init:
	mkdir -p bin

run:
	./bin/flying-toasters -windowed

all: build run
