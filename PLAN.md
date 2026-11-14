# Endless Expanse: Agentic Development Plan

## 1. Vision

A single starship is stranded far from home. The player pilots it across a procedurally generated galaxy of sectors, meeting aliens and hazards, while managing resources on the way home (think *Star Trek: Voyager*).

- **Look:** retro vector-monitor aesthetic (glowing lines, phosphor bloom, CRT-ish feel) rendered at smooth 60 fps.
- **Feel:** modern QOL (pause, autosave, undo-friendly UI, tooltips, rebindable and scalable UI, accessibility options).
- **Input:** 100% mouse (move, zoom, select, interact, menus). The keyboard is never required. Optional keyboard shortcuts may be added later, but nothing depends on them.
- **Tech:** C++20, SDL2 (window, input, audio, timing), OpenGL 3.3 core (graphics), CMake.
- **Art pipeline:** the user draws vector assets later. Until then everything uses procedural basic shapes behind a stable "vector asset" interface.

## 2. Guiding Principles for Agents

1. **Always shippable:** every milestone ends with a game that builds, runs, and passes tests.
2. **Small vertical slices:** finish one thin, working feature before widening.
3. **Data-driven:** ships, aliens, and events are defined in data files (JSON), not hard-coded.
4. **Deterministic simulation:** fixed timestep (e.g. 60 Hz) and a seeded RNG, so bugs are reproducible and tests are stable. Rendering interpolates between steps.
5. **Separate sim from render:** simulation code must not include SDL or GL headers. This makes it headlessly testable.
6. **Do not hardcode art:** all drawables go through `VectorShape`/`VectorAsset`, so imported art drops in without gameplay changes.
7. **Record decisions** in `docs/decisions.md` (short ADR entries) when choosing between alternatives.
8. **Performance budget:** 16.6 ms/frame. No per-frame heap allocations in hot paths, and no GL state thrash.

## 3. Proposed Architecture

```
endless-expanse/
  CMakeLists.txt
  cmake/                  # toolchain, warnings, dependency helpers
  docs/                   # decisions.md, design.md, asset-format.md
  assets/
    vectors/              # user-drawn vector files (SVG subset / custom)
    data/                 # ships.json, aliens.json, events.json, ...
    shaders/
    audio/
  src/
    app/                  # main, Application, main loop, state stack
    platform/             # SDL window, input events, timing, audio wrapper
    render/               # GL context, shader mgr, line renderer, bloom, text
    vector/               # VectorShape, VectorAsset, loaders, tessellation
    core/                 # math, RNG, ECS/entity store, event bus, serialization
    sim/                  # physics, ships, AI, combat, resources, world gen
    world/                # sectors, galaxy map, procedural generation
    camera/               # pan/zoom, LOD logic
    ui/                   # mouse-driven widgets, HUD, menus, tooltips
    game/                 # game states, campaign/progress, save/load
  tests/                  # unit + headless sim tests (Catch2 or doctest)
  tools/                  # asset validators, SVG-to-asset converter
```

**Key choices (confirm or revise in `docs/decisions.md`):**

| Concern | Choice | Reason |
|---|---|---|
| Build | CMake + FetchContent or vcpkg | Portable, agent-friendly |
| GL loader | glad (GL 3.3 core) | macOS-compatible (macOS caps at 4.1) |
| Math | glm | Standard and lightweight |
| Data | nlohmann/json | Easy data-driven content |
| Tests | doctest or Catch2 | Headless sim tests |
| Entities | Simple ECS (e.g. entt) or a hand-rolled SoA store | Many entities when zoomed out |
| Text | Vector stroke font (own) | Matches the retro vector look |
| Audio | SDL_mixer or miniaudio, with synthesized retro sfx | Optional until later |

> macOS note: OpenGL is deprecated but works up to 4.1. Target **3.3 core** and avoid compute shaders and newer features.

