// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct NativeGeniI2c;

/* NativeGeniI2cConfig identifies a board-prepared serial engine and wrapper.
 * The board enables clocks and pinmux before start. The controller maps these
 * resources, checks the loaded I2C firmware, configures FIFO packing and SCL
 * timing, and serializes complete transactions. Mappings live for the boot.
 */
struct NativeGeniI2cConfig {
    uint64_t engine, engineSize;
    uint64_t wrapper, wrapperSize;
    uint32_t sourceHz, busHz, timeoutUs;
    // Optional board-owned open-drain pins when firmware has no FIFO interface.
    void *gpioContext;
    int (*gpioPrepare)(void *context);
    void (*gpioDriveLow)(void *context, unsigned line, bool low);
    int (*gpioRead)(void *context, unsigned line);
};

int native_geni_i2c_start(const struct NativeGeniI2cConfig *config, struct NativeGeniI2c **out);
int native_geni_i2c_write(struct NativeGeniI2c *bus, uint8_t address, const void *bytes, size_t count);

/* A nonempty write precedes the read with a repeated START. With txCount zero,
 * the controller issues a plain read. The final read always releases the bus.
 */
int native_geni_i2c_write_read(struct NativeGeniI2c *bus, uint8_t address,
    const void *tx, size_t txCount, void *rx, size_t rxCount);
void native_geni_i2c_dump(struct NativeGeniI2c *bus);

#ifdef __cplusplus
}
#endif
