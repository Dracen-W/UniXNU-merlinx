// SPDX-License-Identifier: BSD-2-Clause-Patent
// Merlinx IOHIDDevice bridge for FocalTech FT8719 touch and synthetic HID events.
#include "OriginalHidDeviceABI.h"
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSIterator.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSString.h>
#include <pexpert/pexpert.h>
#include <string.h>

extern "C" {
#include "FtsHid.h"
#include "FtsTouchCore.h"
void native_hid_input_observed(unsigned stage, int status);
void native_hid_live_checkpoint(IOService *service);
bool native_hid_input_requested(void);
void merlinx_usb_log_line(const char *fmt, ...);
}

// Report 1: standard boot keyboard report (8 bytes payload).
// Report 2: Consumer Menu / Home button (1 byte payload).
// Keyboard Application collection first ensures Apple's generic IOHIDEventDriver
// personality matches on iPadOS 17.7.11 when HIDDefaultBehavior=true.
static const unsigned char kKeyboardDescriptor[] = {
    0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x75, 0x08, 0x95, 0x01, 0x81, 0x03,
    0x19, 0x00, 0x29, 0x65, 0x15, 0x00, 0x25, 0x65,
    0x75, 0x08, 0x95, 0x06, 0x81, 0x00,
    0x85, 0x02, 0x05, 0x0c, 0x09, 0x40, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x01, 0x81, 0x02,
    0x75, 0x07, 0x95, 0x01, 0x81, 0x03, 0xc0
};

// Report 3: ten-finger Touch Screen Digitizer collection:
// Tip Switch (bit 0), Confidence/Untouch (bit 1), 6-bit pad, Contact ID (8-bit),
// X (16-bit, 0..32767), Y (16-bit, 0..32767) per finger, followed by Contact Count (8-bit).
#define MERLINX_FINGER_DESC \
    0x09, 0x22, 0xa1, 0x02, \
    0x09, 0x42, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01, 0x81, 0x02, \
    0x09, 0x47, 0x81, 0x02, \
    0x75, 0x06, 0x95, 0x01, 0x81, 0x03, \
    0x09, 0x51, 0x75, 0x08, 0x95, 0x01, 0x15, 0x00, 0x26, 0xff, 0x00, 0x81, 0x02, \
    0x05, 0x01, 0x09, 0x30, 0x15, 0x00, 0x26, 0xff, 0x7f, 0x75, 0x10, 0x95, 0x01, 0x81, 0x02, \
    0x09, 0x31, 0x81, 0x02, \
    0x05, 0x0d, 0xc0,

static const unsigned char kTouchDescriptor[] = {
    0x05, 0x0d, 0x09, 0x04, 0xa1, 0x01, 0x85, 0x03,
    MERLINX_FINGER_DESC MERLINX_FINGER_DESC MERLINX_FINGER_DESC MERLINX_FINGER_DESC MERLINX_FINGER_DESC
    MERLINX_FINGER_DESC MERLINX_FINGER_DESC MERLINX_FINGER_DESC MERLINX_FINGER_DESC MERLINX_FINGER_DESC
    0x09, 0x54, 0x75, 0x08, 0x95, 0x01, 0x15, 0x00, 0x26, 0xff, 0x00, 0x81, 0x02, 0xc0
};

#define TOUCH_FINGERS 10

namespace {
static volatile uint32_t gHidStarted = 0;
static volatile uint32_t gHidDispatched = 0;
static volatile uint32_t gHidFailed = 0;
static volatile int32_t gHidLastKr = 0;
static volatile uint16_t gHidLastX = 0;
static volatile uint16_t gHidLastY = 0;
static volatile uint8_t gHidLastContacts = 0;
static volatile uint8_t gHidLastFlags = 0;
} // namespace

class NativeHidInput : public IOHIDDevice {
    OSDeclareDefaultStructors(NativeHidInput)

