# Xenia Canary patches

Patches for players running the original Xbox 360 game in [Xenia Canary](https://github.com/xenia-canary/xenia-canary),
not for this port (the port has these fixes built in).

## Balance beams at 60 / 120 FPS

`415607FF - 007 Quantum of Solace.patch.toml` is Canary's own patch file for the game (default.xex, hash
`A29428CB1432DD9E`, the US/EU disc) with two patches added: **Balance beams at 60 FPS** and **Balance beams at 120 FPS**.

Why: each frame on a balance beam, the game multiplies the player's balance by `bal_wobble_accel` (1.02) when off
balance and by `bal_wobble_decel` (0.93) near the middle. That was tuned at the original 30 FPS, so at higher frame
rates a lean runs away much faster (about 6 times as fast at 120) and the beam becomes very hard with a controller.
The patches set the two values to 1.02^(30/fps) and 0.93^(30/fps), so a locked 60 or 120 FPS behaves like the original
30.

How to use:
1. Back up `patches\415607FF - 007 Quantum of Solace.patch.toml` next to `xenia_canary.exe`, then replace it with this
   file.
2. Enable **Unlock FPS** (60 FPS as shipped) and **Balance beams at 60 FPS** (set `is_enabled = true`). For 120, set
   Unlock FPS to unlimited, limit Canary to 120 FPS and enable **Balance beams at 120 FPS** instead. Enable only one
   balance patch.

The values only fit a locked frame rate: below the cap, beams get easier than intended.
