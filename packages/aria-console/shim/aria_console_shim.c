/*
 * aria_console_shim.c — 64KB virtual memory bus for Aria virtual console
 *
 * Provides bus_read / bus_write against a flat 64KB byte array.
 * The address map is defined in aria_console.aria (pure Aria):
 *   0x0000-0x1FFF  RAM   (8 KB)
 *   0x2000-0x27FF  DISP  (2 KB)
 *   0x2800-0x2FFF  INP   (2 KB)
 *   0x3000-0x37FF  SND   (2 KB)
 *   0x3800-0x3FFF  SYS   (2 KB)
 *   0x4000-0x7FFF  VRAM  (16 KB)
 *   0x8000-0xFFFF  ROM   (32 KB)
 *
 * All functions return int32_t.
 */

#include <stdint.h>
#include <string.h>

#define BUS_SIZE 65536

static uint8_t g_bus[BUS_SIZE];
static int     g_bus_init = 0;

/* Initialize bus memory (zero-fill). Returns 0. */
int32_t aria_console_bus_init(void) {
    memset(g_bus, 0, BUS_SIZE);
    g_bus_init = 1;
    return 0;
}

/* Returns 1 if bus is initialized, 0 otherwise. */
int32_t aria_console_bus_is_init(void) {
    return g_bus_init ? 1 : 0;
}

/*
 * Read a byte from the bus at the given 16-bit address.
 * Returns the byte value (0-255), or -1 if address is out of range.
 */
int32_t aria_console_bus_read(int32_t addr) {
    if (addr < 0 || addr >= BUS_SIZE) return -1;
    return (int32_t)g_bus[addr];
}

/*
 * Write a byte to the bus at the given 16-bit address.
 * value is truncated to 8 bits.
 * Returns 0 on success, -1 if address is out of range.
 */
int32_t aria_console_bus_write(int32_t addr, int32_t value) {
    if (addr < 0 || addr >= BUS_SIZE) return -1;
    g_bus[addr] = (uint8_t)(value & 0xFF);
    return 0;
}

/*
 * Read a 16-bit little-endian word from two consecutive addresses.
 * Returns the word value (0-65535), or -1 if address is out of range.
 */
int32_t aria_console_bus_read16(int32_t addr) {
    if (addr < 0 || addr + 1 >= BUS_SIZE) return -1;
    uint16_t lo = g_bus[addr];
    uint16_t hi = g_bus[addr + 1];
    return (int32_t)(lo | (hi << 8));
}

/*
 * Write a 16-bit little-endian word to two consecutive addresses.
 * Returns 0 on success, -1 if address is out of range.
 */
int32_t aria_console_bus_write16(int32_t addr, int32_t value) {
    if (addr < 0 || addr + 1 >= BUS_SIZE) return -1;
    g_bus[addr]     = (uint8_t)(value & 0xFF);
    g_bus[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    return 0;
}

/*
 * Fill a range of addresses with a single byte value.
 * Returns 0 on success, -1 if range is out of bounds.
 */
int32_t aria_console_bus_fill(int32_t start, int32_t length, int32_t value) {
    if (start < 0 || length < 0 || start + length > BUS_SIZE) return -1;
    memset(&g_bus[start], (uint8_t)(value & 0xFF), (size_t)length);
    return 0;
}

/*
 * Reset the entire bus to zero. Returns 0.
 */
int32_t aria_console_bus_reset(void) {
    memset(g_bus, 0, BUS_SIZE);
    return 0;
}
