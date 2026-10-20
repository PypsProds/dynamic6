# Test plan

## A. Unit tests (automated, SDK-free) — implemented in `tests/test_main.cpp`
Config clamping · FIFO order · duplicate id/state rejection · bounded queue overflow + consistency ·
invalid lane · window boundaries (0, 3; age == window) · hold modes · dead/future timestamps ·
dual Any/All policy and dead-partner handling · two-player lanes · buffered press fires on landing ·
expiry + orphan release · press/release ordering · head-of-line blocking · disabled/paused no-ops ·
reset on death/restart/exit clears queue + held state · tick-regression auto-reset · enable toggle reset ·
SimClock independence from FPS, tps changes, pause · trajectory determinism and Pred≠Obs ·
CSV log emission.
Pass criteria: `ctest` green; also run with `-fsanitize=address,undefined`.

## B. Deterministic harness extensions (to add before going in-game)
1. Scripted traces: land at tick N, press at N-k for k in 0..window+2; assert apply/expire matrix.
2. Randomised press/release streams (seeded) asserting invariants: output order == input order, no press
   applied twice, every applied release follows an applied press, queue never exceeds capacity.
3. Replay a recorded `timing-log.csv` through the scheduler and assert identical decisions (determinism).

## C. In-game verification (manual, instrumented via the CSV log)
1. **Hook proof:** with `enabled=false`, confirm zero behavioural change; with logging on, confirm INPUT rows
   appear for keyboard and touch, one per physical press/release.
2. **Step-rate proof:** log `tick` per rendered frame at 30/60/144/240 FPS limits; ticks/frame must vary
   with FPS while ticks/second stay constant. If not, H2 is wrong.
3. **Window sweep:** window = 0,1,2,4,8 on a fixed gap pattern; compare apply/expire counts to the model.
4. **Lifecycle:** die, restart, practice respawn, pause/resume, quit mid-hold — each must log a RESET and
   leave `pending=0` with no phantom held button on the next attempt.
5. **Dual / mode portals:** transitions Cube→Ship→Ball with dual on; verify decisions against the snapshot columns.
6. **Latency:** measure press→apply delay in ticks with window=0 (should be ≤1 step).
7. **Trajectory experiment:** log PRED at launch and OBS at landing; report `landTickError` distributions.
   Params remain `validated=false` until compared against recorded physics.

## D. Regression guard
Any change to `canAct()`, the queue, or the clock needs a new unit test first.
