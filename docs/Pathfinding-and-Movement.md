# Pathfinding & Movement

How units find their way around the map and how fast they get there. Everything
below is taken straight from the engine code — the numbers are the real
constants, not approximations.

The map is a grid of **16×16-pixel tiles**. A **robot** (grunt, sniper, …)
occupies **1 tile**; a **vehicle** occupies a **2×2 tile block** (32×32 px).
Pathfinding runs on a background thread, one request at a time, and the result
is handed back to the unit a tick or two later.

There are really **two separate systems** that people lump together as
"pathfinding":

1. **How fast a unit moves** once it's walking (its velocity).
2. **Which route a unit picks** to get from A to B (the A\* search).

They use *different* numbers, so they're documented separately.

---

## 1. How fast a unit moves

A unit's actual speed each frame is a base speed multiplied by several
situational factors:

```
real speed  =  base move_speed  ×  terrain  ×  damage  ×  running
```

### Terrain multiplier

This is recomputed live as the unit crosses each tile boundary, based on the
tile it's currently standing on:

| Terrain        | Speed multiplier |
|----------------|------------------|
| **Road**       | **× 1.689**      |
| Open ground    | × 1.0            |
| **Water**      | **× 0.7** (robots only — vehicles can't enter water) |
| Impassable     | × 0 (you don't go there) |

So a unit on a road moves **~69 % faster** than the same unit on open ground.
(If you were expecting something gentle like 1.2× — no, roads are a *big* deal
in Z. Get on the road.)

### Damage multiplier

As a unit takes damage it slows down:

| Condition          | Speed multiplier |
|--------------------|------------------|
| Healthy            | × 1.0            |
| Partially damaged  | × 0.9            |
| Damaged            | × 0.8            |

### Running multiplier

| State    | Speed multiplier |
|----------|------------------|
| Walking  | × 1.0            |
| Running  | × 1.8            |

Running drains stamina and can't be done while *damaged* (a damaged unit always
moves at walk speed). In a group, the **leader's** run state decides whether the
whole group runs. Stamina recharges when not running.

### Base speeds per unit type

The base `move_speed` before any multipliers:

**Robots**

| Unit   | Base speed |
|--------|------------|
| Grunt  | 14 |
| Sniper | 14 |
| Laser  | 14 |
| Psycho | 12 |
| Tough  | 12 |
| Pyro   | 12 |

**Vehicles**

| Unit             | Base speed |
|------------------|------------|
| Jeep             | 17 (fastest) |
| Light tank       | 14 |
| APC              | 14 |
| Crane            | 14 |
| Medium tank      | 12 |
| Heavy tank       | 9  |
| Missile launcher | 6 (slowest) |

Deployed **cannons** (gatling, gun, howitzer, missile cannon) have base
speed **0** — they don't move once placed.

**Worked example:** a healthy Grunt (base 14)…
- …walking on open ground → 14 × 1.0 × 1.0 × 1.0 = **14**
- …running on a road → 14 × 1.689 × 1.0 × 1.8 ≈ **42.6** (3× faster!)
- …wading through water → 14 × 0.7 = **9.8**

---

## 2. How a unit picks its route (A\* search)

Routing uses **A\*** on the tile grid, 8-connected (it can step diagonally).
Each candidate tile has a **movement cost** — the search adds up costs along a
route and picks the cheapest one to the destination, biased toward the goal by a
Manhattan-distance heuristic.

### Tile costs

Lower cost = the pathfinder would rather walk there. Costs come in two flavours:
**orthogonal** (up/down/left/right) and **diagonal** (≈ 1.414× longer, so it
costs more):

| Terrain      | Orthogonal cost | Diagonal cost | Notes |
|--------------|-----------------|---------------|-------|
| **Road**     | **≈ 46**        | ≈ 65          | strongly preferred |
| Open ground  | 100             | 141           | the baseline |
| **Water**    | **≈ 143**       | ≈ 202         | avoided (robots only) |
| Impassable   | —               | —             | never traversed |

The road cost is even lower than its speed bonus alone would suggest: the
pathfinder treats roads as `1 / (road_speed + 0.5)`, i.e. it applies an **extra
+0.5 bias** purely to make routes *hug roads*. The result is the classic Z
behaviour — units will take a noticeably longer road route instead of cutting
straight across open ground, because the road is both cheaper to path over and
faster to drive on.

### Diagonals & corners

A unit won't "cut corners" through a wall. To step diagonally, the tiles it
would brush past must also be clear:

- **Robots** check the two tiles flanking the diagonal.
- **Vehicles** check their full 2×2 footprint *plus* the outer corner tiles in
  the direction of travel — so a vehicle needs a genuinely wide gap to turn a
  corner.

### Shortcuts and early-outs

- **Direct-path check:** before running the full A\* search, the engine walks
  the straight line from start to goal in small steps. If nothing impassable is
  in the way (accounting for the unit's footprint), it just goes straight —
  no waypoints, no search.
- **Region check:** the map is pre-split into connected "regions" per unit type.
  If the start and goal aren't in the same region (e.g. separated by water for a
  vehicle), the engine knows immediately that *no* path exists and doesn't even
  start a search.
- **Path smoothing:** after a route is found, runs of collinear waypoints are
  collapsed into straight segments.

> A\* here uses a Manhattan heuristic with weighted diagonal moves, so routes are
> **near-optimal rather than provably shortest** — occasionally a unit takes a
> slightly quirky line. This is original-engine behaviour.

---

## 3. Rocks, grenades & obstacles

This is the rule people notice most: **a unit with explosives will path *through*
a rock and blow it up, instead of walking around it.**

Under the hood there are **four** routing grids:

- `robot` / `vehicle` — the normal grids, where rocks are solid walls.
- `robot_norocks` / `vehicle_norocks` — identical, *except destroyable obstacles
  (rocks, huts) are treated as passable*.

When a unit asks for a path, the engine picks which grid to use based on whether
the unit **has explosives**:

| Unit has explosives? | Grid used | Behaviour |
|----------------------|-----------|-----------|
| No  | normal      | routes **around** rocks |
| Yes | "norocks"   | routes **through** rocks (and clears them en route) |

A unit "has explosives" if **any** of these is true:
- it's an inherently explosive unit type (e.g. **Tough**, and most armed
  vehicles — heavy/light/medium tanks, missile launcher, APC, the howitzer/gun/
  missile cannons), **or**
