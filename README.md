# Flying Toasters

Classic [After Dark](https://en.wikipedia.org/wiki/After_Dark_(software)) screensaver recreation.

![image](https://user-images.githubusercontent.com/1062217/231195791-1b5be6d7-5461-4243-8199-2a7dc88458d4.png)

Runs on **Wayland**, **X11**, and **Raspberry Pi** using SDL2.

## Building

### Dependencies

- **Raspberry Pi / Debian / Ubuntu:**
  ```bash
  sudo apt install build-essential pkg-config libsdl2-dev
  ```
- **macOS:**
  ```bash
  brew install sdl2
  ```

### Build

```bash
make build
```

The binary will be in `bin/flying-toasters`. Run `make run` to preview in windowed mode, or `./bin/flying-toasters` for fullscreen.

## Raspberry Pi & Wayland

On Raspberry Pi OS (64-bit, Bookworm) with Wayland, SDL2 uses Wayland automatically. No extra setup needed.

To force Wayland if both X11 and Wayland are available:
```bash
SDL_VIDEODRIVER=wayland ./bin/flying-toasters
```

**Controls:** Press Escape or close the window to exit.

## Using as a Screensaver

- **Wayland:** Use with a Wayland screensaver/inhibit daemon. Some options:
  - [swayidle](https://github.com/swaywm/swayidle) + custom script
  - [wayland-idle-inhibit](https://github.com/nwg-piotr/nwg-shell)
- **X11 / XScreensaver:** Add to `~/.xscreensaver`:
  ```
  /usr/local/bin/flying-toasters
  ```

## Docker

Cross-compile for Linux (e.g. from macOS):
```bash
docker build -t flying-toasters .
docker create --name ft flying-toasters
docker cp ft:/flying-toasters ./
docker rm ft
chmod +x flying-toasters
```
