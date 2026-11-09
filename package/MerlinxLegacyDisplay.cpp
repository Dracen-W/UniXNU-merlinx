// SPDX-License-Identifier: BSD-2-Clause-Patent
// Bounded offline original lifecycle instrument + MT6768 OVL0 scanout fixup + FocalTech FT8719 SPI0 glue.
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSIterator.h>
#include <libkern/c++/OSMetaClass.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSSerialize.h>
#include <kern/clock.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/thread_call.h>
#include <mach/kmod.h>
#include <machine/machine_routines.h>
#include <pexpert/pexpert.h>
#include <sys/proc.h>
#include <stdarg.h>
#include <string.h>

extern "C" {
#include "FtsBoard.h"
#include "FtsHid.h"
#include "FtsProtocol.h"
#include "FtsTouchCore.h"

void native_gop_text(unsigned line, const char *text) __attribute__((weak));
void native_gop_redraw_ring(void) __attribute__((weak));
void native_diag_process_summary(void) __attribute__((weak));
void native_musb_log_printf(const char *fmt, ...) __attribute__((weak));
void native_musb_cmd_append(const char *fmt, ...) __attribute__((weak));

int fts_touch_probe(void *board);
extern char fts_touch_rows[4][120];
extern volatile uint32_t fts_touch_stage_req, fts_touch_stage_done;
extern volatile uint32_t fts_touch_step, fts_touch_frames, fts_touch_reads;
extern volatile uint32_t fts_touch_idle, fts_touch_recover, fts_touch_bad;
extern volatile uint32_t fts_touch_contacts, fts_touch_model, fts_touch_version;
extern volatile int fts_touch_error;
extern volatile int fts_touch_last_x, fts_touch_last_y;

void merlinx_hid_static_init(void) __asm__("__GLOBAL__sub_I_MerlinxHidInput.cpp");
IOService *native_hid_input_allocate(void);
IOService *native_hid_touch_allocate(void);
void merlinx_hid_inject_tap(uint16_t x, uint16_t y);
void merlinx_hid_inject_swipe(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t steps);
void merlinx_hid_get_stats(
    uint32_t *outStarted,
    uint32_t *outDispatched,
    uint32_t *outFailed,
    int32_t *outLastKr,
    uint16_t *outLastX,
    uint16_t *outLastY,
    uint8_t *outLastContacts,
    uint8_t *outLastFlags);
extern volatile uint32_t native_legacy_retired_frames;
}

static volatile uint32_t gInUsbCmd = 0;

extern "C" void merlinx_usb_log_line(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) {
        return;
    }
    if (native_musb_log_printf) {
        native_musb_log_printf("%s", buf);
    }
    if (gInUsbCmd && native_musb_cmd_append) {
        native_musb_cmd_append("%s", buf);
    }
}

