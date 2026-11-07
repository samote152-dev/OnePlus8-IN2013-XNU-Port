// SPDX-License-Identifier: BSD-2-Clause-Patent
#include "NativeGeniI2c.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <kern/clock.h>
#include <mach/mach_time.h>

/* NativeGeniI2c owns one GENI engine and the lock shared by its I2C clients.
 * Each message starts one firmware command, fills or drains packed FIFO words,
 * and waits for command completion. The lock remains held across a write and
 * repeated-start read. Error recovery cancels, then aborts if necessary, before
 * a later caller may issue another command.
 */
struct NativeGeniI2c {
    NativeGeniI2cConfig config;
    IOMemoryMap *engineMap;
    IOMemoryMap *wrapperMap;
    uintptr_t base;
    IOLock *lock;
    uint32_t firmware, interfaceDisable, wrapperVersion;
    unsigned txDepth, rxDepth;
    uint32_t transactions, errors, lastIrq, lastStatus;
    bool offline;
    bool gpio;
};

namespace {
enum : uint32_t {
    kForceDefault = 0x20,
    kOutputControl = 0x24,
    kClockGating = 0x28,
    kStatus = 0x40,
    kSerialClock = 0x48,
    kInterfaceDisable = 0x64,
    kFirmware = 0x68,
    kClockSelect = 0x7c,
    kByteGranularity = 0x254,
    kDmaMode = 0x258,
    kTxPacking0 = 0x260,
    kTxPacking1 = 0x264,
    kTxLength = 0x26c,
    kRxLength = 0x270,
    kSclCounters = 0x278,
    kRxPacking0 = 0x284,
    kRxPacking1 = 0x288,
    kCommand = 0x600,
    kCommandControl = 0x604,
    kMasterIrq = 0x610,
    kMasterIrqEnable = 0x614,
    kMasterIrqClear = 0x618,
    kSlaveIrqEnable = 0x644,
    kSlaveIrqClear = 0x648,
    kTxFifo = 0x700,
    kRxFifo = 0x780,
    kTxFifoStatus = 0x800,
    kRxFifoStatus = 0x804,
    kTxWatermark = 0x80c,
    kRxWatermark = 0x810,
    kRxFlowWatermark = 0x814,
    kDmaTxIrqClear = 0xc44,
    kDmaRxIrqClear = 0xd44,
    kGsiEvents = 0xe18,
    kIrqEnable = 0xe1c,
    kHardware0 = 0xe24,
    kHardware1 = 0xe28,
    kDmaGeneral = 0xe30,
    kDone = 1u << 0,
    kOverrun = 1u << 1,
    kIllegal = 1u << 2,
    kFailure = 1u << 3,
    kCancelDone = 1u << 4,
    kAbortDone = 1u << 5,
    kNack = 1u << 10,
    kBusProtocol = 1u << 12,
    kArbitrationLost = 1u << 13,
    kFifoErrors = (3u << 24) | (3u << 28),
    kRxWatermarkIrq = 1u << 26,
    kRxLastIrq = 1u << 27,
    kTxWatermarkIrq = 1u << 30,
    kErrors = kOverrun | kIllegal | kFailure | kNack | kBusProtocol | kArbitrationLost | kFifoErrors,
};

static uint32_t read(NativeGeniI2c *bus, unsigned offset)
{
    uint32_t value = *reinterpret_cast<volatile uint32_t *>(bus->base + offset);
    __asm__ volatile("dmb oshld" ::: "memory");
    return value;
}

static void write(NativeGeniI2c *bus, unsigned offset, uint32_t value)
{
    *reinterpret_cast<volatile uint32_t *>(bus->base + offset) = value;
    __asm__ volatile("dsb sy" ::: "memory");
}

static uint64_t nowUs()
{
    uint64_t nanoseconds = 0;
    absolutetime_to_nanoseconds(mach_absolute_time(), &nanoseconds);
    return nanoseconds / 1000;
}

static void drive(NativeGeniI2c *bus, unsigned line, bool low)
{
    bus->config.gpioDriveLow(bus->config.gpioContext, line, low);
}

static int level(NativeGeniI2c *bus, unsigned line)
{
    return bus->config.gpioRead(bus->config.gpioContext, line);
}

static int releaseClock(NativeGeniI2c *bus)
{
    drive(bus, 1, false);
    uint64_t deadline = nowUs() + bus->config.timeoutUs;
    do {
        int value = level(bus, 1);
        if (value < 0) {
            return value;
        }
        if (value) {
            IODelay(5);
            return 0;
        }
        IODelay(2);
    } while (nowUs() < deadline);
    return -60;
}

static int gpioStop(NativeGeniI2c *bus)
{
    drive(bus, 1, true);
    drive(bus, 0, true);
    IODelay(5);
    int result = releaseClock(bus);
    drive(bus, 0, false);
    IODelay(5);
    if (!result && level(bus, 0) != 1) {
        result = -16;
    }
    return result;
}

static int gpioStart(NativeGeniI2c *bus)
{
    drive(bus, 0, false);
    IODelay(5);
    int result = releaseClock(bus);
    if (result) {
        return result;
    }
    if (level(bus, 0) != 1) {
        return -16;
    }
    drive(bus, 0, true);
    IODelay(5);
    drive(bus, 1, true);
    IODelay(5);
    return 0;
}

static int gpioWriteByte(NativeGeniI2c *bus, uint8_t byte)
{
    for (unsigned bit = 0; bit < 8; ++bit) {
        drive(bus, 0, (byte & 0x80) == 0);
        IODelay(5);
        int result = releaseClock(bus);
        if (result) {
            return result;
        }
        if ((byte & 0x80) && level(bus, 0) != 1) {
            drive(bus, 0, false);
            drive(bus, 1, false);
            return -35;
        }
        drive(bus, 1, true);
        byte <<= 1;
    }
    drive(bus, 0, false);
    IODelay(5);
    int result = releaseClock(bus);
    int ack = result ? -1 : level(bus, 0);
    drive(bus, 1, true);
    IODelay(5);
    return result ? result : (ack == 0 ? 0 : -6);
}

static int gpioReadByte(NativeGeniI2c *bus, uint8_t *byte, bool last)
{
    uint8_t value = 0;
    drive(bus, 0, false);
    for (unsigned bit = 0; bit < 8; ++bit) {
        IODelay(5);
        int result = releaseClock(bus);
        if (result) {
            return result;
        }
        int input = level(bus, 0);
        if (input < 0) {
            return input;
        }
        value = (value << 1) | (input ? 1 : 0);
        drive(bus, 1, true);
    }
    drive(bus, 0, !last);
    IODelay(5);
    int result = releaseClock(bus);
    drive(bus, 1, true);
    drive(bus, 0, false);
    *byte = value;
    return result;
}

static int gpioTransaction(NativeGeniI2c *bus, uint8_t address,
    const uint8_t *tx, size_t txCount, uint8_t *rx, size_t rxCount)
{
    int result = gpioStart(bus);
    if (!result && txCount) {
        result = gpioWriteByte(bus, address << 1);
        for (size_t index = 0; !result && index < txCount; ++index) {
            result = gpioWriteByte(bus, tx[index]);
        }
    }
    if (!result && rxCount) {
        if (txCount) {
            result = gpioStart(bus);
        }
        if (!result) {
            result = gpioWriteByte(bus, (address << 1) | 1);
        }
        for (size_t index = 0; !result && index < rxCount; ++index) {
            result = gpioReadByte(bus, &rx[index], index + 1 == rxCount);
        }
    }
    if (result != -35) {
        int stopped = gpioStop(bus);
        if (!result) {
            result = stopped;
        }
    }
    ++bus->transactions;
    if (result) {
        ++bus->errors;
        printf("NativeGeniI2c: GPIO address=%02x write=%lu read=%lu result=%d\n",
            address, (unsigned long)txCount, (unsigned long)rxCount, result);
    }
    return result;
}

static int prepareGpio(NativeGeniI2c *bus)
{
    if (!bus->config.gpioPrepare || !bus->config.gpioDriveLow || !bus->config.gpioRead) {
        return -95;
    }
    int result = bus->config.gpioPrepare(bus->config.gpioContext);
    if (result) {
        return result;
    }
    drive(bus, 0, false);
    result = releaseClock(bus);
    // Nine clock pulses release a slave left partway through an earlier byte.
    for (unsigned pulse = 0; !result && !level(bus, 0) && pulse < 9; ++pulse) {
        drive(bus, 1, true);
        IODelay(5);
        result = releaseClock(bus);
    }
    if (!result) {
        result = gpioStop(bus);
    }
    if (!result) {
        bus->gpio = true;
        printf("NativeGeniI2c: board open-drain pins selected, maximum 100000 Hz\n");
    }
    return result;
}

static IOMemoryMap *map(uint64_t address, uint64_t length)
{
    IODeviceMemory *memory = IODeviceMemory::withRange(address, length);
    IOMemoryMap *mapping = memory ? memory->map(kIOMapAnywhere | kIOMapInhibitCache) : nullptr;
    OSSafeReleaseNULL(memory);
    return mapping;
}

static void clearStatus(NativeGeniI2c *bus)
{
    write(bus, kMasterIrqClear, 0xffffffff);
    write(bus, kSlaveIrqClear, 0xffffffff);
    write(bus, kDmaTxIrqClear, 0xffffffff);
    write(bus, kDmaRxIrqClear, 0xffffffff);
}

static bool stopCommand(NativeGeniI2c *bus, uint32_t control, uint32_t completed)
{
    write(bus, kCommandControl, control);
    uint64_t deadline = nowUs() + 10000;
    do {
        uint32_t status = read(bus, kMasterIrq);
        if ((status & completed) && !(read(bus, kStatus) & 1)) {
            write(bus, kMasterIrqClear, status);
            return true;
        }
        IODelay(2);
    } while (nowUs() < deadline);
    return false;
}

static bool recover(NativeGeniI2c *bus)
{
    write(bus, kTxWatermark, 0);
    bool stopped = stopCommand(bus, 4, kCancelDone);
    if (!stopped) {
        stopped = stopCommand(bus, 2, kAbortDone);
    }
    if (stopped) {
        unsigned words = read(bus, kRxFifoStatus) & 0x1ffffff;
        for (unsigned index = 0; index < words && index < bus->rxDepth; ++index) {
            (void)read(bus, kRxFifo);
        }
        write(bus, kForceDefault, 1);
        clearStatus(bus);
    } else {
        bus->offline = true;
    }
    return stopped;
}

static int message(NativeGeniI2c *bus, uint8_t address, const uint8_t *tx,
    uint8_t *rx, size_t count, bool keepBus)
{
    if (bus->offline) {
        return -6;
    }
    clearStatus(bus);
    write(bus, rx ? kRxLength : kTxLength, (uint32_t)count);
    write(bus, kTxWatermark, rx ? 0 : 1);
    write(bus, kCommand, ((rx ? 2u : 1u) << 27) | (address << 9) | (keepBus ? 4u : 0u));
    size_t completed = 0;
    int result = -60;
    uint64_t deadline = nowUs() + bus->config.timeoutUs;
    do {
        uint32_t irq = read(bus, kMasterIrq);
        bus->lastIrq = irq;
        if (irq & kErrors) {
            result = (irq & kNack) ? -6 : ((irq & kArbitrationLost) ? -35 : -5);
            break;
        }
        if (rx) {
            uint32_t status = read(bus, kRxFifoStatus);
            unsigned available = status & 0x1ffffff;
            unsigned lastBytes = (status >> 28) & 7;
            if (available > bus->rxDepth) {
                result = -5;
                break;
            }
            for (unsigned index = 0; index < available; ++index) {
                uint32_t word = read(bus, kRxFifo);
                unsigned valid = (index + 1 == available && (status & (1u << 31)) && lastBytes) ? lastBytes : 4;
                for (unsigned byte = 0; byte < valid && completed < count; ++byte) {
                    rx[completed++] = word >> (byte * 8);
                }
            }
        } else {
            unsigned occupied = read(bus, kTxFifoStatus) & 0xfffffff;
            if (occupied > bus->txDepth) {
                result = -5;
                break;
            }
            for (unsigned slot = occupied; slot < bus->txDepth && completed < count; ++slot) {
                uint32_t word = 0;
                for (unsigned byte = 0; byte < 4 && completed < count; ++byte) {
                    word |= (uint32_t)tx[completed++] << (byte * 8);
                }
                write(bus, kTxFifo, word);
            }
            if (completed == count) {
                write(bus, kTxWatermark, 0);
            }
        }
        write(bus, kMasterIrqClear, irq);
        if (irq & kDone) {
            result = completed == count ? 0 : -5;
            break;
        }
        IODelay(2);
    } while (nowUs() < deadline);

    bus->lastStatus = read(bus, kStatus);
    ++bus->transactions;
    if (result) {
        ++bus->errors;
        bool recovered = recover(bus);
        printf("NativeGeniI2c: address=%02x %s bytes=%lu completed=%lu result=%d irq=%x status=%x recovered=%u\n",
            address, rx ? "read" : "write", (unsigned long)count, (unsigned long)completed,
            result, bus->lastIrq, bus->lastStatus, recovered);
    }
    return result;
}

static int prepare(NativeGeniI2c *bus)
{
    bus->firmware = read(bus, kFirmware);
    bus->interfaceDisable = read(bus, kInterfaceDisable);
    bus->wrapperVersion = *reinterpret_cast<volatile uint32_t *>(bus->wrapperMap->getVirtualAddress() + 4);
    uint32_t hardware0 = read(bus, kHardware0);
    uint32_t hardware1 = read(bus, kHardware1);
    unsigned major = bus->wrapperVersion >> 28;
    unsigned minor = (bus->wrapperVersion >> 16) & 0xfff;
    unsigned mask = major > 3 || (major == 3 && minor >= 10) ? 255 : 63;
    bus->txDepth = (hardware0 >> 16) & mask;
    bus->rxDepth = (hardware1 >> 16) & mask;
    printf("NativeGeniI2c: firmware=%x interface=%x wrapper=%x fifo=%u/%u width=%u\n",
        bus->firmware, bus->interfaceDisable, bus->wrapperVersion,
        bus->txDepth, bus->rxDepth, (hardware0 >> 24) & 63);
    if (((bus->firmware >> 8) & 255) != 3 || (bus->interfaceDisable & 1)) {
        return prepareGpio(bus);
    }
    if (((hardware0 >> 24) & 63) != 32 || bus->txDepth < 4 || bus->rxDepth < 4) {
        return -19;
    }
    if (read(bus, kStatus) & 1) {
        if (!recover(bus)) {
            return -16;
        }
    }
    unsigned divider, high, low, cycle;
    switch (bus->config.busHz) {
        case 100000:
            divider = 7;
            high = 10;
            low = 11;
            cycle = 26;
            break;
        case 400000:
            divider = 2;
            high = 5;
            low = 12;
            cycle = 24;
            break;
        case 1000000:
            divider = 1;
            high = 3;
            low = 9;
            cycle = 18;
            break;
        default:
            return -22;
    }
    write(bus, kGsiEvents, 0);
    clearStatus(bus);
    write(bus, kClockGating, read(bus, kClockGating) | 0x7f);
    write(bus, kDmaGeneral, read(bus, kDmaGeneral) | 0xf);
    write(bus, kOutputControl, 0x7f);
    write(bus, kForceDefault, 1);
    write(bus, kDmaMode, read(bus, kDmaMode) & ~1u);
    write(bus, kIrqEnable, 0);
    write(bus, kMasterIrqEnable, kDone | kErrors | kCancelDone | kAbortDone |
        kRxWatermarkIrq | kRxLastIrq | kTxWatermarkIrq);
    write(bus, kSlaveIrqEnable, 0);
    write(bus, kRxWatermark, 0);
    write(bus, kRxFlowWatermark, bus->rxDepth - 2);
    write(bus, kTxWatermark, 0);

    // Four 8-bit bytes occupy each FIFO word in increasing byte order.
    uint32_t packing[4] = {};
    for (unsigned index = 0; index < 4; ++index) {
        packing[index] = ((index * 8 + 7) << 5) | (1u << 4) | (7u << 1);
    }
    packing[3] |= 1;
    uint32_t first = packing[0] | (packing[1] << 10);
    uint32_t second = packing[2] | (packing[3] << 10);
    write(bus, kTxPacking0, first);
    write(bus, kTxPacking1, second);
    write(bus, kRxPacking0, first);
    write(bus, kRxPacking1, second);
    write(bus, kByteGranularity, 0);
    write(bus, kClockSelect, 0);
    write(bus, kSerialClock, (divider << 4) | 1);
    write(bus, kSclCounters, (high << 20) | (low << 10) | cycle);
    return 0;
}
} // namespace

