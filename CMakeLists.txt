# Charge Boots: an OpenRAC native mod for Ratchet & Clank (from the SDK's template).
#
#   python3 tools/fetch_sdk.py                     # the SDK sdk.lock pins, into .sdk/
#   cmake -S . -B build -G Ninja
#   cmake --build build && ctest --test-dir build
#   python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/charge_boots
#
# A tag v<version> (the manifest's) makes the workflow publish the release the launcher installs.
cmake_minimum_required(VERSION 3.20)
project(charge_boots LANGUAGES C CXX)

# The SDK: the release sdk.lock pins (tools/fetch_sdk.py puts it in .sdk/); OPENRAC_SDK=<dir> (an
# unpacked SDK, or sdk/ in a checkout of OpenRAC) or OpenRACMod_DIR=<dir>/cmake for another.
if(NOT OpenRACMod_DIR)
  if(DEFINED OPENRAC_SDK)
    set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" lock)
    string(JSON sdk_version GET "${lock}" version)
    set(OpenRACMod_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${sdk_version}/cmake")
    if(NOT EXISTS "${OpenRACMod_DIR}/OpenRACModConfig.cmake")
      message(FATAL_ERROR "No OpenRAC mod SDK ${sdk_version}: run python3 tools/fetch_sdk.py first "
                          "(or pass -DOPENRAC_SDK=<an unpacked SDK>)")
    endif()
  endif()
endif()
find_package(OpenRACMod 1.23 REQUIRED)

# Going Commando's charge code, copied from rac2-decomp (src/gc/*.c), and the translation layer
# that runs it on Ratchet & Clank's hero (src/gc/rac1_shim.*). The copied files include the
# layer's common.h and names.h (src/gc/shim/) in place of rac2-decomp's; they are compiled as they
# are, so the warnings their PS2-era casts and attributes raise on a host compiler are off for them.
set(gc_copied
  src/gc/hero_l00_update_state_machine.c
  src/gc/hero_l00_set_action.c
  src/gc/hero_l00_decide_state.c
  src/gc/ProcessHeroEquippedItemInput.c)
set(gc_layer src/gc/rac1_shim.cpp)
set_source_files_properties(${gc_copied} PROPERTIES
  INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src/gc/shim"
  COMPILE_OPTIONS "-w")
# hero_l00_update_state_machine passes addresses as 32-bit handles (gc/shim/common.h).
set_source_files_properties(src/gc/hero_l00_update_state_machine.c PROPERTIES
  COMPILE_DEFINITIONS GC_SHIM_HANDLES)
# ProcessHeroEquippedItemInput has its own view of the hero (gc/shim/common.h).
set_source_files_properties(src/gc/ProcessHeroEquippedItemInput.c PROPERTIES
  COMPILE_DEFINITIONS GC_SHIM_UNIT_ITEM_INPUT)

openrac_add_mod(charge_boots MANIFEST openrac-mod.json SOURCES src/mod.cpp ${gc_layer} ${gc_copied})
target_compile_features(charge_boots PRIVATE cxx_std_17)

# The notices of what is copied go in the package (and so the archive, which carries the files
# named LICENSE* beside the manifest).
get_target_property(charge_boots_package charge_boots OPENRAC_MOD_PACKAGE_DIR)
configure_file(THIRD_PARTY_NOTICES.md "${charge_boots_package}/LICENSE.THIRD_PARTY_NOTICES.md" COPYONLY)

# The rules and the translation layer, with the copied code, on a fake game (ctest; the workflow
# runs it on every platform).
enable_testing()
add_executable(charge_boots_test src/charge_boots_test.cpp ${gc_layer} ${gc_copied})
target_compile_features(charge_boots_test PRIVATE cxx_std_17)
add_test(NAME charge_boots COMMAND charge_boots_test)
