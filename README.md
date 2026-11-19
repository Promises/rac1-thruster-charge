# Charge Boots

A native mod for [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s port of Ratchet & Clank: Going
Commando's **Charge Boots**. Double-tap R1 on the ground and hold it: Ratchet charges the way he
faces, at 30 units a second for the first second and 11.5 after, the stick across turning him.
Enemies and crates in his way take a wrench hit. Let go of R1 (after a second at least) and he
stops; run into a wall and he bonks back off it. There is no jump out of a charge.

The boots are his once the game owns the **Persuader**, the gadget Fred, the resort owner on
Pokitaru, gives for the Raritanium. A save that owns it already has them. The setting **Owned from
the start** gives them at once; **Speed** scales the charge.

The charge takes over the Thruster-Pack's double-tap R1 hover: with the mod on, that hover is gone,
whatever the boots. The Thruster-Pack's long jump and its boost in a double jump stay.

**Needs OpenRAC with the `rac1.player.steer_planar_velocity` hook point** (branch
`rac1-steer-hook`, not in a release yet), and mod interface 1.22 or later. A game without the
hook point refuses the mod with the reason. It is built against the SDK's source at a commit of OpenRAC's
`develop` (`sdk.lock`); the hook point is declared in `src/steer_hook.hpp` until the SDK has it.

## How it works

Ratchet & Clank's hover (state 0x81) is nearly Going Commando's charge already: the same double
tap, the same hover 0.17 over the ground, a body lean from his speed. The mod:

- **starts the charge** on the double tap, the second press within 30 frames of the first and
  held, from his feet (idle, running, crouched): it puts him in the hover as the game would
  (`rac1.player.update`, `hook_entry` of `rac1.player.enter_move_state`);
- **keeps the hover the charge's** (`rac1.player.enter_move_state`): the game's own way into it is
  refused, and while he charges its ways out (after 20 ticks without the Thruster-Pack, X, a double
  tap) are too;
- **steers him** (`rac1.player.steer_planar_velocity`) along his facing at Going Commando's speeds
  and easing, the stick across turning him at up to 70 degrees a second (a tenth in the first
  second), and **holds his facing** from the stick's direction (`rac1.player.turn_to_heading`);
- **stops him** (state 3), **bonks him** at a wall (state 0x7A, the game's own pushback) when he moves
  at under three quarters of the speed he is steered at, and ends the charge off an edge (a fall);
- **hits** each moby a little ahead of him at hip height with the wrench's hit (`collision_line`,
  `moby_hit`), each once a half second.

Going Commando's flames at his feet and its charge sound are not there yet: they need a model,
texture and sound from the disc.

## Build

```sh
python3 tools/fetch_sdk.py          # the SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build && ctest --test-dir build
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/charge_boots
```

## License

MIT.