namespace {

static uint32_t gMerlinxGopOsdForce = 0;

// MT6768 physical addresses
static constexpr uint64_t kMtkMblockFbBasePa  = 0x7cdb0000ULL;
static constexpr uint64_t kMtkMblockFbSize    = 0x02250000ULL;
static constexpr uint64_t kMtkSimpleFbBasePa  = 0x7d6b0020ULL;
static constexpr uint64_t kMtkSimpleFbSize    = 0x009a3f80ULL; // 1088 * 2340 * 4 bytes

static constexpr uint64_t kMtkDispsysBasePa   = 0x14000000ULL;
static constexpr uint64_t kMtkDispsysSize     = 0x00020000ULL;
static constexpr uint32_t kMtkMutexOff        = 0x00001000U;
static constexpr uint32_t kMtkOvl0Off         = 0x0000b000U;
static constexpr uint32_t kMtkOvl02LOff       = 0x0000c000U;

static constexpr uint32_t kOvlEn              = 0x00cU;
static constexpr uint32_t kOvlRoiSize         = 0x020U;
static constexpr uint32_t kOvlSrcCon          = 0x02cU;
static constexpr uint32_t kOvlL0Con           = 0x030U;
static constexpr uint32_t kOvlL0Srckey        = 0x034U;
static constexpr uint32_t kOvlL0SrcSize       = 0x038U;
static constexpr uint32_t kOvlL0Offset        = 0x03cU;
static constexpr uint32_t kOvlL0Pitch         = 0x044U;
static constexpr uint32_t kOvlDatapathExtCon  = 0x324U;
static constexpr uint32_t kOvlL0Addr          = 0xf40U;
static constexpr uint32_t kOvlL1Addr          = 0xf60U;
static constexpr uint32_t kOvlL2Addr          = 0xf80U;
static constexpr uint32_t kOvlL3Addr          = 0xfa0U;

static constexpr uint64_t kMtkInfracfgAoPa    = 0x10001000ULL;
static constexpr uint64_t kMtkGpioBasePa      = 0x10005000ULL;
static constexpr uint64_t kMtkApmixedBasePa   = 0x1000c000ULL;
static constexpr uint64_t kMtkPwrapBasePa     = 0x1000d000ULL;
static constexpr uint64_t kMtkSpi0BasePa      = 0x1100a000ULL;

static constexpr uint32_t kSpiCfg0Reg         = 0x0000U;
static constexpr uint32_t kSpiCfg1Reg         = 0x0004U;
static constexpr uint32_t kSpiTxDataReg       = 0x0010U;
static constexpr uint32_t kSpiRxDataReg       = 0x0014U;
static constexpr uint32_t kSpiCmdReg          = 0x0018U;
static constexpr uint32_t kSpiStatus0Reg      = 0x001cU;
static constexpr uint32_t kSpiPadSelReg       = 0x0024U;
static constexpr uint32_t kSpiCfg2Reg         = 0x0028U;

static constexpr uint32_t kSpiCmdAct          = (1U << 0);
static constexpr uint32_t kSpiCmdResume       = (1U << 1);
static constexpr uint32_t kSpiCmdRst          = (1U << 2);
static constexpr uint32_t kSpiCmdPauseEn      = (1U << 4);
static constexpr uint32_t kSpiCmdTxmsbf       = (1U << 12);
static constexpr uint32_t kSpiCmdRxmsbf       = (1U << 13);
static constexpr uint32_t kSpiFifoSize        = 32U;

static constexpr uint32_t kGpioTouchIrq       = 1U;
static constexpr uint32_t kGpioSpi0Miso       = 85U;
static constexpr uint32_t kGpioSpi0Cs         = 86U;
static constexpr uint32_t kGpioSpi0Mosi       = 87U;
static constexpr uint32_t kGpioSpi0Clk        = 88U;
static constexpr uint32_t kGpioTouchRst       = 92U;
static constexpr uint32_t kGpioKeyVolDown     = 94U;

static IODeviceMemory *gDispsysMem = nullptr;
static IOMemoryMap *gDispsysMap = nullptr;
static volatile uint8_t *gDispsysBase = nullptr;

static IODeviceMemory *gGpioMem = nullptr;
static IOMemoryMap *gGpioMap = nullptr;
static volatile uint8_t *gGpioBase = nullptr;

static IODeviceMemory *gInfracfgMem = nullptr;
static IOMemoryMap *gInfracfgMap = nullptr;
static volatile uint8_t *gInfracfgBase = nullptr;

static IODeviceMemory *gSpi0Mem = nullptr;
static IOMemoryMap *gSpi0Map = nullptr;
static volatile uint8_t *gSpi0Base = nullptr;

static IODeviceMemory *gApmixedMem = nullptr;
static IOMemoryMap *gApmixedMap = nullptr;
static volatile uint8_t *gApmixedBase = nullptr;

static IODeviceMemory *gPwrapMem = nullptr;
static IOMemoryMap *gPwrapMap = nullptr;
static volatile uint8_t *gPwrapBase = nullptr;

static uint32_t gOvl0BeforeSrcCon = 0;
static uint32_t gOvl0BeforeL0Con = 0;
static uint32_t gOvl0BeforeL0Addr = 0;
static uint32_t gOvl0BeforeL0Offset = 0;
static uint32_t gOvl0BeforeL0SrcSize = 0;
static uint32_t gOvl0BeforeL0Pitch = 0;
static uint32_t gOvl0BeforeL1Addr = 0;
static uint32_t gOvl02LBeforeSrcCon = 0;
static uint32_t gOvl02LBeforeL0Con = 0;
static uint32_t gOvl02LBeforeL0Addr = 0;
static uint32_t gOvl02LBeforeL1Addr = 0;
static uint32_t gOvlFixupCount = 0;

static uint32_t gSpiUseBitbang = 0;
static uint32_t gSpiTransferCalls = 0;
static uint32_t gSpiHwTimeouts = 0;
static int gFtsProbeResult = -999;
static uint32_t gArmpllLlCon0 = 0;
static uint32_t gArmpllLlCon1 = 0;
static uint32_t gArmpllLCon0 = 0;
static uint32_t gArmpllLCon1 = 0;
static uint32_t gCpuBoostApplied = 0;
static bool gTouchWorkerStarted = false;
static bool gHidMetaRegistered = false;
static IOService *gActiveHidService = nullptr;
static volatile uint32_t gHidInputCommand = 0;

static inline uint32_t mmioRead32(volatile uint8_t *base, uint32_t off)
{
    if (!base) {
        return 0;
    }
    return *reinterpret_cast<volatile uint32_t *>(base + off);
}

static inline void mmioWrite32(volatile uint8_t *base, uint32_t off, uint32_t val)
{
    if (!base) {
        return;
    }
    *reinterpret_cast<volatile uint32_t *>(base + off) = val;
    __asm__ volatile("dsb sy" ::: "memory");
}

static volatile uint8_t *mapMmioWindow(
    uint64_t physAddr,
    uint64_t length,
    IODeviceMemory **outMem,
    IOMemoryMap **outMap)
{
    if (!outMem || !outMap) {
        return nullptr;
    }
    if (*outMap) {
        return reinterpret_cast<volatile uint8_t *>((*outMap)->getVirtualAddress());
    }
    IODeviceMemory *mem = IODeviceMemory::withRange(
        static_cast<IOPhysicalAddress>(physAddr),
        static_cast<IOByteCount>(length));
    if (!mem) {
        return nullptr;
    }
    IOMemoryMap *map = mem->map(kIOMapAnywhere | kIOMapInhibitCache);
    if (!map || !map->getVirtualAddress()) {
        OSSafeReleaseNULL(map);
        mem->release();
        return nullptr;
    }
    *outMem = mem;
    *outMap = map;
    return reinterpret_cast<volatile uint8_t *>(map->getVirtualAddress());
}

static void mtkGpioSetMode(uint32_t pin, uint32_t mode)
{
    if (!gGpioBase) {
        return;
    }
    uint32_t regOff = 0x300U + (pin / 8U) * 0x10U;
    uint32_t shift = (pin % 8U) * 4U;
    uint32_t val = mmioRead32(gGpioBase, regOff);
    val &= ~(0xFU << shift);
    val |= ((mode & 0x7U) << shift);
    mmioWrite32(gGpioBase, regOff, val);
}

static void mtkGpioSetDir(uint32_t pin, bool output)
{
    if (!gGpioBase) {
        return;
    }
    uint32_t bankOff = 0x000U + (pin / 32U) * 0x10U;
    uint32_t bit = (1U << (pin % 32U));
    mmioWrite32(gGpioBase, bankOff + (output ? 4U : 8U), bit);
}

static void mtkGpioSetOut(uint32_t pin, bool high)
{
    if (!gGpioBase) {
        return;
    }
    uint32_t bankOff = 0x100U + (pin / 32U) * 0x10U;
    uint32_t bit = (1U << (pin % 32U));
    mmioWrite32(gGpioBase, bankOff + (high ? 4U : 8U), bit);
}

static int mtkGpioGetIn(uint32_t pin)
{
    if (!gGpioBase) {
        return 1;
    }
    uint32_t bankOff = 0x200U + (pin / 32U) * 0x10U;
    uint32_t val = mmioRead32(gGpioBase, bankOff);
    return ((val >> (pin % 32U)) & 1U) ? 1 : 0;
}

static void merlinxClearCarveoutSlice(uint64_t physStart, uint64_t byteLen)
{
    if (byteLen == 0) {
        return;
    }
    IODeviceMemory *mem = IODeviceMemory::withRange(
        static_cast<IOPhysicalAddress>(physStart),
        static_cast<IOByteCount>(byteLen));
    if (!mem) {
        return;
    }
    IOMemoryMap *map = mem->map(kIOMapAnywhere | kIOMapInhibitCache);
    if (map && map->getVirtualAddress()) {
        volatile uint64_t *p64 = reinterpret_cast<volatile uint64_t *>(map->getVirtualAddress());
        size_t words = static_cast<size_t>(byteLen / sizeof(uint64_t));
        for (size_t i = 0; i < words; ++i) {
            p64[i] = 0xFF000000FF000000ULL;
        }
        __asm__ volatile("dsb sy" ::: "memory");
    }
    OSSafeReleaseNULL(map);
    mem->release();
}

// Reprogram MT6768 DISP_OVL0 (0x1400b000) and DISP_OVL0_2L (0x1400c000) to scan out
// a single opaque BGRA8 layer from 0x7d6b0020 (offset 0, 1080x2340, stride 4352),
// and wipe residual LK OEM/Android splash buffers in mblock-15-framebuffer.
static void merlinxFixupMtkOvl(bool clearCarveout)
{
    if (clearCarveout) {
        if (kMtkSimpleFbBasePa > kMtkMblockFbBasePa) {
            merlinxClearCarveoutSlice(kMtkMblockFbBasePa, kMtkSimpleFbBasePa - kMtkMblockFbBasePa);
        }
        uint64_t suffixStart = kMtkSimpleFbBasePa + kMtkSimpleFbSize;
        uint64_t carveoutEnd = kMtkMblockFbBasePa + kMtkMblockFbSize;
        if (carveoutEnd > suffixStart) {
            merlinxClearCarveoutSlice(suffixStart, carveoutEnd - suffixStart);
        }
    }

    if (!gDispsysBase) {
        gDispsysBase = mapMmioWindow(
            kMtkDispsysBasePa, kMtkDispsysSize, &gDispsysMem, &gDispsysMap);
    }
    if (!gDispsysBase) {
        return;
    }

    volatile uint8_t *ovl0 = gDispsysBase + kMtkOvl0Off;
    volatile uint8_t *ovl02l = gDispsysBase + kMtkOvl02LOff;
    volatile uint8_t *mutex = gDispsysBase + kMtkMutexOff;

    if (gOvlFixupCount == 0) {
        gOvl0BeforeSrcCon    = mmioRead32(ovl0, kOvlSrcCon);
        gOvl0BeforeL0Con     = mmioRead32(ovl0, kOvlL0Con);
        gOvl0BeforeL0Addr    = mmioRead32(ovl0, kOvlL0Addr);
        gOvl0BeforeL0Offset  = mmioRead32(ovl0, kOvlL0Offset);
        gOvl0BeforeL0SrcSize = mmioRead32(ovl0, kOvlL0SrcSize);
        gOvl0BeforeL0Pitch   = mmioRead32(ovl0, kOvlL0Pitch);
        gOvl0BeforeL1Addr    = mmioRead32(ovl0, kOvlL1Addr);

        gOvl02LBeforeSrcCon  = mmioRead32(ovl02l, kOvlSrcCon);
        gOvl02LBeforeL0Con   = mmioRead32(ovl02l, kOvlL0Con);
        gOvl02LBeforeL0Addr  = mmioRead32(ovl02l, kOvlL0Addr);
        gOvl02LBeforeL1Addr  = mmioRead32(ovl02l, kOvlL1Addr);

        merlinx_usb_log_line(
            "[OVL] BEFORE OVL0: SRC=0x%x L0_CON=0x%x L0_ADDR=0x%08x L0_OFF=0x%x L0_SIZE=0x%x L0_PITCH=0x%x L1_ADDR=0x%08x\n",
            gOvl0BeforeSrcCon, gOvl0BeforeL0Con, gOvl0BeforeL0Addr,
            gOvl0BeforeL0Offset, gOvl0BeforeL0SrcSize, gOvl0BeforeL0Pitch, gOvl0BeforeL1Addr);
        merlinx_usb_log_line(
            "[OVL] BEFORE OVL0_2L: SRC=0x%x L0_CON=0x%x L0_ADDR=0x%08x L1_ADDR=0x%08x\n",
            gOvl02LBeforeSrcCon, gOvl02LBeforeL0Con, gOvl02LBeforeL0Addr, gOvl02LBeforeL1Addr);
    }

    const uint32_t targetPa = static_cast<uint32_t>(kMtkSimpleFbBasePa);
    const uint32_t targetSize = (2340U << 16) | 1080U;
    const uint32_t targetPitch = (1088U * 4U) & 0xFFFFU;

    uint32_t ovl0En = mmioRead32(ovl0, kOvlEn);
    uint32_t ovl0Src = mmioRead32(ovl0, kOvlSrcCon);
    if ((ovl0En & 1U) != 0 || ovl0Src != 0) {
        uint32_t l0Con = mmioRead32(ovl0, kOvlL0Con);
        l0Con = (l0Con & ~0x1FFU) | 0xFFU; // Disable AEN (bit 8), set constant alpha = 0xFF
        mmioWrite32(ovl0, kOvlRoiSize, targetSize);
        mmioWrite32(ovl0, kOvlL0Con, l0Con);
        mmioWrite32(ovl0, kOvlL0Srckey, 0U);
        mmioWrite32(ovl0, kOvlL0SrcSize, targetSize);
        mmioWrite32(ovl0, kOvlL0Offset, 0U);
        mmioWrite32(ovl0, kOvlL0Pitch, targetPitch);
        mmioWrite32(ovl0, kOvlL0Addr, targetPa);
        mmioWrite32(ovl0, kOvlL1Addr, targetPa);
        mmioWrite32(ovl0, kOvlL2Addr, targetPa);
        mmioWrite32(ovl0, kOvlL3Addr, targetPa);
        mmioWrite32(ovl0, kOvlDatapathExtCon, 0U);
        mmioWrite32(ovl0, kOvlSrcCon, 1U);
    }

    uint32_t ovl02lEn = mmioRead32(ovl02l, kOvlEn);
    uint32_t ovl02lSrc = mmioRead32(ovl02l, kOvlSrcCon);
    if ((ovl02lEn & 1U) != 0 || ovl02lSrc != 0) {
        uint32_t l0Con2 = mmioRead32(ovl02l, kOvlL0Con);
        l0Con2 = (l0Con2 & ~0x1FFU) | 0xFFU;
        mmioWrite32(ovl02l, kOvlRoiSize, targetSize);
        mmioWrite32(ovl02l, kOvlL0Con, l0Con2);
        mmioWrite32(ovl02l, kOvlL0Srckey, 0U);
        mmioWrite32(ovl02l, kOvlL0SrcSize, targetSize);
        mmioWrite32(ovl02l, kOvlL0Offset, 0U);
        mmioWrite32(ovl02l, kOvlL0Pitch, targetPitch);
        mmioWrite32(ovl02l, kOvlL0Addr, targetPa);
        mmioWrite32(ovl02l, kOvlL1Addr, targetPa);
        mmioWrite32(ovl02l, kOvlDatapathExtCon, 0U);
        mmioWrite32(ovl02l, kOvlSrcCon, 1U);
    }

    mmioWrite32(mutex, 0x020U, 1U);
    gOvlFixupCount++;
}

static void merlinxBoostCpuClocks()
{
    if (!gApmixedBase) {
        gApmixedBase = mapMmioWindow(kMtkApmixedBasePa, 0x1000ULL, &gApmixedMem, &gApmixedMap);
    }
    if (!gPwrapBase) {
        gPwrapBase = mapMmioWindow(kMtkPwrapBasePa, 0x1000ULL, &gPwrapMem, &gPwrapMap);
    }
    if (!gApmixedBase) {
        return;
    }

    gArmpllLlCon0 = mmioRead32(gApmixedBase, 0x200U);
    gArmpllLlCon1 = mmioRead32(gApmixedBase, 0x204U);
    gArmpllLCon0  = mmioRead32(gApmixedBase, 0x210U);
    gArmpllLCon1  = mmioRead32(gApmixedBase, 0x214U);

    auto pwrapWrite = [](uint16_t addr, uint16_t wdata) {
        if (!gPwrapBase) {
            return;
        }
        for (uint32_t i = 0; i < 2000; ++i) {
            if (((mmioRead32(gPwrapBase, 0x0c4U) >> 16) & 0x7U) == 0U) {
                break;
            }
            IODelay(1);
        }
        uint32_t cmd = (1U << 31) | (static_cast<uint32_t>(addr >> 1) << 16) | wdata;
        mmioWrite32(gPwrapBase, 0x0c0U, cmd);
        for (uint32_t i = 0; i < 2000; ++i) {
            if (((mmioRead32(gPwrapBase, 0x0c4U) >> 16) & 0x7U) == 0U) {
                break;
            }
            IODelay(1);
        }
    };

    pwrapWrite(0x19b2U, 0x70U);
    pwrapWrite(0x19b4U, 0x70U);
    pwrapWrite(0x19d0U, 0x70U);
    pwrapWrite(0x19d2U, 0x70U);
    pwrapWrite(0x1488U, 0x60U);
    pwrapWrite(0x148aU, 0x60U);
    pwrapWrite(0x1508U, 0x60U);
    pwrapWrite(0x150aU, 0x60U);
    IODelay(150);

    // Boost ARMPLL_LL (Cortex-A55) to ~1703 MHz and ARMPLL_L (Cortex-A75) to ~1989 MHz
    if ((gArmpllLlCon0 & 1U) != 0) {
        mmioWrite32(gApmixedBase, 0x204U, (1U << 31) | (1U << 24) | (131U << 14));
    }
    if ((gArmpllLCon0 & 1U) != 0) {
        mmioWrite32(gApmixedBase, 0x214U, (1U << 31) | (1U << 24) | (153U << 14));
    }
    IODelay(50);

    gArmpllLlCon1 = mmioRead32(gApmixedBase, 0x204U);
    gArmpllLCon1  = mmioRead32(gApmixedBase, 0x214U);
    gCpuBoostApplied = 1;
    merlinx_usb_log_line(
        "[CPUFREQ] Boosted ARMPLL_LL=0x%08x ARMPLL_L=0x%08x\n",
        gArmpllLlCon1, gArmpllLCon1);
}

static bool merlinxInitTouchHardware()
{
    if (!gGpioBase) {
        gGpioBase = mapMmioWindow(kMtkGpioBasePa, 0x1000ULL, &gGpioMem, &gGpioMap);
    }
    if (!gInfracfgBase) {
        gInfracfgBase = mapMmioWindow(kMtkInfracfgAoPa, 0x1000ULL, &gInfracfgMem, &gInfracfgMap);
    }
    if (!gSpi0Base) {
        gSpi0Base = mapMmioWindow(kMtkSpi0BasePa, 0x1000ULL, &gSpi0Mem, &gSpi0Map);
    }
    if (!gGpioBase || !gSpi0Base) {
        merlinx_usb_log_line("[FTS-HW] Failed to map GPIO or SPI0\n");
        return false;
    }

    if (gInfracfgBase) {
        mmioWrite32(gInfracfgBase, 0x088U, (1U << 21));
        mmioWrite32(gInfracfgBase, 0x0a8U, (1U << 1) | (1U << 21));
        mmioWrite32(gInfracfgBase, 0x0c8U, (1U << 1));
    }

    // GPIO 92 = Reset (output high = released), GPIO 1 = IRQ (input), GPIO 94 = VolDown (input)
    mtkGpioSetMode(kGpioTouchRst, 0U);
    mtkGpioSetDir(kGpioTouchRst, true);
    mtkGpioSetOut(kGpioTouchRst, true);

    mtkGpioSetMode(kGpioTouchIrq, 0U);
    mtkGpioSetDir(kGpioTouchIrq, false);

    mtkGpioSetMode(kGpioKeyVolDown, 0U);
    mtkGpioSetDir(kGpioKeyVolDown, false);

    // GPIO 85..88 = FUNC1 (SPI0_MI, SPI0_CSB, SPI0_MO, SPI0_CLK)
    mtkGpioSetMode(kGpioSpi0Miso, 1U);
    mtkGpioSetMode(kGpioSpi0Cs,   1U);
    mtkGpioSetMode(kGpioSpi0Mosi, 1U);
    mtkGpioSetMode(kGpioSpi0Clk,  1U);

    mmioWrite32(gSpi0Base, kSpiCmdReg, kSpiCmdRst);
    IODelay(10);
    mmioWrite32(gSpi0Base, kSpiCmdReg, 0U);
    mmioWrite32(gSpi0Base, kSpiPadSelReg, 0U);
    mmioWrite32(gSpi0Base, kSpiCfg0Reg, (12U << 16) | 12U);
    mmioWrite32(gSpi0Base, kSpiCfg2Reg, (12U << 16) | 12U);
    mmioWrite32(gSpi0Base, kSpiCfg1Reg, 12U);

    merlinx_usb_log_line("[FTS-HW] SPI0/GPIO initialized (IRQ1=%d RST92=%d)\n",
        mtkGpioGetIn(kGpioTouchIrq), mtkGpioGetIn(kGpioTouchRst));
    return true;
}

static void merlinxSpiSwitchToBitbang()
{
    if (gSpiUseBitbang) {
        return;
    }
    gSpiUseBitbang = 1;
    mtkGpioSetMode(kGpioSpi0Miso, 0U);
    mtkGpioSetDir(kGpioSpi0Miso, false);

    mtkGpioSetMode(kGpioSpi0Cs, 0U);
    mtkGpioSetDir(kGpioSpi0Cs, true);
    mtkGpioSetOut(kGpioSpi0Cs, true);

    mtkGpioSetMode(kGpioSpi0Mosi, 0U);
    mtkGpioSetDir(kGpioSpi0Mosi, true);
    mtkGpioSetOut(kGpioSpi0Mosi, false);

    mtkGpioSetMode(kGpioSpi0Clk, 0U);
    mtkGpioSetDir(kGpioSpi0Clk, true);
    mtkGpioSetOut(kGpioSpi0Clk, false);

    merlinx_usb_log_line("[FTS-HW] Switched SPI0 to GPIO 85..88 bit-bang mode\n");
}

static int merlinxSpiTransferBitbang(
    const uint8_t *tx,
    uint8_t *rx,
    size_t len,
    unsigned firstDelayUs)
{
    if (!gGpioBase) {
        return FTS_ERR_BUS;
    }
    static constexpr uint32_t kBank2DoutSet = 0x124U;
    static constexpr uint32_t kBank2DoutClr = 0x128U;
    static constexpr uint32_t kBank2Din     = 0x220U;
    static constexpr uint32_t kBitMiso      = (1U << 21); // GPIO 85
    static constexpr uint32_t kBitCs        = (1U << 22); // GPIO 86
    static constexpr uint32_t kBitMosi      = (1U << 23); // GPIO 87
    static constexpr uint32_t kBitClk       = (1U << 24); // GPIO 88

    mmioWrite32(gGpioBase, kBank2DoutClr, kBitCs | kBitClk);
    for (size_t i = 0; i < len; ++i) {
        uint8_t outByte = tx ? tx[i] : 0U;
        uint8_t inByte = 0U;
        for (int b = 7; b >= 0; --b) {
            if ((outByte >> b) & 1U) {
                mmioWrite32(gGpioBase, kBank2DoutSet, kBitMosi);
            } else {
                mmioWrite32(gGpioBase, kBank2DoutClr, kBitMosi);
            }
            mmioWrite32(gGpioBase, kBank2DoutSet, kBitClk);
            uint32_t din = mmioRead32(gGpioBase, kBank2Din);
            inByte = static_cast<uint8_t>((inByte << 1) | ((din & kBitMiso) ? 1U : 0U));
            mmioWrite32(gGpioBase, kBank2DoutClr, kBitClk);
        }
        if (rx) {
            rx[i] = inByte;
        }
        if (i == 0 && firstDelayUs > 0 && len > 1) {
            IODelay(firstDelayUs);
        }
    }
    mmioWrite32(gGpioBase, kBank2DoutSet, kBitCs);
    mmioWrite32(gGpioBase, kBank2DoutClr, kBitMosi | kBitClk);
    return FTS_OK;
}

static int merlinxSpiTransferHwFifo(
    const uint8_t *tx,
    uint8_t *rx,
    size_t len,
    unsigned firstDelayUs)
{
    if (!gSpi0Base) {
        return FTS_ERR_BUS;
    }
    mmioWrite32(gSpi0Base, kSpiCmdReg, kSpiCmdRst);
    mmioWrite32(gSpi0Base, kSpiCmdReg, 0U);

    size_t offset = 0;
    bool firstChunk = true;
    while (offset < len) {
        size_t chunk = len - offset;
        if (firstChunk && firstDelayUs > 0 && len > 1) {
            chunk = 1U;
        } else if (chunk > kSpiFifoSize) {
            chunk = kSpiFifoSize;
        }
        bool lastChunk = (offset + chunk >= len);

        uint32_t cfg1 = ((static_cast<uint32_t>(chunk - 1U) & 0x3FFU) << 16) | 12U;
        mmioWrite32(gSpi0Base, kSpiCfg1Reg, cfg1);

        uint32_t cmd = kSpiCmdTxmsbf | kSpiCmdRxmsbf;
        if (!lastChunk) {
            cmd |= kSpiCmdPauseEn;
        }
        mmioWrite32(gSpi0Base, kSpiCmdReg, cmd);

        size_t numWords = (chunk + 3U) / 4U;
        for (size_t w = 0; w < numWords; ++w) {
            uint32_t word = 0U;
            for (size_t b = 0; b < 4U; ++b) {
                size_t idx = offset + w * 4U + b;
                uint8_t byteVal = (tx && idx < offset + chunk) ? tx[idx] : 0U;
                word |= (static_cast<uint32_t>(byteVal) << (b * 8U));
            }
            mmioWrite32(gSpi0Base, kSpiTxDataReg, word);
        }

        cmd |= firstChunk ? kSpiCmdAct : kSpiCmdResume;
        mmioWrite32(gSpi0Base, kSpiCmdReg, cmd);

        bool done = false;
        for (uint32_t poll = 0; poll < 50000U; ++poll) {
            uint32_t st0 = mmioRead32(gSpi0Base, kSpiStatus0Reg);
            if (lastChunk ? ((st0 & 0x1U) != 0) : ((st0 & 0x3U) != 0)) {
                done = true;
                break;
            }
            IODelay(1);
        }
        if (!done) {
            gSpiHwTimeouts++;
            mmioWrite32(gSpi0Base, kSpiCmdReg, kSpiCmdRst);
            mmioWrite32(gSpi0Base, kSpiCmdReg, 0U);
            return FTS_ERR_TIMEOUT;
        }

        for (size_t w = 0; w < numWords; ++w) {
            uint32_t word = mmioRead32(gSpi0Base, kSpiRxDataReg);
            if (rx) {
                for (size_t b = 0; b < 4U; ++b) {
                    size_t idx = offset + w * 4U + b;
                    if (idx < offset + chunk) {
                        rx[idx] = static_cast<uint8_t>((word >> (b * 8U)) & 0xFFU);
                    }
                }
            }
        }

        if (firstChunk && firstDelayUs > 0 && !lastChunk) {
            IODelay(firstDelayUs);
        }
        firstChunk = false;
        offset += chunk;
    }
    return FTS_OK;
}

} // namespace