**Vector rendering approach:**
- Shapes are polylines and polygons (outline-first, as on vector monitors).
- Draw lines as GPU-expanded quads (screen-space width, round/anti-aliased ends) in one batched draw call.
- Glow: render to an offscreen FBO, then do a threshold, blur, and additive composite pass. Add subtle persistence/flicker as a toggle.
- Zoom-independent line width, plus LOD (simplify or replace with icons at far zoom).

## 4. Milestones

Each milestone lists **tasks**, **deliverables**, and **acceptance criteria**. Agents should complete them in order unless noted as parallelizable.

### M0: Project Bootstrap
**Tasks**
- Create the CMake project, `.clang-format`, `.gitignore`, and warnings-as-errors for the game target.
- Pull in SDL2, glm, glad, nlohmann/json, and the test framework.
- Open a window with a GL 3.3 core context, vsync on, and clear the screen.
- Add a README with build and run instructions for macOS (and note other platforms).

**Acceptance**
- `cmake -B build && cmake --build build` succeeds from clean.
- The window opens, closes cleanly, and `ctest` runs (even with one trivial test).

### M1: Core Loop and Timing
**Tasks**
- Fixed-timestep loop (60 Hz sim) with render interpolation, a frame-time cap, and a spiral-of-death guard.
- Input layer that translates SDL events into an `InputState` (mouse position, buttons, wheel, drag deltas).
- Game state stack (Boot, MainMenu, Playing, Paused).
- Debug overlay: FPS, frame ms, entity count (toggle by clicking a debug button).
- Logging utility.

**Acceptance**
- Stable 60 fps. Sim steps are verified by a test to be independent of render rate.

### M2: Vector Renderer
**Tasks**
- Batched line renderer (thick, anti-aliased, round caps) with per-vertex color and an additive blend option.
- `VectorShape` (a list of polylines and polygons, a local origin, and a color/glow tag) and a `Transform2D`.
- Procedural basic shapes: triangle ship, circle planet, asteroid polygon, star cross.
- Vector stroke font for HUD text.
- Bloom post-process pass with a quality setting (off, low, high).
- Resize and HiDPI handling (use the drawable size, not the window size).

**Acceptance**
- A demo scene draws 10,000 line segments at 60 fps with bloom on a mid-range machine.
- Visual check: crisp lines, soft glow, correct at Retina scale.

### M3: Camera, Pan and Zoom (mouse only)
**Tasks**
- 2D camera with smooth (critically damped) zoom and pan.
- Mouse wheel zooms toward the cursor. Middle-drag or edge-drag pans. A click-drag on empty space may also pan.
- Zoom range from close tactical view to full-sector view, plus a "galaxy map" level (see M7).
- Line widths stay constant in screen space. LOD rules swap detailed shapes for icons or blips when far out.
- A visible on-screen zoom slider and "recenter on ship" button, so everything is reachable by mouse.

**Acceptance**
- Zoom is smooth with no jitter. Unit tests cover the screen/world coordinate transforms.
- Zooming far out keeps 60 fps with 5,000+ entities in the sector.

### M4: Ship and Movement
**Tasks**
- Player ship entity with Newtonian-lite physics (inertia with optional drag assist, as a QOL toggle).
- Mouse control scheme (the design decision is recorded in `docs/decisions.md`):
  - Right-click on a world point: set a destination (autopilot with rotate, thrust, then brake).
  - Left-click on an object: select it. Double-click: set it as a target or interact.
  - Hold to steer toward the cursor (optional).
- Thruster-trail vector effect and a path preview line.
- Fixed-step physics with unit tests (arrival, braking distance, no overshoot oscillation).

**Acceptance**
- The ship travels to any clicked point smoothly and stops there. Tests prove this deterministically.

### M5: World, Sectors and Procedural Generation
**Tasks**
- Galaxy is a grid or graph of **sectors**. Each sector is generated from `hash(galaxy_seed, sector_coords)`, so it is deterministic and streamable.
- Sector contents: star(s), planets, asteroid fields, nebulae, anomalies, debris, derelicts.
- Home direction: a long-range goal (e.g. N sectors away) with a vague bearing, which the player refines through exploration.
- Sector streaming: load and unload as the ship crosses boundaries, with no hitches.
- Parallax starfield background layers (vector dots and lines).
- Seamless handling of large coordinates (sector-local origin to avoid float precision issues).

