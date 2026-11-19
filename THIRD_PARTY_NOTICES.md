# Third-party notices

Charge Boots is licensed under the GNU General Public License version 3 ([LICENSE](LICENSE)).
The mod's own files are GPL-3.0-or-later; the files copied from Going Commando's decompilation
are under that project's license, the GPL version 3 (it names no later version, so they are
GPL-3.0-only). Each file says which in its `SPDX-License-Identifier` line.

## Going Commando's decompilation: OpenRAC/rac2-decomp

- Source: <https://github.com/OpenRAC/rac2-decomp>, commit
  `005bfc4dd3b0d715dcbffabdb8835e356e34474a` (2026-11-15), as the submodule
  `games/rac2/decomp` of OpenRAC.
- License: GNU General Public License version 3 (its `LICENSE`: Copyright (c) 2026 Kryštof
  "Lynder063" Malinda). The project was MIT, Copyright (c) 2026 llesieur99, up to its commit
  `d46a3539` (2026-10-11); code from then keeps that notice, reproduced below.
- Target: Ratchet & Clank: Going Commando, USA v1.01.

Copied, unchanged but for the lines marked `(charge-boots)` that cut each function down to the
charge's cases (each file's header lists the exact lines):

| File here | Function | rac2-decomp file and lines | USA v1.01 | USA v2.00 |
|---|---|---|---|---|
| `src/gc/ProcessHeroEquippedItemInput.c` | `ProcessHeroEquippedItemInput`, its Charge Boots block (the double tap) | `src/levels/shared/L00_002C9448-g8.c`: 1-121, 123-126, 131, 146, 368-380 | `func_L00_002C9448` | `func_L00_002C96D0` |
| `src/gc/hero_l00_set_action.c` | `hero_l00_set_action`, cases 0x7B (charge) and 0x77 (bonk) | `src/levels/00_aranos_tutorial/L00_002C42E0-state-g8.c`: 1-147, 149-150, 167-168, 202, 382-388, 1012-1017 | `func_L00_002C42E0` | `func_L00_002C4560` |
| `src/gc/hero_l00_update_state_machine.c` | `hero_l00_update_state_machine`, cases 0x7B and 0x77 | `src/levels/shared/L00_002BA7D8.c`: 1-184, 258, 392, 401, 428, 463, 563, 1699-1720, 1969-2017 | `func_L00_002BA7D8` | `func_L00_002BA8E0` |
| `src/gc/hero_l00_decide_state.c` | `hero_l00_decide_state`, case 0x7B | `src/levels/shared/L00_002CB3A8-decider.c`: 1-143, 214, 892-937 | `func_L00_002CB3A8` | `func_L00_002CB650` |

The USA v2.00 addresses are from the author's own decompilation
(Promises/RaC-Going-Decompiled, GPL-2.0-or-later), matched by body: the same constants, calls and
field offsets. It has these four functions as assembly only, so no code is taken from it.

The translation layer (`src/gc/rac1_shim.cpp`) reimplements, for its polyfills, what these
rac2-decomp functions do: `pad_clamp_stick_to_unit_range`, `ApproachValue`,
`math_spring_float_to_goal`, `hero_rotate_matrix`, `hero_test_reach_blend` (with
`hero_build_relative_offset`) and the `cam_set_limits_*` table; each polyfill's comment cites its
source.

## Ratchet & Clank's decompilation: OpenRAC games/rac1/decomp

- Source: <https://github.com/OpenRAC/OpenRAC>, `games/rac1/decomp` (GNU GPL version 3).
- The translation layer follows its `Spring` and `advance_damped_scalar_velocity`
  (`src/overlays/shared/moby_ground_collision.c`) in the ground spring's polyfill, and its field
  layout (`include/structs/hero.h`) and the NTSC decomp's `hero_eases` address
  (`games/rac1/ntsc-decomp`, GPL-3.0) in `src/layout.hpp` and `src/gc/rac1_shim.hpp`. No code is
  copied from it.

## The OpenRAC mod SDK

The headers under `.sdk/` (fetched, not in this repository) are MIT, Copyright (c) 2026 the
OpenRAC contributors; the mod includes them.

## rac2-decomp's earlier MIT notice

```
MIT License

Copyright (c) 2026 llesieur99

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

Ratchet & Clank and Ratchet & Clank: Going Commando are trademarks of Sony Interactive
Entertainment, developed by Insomniac Games. This mod is not affiliated with or endorsed by
either, and contains none of their code or assets: the decompiled code it copies is the
decompilation projects' reconstruction, and the game runs from the player's own disc.
