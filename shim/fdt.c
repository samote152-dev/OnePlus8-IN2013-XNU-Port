/*
 * Bounded flattened-device-tree parser for the ABL-provided FDT.
 * Only reads /chosen u32/u64 properties; malformed input fails closed.
 */
#include <stdint.h>
#include "fdt.h"

#define FDT_MAGIC       0xd00dfeedU
#define FDT_BEGIN_NODE  1U
#define FDT_END_NODE    2U
#define FDT_PROP        3U
#define FDT_NOP         4U
#define FDT_END         9U
#define FDT_HEADER_SIZE 40U

static uint32_t be32(uintptr_t a)
{
    return __builtin_bswap32(*(volatile const uint32_t *)a);
}

static int range_ok(uint32_t off, uint32_t len, uint32_t total)
{
    return off <= total && len <= total - off;
}

static int bounded_string_eq(uintptr_t p, uintptr_t end, const char *s)
{
    while (p < end && *s) {
        char c = *(volatile const char *)p++;
        if (c != *s++)
            return 0;
    }
    return p < end && *(volatile const char *)p == 0 && *s == 0;
}

static uintptr_t align4(uintptr_t p)
{
    if (p > UINTPTR_MAX - 3U)
        return UINTPTR_MAX;
    return (p + 3U) & ~(uintptr_t)3U;
}

int fdt_chosen_u64(uintptr_t fdt, const char *prop, uint64_t *out)
{
    if (!fdt || !prop || !out || fdt > UINTPTR_MAX - FDT_HEADER_SIZE ||
        be32(fdt) != FDT_MAGIC)
        return 0;

    uint32_t total = be32(fdt + 4);
    uint32_t struct_off = be32(fdt + 8);
    uint32_t strings_off = be32(fdt + 12);
    uint32_t strings_size = be32(fdt + 32);
    uint32_t struct_size = be32(fdt + 36);

    if (total < FDT_HEADER_SIZE || fdt > UINTPTR_MAX - total ||
        (struct_off & 3U) ||
        !range_ok(struct_off, struct_size, total) ||
        !range_ok(strings_off, strings_size, total))
        return 0;

    uintptr_t struct_p = fdt + struct_off;
    uintptr_t struct_end = struct_p + struct_size;
    uintptr_t strings = fdt + strings_off;
    uintptr_t strings_end = strings + strings_size;
    uintptr_t p = struct_p;
    unsigned depth = 0;
    unsigned chosen_depth = 0;

    while (p <= struct_end && struct_end - p >= 4) {
        uint32_t tok = be32(p);
        p += 4;

        switch (tok) {
        case FDT_BEGIN_NODE: {
            uintptr_t name_end = p;
            while (name_end < struct_end &&
                   *(volatile const char *)name_end)
                name_end++;
            if (name_end == struct_end)
                return 0;
            depth++;
            if (depth == 2 &&
                bounded_string_eq(p, name_end + 1, "chosen"))
                chosen_depth = depth;
            p = align4(name_end + 1);
            if (p > struct_end)
                return 0;
            break;
        }
        case FDT_END_NODE:
            if (!depth)
                return 0;
            if (depth == chosen_depth)
                chosen_depth = 0;
            depth--;
            break;

        case FDT_PROP: {
            if (struct_end - p < 8 || !depth)
                return 0;
            uint32_t len = be32(p);
            uint32_t nameoff = be32(p + 4);
            p += 8;
            if (len > (uint32_t)(struct_end - p) ||
                nameoff >= strings_size)
                return 0;

            uintptr_t name = strings + nameoff;
            if (chosen_depth && depth == chosen_depth &&
                bounded_string_eq(name, strings_end, prop)) {
                if (len == 4) {
                    *out = be32(p);
                    return 1;
                }
                if (len == 8) {
                    *out = ((uint64_t)be32(p) << 32) | be32(p + 4);
                    return 1;
                }
                return 0;
            }
            p = align4(p + len);
            if (p > struct_end)
                return 0;
            break;
        }
        case FDT_NOP:
            break;
        case FDT_END:
            return 0;
        default:
            return 0;
        }
    }
    return 0;
}