**Acceptance**
- The same seed always produces the same sector (tested). Crossing sector borders is seamless.

### M6: Hazards, Aliens and Encounters
**Tasks**
- Collision system (broadphase grid, circle/polygon narrow phase) and damage.
- Hazards: asteroids, radiation/nebula zones (shield or sensor effects), black-hole gravity wells, mines.
- Alien factions defined in `assets/data/aliens.json` (shape, speed, behavior, disposition).
- AI behaviors as a small state machine or behavior tree: Idle, Patrol, Hail, Flee, Pursue, Attack, Trade.
- **Encounter director:** a seeded system that randomly spawns encounters at a tuned pace, using a tension/pacing curve. It avoids spamming and guarantees some quiet time.
- Encounter outcomes: hostile combat, negotiation or trade (a simple menu), distress call, ambush, neutral pass-by.
- Combat: mouse-targeted weapons (click an enemy to target, auto-fire with cooldowns), shields, and hull.

**Acceptance**
- The headless sim test runs 10 minutes of simulated time with no crashes, NaNs, or leaks. The encounter rate matches the config.

### M7: Zoom-Out Strategic View
**Tasks**
- Far zoom shows the whole current sector with icons, sensor ranges, and known entities. Unexplored or out-of-sensor areas are fogged.
- Further zoom shows the galaxy map: explored sectors, discovered points of interest, and the route home.
- Mouse-driven waypointing from the map (click a sector to plot a long-range course). Warp or long-range travel interacts with the encounter director.
- Smooth cross-fade between detail levels.

**Acceptance**
- The player can zoom from the tactical view to the galaxy map continuously and plot a course by clicking.

### M8: Resources, Progression and Win Condition
**Tasks**
- Resources: fuel, energy, hull, shields, supplies, crew morale (keep the set small at first).
- Upgrades and repairs (from salvage, trade, and discoveries).
- Distance-to-home tracking and clues, with a final destination sequence and ending.
- Fail states: out of fuel and stranded, or ship destroyed. Provide soft recovery options (QOL).
- Balance pass using data files and a headless sim "bot" that flies routes to find the difficulty curve.

**Acceptance**
- A full run from start to home is possible. A scripted bot can finish an easy-difficulty run, and a "do nothing" run fails.

### M9: UI/UX and QOL
**Tasks**
- Mouse-only UI toolkit: buttons, sliders, toggles, tooltips, context menus, scrollable lists, modal dialogs. All drawn in the vector style.
- HUD: ship status, minimap or sector scale, target panel, comms log, objective tracker.
- Pause with the mouse, and a time-scale control (pause, 1x, 2x).
- Autosave on sector change plus manual save slots (versioned JSON or binary).
- Settings: resolution/fullscreen, vsync, bloom/CRT intensity, UI scale, colorblind-safe palettes, screen shake toggle, and audio volumes.
- Tutorial as contextual hints that can be dismissed or disabled.

**Acceptance**
- Every function is reachable by mouse. Save/load round-trips exactly (tested).

### M10: Audio and Polish
**Tasks**
- Synthesized or chiptune-style sfx and ambient music. Volumes are configurable.
- Particle-like vector effects: explosions (line fragments), shield ripples, warp streaks.
- CRT options: scanlines, curvature, flicker, and chromatic aberration, all toggleable.
- Profile and optimize (batching, instancing, culling), and fix hitches.

**Acceptance**
- Sustained 60 fps in worst-case scenes. A 30-minute soak test shows no memory growth.

