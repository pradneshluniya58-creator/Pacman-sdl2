# Pac‑Man (C + SDL2)

A lightweight Pac‑Man clone written in C using SDL2, SDL_ttf, and SDL2_mixer, featuring classic scatter/chase ghost behavior, frightened mode, deterministic ghost steering, and a clean pause overlay with text 

[Gameplay screenshots](/assets/images/)

## Features
- Classic ghost schedule: global scatter ↔ chase cycles for all four ghosts, with frightened mode from power pellets .
- Deterministic steering at intersections for predictable movement during chase/scatter .
- Pause overlay with “GAME OVER” / “YOU WIN” and quick restart .
- Keyboard controls: Arrow keys and W/A/S/D .
- Main menu with Play, Levels (locked/available), Controls, Credits, and Quit .
- Chiptune background music in gameplay and a calm pause track wired to game states as discussed in this Space .
- Single‑file codebase for easy reading and hacking .

## Requirements
- SDL2 (Simple DirectMedia Layer 2) .
- SDL2_ttf for font/text rendering .
- SDL2_mixer for audio playback (compulsory) .
- A TrueType font at `assets/DejaVuSans.ttf` (or update the path in code) .

## Install (macOS)
- brew install sdl2 sdl2_ttf sdl2_mixer

## Build (macOS)
- clang pacman2.c $(pkg-config –cflags –libs sdl2 sdl2_ttf sdl2_mixer) -o 
pacman2

## Run (macOS)
-./pacman2


---

## Install (Ubuntu/Debian Linux)
- sudo apt update
- sudo apt install -y libsdl2-dev libsdl2-ttf-dev libsdl2-mixer-dev build-essential pkg-config

## Build (Ubuntu/Debian Linux)
- gcc pacman2.c $(pkg-config –cflags –libs sdl2 sdl2_ttf sdl2_mixer) -o pacman2

## Run (Ubuntu/Debian Linux)
- ./pacman2

---

## Windows (MSYS2 — recommended)
- Use the UCRT64 shell in MSYS2, install the toolchain and SDL libraries, then build with pkg-config as below .
- pacman -S --needed mingw-w64-ucrt-x86_64-toolchain 
- mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf 
- mingw-w64-ucrt--x86_64-SDL2_mixer mingw-w64-ucrt-x86_64-pkgconf
- clang pacman2.c $(pkg-config --cflags --libs sdl2 sdl2_ttf sdl2_mixer) -o pacman2
- /.pacman2.exe

---

## Controls
- Arrow keys or W/A/S/D: move .
- Enter or Space (when paused): restart .
- Esc: pause/quit menu .

## Troubleshooting
- If no text is rendered, ensure the font exists at `assets/DejaVuSans.ttf`, or update the path in code .
- If linker errors occur for SDL2/SDL2_ttf/SDL2_mixer, confirm the libraries are installed and verify `pkg-config --libs sdl2 sdl2_ttf sdl2_mixer` in your shell .
- If relative paths break, run the game from the project directory or switch to an absolute font path via base‑path logic in code .

## Roadmap
- Sprites/animations for Pac‑Man and ghosts .
- Additional sound effects and background music polish .
- Multiple levels, fruit, and score table .
- Flashing frightened animation near timeout .

## License
This project is licensed under the GNU General Public License v3 (GPL v3); see the LICENSE file in the repository for the full text and obligations, including source‑code availability with binary distribution and preservation of copyleft terms .

## Credits
- Made by Pradnesh Luniya .
- Music (gameplay): FREE Action Chiptune Music Pack by PPEAK (Preston Peak) — licensed under CC BY 4.0; attribution required as “PPEAK” or “Preston Peak” as specified by the pack .
- Music (pause): “Innocence” from JRPG Music Pack 4 (Calm) by Juhani Junkala (SubspaceAudio) — used per the pack’s OpenGameArt license page with attribution provided here .
- Libraries: SDL2, SDL2_ttf, SDL2_mixer .
- Game design inspired by Namco’s Pac‑Man .
