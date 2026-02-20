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
X11_CFLAGS = $(shell pkg-config --cflags x11 xpm 2>/dev/null)
X11_LIBS = $(shell pkg-config --libs x11 xpm 2>/dev/null)

# xscreensaver X11 path - enable when pkg-config finds it, or on Linux with headers, or FORCE_X11=1
HAVE_X11 =
ifneq ($(X11_CFLAGS),)
  HAVE_X11 = 1
  X11_LIBS := $(or $(X11_LIBS),-lX11 -lXpm)
endif
ifeq ($(HAVE_X11),)
  ifeq ($(shell uname -s 2>/dev/null),Linux)
    ifeq ($(shell test -f /usr/include/X11/Xlib.h 2>/dev/null && echo y),y)
      HAVE_X11 = 1
      X11_CFLAGS =
      X11_LIBS = -lX11 -lXpm
    endif
  endif
endif
ifdef FORCE_X11
  HAVE_X11 = 1
  X11_CFLAGS =
  X11_LIBS = -lX11 -lXpm
endif

SRCS = src/flying-toasters.c src/xpm.c
X11_SRCS =
TARGET = bin/flying-toasters
ifdef HAVE_X11
  X11_SRCS = src/xscreensaver-x11.c
  CFLAGS += -DHAVE_XSCREENSAVER_X11
endif

.PHONY: build clean init run all

build: init clean
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $(X11_CFLAGS) -o $(TARGET) $(SRCS) $(X11_SRCS) $(SDL_LIBS) $(if $(X11_SRCS),$(X11_LIBS),)

clean:
	rm -f $(TARGET)

init:
	mkdir -p bin

run:
	./bin/flying-toasters -windowed

all: build run
