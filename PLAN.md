# Endless Expanse: Agentic Development Plan (City Pivot)

## 1. Vision

A snow-covered, post-apocalyptic city that never ends. The player drives an armoured tumbler (a small armoured jeep) through an endless grid of streets, alleys, rubble and wrecks. Nanobot swarms (the cause of the apocalypse), raiders and scavengers hunt the roads. Friendly traders and rest-and-refuel bases keep the run going. Snow falls over everything as a top layer.

- **Look:** retro vector monitor. The world is greyscale. Amber is the only highlight colour, used for selection, the player, and important UI. No bloom. Shadows are dark translucent polygons cast at a fixed 45° light angle.
- **Feel:** smooth 60 fps. Modern QOL: pause, autosave, tooltips, rebindable and scalable UI, accessibility options.
- **Input:** 100% mouse. Click a point to drive there (the tumbler slows down and stops on it). Click or select objects and NPCs. The keyboard is never required.
- **Tech:** C++20, SDL2, OpenGL 3.3 core, CMake. Procedural placeholder shapes behind `VectorShape` until the user's art arrives.
- **Run:** endless. There is no win state. Survival is about fuel, hull and supplies, with trade and bases to keep going.

## 2. Guiding Principles for Agents

1. **Always shippable:** every milestone ends with a game that builds, runs, and passes tests.
2. **Small vertical slices:** finish one thin, working feature before widening.
3. **Data-driven:** vehicles, factions, items, and encounters live in `assets/data/*.json`.
4. **Deterministic simulation:** fixed 60 Hz step and seeded RNG. Sectors are generated from `hash(seed, x, y)`.
5. **Separate sim from render:** `src/sim`, `src/world`, `src/core`, `src/vector`, and `src/camera` never include SDL or GL.
6. **Do not hardcode art:** all drawables go through `VectorShape`/`VectorAsset`.
7. **Record decisions** in `docs/decisions.md` (ADR entries).
8. **Performance budget:** 16.6 ms/frame. No per-frame heap allocation in hot paths.
9. **Road access is sacred:** the road network must always be drivable. Obstacles may block alleys and blocks but never cut a road.

## 3. Architecture

Kept from the space build: engine (`platform`, `render`, `core`, `camera`, `vector`), the sector streamer, the floating origin, the HUD and button framework, and the test harness.

Retired (removed in M0): starfield, space-ship physics and autopilot orbit, aliens and hazards, nebulae, black holes, exhaust trails, and the space encounter director.

```
src/
  app/        main, Application, states (Menu, Playing, Paused), HUD, trade UI
  platform/   SDL window, input, timing
  render/     GL, line batch, ground/shadow/snow passes, text
  vector/     VectorShape, stroke font, procedural placeholder art
  core/       fixed timestep, RNG, state stack, logging
  camera/     2D camera, zoom and pan, culling
  world/      sectors, road graph, blocks, buildings, debris, bases
  sim/        tumbler physics and driving, tire tracks, collisions,
              factions and NPCs, combat, trade, vitals, director
assets/data/  vehicles.json, factions.json, items.json, encounters.json
tests/        doctest unit and headless sim tests
```

## 4. Milestones

Each milestone lists **tasks**, **deliverables**, and **acceptance**. Complete them in order unless noted.

### P0: Pivot and Clean-Up
**Tasks**
- Remove the space content: starfield, ship, aliens, hazards, nebulae, exhaust, space director. Keep the engine parts and the streamer.
- Remove bloom from the default path. The `PostProcess` code may stay for later use, but it is off and unused.
- Rename the app from Endless Expanse only where it is user-facing. Keep the repo name until the user decides.
- Update `README.md` and `docs/decisions.md`.

**Acceptance:** builds, runs, and shows an empty greyscale screen with a working pan and zoom. Tests pass with the space tests removed.

### P1: Sectors, Ground and Road Network
**Tasks**
- Each 6000x6000 sector gets a seeded street grid. Roads connect across sector borders. A road crossing an edge is decided by a hash of that edge, so both neighbours agree without loading each other.
- Blocks sit between roads. Alleys branch from roads into blocks.
- Ground is flat grey with a faint vector grid or texture cue for speed reading.
- Debris is scattered as static dots and small shapes on the ground.

**Acceptance:** the same seed always produces the same sector (tested). A road exists at every sector border on both sides. Walking the road graph reaches any sector from any other (tested).

