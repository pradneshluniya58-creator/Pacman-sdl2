# Pac‑Man (C + SDL2)

A lightweight Pac‑Man clone written in C using SDL2 and SDL_ttf, featuring classic scatter/chase ghost behavior, frightened mode, and a clean pause overlay with text.

![Gameplay Screenshot]('assets/Screenshot 2025-10-21 at 8.51.57 PM.png')

## Features
- Classic ghost schedule: global scatter ↔ chase cycles for all four ghosts, with frightened mode on power pellets.
- Deterministic steering at intersections (predictable turns, no random wiggle during chase/scatter).
- Pause overlay with “GAME OVER” / “YOU WIN” and quick restart.
- Single‑file codebase for easy reading and hacking.

## Requirements
- SDL2
- SDL2_ttf
- A TrueType font at `assets/DejaVuSans.ttf` (or update the path in `TTF_OpenFont`).

## Install (macOS)
brew install sdl2 sdl2_ttf

## Build (macOS)
clang pacman2.c $(pkg-config --cflags --libs sdl2 sdl2_ttf) -o pacman2

## Run (macOS)
./pacman2

---

## Install (Ubuntu/Debian Linux)
sudo apt update 
sudo apt install -y libsdl2-dev libsdl2-ttf-dev build-essential pkg-config

## Build (Linux)
gcc pacman2.c $(pkg-config --cflags --libs sdl2 sdl2_ttf) -o pacman2

## Run (Linux)
./pacman2

---

## Windows (MSYS2 — recommended)

1. Install MSYS2: https://www.msys2.org (open the “UCRT64” shell after install).  
2. Install toolchain and SDL:
pacman -S –needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf pkgconf

## Build
clang pacman2.c $(pkg-config --cflags --libs sdl2 sdl2_ttf) -o pacman2

## Run
/.pacman2

## Controls
- Arrow keys: move
- Enter or Space (when paused): restart
- Esc: quit

## Troubleshooting
- No text rendered: ensure the font exists at `assets/DejaVuSans.ttf`, or change the path passed to `TTF_OpenFont`.
- Linker errors for SDL: confirm SDL2/SDL2_ttf are installed and that `pkg-config` is on PATH; try `pkg-config --libs sdl2 sdl2_ttf` to verify.
- Relative paths: run the game from the project directory, or switch to building the absolute font path via `SDL_GetBasePath`.

## Roadmap
- Sprites/animations for Pac‑Man and ghosts
- Sound effects
- Multiple levels and fruit/score table
- Frightened flashing near timeout

## License
Not yet

## Credits
- SDL2 and SDL_ttf
- Game design inspired by Namco’s Pac‑Man
