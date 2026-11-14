# Endless Expanse

A retro vector-style space exploration game: one starship, lost far from home. C++20, SDL2, OpenGL 3.3 core. See `PLAN.md` for the development roadmap.

## Build and run (macOS)

```sh
brew install cmake sdl2
cmake -B build
cmake --build build -j
./build/endless_expanse
ctest --test-dir build --output-on-failure
```

The first configure fetches doctest from GitHub. Other platforms need an OpenGL loader (glad) added in `src/render/gl.h`.

## Current controls (M1 placeholder)

| Action | Result |
|---|---|
| Left-click on the main menu | Start playing |
| Right-click while playing | Pause |
| Left-click while paused | Resume |
| Right-click while paused | Back to the main menu |
| Middle-click | Toggle the FPS/frame-time readout in the window title |

## Layout

- `src/core`: simulation-side code. It must not include SDL or GL (fixed timestep, state stack, input snapshot, logging).
- `src/platform`: SDL window, GL context, and event polling.
- `src/render/gl.h`: the only file that includes OpenGL headers.
- `src/app`: the application loop and game states.
- `tests`: doctest unit tests for `ee_core`.
