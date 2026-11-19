# Charge Boots

A native mod for [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s port of Ratchet & Clank: Going
Commando's **Charge Boots**, run on **Going Commando's own charge code**. Double-tap R1 on the
ground and hold it: Ratchet charges the way he faces, at 30 units a second for the first second and
11.5 after, the stick across turning him. Enemies and crates in his way take a wrench hit. Let go
of R1 (after a second at least) and he stops; run into a wall and he bonks back off it. There is
no jump out of a charge.

The boots are his once the game owns the **Persuader**, the gadget Fred, the resort owner on
Pokitaru, gives for the Raritanium. A save that owns it already has them. The setting **Owned from
the start** gives them at once; **Speed** scales the charge.

The charge takes over the Thruster-Pack's double-tap R1 hover: with the mod on, that hover is gone,
whatever the boots. The Thruster-Pack's long jump and its boost in a double jump stay.

**Needs OpenRAC with the `rac1.player.update_movement` hook point** (branch `rac1-steer-hook`, not
in a release yet), and mod interface 1.22 or later. A game without the hook point refuses the mod
with the reason. It is built against the SDK's source at a commit of OpenRAC's `develop`
(`sdk.lock`); the hook point is declared in `src/movement_hook.hpp` until the SDK has it.

## How it works

Version 0.2.0 copies the charge from Going Commando's decompilation ([rac2-decomp](https://github.com/OpenRAC/rac2-decomp),
GPL-3.0) into `src/gc/`, unchanged but for marked lines that cut each function down to the
charge:

- `ProcessHeroEquippedItemInput`: the double tap that starts it;
- `hero_l00_set_action`: what entering the charge and the bonk set up;
- `hero_l00_update_state_machine`: the charge's movement (speeds, turn, ground spring, hits,
  lean) and the bonk's;
- `hero_l00_decide_state`: R1 let go stops it, a wall bonks it.

A translation layer (`src/gc/rac1_shim.hpp`, `src/gc/rac1_shim.cpp`, `src/gc/shim/common.h`) runs
that code on Ratchet & Clank's hero:

- **The hero**: the copied code works on Going Commando's hero, a copy the layer fills from
  Ratchet's field by field (velocity, speeds, facing, timers, the stick, the ground...) and writes
  back after, only what the code changed. Going Commando counts 60 frames a second; the layer
  scales speeds, rates and frame counts to the game's tick (a 50th on PAL).
- **States**: Going Commando's charge (state 0x7B) runs in Ratchet & Clank's Thruster-Pack hover
  (0x81), its bonk (0x77) in the game's wall bonk (0x7A), its stop in the stop (3).
- **Functions**: Going Commando's are polyfills: Ratchet & Clank's own `speed_step`,
  `set_planar_vel`, `turn_to` and change of state, through their hook points' entries; the SDK's
  collision lines and `moby_hit` for the hits; the hover's hum for the charge's sound; Ratchet's
  body-tilt records for the lean. Each polyfill's comment says what differs from Going Commando.

The mod's own parts: the hooks (`rac1.player.update` around the hero's update,
`rac1.player.update_movement` to move him by the copied code while he charges,
`rac1.player.enter_move_state` to keep the hover the charge's), the boots' ownership in the mod's
save data, and the settings.

Not there yet: Going Commando's flames at his feet (they need a model and texture of the mod's)
and its own charge sound and stop sound (Ratchet & Clank has neither).

## Build

```sh
python3 tools/fetch_sdk.py          # the SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build && ctest --test-dir build
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/charge_boots
```

## License

GNU General Public License version 3 ([LICENSE](LICENSE)). The files copied from rac2-decomp are
GPL-3.0-only, as that project gives no later version; the mod's own files are GPL-3.0-or-later.
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) names the source, its commit, the files and lines
copied, and its earlier MIT notice; the package carries it as `LICENSE.THIRD_PARTY_NOTICES.md`.
