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

The first configure fetches doctest and glm from GitHub. Other platforms need an OpenGL loader (glad) added in `src/render/gl.h`.

## Current controls (M2 demo)

| Action | Result |
|---|---|
| Left-click on the main menu | Start the demo scene |
| Left-click while playing | Cycle bloom quality: Off, Low, High |
| Mouse wheel while playing | Add or remove 50 asteroids (stress test, up to 3000 / about 30,000 segments) |
| Move the mouse while playing | The ship turns to face the cursor |
| Right-click while playing | Pause |
| Left-click while paused | Resume |
| Right-click while paused | Back to the main menu |
| Middle-click | Toggle the on-screen FPS / frame-time / segment-count readout |

## Layout

- `src/core`: simulation-side code. It must not include SDL or GL (fixed timestep, state stack, input snapshot, logging, RNG).
- `src/vector`: vector shapes, procedural placeholder art, the stroke font, and `Transform2D` (no GL).
- `src/render`: line batch, bloom post-process, and `Renderer`. `gl.h` is the only file that includes OpenGL headers.
- `src/platform`: SDL window, GL context, and event polling.
- `src/app`: the application loop and game states.
- `tests`: doctest unit tests for `ee_core`.
- `docs/decisions.md`: design decisions.