// FocalTechTouch submodule board transport contract
extern "C" int fts_board_spi_transfer(
    void *context,
    const uint8_t *tx,
    uint8_t *rx,
    size_t len,
    unsigned first_delay_us)
{
    (void)context;
    if (len == 0) {
        return FTS_OK;
    }
    gSpiTransferCalls++;
    if (!gSpiUseBitbang) {
        int rc = merlinxSpiTransferHwFifo(tx, rx, len, first_delay_us);
        if (rc == FTS_OK) {
            return FTS_OK;
        }
        merlinxSpiSwitchToBitbang();
    }
    return merlinxSpiTransferBitbang(tx, rx, len, first_delay_us);
}

extern "C" void fts_board_delay_us(void *context, unsigned us)
{
    (void)context;
    IODelay(us);
}

extern "C" void fts_board_delay_ms(void *context, unsigned ms)
{
    (void)context;
    if (ms == 0) {
        return;
    }
    if (ms >= 5U) {
        IOSleep(ms);
    } else {
        IODelay(ms * 1000U);
    }
}

extern "C" void fts_board_reset(void *context, int asserted)
{
    (void)context;
    // asserted=1 drives active-low reset GPIO 92 low; asserted=0 releases it high.
    mtkGpioSetOut(kGpioTouchRst, asserted == 0);
}

extern "C" int fts_board_irq_wait(void *context, unsigned timeout_ms)
{
    (void)context;
    unsigned waitedUs = 0;
    unsigned limitUs = (timeout_ms == 0 || timeout_ms > 12U) ? 10000U : (timeout_ms * 1000U);
    while (waitedUs < limitUs) {
        if (mtkGpioGetIn(kGpioTouchIrq) == 0) {
            return FTS_OK;
        }
        IOSleep(2);
        waitedUs += 2000U;
    }
    // Return FTS_OK at the end of the 10 ms window so FtsProbe polls status smoothly
    // even if the panel interrupt polarity or EINT latch differs.
    return FTS_OK;
}

extern "C" void native_hid_input_observed(unsigned stage, int status)
{
    (void)stage;
    (void)status;
}

static void decorateDigitizerHierarchy(IORegistryEntry *root, unsigned depth)
{
    if (!root || !depth) {
        return;
    }
    IOService *svc = OSDynamicCast(IOService, root);
    if (svc) {
        svc->setProperty("Built-In", kOSBooleanTrue);
        svc->setProperty("DisplayIntegrated", kOSBooleanTrue);
        svc->setProperty("AppleEmbedded", kOSBooleanTrue);
        if (svc->metaCast("IOHIDEventDriver")) {
            if (OSNumber *up = OSNumber::withNumber(0x0dULL, 32)) {
                svc->setProperty("PrimaryUsagePage", up);
                up->release();
            }
            if (OSNumber *u = OSNumber::withNumber(0x04ULL, 32)) {
                svc->setProperty("PrimaryUsage", u);
                u->release();
            }
        }
    }
    OSIterator *it = root->getChildIterator(gIOServicePlane);
    if (!it) {
        return;
    }
    while (OSObject *obj = it->getNextObject()) {
        if (IORegistryEntry *child = OSDynamicCast(IORegistryEntry, obj)) {
            decorateDigitizerHierarchy(child, depth - 1);
        }
    }
    it->release();
}

