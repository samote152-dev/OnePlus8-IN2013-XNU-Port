/*
 * Bounded ABL-ramdisk payload loader.
 * Validate every source and destination range before touching kernel memory.
 */
#include <stdint.h>
#include "console.h"
#include "fdt.h"
#include "loader.h"

#define BOOT_LINE_LENGTH 608U
#define PAGE_16K         0x4000ULL
#define MAX_PAYLOAD_SEGS 64U

struct payload_seg {
    uint64_t vmaddr, vmsize, file_off, file_size;
};

struct payload {
    char magic[8];
    uint32_t version, nsegs;
    uint64_t virt_base, entry_va, adt_off, adt_size, kernel_end;
    uint64_t phys_base, mem_size;
    char cmdline[BOOT_LINE_LENGTH];
    struct payload_seg segs[];
};

/* pexpert/pexpert/arm64/boot.h, Revision/Version 2 */
struct boot_video {
    uint64_t v_baseAddr, v_display, v_rowBytes, v_width, v_height, v_depth;
};

struct boot_args {
    uint16_t Revision, Version;
    uint64_t virtBase, physBase, memSize, topOfKernelData;
    struct boot_video Video;
    uint32_t machineType;
    uint64_t deviceTreeP;
    uint32_t deviceTreeLength;
    char CommandLine[BOOT_LINE_LENGTH];
    uint64_t bootFlags, memSizeActual;
};

_Static_assert(__builtin_offsetof(struct payload, segs) == 0x2a8,
               "payload header layout");
_Static_assert(__builtin_offsetof(struct boot_args, Video) == 40,
               "boot_args layout");
_Static_assert(__builtin_offsetof(struct boot_args, CommandLine) == 108,
               "boot_args layout");
_Static_assert(sizeof(struct boot_args) == 736, "boot_args layout");

typedef void (*xnu_entry_t)(uint64_t boot_args_pa);

static int add_ok(uint64_t a, uint64_t b, uint64_t *out)
{
    if (b > UINT64_MAX - a)
        return 0;
    *out = a + b;
    return 1;
}

static int range_inside(uint64_t start, uint64_t size,
                        uint64_t lo, uint64_t hi)
{
    uint64_t end;
    return start >= lo && add_ok(start, size, &end) && end <= hi;
}

static int ranges_overlap(uint64_t a, uint64_t as,
                          uint64_t b, uint64_t bs)
{
    uint64_t ae, be;
    if (!add_ok(a, as, &ae) || !add_ok(b, bs, &be))
        return 1;
    return a < be && b < ae;
}

static int magic_ok(const char *m)
{
    static const char want[] = "SWEETXNU";
    for (unsigned i = 0; i < 8; i++)
        if (m[i] != want[i])
            return 0;
    return 1;
}

/* Convert a kernel VA to its corresponding PA without signed underflow. */
static int va_to_pa(const struct payload *p, uint64_t va,
                    uint64_t size, uint64_t *pa)
{
    if (va < p->virt_base)
        return 0;
    uint64_t delta = va - p->virt_base;
    if (delta > p->mem_size || size > p->mem_size - delta)
        return 0;
    return add_ok(p->phys_base, delta, pa);
}

/* Byte copies avoid reading or writing beyond a non-8-byte segment tail. */
static void copy_bytes(uintptr_t dst, uintptr_t src, uint64_t n)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    volatile const uint8_t *s = (volatile const uint8_t *)src;
    for (uint64_t i = 0; i < n; i++)
        d[i] = s[i];
}

static void zero_bytes(uintptr_t dst, uint64_t n)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    for (uint64_t i = 0; i < n; i++)
        d[i] = 0;
}