### M11: Vector Asset Import Pipeline (can start in parallel after M2)
**Tasks**
- Define the asset format in `docs/asset-format.md`. Recommended: a **restricted SVG subset** (paths, polylines, polygons, circles, with named groups for hardpoints such as `thruster`, `weapon`, and `hitbox`), converted offline into a compact binary or JSON form by `tools/`.
- A runtime loader and validator with clear error messages.
- Hot-reload in debug builds when asset files change.
- Collision hulls are derived from, or authored alongside, the shape.
- Fallback: if an asset is missing, use the procedural placeholder shape.

**Acceptance**
- The user drops a ship `.svg` into `assets/vectors/`, references it in `ships.json`, and sees it in game with working hitbox and thruster points.

### M12: Release Readiness
**Tasks**
- Packaging (macOS `.app` bundle, and optionally Windows/Linux), versioning, a crash-log file, and CI that builds and runs the tests.
- Final balance, bug bash, and README/screenshots.

## 5. Agent Workflow

For each task, an agent should:

1. **Read** `docs/decisions.md`, this plan, and the relevant source before coding.
2. **Plan** a short checklist of what will change, and flag any deviation from the plan.
3. **Implement** the smallest coherent change. Keep the sim free of SDL/GL dependencies.
4. **Test:** add or update unit or headless tests. Build and run `ctest`.
5. **Verify visually** when the change is graphical: run the game, then capture a screenshot or describe what was seen.
6. **Report** what changed, what was validated, and any open questions.

**Suggested parallel tracks** (disjoint write scopes, so agents don't conflict):
- *Track A, Engine:* M0 to M3 (platform, loop, renderer, camera).
- *Track B, Simulation:* M4 to M6 (physics, world, AI). Starts after M1 and uses the headless test harness.
- *Track C, Tools/UI:* M9 (UI toolkit) and M11 (asset pipeline). Starts after M2.
- Integration points: `VectorShape`, the entity/component definitions, and the data schemas. Change these only via an agreed PR-sized task.

## 6. Testing and Quality Strategy

- **Unit tests:** math, camera transforms, RNG determinism, world generation, physics, serialization.
- **Headless sim tests:** run N simulated minutes with a seed and assert invariants (no NaN, bounded entities, resources stay within range).
- **Golden/replay tests:** record an input sequence plus a seed, and replay it to check the final state hash.
- **Render checks:** manual screenshot review, plus optional image-diff smoke tests.
- **Tooling:** AddressSanitizer/UBSan builds in CI, `clang-tidy` and `clang-format`.
- **Perf:** frame-time capture in the debug overlay, and a benchmark scene at max entity count.

## 7. Risks and Mitigations

| Risk | Mitigation |
|---|---|
| macOS OpenGL deprecation/4.1 cap | Stick to GL 3.3 core and keep the renderer behind an interface |
| Float precision in huge space | Sector-local coordinates plus a camera-relative origin |
| Mouse-only control is awkward in combat | Autopilot, auto-fire, targeting by click, pause-and-order, and time scaling |
| Entity count when zoomed out | LOD, spatial hashing, and simulating distant sectors at a lower rate |
| Art changes later | Everything goes through the `VectorAsset` interface and data files |
| Scope creep | Hold to the milestone order. Park ideas in `docs/backlog.md` |

## 8. Open Questions for the Designer

1. Real-time with pause (as assumed), or turn-based or hybrid?
2. Is combat central or occasional? Are the aliens mostly diplomatic?
3. Is the galaxy infinite and random, or a fixed-length journey with a guaranteed route home?
4. Should there be a crew, a story, or narrative events (Voyager-style episodic encounters)?
5. Target platforms (macOS only at first?) and a minimum spec for 60 fps?
6. Preferred vector format for your art (SVG from Inkscape or Illustrator, or a custom editor)?

## 9. Suggested First Prompt for the Coding Agent

> Read `PLAN.md`. Complete **M0** and **M1**. Use CMake, SDL2, glad (GL 3.3 core), glm, and doctest. Create the directory layout from section 3, with simulation code free of SDL/GL includes. Build, run the tests, and report. Do not start M2 until I confirm.