extern "C" int native_geni_i2c_start(const NativeGeniI2cConfig *config, NativeGeniI2c **out)
{
    if (!config || !out || config->engineSize < 0x1000 || config->wrapperSize < 8 ||
        config->sourceHz != 19200000 || !config->timeoutUs) {
        return -22;
    }
    *out = nullptr;
    NativeGeniI2c *bus = static_cast<NativeGeniI2c *>(IOMallocZero(sizeof(NativeGeniI2c)));
    if (!bus) {
        return -12;
    }
    bus->config = *config;
    bus->lock = IOLockAlloc();
    bus->engineMap = map(config->engine, config->engineSize);
    bus->wrapperMap = map(config->wrapper, config->wrapperSize);
    int result = -12;
    if (bus->lock && bus->engineMap && bus->wrapperMap) {
        bus->base = bus->engineMap->getVirtualAddress();
        result = prepare(bus);
    }
    if (result) {
        OSSafeReleaseNULL(bus->engineMap);
        OSSafeReleaseNULL(bus->wrapperMap);
        if (bus->lock) {
            IOLockFree(bus->lock);
        }
        IOFree(bus, sizeof(NativeGeniI2c));
        return result;
    }
    *out = bus;
    return 0;
}

extern "C" int native_geni_i2c_write(NativeGeniI2c *bus, uint8_t address, const void *bytes, size_t count)
{
    if (!bus || address > 127 || !bytes || !count || count > 65535) {
        return -22;
    }
    IOLockLock(bus->lock);
    int result = bus->gpio ? gpioTransaction(bus, address, static_cast<const uint8_t *>(bytes), count, nullptr, 0) :
        message(bus, address, static_cast<const uint8_t *>(bytes), nullptr, count, false);
    IOLockUnlock(bus->lock);
    return result;
}