### P2: Buildings and Shadows
**Tasks**
- Buildings are placed in blocks with varied footprints and heights. The view is from above, so only rooftop outlines are drawn.
- Fixed light: the shadow is cast at 45° to the ground, with length proportional to height. Shadows are dark translucent polygons drawn on the ground layer, under the roofs.
- Shadows are also cast by the tumbler, wrecks, rubble and NPCs (lower heights).
- Draw order: ground, debris, shadows, roofs and vehicles, then snow (P5).

**Acceptance:** shadows line up at 45° for every object (tested for the offset direction). Visual check: a clear shadow for each building.

### P3: Tumbler and Driving
**Tasks**
- Tumbler physics: top speed, acceleration, braking, and minor traction. Traction is reduced on ice and snow patches, so the vehicle slides slightly on turns.
- Click a point to drive there. The tumbler decelerates and stops on the point. No orbit, no circling, and no turning around to stop.
- Tire tracks: faint ribbons that fade out over time, drawn on the ground layer under the shadows.
- Keep the FOLLOW camera, the zoom, and pan from the current build.

**Acceptance:** the tumbler reaches any clicked point and stops on it without overshoot (tested). Tracks fade to zero in a fixed time (tested).

### P4: Obstacles, Rubble and Wrecks
**Tasks**
- Obstacles: wrecks, rocks, rubble, collapsed buildings. They are placed in blocks and alleys, never on road centrelines.
- Collisions: the tumbler is stopped or pushed by solids. Impact damage is modest.
- Passability guarantee: a road-clearance margin is kept free, and tests check that every road remains drivable across sector borders.

**Acceptance:** obstacles never block a road (tested). The headless sim test drives a random route across 20 sectors without getting stuck.

### P5: Snow Layer and Greyscale Look
**Tasks**
- Snow is a top-layer particle system: purely visual, drawn last, with no effect on physics or traction.
- The world is greyscale. Amber is the highlight colour for the player, the selection, the target, and key HUD states.
- Remove bloom from the render path. Keep line rendering and vector style.
- Snow density and wind are settings. The default is light, with a toggle to turn it off.

**Acceptance:** visual check against the greyscale spec. Snow stays at 60 fps with a dense setting.

### P6: Factions and NPCs
**Tasks**
- Nanobot swarms: random swarms that drift, pursue and split apart, with flocking behaviour. They are the main threat.
- Raiders: armed vehicles that pursue and attack.
- Scavengers: unarmed vehicles that loot wrecks and flee when threatened.
- Friendlies: traders who can be hailed, bought from, and sold to.
- The encounter director spawns swarms, raiders, and scavengers at a tuned pace. It guarantees quiet time.

**Acceptance:** a 10-minute headless soak runs with no crashes, NaNs, or leaks. The spawn rate matches the config.

### P7: Rest Bases, Refuel and Trade
**Tasks**
- Rest bases appear at regular intervals along the road network, marked on the ground map.
- At a base, the player can rest (restore crew morale and hull), refuel, and repair.
- Trading: buy and sell items with friendly traders and at bases. Prices are data-driven.
- Vitals: fuel, hull, supplies, and scrap currency. Keep this set small.

**Acceptance:** a headless test checks that refuel and rest restore the right values and that trade transactions balance.

### P8: Strategic Zoom-Out
**Tasks**
- Far zoom shows the local area: streets, bases and known factions, with fog over unexplored sectors.
- Further zoom shows the explored district map, with known bases and the route taken.
- Mouse-driven route planning: click a base to plan a route on the explored map.

**Acceptance:** zoom is continuous from street level to the district map without a hitch.

### P9: Survival Loop and Difficulty
**Tasks**
- Permadeath: the tumbler is destroyed or runs out of fuel and supplies. The run ends and restarts from the beginning, with nothing carried over. Save slots store only the current run.
- Difficulty ramps with distance from the start.
- Home is a long-term goal, found by triangulating its signal. The home base broadcasts periodically. The player must locate enough transmission relays across the city to fix its position. Reaching the signal source and returning home is the win state.
- Balance pass using data and a headless sim bot that drives routes, to find a playable curve.

**Acceptance:** a scripted bot survives a 30-minute headless run on normal difficulty. A do-nothing run fails within a reasonable time.