extern "C" void native_hid_live_checkpoint(IOService *service)
{
    decorateDigitizerHierarchy(service, 4);
}

extern "C" bool native_hid_input_requested(void)
{
    if (__atomic_exchange_n(&gHidInputCommand, 0U, __ATOMIC_ACQUIRE) != 0U) {
        return true;
    }
    return false;
}

static void merlinxStartHidAndTouchThread(void *, wait_result_t)
{
    IOSleep(400);
    merlinxBoostCpuClocks();

    if (!merlinxInitTouchHardware()) {
        return;
    }

    int rcFam = OSKextLoadKextWithIdentifier("com.apple.iokit.IOHIDFamily");
    int rcDrv = OSKextLoadKextWithIdentifier("com.apple.iokit.IOHIDEventDriver");
    merlinx_usb_log_line("[FTS-HID] Load IOHIDFamily=0x%x IOHIDEventDriver=0x%x\n", rcFam, rcDrv);

    if (!gHidMetaRegistered) {
        void *handle = OSMetaClass::preModLoad("com.apple.driver.MerlinxHidInput");
        if (handle) {
            merlinx_hid_static_init();
            IOReturn mret = OSMetaClass::postModLoad(handle);
            gHidMetaRegistered = (mret == kIOReturnSuccess);
            merlinx_usb_log_line("[FTS-HID] postModLoad MerlinxHidInput=0x%x\n", mret);
        }
    }

    if (gHidMetaRegistered && !gActiveHidService) {
        IOService *parent = IOService::getResourceService();
        IOService *hid = native_hid_input_allocate();
        if (parent && hid && hid->init() && hid->attach(parent) && hid->start(parent)) {
            hid->registerService(kIOServiceAsynchronous);
            gActiveHidService = hid;
            decorateDigitizerHierarchy(hid, 4);
            merlinx_usb_log_line("[FTS-HID] Published NativeHidInput=%p on IOResources\n", hid);
        } else if (hid) {
            hid->release();
        }
    }

    gFtsProbeResult = fts_touch_probe(nullptr);
    merlinx_usb_log_line("[FTS-HW] fts_touch_probe() => %d\n", gFtsProbeResult);

    int prevVolDown = 1;
    uint32_t ticks = 0;
    for (;;) {
        IOSleep(60);
        ticks++;
        int curVolDown = mtkGpioGetIn(kGpioKeyVolDown);
        if (prevVolDown == 1 && curVolDown == 0) {
            merlinx_usb_log_line("[FTS-KEY] GPIO94 Volume Down pressed -> Home + unlock swipe\n");
            __atomic_store_n(&gHidInputCommand, 1U, __ATOMIC_RELEASE);
            merlinx_hid_inject_swipe(540, 2336, 540, 1100, 16);
        }
        prevVolDown = curVolDown;

        if (ticks == 25 || ticks == 100) {
            merlinxFixupMtkOvl(false);
            if (gActiveHidService) {
                decorateDigitizerHierarchy(gActiveHidService, 4);
            }
        }
        if ((ticks % 50U) == 0 && fts_touch_rows[0][0] != '\0') {
            merlinx_usb_log_line("[FTS] %s | %s\n", fts_touch_rows[0], fts_touch_rows[2]);
        }
    }
}

static void dline(unsigned line, const char *format, ...) {
    char text[96];
    va_list list;
    if (!native_gop_text) return;
    if (native_legacy_retired_frames >= 6 && !gMerlinxGopOsdForce) return;
    va_start(list, format);
    vsnprintf(text, sizeof(text), format, list);
    va_end(list);
    native_gop_text(line, text);
}

/* Last-resort visual path: prove the boot GOP still scans out even when the
 * original framebuffer provider is not accepted by userland. */
static void gopPaint(const char *why, bool full) {
    PE_Video video = {};
    if (PE_current_console(&video) || !video.v_baseAddr || !video.v_width ||
        !video.v_height || video.v_depth != 32 || video.v_rotate ||
        !video.v_rowBytes || video.v_width > 16384 || video.v_height > 16384) {
        dline(21, "GOP NOVID %s", why);
        return;
    }
    IODeviceMemory *memory = IODeviceMemory::withRange(video.v_baseAddr,
        (IOByteCount)video.v_rowBytes * video.v_height);
    IOMemoryMap *mapping = memory ? memory->map(kIOMapAnywhere | kIOMapInhibitCache) : nullptr;
    if (memory) memory->release();
    if (!mapping || !mapping->getVirtualAddress()) {
        dline(21, "GOP NOMAP %s", why);
        OSSafeReleaseNULL(mapping);
        return;
    }
    static const uint32_t colors[8] = {
        0xFFFFFFFFU, 0xFFFFFF00U, 0xFF00FFFFU, 0xFF00FF00U,
        0xFFFF00FFU, 0xFFFF0000U, 0xFF0000FFU, 0xFF000000U,
    };
    volatile uint8_t *base = reinterpret_cast<volatile uint8_t *>(mapping->getVirtualAddress());
    const uint32_t first = full ? 0 : (video.v_height > 320 ? video.v_height - 320 : 0);
    for (uint32_t y = first; y < video.v_height; ++y) {
        volatile uint32_t *row = reinterpret_cast<volatile uint32_t *>(base + (uint64_t)y * video.v_rowBytes);
        for (uint32_t x = 0; x < video.v_width; ++x)
            row[x] = colors[(uint64_t)x * 8 / video.v_width];
    }
    __asm__ volatile("dsb sy" ::: "memory");
    dline(21, "GOP FALLBACK %s", why);
    mapping->release();
}

