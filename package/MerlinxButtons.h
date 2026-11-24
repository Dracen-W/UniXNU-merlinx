// SPDX-License-Identifier: BSD-2-Clause-Patent
// MT6768 Keypad + MT6358 PMIC hardware button driver interface.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Consumer Page Report 2 bit positions matching NativeHidInput (MerlinxHidInput.cpp).
enum : uint32_t {
    MERLINX_BTN_HOME     = (1U << 0), // Consumer Menu (0x40)
    MERLINX_BTN_POWER    = (1U << 1), // Consumer Power (0x30)
    MERLINX_BTN_VOL_UP   = (1U << 2), // Consumer Volume Increment (0xE9)
    MERLINX_BTN_VOL_DOWN = (1U << 3), // Consumer Volume Decrement (0xEA)
    MERLINX_BTN_HW_MASK  = (MERLINX_BTN_POWER | MERLINX_BTN_VOL_UP | MERLINX_BTN_VOL_DOWN),
};

// Start the MT6768/MT6358 physical button polling thread (idempotent).
// Can be disabled at boot via boot-arg `merlinx-buttons=0`.
void merlinx_buttons_start(void);

#ifdef __cplusplus
}
#endif