    IOWorkLoop *inputLoop = nullptr;
    IOTimerEventSource *timer = nullptr;
    IOTimerEventSource *touchTimer = nullptr;
    bool down = false;
    bool touchEnabled = true;
    bool touchOnly = false;
    bool eventDriverFound = false;
    uint32_t lastSeq = 0;
    int prevIds[TOUCH_FINGERS] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    int prevX[TOUCH_FINGERS] = {};
    int prevY[TOUCH_FINGERS] = {};

    static unsigned transformCoordinate(int value, int maximum, bool clampBottomEdge) {
        if (maximum <= 1) {
            return 0;
        }
        if (value < 0) {
            value = 0;
        }
        if (value >= maximum) {
            value = maximum - 1;
        }
        if (clampBottomEdge && value >= maximum - 14) {
            return 32767U;
        }
        return static_cast<unsigned>((static_cast<long long>(value) * 32767LL) / (maximum - 1));
    }

    static bool hasEventDriver(IORegistryEntry *parent, unsigned depth) {
        if (!depth || !parent) {
            return false;
        }
        OSIterator *it = parent->getChildIterator(gIOServicePlane);
        if (!it) {
            return false;
        }
        bool found = false;
        while (OSObject *child = it->getNextObject()) {
            IORegistryEntry *entry = OSDynamicCast(IORegistryEntry, child);
            if (entry && (entry->metaCast("IOHIDEventDriver") || hasEventDriver(entry, depth - 1))) {
                found = true;
                break;
            }
        }
        it->release();
        return found;
    }

    bool hasEventDriverCached() {
        if (!eventDriverFound) {
            eventDriverFound = hasEventDriver(this, 4);
            if (eventDriverFound) {
                native_hid_live_checkpoint(this);
            }
        }
        return eventDriverFound;
    }

    static void onFtsSink(void *context) {
        NativeHidInput *self = static_cast<NativeHidInput *>(context);
        if (self && self->touchTimer) {
            self->touchTimer->setTimeoutUS(100);
        }
    }

