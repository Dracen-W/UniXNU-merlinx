// SPDX-License-Identifier: BSD-2-Clause-Patent
// MT6768 Keypad (kp@10010000) + MT6358 PMIC (TOPSTATUS) physical button driver.
//
// Hardware wiring verified against merlinx.dts (kp@10010000) and MT6358PmicDxe:
//   - Power ("pwrkey"):      MT6358 PMIC TOPSTATUS (0x0028) bit 1 (active low)
//   - Volume Up ("rstkey"):  MT6358 PMIC TOPSTATUS (0x0028) bit 3 (active low)
//   - Volume Down (key 0):   MT6768 KPD KP_MEM1 (0x10010004) bit 0 (active low)
#include "MerlinxButtons.h"

#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOLib.h>
#include <kern/thread.h>
#include <pexpert/pexpert.h>
#include <stdint.h>

extern "C" {
bool merlinx_pmic_read(uint32_t reg, uint32_t *outVal);
void merlinx_hid_set_buttons(uint32_t mask);
void merlinx_usb_log_line(const char *fmt, ...);
}

namespace {

constexpr uint64_t kMtkKpdBasePa      = 0x10010000ULL;
constexpr uint64_t kMtkKpdMapSize     = 0x1000ULL;
constexpr uint32_t kMtkKpMem1Offset   = 0x0004U;
constexpr uint32_t kMt6358TopStatus   = 0x0028U;
constexpr uint32_t kMt6358PwrkeyMask  = (1U << 1);
constexpr uint32_t kMt6358HomekeyMask = (1U << 3);
constexpr uint32_t kPollIntervalMs    = 20U;
constexpr uint32_t kPmicErrLogStride  = 250U;

struct KeyState {
    uint32_t hidBit;
    bool down;
    bool seen;
};

IODeviceMemory *gKpdMem = nullptr;
IOMemoryMap *gKpdMap = nullptr;
volatile uint8_t *gKpdBase = nullptr;
volatile uint32_t gButtonsStarted = 0;

bool mapKeypadMmio(void)
{
    if (gKpdBase) {
        return true;
    }
    IODeviceMemory *mem = IODeviceMemory::withRange(
        static_cast<IOPhysicalAddress>(kMtkKpdBasePa),
        static_cast<IOByteCount>(kMtkKpdMapSize));
    if (!mem) {
        return false;
    }
    IOMemoryMap *map = mem->map(kIOMapAnywhere | kIOMapInhibitCache);
    if (!map || !map->getVirtualAddress()) {
        OSSafeReleaseNULL(map);
        mem->release();
        return false;
    }
    gKpdMem = mem;
    gKpdMap = map;
    gKpdBase = reinterpret_cast<volatile uint8_t *>(map->getVirtualAddress());
    return true;
}

uint16_t readKpdMem1(void)
{
    if (!gKpdBase) {
        return 0xFFFFU;
    }
    uint32_t val = *reinterpret_cast<volatile uint32_t *>(gKpdBase + kMtkKpMem1Offset);
    return static_cast<uint16_t>(val & 0xFFFFU);
}

void merlinxButtonsThread(void *, wait_result_t)
{
    uint32_t enabled = 1U;
    PE_parse_boot_argn("merlinx-buttons", &enabled, sizeof(enabled));
    if (enabled == 0U) {
        merlinx_usb_log_line("[BTN] disabled by merlinx-buttons=0\n");
        return;
    }

    bool kpdOk = mapKeypadMmio();
    uint32_t initTop = 0xFFFFU;
    bool pmicOk = merlinx_pmic_read(kMt6358TopStatus, &initTop);
    uint16_t initMem1 = readKpdMem1();

    merlinx_usb_log_line(
        "[BTN] started kpd=%d top_ok=%d top=0x%04x mem1=0x%04x\n",
        kpdOk ? 1 : 0,
        pmicOk ? 1 : 0,
        static_cast<unsigned>(initTop & 0xFFFFU),
        static_cast<unsigned>(initMem1));

    if (!kpdOk && !pmicOk) {
        merlinx_usb_log_line("[BTN] neither KPD nor PMIC is reachable; stopping\n");
        return;
    }

    KeyState keys[3] = {
        {MERLINX_BTN_POWER,    false, false},
        {MERLINX_BTN_VOL_UP,   false, false},
        {MERLINX_BTN_VOL_DOWN, false, false},
    };
    uint32_t lastMask = 0U;
    uint32_t pmicErrors = 0U;

    for (;;) {
        uint32_t top = 0xFFFFU;
        bool topOk = merlinx_pmic_read(kMt6358TopStatus, &top);
        uint16_t mem1 = readKpdMem1();

        if (topOk) {
            pmicErrors = 0U;
        } else if ((pmicErrors++ % kPmicErrLogStride) == 0U) {
            merlinx_usb_log_line("[BTN] PMIC TOPSTATUS read failed\n");
        }

        const bool sampled[3] = {
            topOk ? ((top & kMt6358PwrkeyMask) == 0U)  : keys[0].down,
            topOk ? ((top & kMt6358HomekeyMask) == 0U) : keys[1].down,
            kpdOk ? ((mem1 & 1U) == 0U)                : false,
        };

        // 2-sample software debounce (40 ms) before reporting transitions.
        uint32_t mask = 0U;
        for (unsigned i = 0; i < 3U; ++i) {
            if (sampled[i] == keys[i].seen && sampled[i] != keys[i].down) {
                keys[i].down = sampled[i];
            }
            keys[i].seen = sampled[i];
            if (keys[i].down) {
                mask |= keys[i].hidBit;
            }
        }

        if (mask != lastMask) {
            lastMask = mask;
            merlinx_hid_set_buttons(mask);
            merlinx_usb_log_line(
                "[BTN] mask=0x%x (pwr=%u vol+=%u vol-=%u) kpd=0x%04x top=0x%04x\n",
                static_cast<unsigned>(mask),
                keys[0].down ? 1U : 0U,
                keys[1].down ? 1U : 0U,
                keys[2].down ? 1U : 0U,
                static_cast<unsigned>(mem1),
                static_cast<unsigned>(top & 0xFFFFU));
        }

        IOSleep(kPollIntervalMs);
    }
}

} // namespace

extern "C" void merlinx_buttons_start(void)
{
    if (__atomic_exchange_n(&gButtonsStarted, 1U, __ATOMIC_ACQ_REL) != 0U) {
        return;
    }
    thread_t thread = THREAD_NULL;
    if (kernel_thread_start(merlinxButtonsThread, nullptr, &thread) == KERN_SUCCESS) {
        thread_deallocate(thread);
    } else {
        __atomic_store_n(&gButtonsStarted, 0U, __ATOMIC_RELEASE);
        merlinx_usb_log_line("[BTN] kernel_thread_start failed\n");
    }
}