- it is **carrying grenades**, **or**
- it's in a group whose **leader is carrying grenades**.

**Clearing the rock mid-route:** when such a unit reaches a rock that's blocking
it, the engine inserts an *attack* order at the front of its to-do list, the unit
blows the rock up, and then continues along its path. A unit *without* explosives
that runs into an obstacle just stops.

**Bridges are different:** a raised bridge registers as a **non-destroyable**
barrier, so it blocks *both* grids. Grenades won't get you across a bridge — only
rocks and huts are "blast-through" obstacles.

---

## 4. Robots vs vehicles

| | Robots | Vehicles |
|---|--------|----------|
| Footprint | 1 tile (16×16) | 2×2 tiles (32×32) |
| Water | can wade (slowly, ×0.7) | **cannot enter** |
| Cost per step | base | ≈ 4× (summed over its 2×2 footprint) |
| Cornering | needs a 1-tile gap | needs a wider gap (checks corner tiles) |

Because a vehicle's cost is summed across four tiles, and it needs more room to
manoeuvre, vehicles favour open roads and wide terrain even more strongly than
robots do — and they simply can't take water shortcuts at all.

---

## 5. Other movement behaviour

- **Following the path:** the unit heads to each waypoint in turn; when it
  reaches the last one it stops. Arrival is detected by either hitting the point
  exactly or overshooting it.
- **Blocked mid-route:** if the next step would collide with something impassable,
  a normal unit stops (or, with explosives, attacks a destroyable obstacle).
  Group *minions* are "unstoppable" — they push through rather than halting.
- **Auto-grab:** while moving, a unit will automatically divert to an empty enemy
  flag or an empty (grey) vehicle within **220 px**, grabbing/crewing it. This
  injects a new destination; it doesn't change the routing rules.
- **Pathfinding load:** the background pathfinder throttles itself when many
  requests are queued, so on a busy map paths can take a moment to come back —
  units may pause briefly before setting off.

---

### Source

Implemented in `src/zpath_finding.cpp`, `src/zpath_finding_astar.cpp` (the search,
grids, and costs), `src/zmap.cpp` (terrain → speed/grid), `src/zobject.cpp`
(movement execution, explosives, obstacle clearing), `src/zsettings.cpp` and
`src/constants.h` (the speed/cost constants). Robots are 1 tile, vehicles 2×2;
roads are `ROAD_SPEED = 1.689`, water `WATER_SPEED = 0.7`.
