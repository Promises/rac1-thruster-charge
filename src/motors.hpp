// SPDX-License-Identifier: GPL-3.0-or-later
//
// The port's set_motors (mod interface 1.37), for a mod built for 1.23 and still loaded by a port
// of 1.23: an optional call, found in the table only when the port says it has it.
//
// Until the SDK sdk.lock pins has interface 1.37, the call is read from the table at the place 1.37
// puts it, only when the port reports api_minor 37 or above and a table whose size covers it, as
// the SDK's C++ layer checks its own calls (mod.hpp, has_...()).
//
// Where 1.37 puts it: every entry the table gained after 1.23's last (moby_material) is a function
// pointer, set_motors the 48th of them (1.24 to 1.36 add 47, sharp_cells, 1.36's, the last), so
// it is at offsetof(openrac_api, moby_material) plus 48 function pointers: byte 1800 on a 64-bit
// platform. Verified against the openrac_mod.h of OpenRAC's sdk-set-motors branch (a93d22eb8,
// SDK 1.37.0): offsetof(moby_material) 1416, offsetof(sharp_cells) 1792, offsetof(set_motors)
// 1800, sizeof(openrac_api) 1808. Built against an SDK with 1.37 (OPENRAC_SDK=<its sdk/>), the
// static_assert below checks the count against that header's own offset.
#pragma once

#include <cstddef>
#include <cstring>

#include "openrac/openrac_mod.h"

namespace thruster_charge {

// openrac_api.set_motors: pad `port`'s motors (0 for player 1), `small_motor` 0 or 1, `large_motor` 0 to 255.
// Returns OPENRAC_HOOK_OK, or OPENRAC_HOOK_UNKNOWN when refused.
using SetMotorsFn = int (*)(openrac_mod* mod, int port, int small_motor, int large_motor);

// The interface that has it.
constexpr int kSetMotorsMinor = 37;
// The table's function pointers after moby_material up to set_motors.
constexpr std::size_t kSetMotorsAfterMobyMaterial = 48;
// Its offset in the table.
constexpr std::size_t kSetMotorsOffset =
    offsetof(openrac_api, moby_material) + kSetMotorsAfterMobyMaterial * sizeof(SetMotorsFn);

#if OPENRAC_MOD_API_MINOR >= 37
static_assert(offsetof(openrac_api, set_motors) == kSetMotorsOffset,
              "set_motors is not where motors.hpp reads it from the table");
#endif

// The port's set_motors, or null when it has none (an interface before 1.37, or a table too short).
inline SetMotorsFn set_motors_of(const openrac_api* api) {
    if (api == nullptr || api->api_minor < kSetMotorsMinor ||
        api->size < kSetMotorsOffset + sizeof(SetMotorsFn)) {
        return nullptr;
    }
    SetMotorsFn fn = nullptr;
    std::memcpy(&fn, reinterpret_cast<const unsigned char*>(api) + kSetMotorsOffset, sizeof fn);
    return fn;
}

}  // namespace thruster_charge