### P10: UI/UX and QOL
**Tasks**
- Mouse-only UI: buttons, sliders, toggles, tooltips, context menus, scrollable lists, and modal dialogs, in the vector style.
- HUD: vehicle status, fuel, hull, supplies, a comms log, the trade panel, and a sector scale.
- Pause, time-scale (1x, 2x), autosave on sector change, and manual save slots.
- Settings: resolution, vsync, snow density, UI scale, colour-blind safe modes (amber-based highlights suit most), screen shake toggle, and audio volumes.

**Acceptance:** every function is reachable by mouse. Save/load round-trips exactly (tested).

### P11: Audio and Polish
**Tasks**
- Synthesized sfx: engine hum, tyre slide, gunfire, and nanobot swarm buzz. Ambient wind. Volumes are configurable.
- Vector effects: debris fragments on impact, dust puffs, and swarm flicker.
- CRT options: scanlines, curvature, flicker, toggleable. Bloom stays off.
- Profile and optimize batching, culling, and snow.

**Acceptance:** sustained 60 fps in worst-case scenes. A 30-minute soak shows no memory growth.

### P12: Vector Asset Import Pipeline (can start after P2)
**Tasks**
- Define the asset format in `docs/asset-format.md`: a restricted SVG subset with named groups for `wheel`, `hitbox`, `muzzle`, and `roof`.
- Runtime loader and validator with clear errors. Hot reload in debug builds.
- Fallback to the procedural placeholder if an asset is missing.

**Acceptance:** the user drops a vehicle `.svg` into `assets/vectors/`, references it in `vehicles.json`, and sees it in game with working hitbox and wheel points.

### P13: Release Readiness
**Tasks**
- macOS `.app` bundle (optional Windows/Linux), versioning, crash log, CI that builds and runs tests.
- Bug bash, balance, README and screenshots.

## 5. Agent Workflow

For each task:

1. **Read** `docs/decisions.md`, this plan, and the relevant source.
2. **Plan** a short checklist and flag deviations.
3. **Implement** the smallest coherent change. Keep the sim free of SDL/GL.
4. **Test:** unit or headless tests. Build, then run the tests.
5. **Verify visually** for graphical work. Report what the user should look at.
6. **Report** what changed, what was validated, and open questions.

## 6. Testing and Quality

- **Unit tests:** math, camera transforms, RNG determinism, road graph and sector generation, shadow geometry, tumbler physics, track fade, and trade maths.
- **Headless sim tests:** N simulated minutes per seed with invariant checks (no NaN, bounded entity counts, resources in range).
- **Passability test:** a road path check across many sectors.
- **Tooling:** AddressSanitizer/UBSan builds, `clang-format`.
- **Perf:** frame-time readout in the debug overlay, and a benchmark scene with dense snow and many NPCs.

## 7. Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Endless city with a seam at sector borders | Road edges are hashed, so neighbours agree without loading each other. Tested across borders |
| Obstacles trap the player | Roads are cleared by construction. A passability test runs over random routes |
| Too many shadows and NPCs at zoom-out | LOD: far zoom draws icons and simplified shadows. Snow stays a single batched pass |
| Snow hides gameplay information | Snow is a light, toggleable layer, and highlights stay amber |
| Art changes later | All drawables go through `VectorShape` and data files |
| Scope creep | Hold to the milestone order. Park ideas in `docs/backlog.md` |
| Permadeath frustration | Clear run summary on death, and a fast restart. Easier early game |

## 8. Decisions Made by the Designer

1. **Home:** the home base broadcasts a periodic signal. The player finds transmission relays across the city, and enough relays triangulate its position. Victory is reaching the signal source and returning home.
2. **Name:** keep "Endless Expanse".
3. **Snow:** purely visual. Traction changes only on ice patches.
4. **Permadeath:** when the tumbler dies, the run ends and the player starts again from the beginning. Nothing carries over.

## 8a. Open Questions

1. **Signal source vs home:** is the source a separate beacon that the player must reach, with "home" being the player's base elsewhere? Or is the source the home base itself? (Assumed: the source is the beacon at home, and "return home" means bringing the tumbler back to the start.)
2. **Relay count:** how many relays are needed to triangulate (minimum 3 for a 2D fix)? Are relays guarded, and do they show on the map once found?
3. **Target platforms:** macOS only at first?

## 9. Suggested First Prompt for the Coding Agent

> Read `PLAN.md`. Complete **P0**. Retire the space content, keep the engine, and leave a greyscale empty world with working pan and zoom. Build and run the tests, then report. Do not start P1 until I confirm.