const char *loader_boot(uintptr_t dtb)
{
    uint64_t rd_start, rd_end;
    if (!fdt_chosen_u64(dtb, "linux,initrd-start", &rd_start) ||
        !fdt_chosen_u64(dtb, "linux,initrd-end", &rd_end))
        return "no initrd in /chosen";
    if (rd_end <= rd_start || (rd_start & 7))
        return "invalid initrd range";

    con_hex2("INITRD", "lo=", rd_start, "hi=", rd_end);
    if (rd_end - rd_start < sizeof(struct payload))
        return "initrd too small for payload header";

    const struct payload *p = (const struct payload *)(uintptr_t)rd_start;
    if (!magic_ok(p->magic))
        return "no SWEETXNU payload";
    if (p->version != 1 || !p->nsegs ||
        p->nsegs > MAX_PAYLOAD_SEGS || (p->adt_off & 7) ||
        (p->phys_base & (PAGE_16K - 1)) ||
        (p->virt_base & (PAGE_16K - 1)) || !p->mem_size)
        return "bad payload header";

    uint64_t phys_end, virt_end, table_bytes, table_end;
    if (!add_ok(p->phys_base, p->mem_size, &phys_end) ||
        !add_ok(p->virt_base, p->mem_size, &virt_end) ||
        p->nsegs > (UINT64_MAX - sizeof(struct payload)) /
                   sizeof(struct payload_seg))
        return "payload memory range overflow";

    table_bytes = (uint64_t)p->nsegs * sizeof(struct payload_seg);
    if (!add_ok(sizeof(struct payload), table_bytes, &table_end) ||
        table_end > p->adt_off || p->adt_off > rd_end - rd_start ||
        p->adt_size > rd_end - rd_start - p->adt_off ||
        !p->adt_size || p->adt_size > UINT32_MAX)
        return "payload table or ADT outside initrd";

    uint64_t adt_src_end;
    if (!add_ok(p->adt_off, p->adt_size, &adt_src_end) ||
        adt_src_end > rd_end - rd_start)
        return "ADT source range overflow";

    /* Never let the input ramdisk alias the physical memory being replaced. */
    if (ranges_overlap(rd_start, rd_end - rd_start,
                       p->phys_base, p->mem_size))
        return "ramdisk overlaps kernel memory";

    con_hex2("PHYS", "base=", p->phys_base, "size=", p->mem_size);
    con_hex("VIRT", p->virt_base);
    con_hex("ENTRY", p->entry_va);

    uint64_t max_va_end = p->virt_base;
    int entry_in_segment = 0;

    for (uint32_t i = 0; i < p->nsegs; i++) {
        const struct payload_seg *s = &p->segs[i];
        uint64_t src_end, va_end, dst;
        if (!s->vmsize || s->file_size > s->vmsize ||
            s->file_off < table_end ||
            !add_ok(s->file_off, s->file_size, &src_end) ||
            src_end > p->adt_off ||
            !add_ok(s->vmaddr, s->vmsize, &va_end) ||
            !va_to_pa(p, s->vmaddr, s->vmsize, &dst))
            return "invalid payload segment range";

        if (s->file_size &&
            !range_inside(rd_start + s->file_off, s->file_size,
                          rd_start, rd_start + p->adt_off))
            return "segment source outside payload data";

        if (p->entry_va >= s->vmaddr && p->entry_va < va_end)
            entry_in_segment = 1;
        if (va_end > max_va_end)
            max_va_end = va_end;

        for (uint32_t j = 0; j < i; j++) {
            const struct payload_seg *q = &p->segs[j];
            if (ranges_overlap(s->vmaddr, s->vmsize,
                               q->vmaddr, q->vmsize))
                return "overlapping kernel segments";
        }
    }

    if (!entry_in_segment || p->kernel_end < max_va_end ||
        p->kernel_end > virt_end || (p->kernel_end & (PAGE_16K - 1)))
        return "invalid kernel entry or end";

    uint64_t adt_pa, ba_unaligned, ba_pa, top;
    if (!va_to_pa(p, p->kernel_end, p->adt_size, &adt_pa) ||
        !add_ok(adt_pa, p->adt_size, &ba_unaligned) ||
        !add_ok(ba_unaligned, PAGE_16K - 1, &ba_unaligned))
        return "ADT destination outside kernel memory";
    ba_pa = ba_unaligned & ~(PAGE_16K - 1);
    if (ba_pa < adt_pa ||
        !add_ok(ba_pa, PAGE_16K, &top) ||
        top > phys_end ||
        !range_inside(adt_pa, p->adt_size, p->phys_base, phys_end))
        return "boot data exceeds kernel memory";

    uint64_t entry_pa, adt_va_mapped;
    if (!va_to_pa(p, p->entry_va, 1, &entry_pa) ||
        !add_ok(p->virt_base, adt_pa - p->phys_base, &adt_va_mapped))
        return "entry or ADT address overflow";

    /* All validation is complete. Only now begin writing kernel memory. */
    copy_bytes((uintptr_t)adt_pa,
               (uintptr_t)(rd_start + p->adt_off), p->adt_size);

    zero_bytes((uintptr_t)ba_pa, PAGE_16K);
    volatile struct boot_args *ba = (volatile struct boot_args *)ba_pa;
    ba->Revision = 2;
    ba->Version = 2;
    ba->virtBase = p->virt_base;
    ba->physBase = p->phys_base;
    ba->memSize = p->mem_size;
    ba->topOfKernelData = top;
#if defined(SWEET_FB_VERIFIED) && !defined(SWEET_QEMU)
    /* Explicit opt-in only after ABL confirms the IN2013 framebuffer handoff. */
    ba->Video.v_baseAddr = FB_BASE;
    ba->Video.v_display = 1;
    ba->Video.v_rowBytes = FB_STRIDE;
    ba->Video.v_width = FB_W;
    ba->Video.v_height = FB_H;
    ba->Video.v_depth = 32;
#endif
    ba->deviceTreeP = adt_va_mapped;
    ba->deviceTreeLength = (uint32_t)p->adt_size;
    for (unsigned i = 0; i < BOOT_LINE_LENGTH; i++)
        ba->CommandLine[i] = p->cmdline[i];
    ba->memSizeActual = p->mem_size;

    for (uint32_t i = 0; i < p->nsegs; i++) {
        const struct payload_seg *s = &p->segs[i];
        uint64_t dst;
        if (!va_to_pa(p, s->vmaddr, s->vmsize, &dst))
            return "segment mapping changed after validation";
        copy_bytes((uintptr_t)dst,
                   (uintptr_t)(rd_start + s->file_off), s->file_size);
        zero_bytes((uintptr_t)(dst + s->file_size),
                   s->vmsize - s->file_size);
    }

    con_hex2("SEGS", "n=", p->nsegs, "end=", p->kernel_end);
    con_hex2("BOOTARG", "pa=", ba_pa, "top=", top);
    con_hex("JUMP", entry_pa);
    con_str("XNU", "validated payload; jumping");

    __asm__ volatile("dsb sy; ic iallu; dsb sy; isb" ::: "memory");
    ((xnu_entry_t)(uintptr_t)entry_pa)(ba_pa);
    return "xnu returned";
}