    static void touchTick(OSObject *owner, IOTimerEventSource *source) {
        NativeHidInput *self = OSDynamicCast(NativeHidInput, owner);
        if (!self) {
            return;
        }
        source->setTimeoutMS(8);
        if (!self->hasEventDriverCached()) {
            return;
        }

        uint32_t seqNow = __atomic_load_n(&fts_touch_seq, __ATOMIC_ACQUIRE);
        if ((seqNow & 1U) != 0 || seqNow == self->lastSeq) {
            return;
        }

        struct fts_hid_state state;
        memset(&state, 0, sizeof(state));
        if (fts_hid_read(&state) != FTS_OK) {
            return;
        }
        self->lastSeq = __atomic_load_n(&fts_touch_seq, __ATOMIC_ACQUIRE);

        int xMax = state.x_max ? static_cast<int>(state.x_max) : 1080;
        int yMax = state.y_max ? static_cast<int>(state.y_max) : 2340;
        unsigned count = state.count;
        if (count > TOUCH_FINGERS) {
            count = TOUCH_FINGERS;
        }

        unsigned char report[1 + TOUCH_FINGERS * 6 + 1];
        memset(report, 0, sizeof(report));
        report[0] = 3;

        int nextPrevIds[TOUCH_FINGERS];
        int nextPrevX[TOUCH_FINGERS];
        int nextPrevY[TOUCH_FINGERS];
        for (unsigned i = 0; i < TOUCH_FINGERS; ++i) {
            nextPrevIds[i] = -1;
            nextPrevX[i] = 0;
            nextPrevY[i] = 0;
        }

        unsigned slot = 0;
        unsigned activeCount = 0;
        uint16_t seenMask = 0;

        for (unsigned i = 0; i < count && slot < TOUCH_FINGERS; ++i) {
            int rawId = static_cast<int>(state.points[i].id);
            int contactId = (rawId + 1) & 0xff;
            if (contactId == 0) {
                contactId = 1;
            }
            bool tipDown = (state.points[i].tip != 0);
            unsigned x = transformCoordinate(state.points[i].x, xMax, false);
            unsigned y = transformCoordinate(state.points[i].y, yMax, true);

            unsigned char *f = report + 1 + slot * 6;
            f[0] = tipDown ? 1U : 2U;
            f[1] = static_cast<unsigned char>(contactId);
            f[2] = static_cast<unsigned char>(x & 0xffU);
            f[3] = static_cast<unsigned char>((x >> 8) & 0xffU);
            f[4] = static_cast<unsigned char>(y & 0xffU);
            f[5] = static_cast<unsigned char>((y >> 8) & 0xffU);

            if (slot == 0) {
                gHidLastFlags = f[0];
                gHidLastX = static_cast<uint16_t>(state.points[i].x);
                gHidLastY = static_cast<uint16_t>(state.points[i].y);
            }
            if (rawId >= 0 && rawId < 16) {
                seenMask |= static_cast<uint16_t>(1U << rawId);
            }
            if (tipDown) {
                nextPrevIds[activeCount] = rawId;
                nextPrevX[activeCount] = static_cast<int>(x);
                nextPrevY[activeCount] = static_cast<int>(y);
                activeCount++;
            }
            slot++;
        }

        // Any finger that was down in the previous report and was not included in state.points
        // gets an explicit tip-up (f[0]=2) entry at its last position.
        for (unsigned j = 0; j < TOUCH_FINGERS && slot < TOUCH_FINGERS; ++j) {
            int oldId = self->prevIds[j];
            if (oldId < 0) {
                continue;
            }
            if (oldId < 16 && (seenMask & static_cast<uint16_t>(1U << oldId)) != 0) {
                continue;
            }
            int contactId = (oldId + 1) & 0xff;
            if (contactId == 0) {
                contactId = 1;
            }
            unsigned char *f = report + 1 + slot * 6;
            f[0] = 2U;
            f[1] = static_cast<unsigned char>(contactId);
            f[2] = static_cast<unsigned char>(self->prevX[j] & 0xff);
            f[3] = static_cast<unsigned char>((self->prevX[j] >> 8) & 0xff);
            f[4] = static_cast<unsigned char>(self->prevY[j] & 0xff);
            f[5] = static_cast<unsigned char>((self->prevY[j] >> 8) & 0xff);
            slot++;
        }

        for (unsigned i = 0; i < TOUCH_FINGERS; ++i) {
            self->prevIds[i] = nextPrevIds[i];
            self->prevX[i] = nextPrevX[i];
            self->prevY[i] = nextPrevY[i];
        }

        if (!slot) {
            return;
        }
        report[1 + TOUCH_FINGERS * 6] = static_cast<unsigned char>(slot);
        gHidLastContacts = static_cast<uint8_t>(activeCount);

        IOBufferMemoryDescriptor *buffer = IOBufferMemoryDescriptor::withBytes(
            report, sizeof(report), kIODirectionNone);
        IOReturn result = buffer ? self->handleReport(buffer, 0, 0) : kIOReturnNoMemory;
        OSSafeReleaseNULL(buffer);

        gHidLastKr = result;
        if (result == kIOReturnSuccess) {
            uint32_t n = __atomic_fetch_add(&gHidDispatched, 1U, __ATOMIC_RELAXED) + 1U;
            if (n <= 10 || activeCount == 0 || (n % 30U) == 0) {
                merlinx_usb_log_line(
                    "[FTS-HID] rpt#%u kr=0x%x active=%u slots=%u flags=0x%02x xy=(%u,%u)\n",
                    n,
                    static_cast<unsigned>(result),
                    activeCount,
                    slot,
                    static_cast<unsigned>(gHidLastFlags),
                    static_cast<unsigned>(gHidLastX),
                    static_cast<unsigned>(gHidLastY));
            }
        } else {
            __atomic_fetch_add(&gHidFailed, 1U, __ATOMIC_RELAXED);
        }
        native_hid_input_observed(6, result);
    }

