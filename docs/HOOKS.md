# Target: GD 2.2081, Geode 5.11.2

## What was and was not checked (2026-10-20)
I could NOT read `GeometryDash.bro` for 2.2081 (GitHub blocks automated access), so **no signature
here is confirmed against the bindings.** The bindings README does confirm 2.2081 is the current
target version. The points below come from public 2.2081 mods and are *secondary evidence*:

- **Click Between Frames v1.5.0 (ported to 2.2081)** states it now "switches to using Robtop's input
  handling". => 2.2081 appears to have **native sub-frame/sub-step input handling**. Our buffer may
  overlap with it. Decide whether this study should measure vanilla behaviour first (log only, no
  injection) before layering buffering on top.
- **Superb Input Precision (2.2081, Geode >= 5.10.1)** has a changelog entry "changed
  `processQueuedButtons` hook priority". => a `processQueuedButtons`-style function exists and is
  hooked by shipping mods. It is a strong candidate for the real interception point (H1b below).
- **GPRL mod** records inputs from `GJBaseGameLayer::handleButton`, and hooks `PlayerObject::pushButton`
  / `releaseButton`; it reports that in dual mode one `handleButton` press pushes **both** players.
  (Matches the `dual && !twoPlayer` assumption in the evaluator, but still needs your own proof.)
- **Frame-window-counter style projects** hook `processCommands` for a tick counter, but their own
  notes list "called once per physics step" as something to verify. Treat H2 as still open.

## Required verification commands (run in your SDK checkout)
```
grep -n "handleButton\|processQueuedButtons\|queueButton\|processCommands" \
    $GEODE_SDK/bindings/bindings/2.2081/GeometryDash.bro
grep -n "pushButton\|releaseButton\|m_isOnGround\|m_isDead\|m_yVelocity" \
    $GEODE_SDK/bindings/bindings/2.2081/GeometryDash.bro
grep -n "resetLevel\|destroyPlayer\|levelComplete\|onQuit\|pauseGame" \
    $GEODE_SDK/bindings/bindings/2.2081/GeometryDash.bro
```
Paste the matching lines back and the glue can be corrected against the real signatures.

## New hook candidate
| ID | Symbol (candidate) | Why | Status |
|---|---|---|---|
| H1b | `GJBaseGameLayer::processQueuedButtons(...)` | GD's own per-step drain of buffered clicks; likely the earliest correct place to intercept/observe input in 2.2081 | UNVERIFIED (name only seen in a mod changelog; signature unknown) |
| H1c | `PlayerObject::pushButton(PlayerButton)` / `releaseButton(PlayerButton)` | Observe what the game actually delivered, independent of our injection (ground truth for the log) | UNVERIFIED |

## Geode 5 caveats (unverified)
Geode 5 changed parts of the event/settings API. `listenForSettingChanges<T>`, `$on_mod(Loaded)`,
`Mod::getSaveDir()` and the `$modify` re-entrant-call pattern in `main.cpp` should be treated as
compile-time checks: if one fails, fix it from the Geode 5.11.2 docs/headers rather than guessing.

---

# Hook inventory and verification checklist

**Nothing below has been compiled against a Geode SDK by this project.** Names are candidates from
memory of the public bindings. Verify each in `$GEODE_SDK/bindings/bindings/2.2081/GeometryDash.bro`
(and the generated headers under the build tree), then change the status column.

Method for each item: (1) grep the `.bro` for the symbol, (2) check exact signature + platform
coverage for your GD version, (3) compile, (4) confirm runtime behaviour with the log (see TEST_PLAN §C).

| ID | Symbol (candidate) | Why we hook it | What must be proven | Status |
|---|---|---|---|---|
| H1 | `GJBaseGameLayer::handleButton(bool down, int button, bool isPlayer1)` | Intercept press/release so the scheduler can hold it | Signature; button ids (1 jump, 2 left, 3 right); that both keyboard **and** touch reach it; whether GD 2.2's queued-button path (`queueButton`/`processQueuedButtons`-style functions) is called *before* it, making a different point the better interception site | UNVERIFIED |
| H2 | `GJBaseGameLayer::processCommands(float)` | Per-step tick for the scheduler and re-injection | Called **once per physics step**, not once per render frame, and how many times per frame at high refresh/low FPS. If not, pick the per-step function and re-run SimClock tests with real traces | UNVERIFIED |
| H3 | `PlayLayer::resetLevel()` | Reset on restart / checkpoint respawn | Signature; fires for practice-mode checkpoint respawns too | UNVERIFIED |
| H4 | `PlayLayer::destroyPlayer(PlayerObject*, GameObject*)` | Reset on death | Signature; fires once per death; fires for both players in dual | UNVERIFIED |
| H5 | `PlayLayer::levelComplete()` | Reset on completion | Signature | UNVERIFIED |
| H6 | `PlayLayer::onQuit()` | Reset on level exit | Signature; also covered when quitting from pause menu | UNVERIFIED |
| H7 | `PlayLayer::pauseGame(bool)` | Reset on pause so queued input can't replay stale | Signature; alternative `PauseLayer` hooks if absent | UNVERIFIED |

Member fields read by `snap()` / `world()` (all UNVERIFIED): `PlayerObject::m_isOnGround`,
`m_isDead`, `m_yVelocity`, `m_isShip`, `m_isBall`, `m_isBird`, `m_isDart`, `m_isRobot`,
`m_isSpider`, `m_isSwing`; `GJBaseGameLayer::m_player1/m_player2`, `m_gameState.m_isDualMode`.
Also verify: whether `m_isOnGround` is updated *before* `processCommands` runs in a step (this decides
whether "can act" is evaluated against the right state — off-by-one-tick errors live here).

Geode API items to verify: `$on_mod(Loaded)`, `Mod::get()->getSettingValue<T>`,
`listenForSettingChanges<T>`, `Mod::getSaveDir()`, `$modify` re-entrant calls to the original
(`GJBaseGameLayer::handleButton(...)` from inside the hook), and the `mod.json` schema for your
Geode version (setting `type`/`control` keys, `geode` and `gd` version strings).

## Things to find out empirically (not assumable)

- Physics step rate vs. render rate, and how the game's catch-up loop behaves when FPS drops.
- Whether time-warp / TPS-changing settings alter the ticks-per-second the SimClock needs.
- Which `PlayerObject` member (if any) exposes "touching an orb/pad this step" for `orbReady`.
- Whether the game's own click-queueing for orbs already overlaps with this buffer (double buffering).