extern "C" int native_geni_i2c_write_read(NativeGeniI2c *bus, uint8_t address,
    const void *tx, size_t txCount, void *rx, size_t rxCount)
{
    if (!bus || address > 127 || (txCount && !tx) || !rx || !rxCount || txCount > 65535 || rxCount > 65535) {
        return -22;
    }
    IOLockLock(bus->lock);
    if (bus->gpio) {
        int result = gpioTransaction(bus, address, static_cast<const uint8_t *>(tx), txCount,
            static_cast<uint8_t *>(rx), rxCount);
        IOLockUnlock(bus->lock);
        return result;
    }
    int result = txCount ? message(bus, address, static_cast<const uint8_t *>(tx), nullptr, txCount, true) : 0;
    if (!result) {
        result = message(bus, address, nullptr, static_cast<uint8_t *>(rx), rxCount, false);
    }
    IOLockUnlock(bus->lock);
    return result;
}

extern "C" void native_geni_i2c_dump(NativeGeniI2c *bus)
{
    if (bus) {
        printf("NativeGeniI2c: engine=%llx gpio=%u bus-hz=%u transactions=%u errors=%u irq=%x status=%x offline=%u\n",
            bus->config.engine, bus->gpio, bus->gpio ? 100000 : bus->config.busHz, bus->transactions, bus->errors,
            bus->lastIrq, bus->lastStatus, bus->offline);
    }
}