    static void tick(OSObject *owner, IOTimerEventSource *source) {
        NativeHidInput *self = OSDynamicCast(NativeHidInput, owner);
        if (!self) {
            return;
        }
        native_hid_live_checkpoint(self);
        if (self->touchOnly) {
            source->setTimeoutMS(1000);
            return;
        }
        if (!self->down && !hasEventDriver(self, 4)) {
            native_hid_input_observed(3, 0);
            source->setTimeoutMS(1000);
            return;
        }
        native_hid_input_observed(3, 1);
        if (!self->down && !native_hid_input_requested()) {
            source->setTimeoutMS(1000);
            return;
        }
        unsigned char report[] = {2, static_cast<unsigned char>(self->down ? 0 : 1)};
        IOBufferMemoryDescriptor *buffer = IOBufferMemoryDescriptor::withBytes(
            report, sizeof(report), kIODirectionNone);
        IOReturn result = buffer ? self->handleReport(buffer, 0, 0) : kIOReturnNoMemory;
        OSSafeReleaseNULL(buffer);
        native_hid_input_observed(self->down ? 5 : 4, result);
        if (!self->down) {
            self->down = true;
            source->setTimeoutMS(100);
        } else {
            self->down = false;
            source->setTimeoutMS(1000);
        }
    }

public:
    void setTouchOnly() { touchOnly = true; }
    IOWorkLoop *getWorkLoop() const override { return inputLoop; }
    OSString *newTransportString() const override { return OSString::withCString(touchOnly ? "SPI" : "Virtual"); }
    OSString *newProductString() const override { return OSString::withCString(touchOnly ? "FocalTech FT8719 Touch Digitizer" : "Merlinx HID Touch Digitizer"); }
    IOReturn newReportDescriptor(IOMemoryDescriptor **out) const override {
        if (!out) {
            return kIOReturnBadArgument;
        }
        unsigned char combined[sizeof(kKeyboardDescriptor) + sizeof(kTouchDescriptor)];
        size_t size = 0;
        if (touchOnly) {
            size = sizeof(kTouchDescriptor);
            memcpy(combined, kTouchDescriptor, size);
        } else {
            size = sizeof(kKeyboardDescriptor);
            memcpy(combined, kKeyboardDescriptor, size);
            if (touchEnabled) {
                memcpy(combined + size, kTouchDescriptor, sizeof(kTouchDescriptor));
                size += sizeof(kTouchDescriptor);
            }
        }
        *out = IOBufferMemoryDescriptor::withBytes(combined, size, kIODirectionNone);
        return *out ? kIOReturnSuccess : kIOReturnNoMemory;
    }
    bool start(IOService *provider) override {
        inputLoop = IOWorkLoop::workLoop();
        if (!inputLoop) {
            return false;
        }
        if (!setProperty("HIDDefaultBehavior", true)) {
            return false;
        }
        unsigned touch = 1;
        PE_parse_boot_argn("sheng-touch-hid", &touch, sizeof(touch));
        touchEnabled = (touch != 0);
        if (touchOnly || touchEnabled) {
            setProperty("Built-In", true);
            setProperty("DisplayIntegrated", true);
        }
        if (!IOHIDDevice::start(provider)) {
            return false;
        }
        timer = IOTimerEventSource::timerEventSource(this, tick);
        if (!timer || inputLoop->addEventSource(timer) != kIOReturnSuccess) {
            OSSafeReleaseNULL(timer);
            IOHIDDevice::stop(provider);
            return false;
        }
        if (touchOnly || touchEnabled) {
            touchTimer = IOTimerEventSource::timerEventSource(this, touchTick);
            if (touchTimer && inputLoop->addEventSource(touchTimer) == kIOReturnSuccess) {
                touchTimer->setTimeoutMS(50);
                fts_hid_set_sink(&NativeHidInput::onFtsSink, this);
            } else {
                OSSafeReleaseNULL(touchTimer);
            }
        }
        setProperty("OfflineInputProbe", true);
        gHidStarted = 1;
        native_hid_input_observed(2, 0);
        timer->setTimeoutMS(500);
        merlinx_usb_log_line("[FTS-HID] NativeHidInput started (this=%p provider=%p touchOnly=%d)\n",
            this, provider, touchOnly ? 1 : 0);
        return true;
    }
    void stop(IOService *provider) override {
        fts_hid_set_sink(nullptr, nullptr);
        if (touchTimer) {
            touchTimer->cancelTimeout();
            inputLoop->removeEventSource(touchTimer);
            OSSafeReleaseNULL(touchTimer);
        }
        if (timer) {
            timer->cancelTimeout();
            inputLoop->removeEventSource(timer);
            OSSafeReleaseNULL(timer);
        }
        gHidStarted = 0;
        IOHIDDevice::stop(provider);
    }
    void free() override {
        OSSafeReleaseNULL(inputLoop);
        IOHIDDevice::free();
    }
};

