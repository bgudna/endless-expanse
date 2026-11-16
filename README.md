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
| Right-click on the map | Fly to the point and circle it at 120 units (autopilot: turn, thrust, coast, flip, brake, then orbit) |
| Hold the right button | Keep steering toward the cursor |
| Left-click an object | Select it (info shown at left) |
| Double-click an object | Start an orbit of it (same as the ORBIT button) |
| Left-click an object, then ORBIT | Circle it, 50 units off its surface. The orbit follows moving ships. Selecting something else does not end it |
| Right-click or hold the right button | Ends an orbit and flies to the clicked point |
| Mouse wheel | Zoom (around the ship while FOLLOW is on, otherwise toward the cursor) |
| Left-drag or middle-drag | Pan the camera (turns FOLLOW off) |
| Zoom slider (right edge) | Drag to zoom |
| HUD buttons (bottom-left) | FOLLOW on/off, drag ASSIST on/off, PAUSE, cycle BLOOM, GRID on/off (sector borders, labels, grid), FIRE mode, ORBIT (enabled while something is selected) |
| Middle-click (no drag) | Toggle the on-screen FPS / frame-time / segment-count readout |
| Left-click an alien | Select and target it (turret auto-fires per the Weapons toggle) |
| Weapons button | FIRE: FREE / FIRE: HOSTILE ONLY |
| Hail panel | Click Accept / Decline when a trader or survivor hails |
| Left-click after ship destroyed | Restart |
| Left-click while paused | Resume |
| Right-click while paused | Back to the main menu |

Alien types are defined in `assets/data/aliens.json`. Hazards: rocks, mines, black holes, radiation nebulae.

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
