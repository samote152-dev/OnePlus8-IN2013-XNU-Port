# NativeGeniI2c

NativeGeniI2c provides serialized seven-bit I2C writes, reads and repeated-start
write/read transactions for UniXNU. Board configuration supplies the GENI engine
and wrapper resources after preparing clocks and pinmux. The controller reads
the firmware protocol and FIFO capability before selecting its transfer method.

The FIFO implementation configures four packed bytes per word, uses the vendor
19.2-MHz timing tables for 100 kHz, 400 kHz and 1 MHz, and handles NACK, bus
protocol errors, arbitration loss and bounded command cancellation/abort.
The CPU polls status during a transaction; the controller does not request a
GIC interrupt or program a DMA descriptor.

When the engine firmware has no FIFO interface, optional board callbacks
provide the same physical SDA/SCL pins as open-drain GPIOs. Line 0 is SDA and
line 1 is SCL. `gpioDriveLow(..., false)` must release a line to input;
`gpioDriveLow(..., true)` must drive only zero. The software host supplies
START, repeated START, ACK/NACK, STOP, clock stretching and initial bus recovery
at a maximum 100 kHz. The board callback changes mux only after ensuring it owns
both pins. A board without those callbacks receives an unsupported result.

One `NativeGeniI2c` object owns a boot-lifetime mapping and transaction lock.
Callers must invoke it from thread context. The SM-T733 display board uses this
interface to program MAX77816 on SE10 after enabling its boost supply.

License: BSD-2-Clause-Patent. Source references are listed in NOTICE.