extern "C" kmod_info_t g_kernel_kmod_info;
extern "C" void native_legacy_display_static_init(void) __asm__("__GLOBAL__sub_I_NativeLegacyDisplay.cpp");
extern "C" IOService *native_legacy_display_allocate(void);
extern "C" void native_gop_presenter_static_init(void) __asm__("__GLOBAL__sub_I_NativeGOPPresenter.cpp");
extern "C" IOService *native_gop_presenter_create(void);
static IOService *pendingLegacyDisplay;
static IOService *activeGOPPresenter,*activeGOPNub;
static IOService *gpuHandoffPresenter;
extern "C" bool native_gop_gpu_offer(IOService *,void *display,IOMemoryDescriptor *,uint64_t stride,uint32_t width,uint32_t height);
extern "C" void native_gop_presenter_set_source(IOService *presenter,IOService *nub);
volatile uint32_t native_legacy_gpu_fallbacks,native_legacy_gpu_last_status;
static IOService *activeLegacyDisplay;
static IOService *activeKeyStore;
extern "C" IOService *native_keyevents_allocate(void);
volatile uint32_t native_keyevents_refused_opens,native_aks_opens,native_aks_calls;
extern "C" void native_keyevents_open_refused(void) {__atomic_fetch_add(&native_keyevents_refused_opens,1,__ATOMIC_RELAXED);}
extern "C" bool native_aks_state_enabled(void) {
    unsigned v=0;return PE_parse_boot_argn("sheng-aks",&v,sizeof(v)) && v==1;
}
extern "C" uint32_t native_aks_lock_state(void) {
    unsigned v=0;return PE_parse_boot_argn("sheng-aks-lock",&v,sizeof(v))?v:0;
}
extern "C" void native_aks_opened(void) {
    unsigned n=__atomic_fetch_add(&native_aks_opens,1,__ATOMIC_RELAXED)+1;
    if(n<=4)dline(100,"AKS OPEN #%u",n);
}
extern "C" void native_aks_call(uint32_t selector,int32_t rc) {
    unsigned n=__atomic_fetch_add(&native_aks_calls,1,__ATOMIC_RELAXED)+1;
    if(n==1 || (rc!=0 && n<=6))dline(100,"AKS S%u %x",selector,(unsigned)rc);
}
static IOMemoryMap *diagnosticAnchorMapping;
extern "C" int boot_progress_pulse_anchor(uintptr_t,uint64_t,unsigned,unsigned,unsigned) { return 0; }
extern "C" uint32_t native_ufs_dpu_map(unsigned,const uint64_t *,unsigned) { return 0; }
extern "C" volatile uint32_t native_ufs_dpu_state = 0;
static IORegistryEntry *unitScaleChosen;
static bool unitScaleAdded,unitRotationAdded;
static void finishUnitScalePolicy(bool keep) {
    if(unitScaleChosen && !keep) {
        if(unitScaleAdded)unitScaleChosen->removeProperty("display-scale");
        if(unitRotationAdded)unitScaleChosen->removeProperty("display-rotation");
    }
    OSSafeReleaseNULL(unitScaleChosen);unitScaleAdded=false;unitRotationAdded=false;
}
static bool publishOrMatchWord(IORegistryEntry *entry,const char *key,uint32_t value,bool &added) {
    OSObject *previous=entry->getProperty(key);
    if(previous) {
        OSData *data=OSDynamicCast(OSData,previous);uint32_t word=0;
        if(!data || data->getLength()!=sizeof(word))return false;
        memcpy(&word,data->getBytesNoCopy(),sizeof(word));return word==value;
    }
    OSData *data=OSData::withBytes(&value,sizeof(value));
    bool result=data && entry->setProperty(key,data);OSSafeReleaseNULL(data);added=result;return result;
}
extern "C" {
volatile uint32_t native_legacy_stage;
volatile int native_legacy_meta_status=-1;
volatile uint32_t native_legacy_init_result,native_legacy_start_result,native_legacy_probe_match;
volatile uint32_t native_legacy_parent_match,native_legacy_object_size,native_legacy_released;
volatile uint64_t native_legacy_object,native_legacy_nub,native_legacy_workloop;
volatile uint32_t native_legacy_guard_slot;
volatile uint64_t native_legacy_guard_object,native_legacy_guard_caller;
volatile int native_legacy_surface_status=-1;
volatile uint32_t native_legacy_surface_found;
volatile uint64_t native_legacy_video_base,native_legacy_video_bytes;
volatile uint64_t native_legacy_gop_map,native_legacy_gop_virtual,native_legacy_gop_physical,native_legacy_gop_bytes;
volatile uint32_t native_legacy_gop_width,native_legacy_gop_height;
volatile uint32_t native_legacy_registration_calls,native_legacy_properties_length;
volatile uint32_t native_legacy_dt_attached;
volatile int native_legacy_unit_scale_status=-1;
volatile uint32_t native_legacy_unit_scale_calls,native_legacy_chosen_scale,native_legacy_chosen_rotation;
volatile uint32_t native_legacy_pm_stage,native_legacy_pm_calls,native_legacy_pm_callbacks,native_legacy_pm_ordinal;
volatile int native_legacy_pm_status=-1;
volatile uint32_t native_legacy_swap_validation_calls,native_legacy_swap_validation_result,native_legacy_swap_validation_reason;
volatile uint64_t native_legacy_swap_request,native_legacy_swap_surface;
volatile uint32_t native_legacy_transfer_calls;
volatile int native_legacy_transfer_status=-1;
volatile uint64_t native_legacy_transfer_bytes,native_legacy_transfer_request;
volatile uint32_t native_legacy_retirement_calls,native_legacy_retired_generation,native_legacy_presented_generation,native_legacy_retired_frames;
volatile uint64_t native_legacy_retired_current,native_legacy_retired_pending;
char native_legacy_properties[4096];
volatile uint32_t native_legacy_client_calls,native_legacy_client_type;
volatile int native_legacy_client_pid,native_legacy_client_status=-1;
volatile int native_legacy_swap_pid;
volatile uint64_t native_legacy_client_task,native_legacy_client_object;
char native_legacy_client_class[64];
static char native_legacy_client_name[32];
static char native_legacy_swap_name[32];
extern kern_return_t OSKextLoadKextWithIdentifier(const char *);
int boot_progress_prepare_scanout(void) { return 1; }
void boot_progress_cancel_scanout(void) {}
void boot_progress_stage(const char *) {}
volatile uint32_t native_legacy_prepare_calls,native_legacy_prepare_reason,native_legacy_prepare_attempts;
volatile uint32_t native_legacy_prepare_failures,native_legacy_prepare_retried,native_legacy_prepare_max_attempts;
volatile uint64_t native_legacy_prepare_wait_us;
static uint64_t probeNowUs(void) {uint64_t ns;absolutetime_to_nanoseconds(mach_absolute_time(),&ns);return ns/1000;}
bool native_legacy_prepare_scanout(void) {
    native_legacy_prepare_calls=native_legacy_prepare_calls+1;native_legacy_prepare_reason=0;native_legacy_prepare_attempts=0;
    dline(29,"DISP PREP %u",native_legacy_prepare_calls);
    if (ml_at_interrupt_context()) {native_legacy_prepare_reason=1;return false;}
    if (!ml_get_interrupts_enabled()) {native_legacy_prepare_reason=2;return false;}
    const uint64_t began=probeNowUs();
    for (unsigned attempt=0;attempt<100;++attempt) {
        native_legacy_prepare_attempts=attempt+1;
        if (boot_progress_prepare_scanout()) {
            if(attempt) {
                native_legacy_prepare_retried=native_legacy_prepare_retried+1;native_legacy_prepare_wait_us=native_legacy_prepare_wait_us+(probeNowUs()-began);
                if(attempt+1>native_legacy_prepare_max_attempts)native_legacy_prepare_max_attempts=attempt+1;
            }
            return true;
        }
        IOSleep(1);
    }
    native_legacy_prepare_failures=native_legacy_prepare_failures+1;native_legacy_prepare_wait_us=native_legacy_prepare_wait_us+(probeNowUs()-began);
    native_legacy_prepare_reason=3;
    boot_progress_cancel_scanout();
    return false;
}
__attribute__((noinline)) void native_legacy_display_checkpoint(void) {__asm__ volatile("" ::: "memory");}
__attribute__((noinline)) void native_legacy_gop_mapping_checkpoint(void) {__asm__ volatile("" ::: "memory");}
__attribute__((noinline)) void native_legacy_client_checkpoint(void) {__asm__ volatile("" ::: "memory");}
void native_legacy_pm_observed(unsigned stage,IOReturn status,unsigned long ordinal) {
    native_legacy_pm_stage=stage;native_legacy_pm_status=status;native_legacy_pm_ordinal=(uint32_t)ordinal;
    native_legacy_pm_calls=native_legacy_pm_calls+1;
    if(stage==3)native_legacy_pm_callbacks=native_legacy_pm_callbacks+1;
    dline(29,"DISP PM %u %x",stage,(unsigned)status);
}
__attribute__((noinline)) void native_legacy_swap_validation_checkpoint(bool,unsigned,const void *,const void *) {__asm__ volatile("" ::: "memory");}
volatile uint32_t native_legacy_swap_accepted_count,native_legacy_swap_rejected_count,native_legacy_rej_reason;
volatile uint64_t native_legacy_rej_surfaces[4];
volatile uint32_t native_legacy_rej_state[3],native_legacy_rej_options[3],native_legacy_rej_size[2],native_legacy_rej_rects[2][4];
char native_legacy_rej_class[32];
volatile uint64_t native_legacy_rej_surface[7];
void native_legacy_surface_rejected(uint64_t width,uint64_t height,uint32_t format,uint32_t planes,uint64_t stride,uint64_t allocation,uint64_t bytes) {
    native_legacy_rej_surface[0]=width;native_legacy_rej_surface[1]=height;native_legacy_rej_surface[2]=format;native_legacy_rej_surface[3]=planes;
    native_legacy_rej_surface[4]=stride;native_legacy_rej_surface[5]=allocation;native_legacy_rej_surface[6]=bytes;
    dline(30,"DISP REJ %llux%llu %x",(unsigned long long)width,(unsigned long long)height,format);
}
static void recordRejectedSwap(unsigned reason,const void *request,const void *surface) {
    const unsigned char *bytes=reinterpret_cast<const unsigned char *>(request);
    if(!bytes)return;
    native_legacy_rej_reason=reason;
    uint64_t surfaces[4]={};memcpy(surfaces,bytes+2624,sizeof(surfaces));
    for(unsigned i=0;i<4;++i)native_legacy_rej_surfaces[i]=surfaces[i];
    const unsigned stateOffsets[3]={792,796,800},optionOffsets[3]={804,808,812};
    for(unsigned i=0;i<3;++i){uint32_t a=0,b=0;memcpy(&a,bytes+stateOffsets[i],4);memcpy(&b,bytes+optionOffsets[i],4);
        native_legacy_rej_state[i]=a;native_legacy_rej_options[i]=b;}
    uint32_t w=0,h=0;memcpy(&w,bytes+816,4);memcpy(&h,bytes+820,4);native_legacy_rej_size[0]=w;native_legacy_rej_size[1]=h;
    const unsigned rectOffsets[2]={17,81};
    for(unsigned r=0;r<2;++r){uint32_t rect[4]={};memcpy(rect,bytes+rectOffsets[r],sizeof(rect));
        for(unsigned i=0;i<4;++i)native_legacy_rej_rects[r][i]=rect[i];}
    char name[32]={};
    if(surface){const OSObject *object=reinterpret_cast<const OSObject *>(surface);
        const OSMetaClass *meta=object->getMetaClass();const char *cls=meta?meta->getClassName():nullptr;
        if(cls)strlcpy(name,cls,sizeof(name));}
    memcpy(native_legacy_rej_class,name,sizeof(name));
}
#define NATIVE_LEGACY_FRAMES 32
volatile uint32_t native_legacy_frame_count;
volatile uint32_t native_legacy_frame_kind[NATIVE_LEGACY_FRAMES];
volatile uint32_t native_legacy_frame_time[NATIVE_LEGACY_FRAMES];
volatile uint64_t native_legacy_frame_stats[NATIVE_LEGACY_FRAMES][5];
static uint64_t frameEpoch;
void native_legacy_frame_observed(unsigned kind,uint64_t count,uint64_t minx,uint64_t miny,uint64_t maxx,uint64_t maxy) {
    uint32_t n=native_legacy_frame_count;
    uint64_t now,frequency;
    __asm__ volatile("isb\n\tmrs %0, cntpct_el0":"=r"(now));__asm__ volatile("mrs %0, cntfrq_el0":"=r"(frequency));
    if(!frameEpoch)frameEpoch=now;
    if(n<NATIVE_LEGACY_FRAMES) {
        native_legacy_frame_kind[n]=kind;
        native_legacy_frame_time[n]=frequency?(uint32_t)((now-frameEpoch)/frequency):0;
        native_legacy_frame_stats[n][0]=count;native_legacy_frame_stats[n][1]=minx;native_legacy_frame_stats[n][2]=miny;
        native_legacy_frame_stats[n][3]=maxx;native_legacy_frame_stats[n][4]=maxy;
    }
    native_legacy_frame_count=n+1;
    dline(31,"DISP FRAME %u K%u",n+1,kind);
    if(n<NATIVE_LEGACY_FRAMES)
        printf("sheng-fb: frame %u kind %u: %llu lit pixels in (%llu,%llu)-(%llu,%llu)\n",n,kind,count,minx,miny,maxx,maxy);
}
volatile uint32_t native_legacy_frame_colors[8][4];
volatile uint64_t native_legacy_frame_color_counts[8][4],native_legacy_frame_color_other[8];
void native_legacy_frame_colors_observed(const uint32_t *colors,const uint64_t *counts,uint64_t other) {
    uint32_t n=native_legacy_frame_count;
    if(!n || n>8)return;
    n-=1;
    for(unsigned i=0;i<4;++i){native_legacy_frame_colors[n][i]=colors[i];native_legacy_frame_color_counts[n][i]=counts[i];}
    native_legacy_frame_color_other[n]=other;
}
struct SwapEvent {uint64_t submitted,offered,done;uint32_t number;};
static SwapEvent swapEvents[64];static uint32_t swapEventCount;
extern "C" void native_legacy_swap_stage(unsigned stage) {
    const uint64_t now=probeNowUs();
    uint32_t total=swapEventCount;
    if(stage==0) {
        SwapEvent &e=swapEvents[total%64];e={now,0,0,total+1};swapEventCount=total+1;return;
    }
    uint32_t first=total>64?total-64:0;
    for(uint32_t i=first;i<total;++i) {
        SwapEvent &e=swapEvents[i%64];
        if(stage==1 && !e.offered && !e.done){e.offered=now;return;}
        if(stage==2 && !e.done){e.done=now;if(!e.offered)e.offered=now;return;}
    }
}
extern "C" unsigned native_legacy_swap_timeline(char *out,unsigned cap) {
    unsigned at=0;out[0]=0;
    uint32_t total=swapEventCount,first=total>40?total-40:0;
    for(uint32_t i=first;i<total && at+90<cap;++i) {
        const SwapEvent &e=swapEvents[i%64];
        uint64_t previous=i>first?swapEvents[(i-1)%64].submitted:e.submitted;
        at+=snprintf(out+at,cap-at,"W%u t=%llu.%03llus +%llums offer=%lldms done=%lldms\n",e.number,(unsigned long long)(e.submitted/1000000),
            (unsigned long long)((e.submitted/1000)%1000),(unsigned long long)((e.submitted-previous)/1000),
            e.offered?(long long)((e.offered-e.submitted)/1000):-1LL,e.done?(long long)((e.done-e.submitted)/1000):-1LL);
    }
    return at;
}
extern "C" void native_legacy_vsync_notify_original(void *display,uint64_t timestamp,uint64_t continuousTime,uint64_t interval);
static volatile uint32_t vsyncInNotify;
extern "C" void native_legacy_vsync_notify(void *display,uint64_t timestamp,uint64_t continuousTime,uint64_t interval) {
    vsyncInNotify=1;
    native_legacy_vsync_notify_original(display,timestamp,continuousTime,interval);
    vsyncInNotify=0;
}
static volatile uint64_t vsyncEvents[6];
static volatile unsigned vsyncHz;
static thread_call_t vsyncWatch;
static void vsyncWatchFn(thread_call_param_t,thread_call_param_t) {
    dline(32,"VS s%llu a%llu i%llu l%llu n%u",
        (unsigned long long)vsyncEvents[1],(unsigned long long)vsyncEvents[0],
        (unsigned long long)vsyncEvents[2],(unsigned long long)vsyncEvents[3],(unsigned)vsyncInNotify);
    uint64_t deadline;clock_interval_to_deadline(2000,kMillisecondScale,&deadline);
    thread_call_enter_delayed(vsyncWatch,deadline);
}
void native_legacy_vsync_observed(unsigned event,unsigned hz) {
    if(event<6)vsyncEvents[event]=vsyncEvents[event]+1;
    vsyncHz=hz;
    dline(31,"DISP VSYNC %u %u",event,hz);
    if(event==4 && !vsyncWatch) {
        vsyncWatch=thread_call_allocate(vsyncWatchFn,nullptr);
        if(vsyncWatch)vsyncWatchFn(nullptr,nullptr);
    }
}
extern "C" unsigned native_legacy_prepare_report(char *out,unsigned cap) {
    return snprintf(out,cap,"prepare calls=%u failures=%u retried=%u max_attempts=%u waited_ms=%llu vsync hz=%u sent=%llu arms=%llu idle=%llu late=%llu start=%llu refresh=%llu",
        native_legacy_prepare_calls,native_legacy_prepare_failures,native_legacy_prepare_retried,native_legacy_prepare_max_attempts,
        (unsigned long long)(native_legacy_prepare_wait_us/1000),vsyncHz,(unsigned long long)vsyncEvents[1],(unsigned long long)vsyncEvents[0],
        (unsigned long long)vsyncEvents[2],(unsigned long long)vsyncEvents[3],(unsigned long long)vsyncEvents[4],(unsigned long long)vsyncEvents[5]);
}
void native_legacy_swap_validation_observed(bool accepted,unsigned reason,const void *request,const void *surface) {
    if(accepted) {
        native_legacy_swap_stage(0);
        native_legacy_swap_accepted_count=native_legacy_swap_accepted_count+1;
        task_t stask=current_task();
        int spid=stask?proc_pid((proc_t)get_bsdtask_info(stask)):0;
        if(spid>0) {
            native_legacy_swap_pid=spid;
            native_legacy_swap_name[0]=0;
            proc_name(spid,native_legacy_swap_name,sizeof(native_legacy_swap_name));
        }
    }
    else {native_legacy_swap_rejected_count=native_legacy_swap_rejected_count+1;recordRejectedSwap(reason,request,surface);}
    native_legacy_swap_validation_calls=native_legacy_swap_validation_calls+1;
    native_legacy_swap_validation_result=accepted;native_legacy_swap_validation_reason=reason;
    dline(26,"DISP VAL %d R%u",accepted,reason);
    native_legacy_swap_request=(uintptr_t)request;native_legacy_swap_surface=(uintptr_t)surface;
    native_legacy_swap_validation_checkpoint(accepted,reason,request,surface);
}
volatile uint64_t native_legacy_copy_n,native_legacy_copy_last,native_legacy_copy_max,native_legacy_copy_sum,native_legacy_copy_bytes_last,native_legacy_copy_bytes_sum;
void native_legacy_copy_timing_observed(uint64_t ticks,uint64_t bytes) {
    native_legacy_copy_bytes_last=bytes;native_legacy_copy_bytes_sum=native_legacy_copy_bytes_sum+bytes;
    native_legacy_copy_n=native_legacy_copy_n+1;native_legacy_copy_last=ticks;native_legacy_copy_sum=native_legacy_copy_sum+ticks;
    if(ticks>native_legacy_copy_max)native_legacy_copy_max=ticks;
    if(native_legacy_copy_n<=32) {
        uint64_t frequency;__asm__ volatile("mrs %0, cntfrq_el0":"=r"(frequency));
        printf("sheng-fb: copy %u: %llu bytes in %llu us\n",(unsigned)native_legacy_copy_n,bytes,frequency?ticks*1000000/frequency:0);
    }
}
__attribute__((noinline)) void native_legacy_transfer_observed(IOReturn status,const void *request,uint64_t bytes) {
    native_legacy_transfer_calls=native_legacy_transfer_calls+1;
    if(status==kIOReturnSuccess)native_legacy_swap_stage(2);
    native_legacy_transfer_status=status;native_legacy_transfer_request=(uintptr_t)request;native_legacy_transfer_bytes=bytes;
    dline(30,"DISP COPY %x %llu",(unsigned)status,(unsigned long long)bytes);
    if(status!=kIOReturnSuccess) {
        char message[120];snprintf(message,sizeof(message),"DISPLAY COPY FAILED status=%08x prepare=%u tries=%u",
          (unsigned)status,native_legacy_prepare_reason,native_legacy_prepare_attempts);
        boot_progress_stage(message);
    }
}
bool native_legacy_gpu_offer(void *display,IOMemoryDescriptor *memory,uint64_t stride,uint32_t width,uint32_t height,uint32_t format) {
    (void)format;
    return gpuHandoffPresenter && native_gop_gpu_offer(gpuHandoffPresenter,display,memory,stride,width,height);
}
void native_legacy_gpu_fallback_observed(int status) {
    native_legacy_gpu_last_status=(uint32_t)status;native_legacy_gpu_fallbacks=native_legacy_gpu_fallbacks+1;
}
__attribute__((noinline)) void native_legacy_retirement_observed(IOService *display) {
    const unsigned char *bytes=reinterpret_cast<const unsigned char *>(display);
    uint64_t current=0,pending=0;uint32_t completed=0,presented=0,frames=0;
    memcpy(&current,bytes+5472,8);memcpy(&pending,bytes+5480,8);
    memcpy(&completed,bytes+5820,4);memcpy(&presented,bytes+5824,4);memcpy(&frames,bytes+252,4);
    native_legacy_retired_current=current;native_legacy_retired_pending=pending;
    native_legacy_retired_generation=completed;native_legacy_presented_generation=presented;native_legacy_retired_frames=frames;
    native_legacy_retirement_calls=native_legacy_retirement_calls+1;
    if (frames <= 2) {
        merlinxFixupMtkOvl(false);
    }
    dline(30,"DISP RET %u/%u F%u S%d:%.6s C%u/%d:%.6s",completed,presented,frames,
        native_legacy_swap_pid,native_legacy_swap_name[0]?native_legacy_swap_name:"-",
        native_legacy_client_calls,native_legacy_client_pid,native_legacy_client_name[0]?native_legacy_client_name:"-");
    if (frames < 6 || gMerlinxGopOsdForce) {
        if(native_diag_process_summary)native_diag_process_summary();
        if(native_gop_redraw_ring)native_gop_redraw_ring();
    }
}
extern "C" unsigned native_legacy_properties_now(char *out,unsigned cap) {
    if(!out || !cap)return 0;
    out[0]=0;
    IOService *display=reinterpret_cast<IOService *>(native_legacy_object);
    if(!display)return 0;
    OSSerialize *xml=OSSerialize::withCapacity(8192);
    unsigned size=0;
    if(xml && display->serializeProperties(xml)) {
        size=xml->getLength();if(size>=cap)size=cap-1;
        memcpy(out,xml->text(),size);out[size]=0;
    }
    OSSafeReleaseNULL(xml);
    return size;
}
void native_legacy_registration_observed(IOService *display) {
    native_legacy_registration_calls=native_legacy_registration_calls+1;
    dline(27,"DISP REG %u",native_legacy_registration_calls);
    OSSerialize *xml=OSSerialize::withCapacity(2048);
    if(xml && display->serializeProperties(xml)) {
        unsigned size=xml->getLength();if(size>=sizeof(native_legacy_properties))size=sizeof(native_legacy_properties)-1;
        memcpy(native_legacy_properties,xml->text(),size);native_legacy_properties[size]=0;native_legacy_properties_length=size;
    }
    OSSafeReleaseNULL(xml);
}
void native_legacy_client_enter(task_t task,uint32_t type) {
    native_legacy_client_calls=native_legacy_client_calls+1;native_legacy_client_task=(uintptr_t)task;
    native_legacy_client_type=type;native_legacy_client_pid=task?proc_pid((proc_t)get_bsdtask_info(task)):0;
    native_legacy_client_name[0]=0;
    if(native_legacy_client_pid)proc_name(native_legacy_client_pid,native_legacy_client_name,sizeof(native_legacy_client_name));
    dline(28,"DISP CLIENT %d %s T%u",native_legacy_client_pid,native_legacy_client_name,type);
    dline(100,"CL#%u %d %.12s T%u",native_legacy_client_calls,native_legacy_client_pid,
        native_legacy_client_name[0]?native_legacy_client_name:"-",type);
}
enum {kSeenClients=8,kSelectors=96,kRegEvents=8,kVtableSlots=256};
struct ClientStat {
    uintptr_t object;int pid;uint32_t calls,fails;uint32_t selector_calls[kSelectors],selector_fails[kSelectors],selector_status[kSelectors];
    unsigned regs;struct {uint32_t type;uint64_t port;uint32_t ref,status;} reg[kRegEvents];
    uint64_t sel6_first[3],sel6_last[3];uint32_t sel6_distinct;uint64_t sel6_seen[8][3];uint32_t sel6_calls[8],sel6_fails[8],sel6_status[8];
    uint32_t seq_count;struct {uint32_t a,b,c,status,done,presented;} seq[24];
    uintptr_t original_external,original_register;
};
static ClientStat seenClients[kSeenClients];static unsigned seenClientCount;
typedef IOReturn (*ExternalFn)(IOUserClient *,uint32_t,IOExternalMethodArguments *,IOExternalMethodDispatch *,OSObject *,void *);
typedef IOReturn (*RegisterFn)(IOUserClient *,mach_port_t,UInt32,UInt32);
static ClientStat *statFor(IOUserClient *client) {
    for(unsigned i=0;i<seenClientCount;++i)if(seenClients[i].object==(uintptr_t)client)return &seenClients[i];
    return nullptr;
}
static IOReturn wrappedExternal(IOUserClient *client,uint32_t selector,IOExternalMethodArguments *args,IOExternalMethodDispatch *dispatch,OSObject *target,void *ref) {
    ClientStat *stat=statFor(client);
    IOReturn status=stat?reinterpret_cast<ExternalFn>(stat->original_external)(client,selector,args,dispatch,target,ref):kIOReturnNotAttached;
    if(stat) {
        if(selector==6 && args && args->scalarInput && args->scalarInputCount>=3) {
            const uint64_t *in=args->scalarInput;
            if(!stat->sel6_distinct && !stat->sel6_first[0] && !stat->sel6_first[1])memcpy(stat->sel6_first,in,sizeof stat->sel6_first);
            memcpy(stat->sel6_last,in,sizeof stat->sel6_last);
            int at=-1;
            for(unsigned k=0;k<stat->sel6_distinct && k<8;++k)if(!memcmp(stat->sel6_seen[k],in,24))at=(int)k;
            if(at<0 && stat->sel6_distinct<8){at=(int)stat->sel6_distinct++;memcpy(stat->sel6_seen[at],in,24);}
            if(stat->seq_count<24){
                auto &e=stat->seq[stat->seq_count++];e.a=(uint32_t)in[0];e.b=(uint32_t)in[1];e.c=(uint32_t)in[2];e.status=(uint32_t)status;
                const unsigned char *object=reinterpret_cast<const unsigned char *>(native_legacy_object);
                e.done=e.presented=0;if(object){memcpy(&e.done,object+5820,4);memcpy(&e.presented,object+5824,4);}
            }
            if(at>=0){++stat->sel6_calls[at];if(status)++stat->sel6_fails[at];stat->sel6_status[at]=(uint32_t)status;}
        }
        ++stat->calls;if(status)++stat->fails;
        if(selector<kSelectors){++stat->selector_calls[selector];if(status)++stat->selector_fails[selector];stat->selector_status[selector]=(uint32_t)status;}
    }
    return status;
}
static IOReturn wrappedRegister(IOUserClient *client,mach_port_t port,UInt32 type,UInt32 refCon) {
    ClientStat *stat=statFor(client);
    IOReturn status=stat?reinterpret_cast<RegisterFn>(stat->original_register)(client,port,type,refCon):kIOReturnNotAttached;
    if(stat && stat->regs<kRegEvents){auto &e=stat->reg[stat->regs++];e.type=type;e.port=(uint64_t)(uintptr_t)port;e.ref=refCon;e.status=(uint32_t)status;}
    return status;
}
static void interpose(IOUserClient *client,int pid) {
#if __has_feature(ptrauth_calls)
    (void)client;(void)pid;return;
#endif
    if(seenClientCount>=kSeenClients)return;
    ClientStat &stat=seenClients[seenClientCount];
    memset(&stat,0,sizeof stat);
    uintptr_t *vtable=*reinterpret_cast<uintptr_t **>(client);
    uintptr_t *copy=static_cast<uintptr_t *>(IOMalloc(kVtableSlots*sizeof(uintptr_t)));
    if(!copy)return;
    memcpy(copy,vtable,kVtableSlots*sizeof(uintptr_t));
    stat.object=(uintptr_t)client;stat.pid=pid;stat.original_external=copy[168];stat.original_register=copy[175];
    copy[168]=(uintptr_t)&wrappedExternal;copy[175]=(uintptr_t)&wrappedRegister;
    ++seenClientCount;
    *reinterpret_cast<uintptr_t **>(client)=copy;
}
extern "C" unsigned native_legacy_state_words(char *out,unsigned cap) {
    out[0]=0;
    const unsigned char *object=reinterpret_cast<const unsigned char *>(native_legacy_object);
    if(!object)return 0;
    unsigned at=snprintf(out,cap,"d+5808:");
    for(unsigned o=5808;o<5872 && at+16<cap;o+=4){uint32_t v;memcpy(&v,object+o,4);at+=snprintf(out+at,cap-at," %x",v);}
    return at;
}
extern "C" unsigned native_legacy_clients_dump(char *out,unsigned cap) {
    unsigned at=0;out[0]=0;
    for(unsigned c=0;c<seenClientCount && at+120<cap;++c) {
        const ClientStat &s=seenClients[c];
        at+=snprintf(out+at,cap-at,"C%u pid=%d calls=%u fails=%u regs=%u",c,s.pid,s.calls,s.fails,s.regs);
        for(unsigned r=0;r<s.regs && r<kRegEvents && at+60<cap;++r)
            at+=snprintf(out+at,cap-at," reg(t%u,p%llx,r%x)=%x",s.reg[r].type,(unsigned long long)s.reg[r].port,s.reg[r].ref,s.reg[r].status);
        at+=snprintf(out+at,cap-at,"\n");
        for(unsigned k=0;k<s.sel6_distinct && k<8 && at+100<cap;++k)
            at+=snprintf(out+at,cap-at,"  sel6 args %llx %llx %llx: %u calls %u fails last %x\n",(unsigned long long)s.sel6_seen[k][0],(unsigned long long)s.sel6_seen[k][1],(unsigned long long)s.sel6_seen[k][2],s.sel6_calls[k],s.sel6_fails[k],s.sel6_status[k]);
        for(unsigned k=0;k<s.seq_count && at+90<cap;++k)
            at+=snprintf(out+at,cap-at,"  s6#%u %x %x %x -> %x [5820]=%x [5824]=%x\n",k,s.seq[k].a,s.seq[k].b,s.seq[k].c,s.seq[k].status,s.seq[k].done,s.seq[k].presented);
        unsigned n=0;
        for(unsigned sel=0;sel<kSelectors && at+40<cap;++sel) {
            if(!s.selector_calls[sel])continue;
            if(!n)at+=snprintf(out+at,cap-at,"  sel");
            at+=snprintf(out+at,cap-at," %u:%u/%u/%x",sel,s.selector_calls[sel],s.selector_fails[sel],s.selector_status[sel]);
            if(++n%6==0)at+=snprintf(out+at,cap-at,"\n  sel");
        }
        if(n)at+=snprintf(out+at,cap-at,"\n");
    }
    return at;
}
void native_legacy_client_observed(IOReturn status,IOUserClient *client) {
    if(client && !status)interpose(client,native_legacy_client_pid);
    native_legacy_client_status=status;native_legacy_client_object=(uintptr_t)client;
    if(client)strlcpy(native_legacy_client_class,client->getMetaClass()->getClassName(),sizeof(native_legacy_client_class));
    dline(28,"DISP CLIENT %d %s %x",native_legacy_client_pid,native_legacy_client_name,(unsigned)status);
    dline(100,"CL %d %.16s %x",native_legacy_client_pid,native_legacy_client_name,(unsigned)status);
    native_legacy_client_checkpoint();
}
__attribute__((noinline)) bool native_legacy_gop_mapping_observed(IOService *object,IOMemoryMap *mapping,uint32_t width,uint32_t height) {
    if((uintptr_t)object!=native_legacy_object)panic("NativeLegacyDisplay: wrong mapping owner");
    native_legacy_gop_map=(uintptr_t)mapping;native_legacy_gop_virtual=mapping->getVirtualAddress();
    native_legacy_gop_physical=mapping->getPhysicalAddress();native_legacy_gop_bytes=mapping->getLength();
    native_legacy_gop_width=width;native_legacy_gop_height=height;
    PE_parse_boot_argn("merlinx-gop-osd",&gMerlinxGopOsdForce,sizeof(gMerlinxGopOsdForce));
    merlinxFixupMtkOvl(true);
    dline(24,"DISP MAP %ux%u %llx",width,height,(unsigned long long)mapping->getPhysicalAddress());
    if(!diagnosticAnchorMapping) {
        PE_Video anchorVideo={};
        if(!PE_current_console(&anchorVideo) && anchorVideo.v_depth==32 &&
           anchorVideo.v_rowBytes<=65536 &&
           anchorVideo.v_width==width && anchorVideo.v_height==height &&
           anchorVideo.v_baseAddr==mapping->getPhysicalAddress() && !anchorVideo.v_rotate) {
            mapping->retain();
            if(boot_progress_pulse_anchor(mapping->getVirtualAddress(),mapping->getLength(),width,height,
                (unsigned)anchorVideo.v_rowBytes))diagnosticAnchorMapping=mapping;
            else mapping->release();
        }
    }
    native_legacy_gop_mapping_checkpoint();
    unsigned enabled=0;PE_parse_boot_argn("offline-legacy-unit-scale",&enabled,sizeof(enabled));
    if(!enabled)return true;
    native_legacy_unit_scale_calls=native_legacy_unit_scale_calls+1;native_legacy_unit_scale_status=1;
    unsigned phase=0;PE_parse_boot_argn("offline-legacy-phase",&phase,sizeof(phase));
    PE_Video video={};
    if(enabled<1 || enabled>3 || phase!=4 || unitScaleChosen || PE_current_console(&video) ||
       video.v_width!=width || video.v_height!=height || video.v_rotate ||
       mapping->getPhysicalAddress()!=video.v_baseAddr)return false;
    unitScaleChosen=IORegistryEntry::fromPath("/chosen",gIODTPlane);
    if(!unitScaleChosen || !publishOrMatchWord(unitScaleChosen,"display-scale",enabled,unitScaleAdded) ||
       !publishOrMatchWord(unitScaleChosen,"display-rotation",video.v_rotate,unitRotationAdded)) {
        finishUnitScalePolicy(false);return false;
    }
    OSData *scale=OSDynamicCast(OSData,unitScaleChosen->getProperty("display-scale"));
    OSData *rotation=OSDynamicCast(OSData,unitScaleChosen->getProperty("display-rotation"));
    uint32_t actualScale=0,actualRotation=0;
    if(!scale || !rotation || scale->getLength()!=4 || rotation->getLength()!=4) {
        finishUnitScalePolicy(false);return false;
    }
    memcpy(&actualScale,scale->getBytesNoCopy(),4);memcpy(&actualRotation,rotation->getBytesNoCopy(),4);
    native_legacy_chosen_scale=actualScale;native_legacy_chosen_rotation=actualRotation;
    native_legacy_unit_scale_status=actualScale==enabled && actualRotation==video.v_rotate?0:2;
    if(native_legacy_unit_scale_status){finishUnitScalePolicy(false);return false;}
    return true;
}
__attribute__((noreturn,noinline)) void native_legacy_backend_guard(unsigned slot,void *object,void *caller) {
    native_legacy_guard_slot=slot;native_legacy_guard_object=(uintptr_t)object;native_legacy_guard_caller=(uintptr_t)caller;
    panic("NativeLegacyDisplay: leaf=%u caller=%llx stage=%u PM=%u swap=%u/%u copy=%08x bytes=%llu retire=%u handoff=%u/%u",
      slot,(uint64_t)(uintptr_t)caller,native_legacy_stage,native_legacy_pm_stage,
      native_legacy_swap_validation_result,native_legacy_swap_validation_reason,
      (unsigned)native_legacy_transfer_status,native_legacy_transfer_bytes,native_legacy_retirement_calls,
      native_legacy_prepare_reason,native_legacy_prepare_attempts);
}
void native_legacy_display_probe(void);
void native_legacy_display_probe(void) {
    if(native_legacy_stage)return;
    native_legacy_stage=1;dline(21,"DISP PROBE 1");void *handle=OSMetaClass::preModLoad(g_kernel_kmod_info.name);
    if(!handle){native_legacy_display_checkpoint();return;}
    native_legacy_display_static_init();native_gop_presenter_static_init();native_legacy_meta_status=OSMetaClass::postModLoad(handle);
    if(native_legacy_meta_status){native_legacy_display_checkpoint();return;}
    PE_Video video={};native_legacy_stage=2;dline(21,"DISP PROBE 2");
    if(PE_current_console(&video) || !video.v_baseAddr || !video.v_width || !video.v_height || video.v_depth!=32
        || video.v_rotate || video.v_width>16384 || video.v_height>16384 || video.v_rowBytes<video.v_width*4
        || video.v_rowBytes>UINT64_MAX/video.v_height){dline(22,"DISP NOGOP");native_legacy_display_checkpoint();return;}
    IOService *display=native_legacy_display_allocate();native_legacy_object=(uintptr_t)display;
    if(!display){native_legacy_display_checkpoint();return;}
    native_legacy_object_size=display->getMetaClass()->getClassSize();
    native_legacy_parent_match=(display->metaCast("IOMobileFramebufferLegacy") && display->metaCast("IOMobileFramebuffer") && display->metaCast("IOService"));
    native_legacy_stage=3;native_legacy_init_result=display->init();dline(22,"DISP INIT %d",native_legacy_init_result);
    unsigned phase=1;PE_parse_boot_argn("offline-legacy-phase",&phase,sizeof(phase));
    if(native_legacy_init_result && (phase==2 || phase==3 || phase==4)) {
        pendingLegacyDisplay=display;return;
    }
    native_legacy_stage=7;gopPaint("EARLY", true);display->release();native_legacy_released=1;native_legacy_stage=8;
    native_legacy_display_checkpoint();
}
void native_legacy_display_late_probe(void);
void native_legacy_display_late_probe(void) {
    IOService *display=pendingLegacyDisplay;if(!display)return;pendingLegacyDisplay=nullptr;dline(21,"DISP LATE");
    native_legacy_surface_status=OSKextLoadKextWithIdentifier("com.apple.iokit.IOSurface");
    OSDictionary *matching=IOService::nameMatching("IOCoreSurfaceRoot");
    IOService *surface=matching?IOService::waitForMatchingService(matching,5000000000ULL):nullptr;
    native_legacy_surface_found=surface!=nullptr;dline(23,"DISP SURF %d F%d",native_legacy_surface_status,native_legacy_surface_found);OSSafeReleaseNULL(surface);OSSafeReleaseNULL(matching);
    PE_Video video={};
    if(!native_legacy_surface_status && native_legacy_surface_found && !PE_current_console(&video)
        && video.v_baseAddr && video.v_width && video.v_height && video.v_depth==32
        && !video.v_rotate && video.v_width<=16384 && video.v_height<=16384
        && video.v_rowBytes>=video.v_width*4 && video.v_rowBytes<=UINT64_MAX/video.v_height) {
        native_legacy_video_base=video.v_baseAddr;native_legacy_video_bytes=video.v_rowBytes*video.v_height;dline(23,"DISP VID %llx %ux%u",(unsigned long long)video.v_baseAddr,(unsigned)video.v_width,(unsigned)video.v_height);
        native_legacy_stage=4;IOService *nub=new IOService;
        native_legacy_nub=(uintptr_t)nub;bool nubAttached=false,nubStarted=false,attached=false,started=false;
        IORegistryEntry *displayTreeParent=nullptr;bool dtAttached=false;
        IOService *parent=IOService::getResourceService();
        if(!activeKeyStore && parent) {
            IOService *aks=native_keyevents_allocate();
            if(aks && aks->init() && aks->attach(parent) && aks->start(parent)) {
                activeKeyStore=aks;dline(100,"AKS UP");
            } else if(aks) {
                aks->release();
            }
        }
        IODeviceMemory *memory=IODeviceMemory::withRange(video.v_baseAddr,video.v_rowBytes*video.v_height);
        OSArray *ranges=OSArray::withCapacity(1);
        if(nub && memory && ranges && parent && nub->init()) {
            ranges->setObject(memory);nub->setDeviceMemory(ranges);nub->setName("NativeGOPFramebuffer");
            nubAttached=nub->attach(parent);if(nubAttached)nubStarted=nub->start(parent);dline(25,"DISP NUB A%d S%d",nubAttached,nubStarted);
            if(nubStarted) {
                unsigned phase=0;PE_parse_boot_argn("offline-legacy-phase",&phase,sizeof(phase));
                if(phase==4) {
                    displayTreeParent=IORegistryEntry::fromPath("/arm-io",gIODTPlane);
                    const char type[]="display-subsystem";OSData *data=OSData::withBytes(type,sizeof(type));
                    if(displayTreeParent && data && nub->setProperty("device_type",data)) {
                        nub->setName("gop-display",gIODTPlane);dtAttached=nub->attachToParent(displayTreeParent,gIODTPlane);
                    }
                    OSSafeReleaseNULL(data);native_legacy_dt_attached=dtAttached;
                }
                if(phase==3) {
                    IOService *presenter=native_gop_presenter_create();bool presenterAttached=false;
                    if(presenter && presenter->init() && (presenterAttached=presenter->attach(nub)) && presenter->start(nub)) {
                        activeGOPPresenter=presenter;activeGOPNub=nub;nub=nullptr;nubAttached=false;nubStarted=false;
                    } else {if(presenterAttached)presenter->detach(nub);OSSafeReleaseNULL(presenter);}
                } else {
                    SInt32 score=0;native_legacy_probe_match=(phase!=4 || dtAttached) && display->probe(nub,&score)==display;
                    if(native_legacy_probe_match)attached=display->attach(nub);
                    if(attached){native_legacy_stage=5;started=display->start(nub);native_legacy_start_result=started;dline(26,"DISP P%d A%d S%d",native_legacy_probe_match,attached,started);}
                    if(started && phase==4) {
                        activeLegacyDisplay=display;activeGOPNub=nub;native_legacy_stage=10;dline(31,"DISP UP");
                        merlinxFixupMtkOvl(false);
                        unsigned gopTest=0;PE_parse_boot_argn("merlinx-gop-test",&gopTest,sizeof(gopTest));
                        if(gopTest==1)gopPaint("TEST", false);
                        {
                            unsigned gpu=0;PE_parse_boot_argn("sheng-fb-gpu",&gpu,sizeof gpu);
                            if(gpu==1) {
                                IOService *handoff=native_gop_presenter_create();
                                IOService *home=IOService::getResourceService();
                                bool handoffAttached=false;
                                if(handoff && home)native_gop_presenter_set_source(handoff,nub);
                                if(handoff && home && handoff->init() && (handoffAttached=handoff->attach(home)) && handoff->start(home))gpuHandoffPresenter=handoff;
                                else {if(handoffAttached)handoff->detach(home);OSSafeReleaseNULL(handoff);}
                            }
                        }
                        if (!gTouchWorkerStarted) {
                            gTouchWorkerStarted = true;
                            thread_t touchThread = THREAD_NULL;
                            if (kernel_thread_start(merlinxStartHidAndTouchThread, nullptr, &touchThread) == KERN_SUCCESS) {
                                thread_deallocate(touchThread);
                            }
                        }
                        finishUnitScalePolicy(true);
                        OSSafeReleaseNULL(displayTreeParent);
                        native_legacy_display_checkpoint();return;
                    }
                }
            }
        }
        native_legacy_stage=6;dline(31,"DISP FAIL");finishUnitScalePolicy(false);
        if(started)display->stop(nub);if(attached)display->detach(nub);
        if(dtAttached)nub->detachFromParent(displayTreeParent,gIODTPlane);
        if(nubStarted)nub->stop(parent);if(nubAttached)nub->detach(parent);
        OSSafeReleaseNULL(ranges);OSSafeReleaseNULL(memory);OSSafeReleaseNULL(nub);
        OSSafeReleaseNULL(displayTreeParent);
    }
    native_legacy_stage=7;gopPaint("FALLBACK", true);display->release();native_legacy_released=1;native_legacy_stage=8;
    native_legacy_display_checkpoint();
}

// Interactive USB diagnostic commands
void merlinx_usb_cmd_disp(void) {
    gInUsbCmd = 1;
    merlinxFixupMtkOvl(false);
    volatile uint8_t *ovl0 = gDispsysBase ? (gDispsysBase + kMtkOvl0Off) : nullptr;
    volatile uint8_t *ovl02l = gDispsysBase ? (gDispsysBase + kMtkOvl02LOff) : nullptr;
    merlinx_usb_log_line(
        "[DISP] gop_pa=0x%llx gop_va=0x%llx %ux%u retired=%u accepted=%u rejected=%u copy_n=%llu\n",
        (unsigned long long)native_legacy_gop_physical,
        (unsigned long long)native_legacy_gop_virtual,
        native_legacy_gop_width,
        native_legacy_gop_height,
        native_legacy_retired_frames,
        native_legacy_swap_accepted_count,
        native_legacy_swap_rejected_count,
        (unsigned long long)native_legacy_copy_n);
    if (ovl0 && ovl02l) {
        merlinx_usb_log_line(
            "[DISP] OVL0(before->now): SRC=0x%x->0x%x L0_CON=0x%x->0x%x L0_ADDR=0x%08x->0x%08x L0_OFF=0x%x->0x%x L1_ADDR=0x%08x->0x%08x\n",
            gOvl0BeforeSrcCon, mmioRead32(ovl0, kOvlSrcCon),
            gOvl0BeforeL0Con, mmioRead32(ovl0, kOvlL0Con),
            gOvl0BeforeL0Addr, mmioRead32(ovl0, kOvlL0Addr),
            gOvl0BeforeL0Offset, mmioRead32(ovl0, kOvlL0Offset),
            gOvl0BeforeL1Addr, mmioRead32(ovl0, kOvlL1Addr));
        merlinx_usb_log_line(
            "[DISP] OVL0_2L(before->now): SRC=0x%x->0x%x L0_CON=0x%x->0x%x L0_ADDR=0x%08x->0x%08x L1_ADDR=0x%08x->0x%08x\n",
            gOvl02LBeforeSrcCon, mmioRead32(ovl02l, kOvlSrcCon),
            gOvl02LBeforeL0Con, mmioRead32(ovl02l, kOvlL0Con),
            gOvl02LBeforeL0Addr, mmioRead32(ovl02l, kOvlL0Addr),
            gOvl02LBeforeL1Addr, mmioRead32(ovl02l, kOvlL1Addr));
    }
    gInUsbCmd = 0;
}

void merlinx_usb_cmd_touch(void) {
    gInUsbCmd = 1;
    uint32_t started = 0, dispatched = 0, failed = 0;
    int32_t lastKr = 0;
    uint16_t lastX = 0, lastY = 0;
    uint8_t lastContacts = 0, lastFlags = 0;
    merlinx_hid_get_stats(&started, &dispatched, &failed, &lastKr, &lastX, &lastY, &lastContacts, &lastFlags);
    merlinx_usb_log_line(
        "[TOUCH] probe_rc=%d spi_bitbang=%u spi_xfers=%u spi_timeouts=%u irq_gpio1=%d rst_gpio92=%d\n",
        gFtsProbeResult, gSpiUseBitbang, gSpiTransferCalls, gSpiHwTimeouts,
        mtkGpioGetIn(kGpioTouchIrq), mtkGpioGetIn(kGpioTouchRst));
    for (unsigned i = 0; i < 4; ++i) {
        if (fts_touch_rows[i][0]) {
            merlinx_usb_log_line("[TOUCH] %s\n", fts_touch_rows[i]);
        }
    }
    merlinx_usb_log_line(
        "[TOUCH] hid_started=%u rpt_ok=%u rpt_fail=%u last_kr=0x%x contacts=%u flags=0x%02x last_xy=(%u,%u)\n",
        started, dispatched, failed, static_cast<unsigned>(lastKr),
        lastContacts, lastFlags, lastX, lastY);
    gInUsbCmd = 0;
}

void merlinx_usb_cmd_tap(uint32_t x, uint32_t y) {
    gInUsbCmd = 1;
    merlinx_usb_log_line("[TOUCH] Injecting tap (%u, %u)\n", x, y);
    merlinx_hid_inject_tap(static_cast<uint16_t>(x), static_cast<uint16_t>(y));
    gInUsbCmd = 0;
}

void merlinx_usb_cmd_swipe(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) {
    gInUsbCmd = 1;
    merlinx_usb_log_line("[TOUCH] Injecting swipe (%u,%u)->(%u,%u)\n", x1, y1, x2, y2);
    merlinx_hid_inject_swipe(
        static_cast<uint16_t>(x1), static_cast<uint16_t>(y1),
        static_cast<uint16_t>(x2), static_cast<uint16_t>(y2), 16);
    gInUsbCmd = 0;
}

void merlinx_usb_cmd_home(void) {
    gInUsbCmd = 1;
    merlinx_usb_log_line("[TOUCH] Injecting Home button + bottom-edge unlock swipe\n");
    __atomic_store_n(&gHidInputCommand, 1U, __ATOMIC_RELEASE);
    merlinx_hid_inject_swipe(540, 2336, 540, 1100, 16);
    gInUsbCmd = 0;
}

void merlinx_usb_cmd_cpufreq(void) {
    gInUsbCmd = 1;
    merlinxBoostCpuClocks();
    merlinx_usb_log_line(
        "[CPUFREQ] boost=%u ARMPLL_LL(A55)=0x%08x/0x%08x ARMPLL_L(A75)=0x%08x/0x%08x\n",
        gCpuBoostApplied, gArmpllLlCon0, gArmpllLlCon1, gArmpllLCon0, gArmpllLCon1);
}
}
