# Decisions

Short records of choices made during development. Newest last.

## D1: OpenGL 3.3 core, renderer isolated in `src/render`
macOS caps OpenGL at 4.1, so we target 3.3 core. Only `src/render` and `src/platform` touch GL/SDL, so a different backend can be swapped in later. `src/core` and `src/vector` must stay free of SDL and GL.

## D2: World space is Y-down
World coordinates match screen and mouse coordinates (origin top-left in screen space, +Y downwards), so no flips are needed anywhere. A positive rotation turns clockwise on screen. A shape's forward direction is +X. Cameras (M3) build on `Renderer::worldView()`.

## D3: Logical points for all drawing and line widths
Everything is drawn in logical points (the same units as mouse input). `Renderer` converts to drawable pixels using the pixel ratio (2.0 on Retina). Line widths are specified in points and stay constant when zooming.

## D4: Line rendering
Each segment is one GPU instance, expanded to a quad in the vertex shader. The fragment shader draws a capsule distance field, giving anti-aliased edges and round caps/joins with no MSAA. Blending is additive (`SRC_ALPHA, ONE`) so overlapping strokes brighten, like a vector monitor.

## D5: Bloom
Scene renders to an RGBA16F target. Bloom is a downsample plus separable gaussian chain. Off: render straight to the screen. Low: one level at half resolution. High: three levels (1/2, 1/4, 1/8).

## D6: Stroke font
Text is drawn with a built-in vector stroke font (4x6 grid, A-Z, 0-9, common punctuation, lowercase shown as uppercase) so the HUD matches the vector look. Glyphs are data in `src/vector/StrokeFont.cpp`.

## D7: GL loader deferred
macOS provides GL 3.3 core functions directly via `<OpenGL/gl3.h>`. A loader (glad) will be added in `src/render/gl.h` when another platform is targeted.

## D8: Camera
`Camera2D` lives in `src/camera` (no GL) and is stepped by the fixed simulation timestep; rendering uses `view(alpha)` to interpolate. Zoom eases exponentially in log space, and zooming holds a world-space anchor under the cursor until the zoom settles. Dragging pans immediately (no lag). Zoom range is 0.02x (far out) to 4x (close). Mouse conversions use `current()`; drawing uses `view(alpha)`.

## D9: Mouse button assignments
- Wheel: zoom at cursor.
- Left-drag (after 4 px) or middle-drag on empty space: pan.
- Plain left-click on the world is reserved for selection (M4). Right-click is reserved for "go here" (M4).
- Middle-click without dragging toggles the debug readout.
- Pause is an on-screen button, not a click gesture.

## D10: Culling and level of detail
Shapes outside the viewport are skipped on the CPU. Shapes whose on-screen radius is under 3 px collapse to a single dot (1 segment instead of ~10-50). The player ship is drawn as an enlarged icon when zoomed far out so it never disappears. The grid spacing grows by 10x steps to stay readable.

## D11: Ship model and autopilot
`Ship` (in `src/sim`, no GL) has inertia, a turn-rate limit and forward-only thrust, so stopping means flipping and braking. The autopilot steers toward a desired velocity: a sqrt(2*a*d) braking profile (planned at 55% of full thrust, with the turn-around time subtracted from the available distance), linear within 20 units for a smooth final approach, and capped at max speed. It only thrusts when the nose is within 60 degrees of the desired direction, ignores tiny velocity errors (dead zone) so it never flip-flops, and finishes inside 4 units at under 30 units/s. Drag assist (QOL, on by default) bleeds speed when there is no destination. Tests cover arrival, overshoot, determinism, speed and turn-rate limits, and mid-flight retargeting.

## D12: Mouse scheme in play
Right-click sets a destination (hold to keep steering). Left-click selects; double-click flies to the selection. Left or middle drag pans and turns camera FOLLOW off; the FOLLOW button turns it back on. While following, the wheel zooms around the ship so it stays centred.