OSDefineMetaClassAndStructors(NativeHidInput, IOHIDDevice)

extern "C" IOService *native_hid_input_allocate(void) {
    return new NativeHidInput;
}

extern "C" IOService *native_hid_touch_allocate(void) {
    NativeHidInput *device = new NativeHidInput;
    if (device) {
        device->setTouchOnly();
    }
    return device;
}

extern "C" void merlinx_hid_inject_touch_frame(uint16_t x, uint16_t y, bool tipDown) {
    struct fts_core_report rep;
    memset(&rep, 0, sizeof(rep));
    rep.x_max = 1080;
    rep.y_max = 2340;
    rep.count = 1;
    rep.active = tipDown ? 1U : 0U;
    rep.down = tipDown ? 1U : 0U;
    rep.up = tipDown ? 0U : 1U;
    rep.points[0].id = 0;
    rep.points[0].active = tipDown ? 1U : 0U;
    rep.points[0].x = (x < 1080U) ? x : 1079U;
    rep.points[0].y = (y < 2340U) ? y : 2339U;
    rep.points[0].pressure = tipDown ? 0x30U : 0U;
    rep.points[0].area = tipDown ? 0x08U : 0U;
    fts_hid_publish(&rep);
}

extern "C" void merlinx_hid_inject_tap(uint16_t x, uint16_t y) {
    merlinx_hid_inject_touch_frame(x, y, true);
    IOSleep(45);
    merlinx_hid_inject_touch_frame(x, y, false);
}

extern "C" void merlinx_hid_inject_swipe(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t steps) {
    if (steps < 2) {
        steps = 14;
    }
    if (steps > 60) {
        steps = 60;
    }
    for (uint32_t i = 0; i <= steps; ++i) {
        int32_t cx = static_cast<int32_t>(x1) +
            ((static_cast<int32_t>(x2) - static_cast<int32_t>(x1)) * static_cast<int32_t>(i)) /
            static_cast<int32_t>(steps);
        int32_t cy = static_cast<int32_t>(y1) +
            ((static_cast<int32_t>(y2) - static_cast<int32_t>(y1)) * static_cast<int32_t>(i)) /
            static_cast<int32_t>(steps);
        merlinx_hid_inject_touch_frame(
            static_cast<uint16_t>(cx < 0 ? 0 : cx),
            static_cast<uint16_t>(cy < 0 ? 0 : cy),
            true);
        IOSleep(12);
    }
    merlinx_hid_inject_touch_frame(x2, y2, false);
}

extern "C" void merlinx_hid_get_stats(
    uint32_t *outStarted,
    uint32_t *outDispatched,
    uint32_t *outFailed,
    int32_t *outLastKr,
    uint16_t *outLastX,
    uint16_t *outLastY,
    uint8_t *outLastContacts,
    uint8_t *outLastFlags)
{
    if (outStarted) *outStarted = gHidStarted;
    if (outDispatched) *outDispatched = gHidDispatched;
    if (outFailed) *outFailed = gHidFailed;
    if (outLastKr) *outLastKr = gHidLastKr;
    if (outLastX) *outLastX = gHidLastX;
    if (outLastY) *outLastY = gHidLastY;
    if (outLastContacts) *outLastContacts = gHidLastContacts;
    if (outLastFlags) *outLastFlags = gHidLastFlags;
}
