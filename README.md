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

## Current controls (M5 demo)

| Action | Result |
|---|---|
| Left-click on the main menu | Start the demo scene |
| Right-click on the map | Fly the ship there (autopilot: turn, thrust, coast, flip, brake) |
| Hold the right button | Keep steering toward the cursor |
| Left-click an object | Select it (info shown at left) |
| Double-click an object | Fly to it, stopping 50 units off its surface |
| Mouse wheel | Zoom (around the ship while FOLLOW is on, otherwise toward the cursor) |
| Left-drag or middle-drag | Pan the camera (turns FOLLOW off) |
| Zoom slider (right edge) | Drag to zoom |
| HUD buttons (bottom-left) | FOLLOW on/off, drag ASSIST on/off, PAUSE, cycle BLOOM, GRID on/off (sector borders, labels, grid) |
| Middle-click (no drag) | Toggle the on-screen FPS / frame-time / segment-count readout |
| Left-click while paused | Resume |
| Right-click while paused | Back to the main menu |

## Layout

- `src/core`: simulation-side code. It must not include SDL or GL (fixed timestep, state stack, input snapshot, logging, RNG).
- `src/camera`: `Camera2D` (smooth pan/zoom, zoom-at-cursor) and `CameraView` (coordinate transforms, culling). No GL.
- `src/sim`: ship physics and autopilot, engine exhaust (no GL, deterministic).
- `src/vector`: vector shapes, procedural placeholder art, the stroke font, and `Transform2D` (no GL).
- `src/render`: line batch, bloom post-process, and `Renderer`. `gl.h` is the only file that includes OpenGL headers.
- `src/platform`: SDL window, GL context, and event polling.
- `src/app`: the application loop and game states.
- `tests`: doctest unit tests for `ee_core`.
- `docs/decisions.md`: design decisions.
