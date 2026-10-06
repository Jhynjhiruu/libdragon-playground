#include <libdragon.h>

#include <regsinternal.h>

#include <fcntl.h>
#include <unistd.h>

#include "pif.h"

#define PIFROM_SIZE (0x07C0)

uint32_t pifrom_buf[PIFROM_SIZE / sizeof(uint32_t)];

#define IPL3_SIZE (0x1000 - 0x40)

uint32_t ipl3_buf[IPL3_SIZE / sizeof(uint32_t)];

uint64_t calc_crc(uint32_t seedmul, void* ipl3buf, uint32_t ipl3len, void* addr);

asm (
        ".text                 \n"
        ".set noreorder        \n"
        ".ent calc_crc         \n"
        "calc_crc:             \n"
        "addi $sp, $sp, -64    \n"
        "sd $ra, 56($sp)       \n"
        "jalr $a3              \n"
        " nop                  \n"
        "andi $a0, $a0, 0xFFFF \n"
        "dsll32 $a1, $a1, 0    \n"
        "dsrl32 $a1, $a1, 0    \n"
        "dsll32 $a0, $a0, 0    \n"
        "daddu  $v0, $a0, $a1  \n"
        "ld $ra, 56($sp)       \n"
        "jr $ra                \n"
        "addi $sp, $sp, 64     \n"
        ".end calc_crc         \n"
        ".set reorder          \n"
    );

static volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *)0xa4800000;
static uint32_t * const PIF_RAM = VirtualUncachedAddr(0x1fc007c0);

uint32_t si_ram_read(uint32_t offset) {
    assertf((offset % 4) == 0, "offset misaligned");
    #define SI_STATUS_DMA_BUSY  ( 1 << 0 )
    #define SI_STATUS_IO_BUSY   ( 1 << 1 )

    disable_interrupts();
    while (SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) {
        continue;
    }
    uint32_t rv = PIF_RAM[offset / sizeof(uint32_t)];
    wait_ms(10);
    while (SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) {
        continue;
    }
    SI_regs->status = 0;
    enable_interrupts();
    return rv;
}

void si_ram_write(uint32_t offset, uint32_t data) {
    assertf((offset % 4) == 0, "offset misaligned");
    #define SI_STATUS_DMA_BUSY  ( 1 << 0 )
    #define SI_STATUS_IO_BUSY   ( 1 << 1 )

    disable_interrupts();
    while (SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) {
        continue;
    }
    PIF_RAM[offset / sizeof(uint32_t)] = data;
    wait_ms(10);
    while (SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) {
        continue;
    }
    SI_regs->status = 0;
    enable_interrupts();
}

volatile uint32_t* osTvType = (void*)0x80000300;
volatile uint32_t* osRomType = (void*)0x80000304;
volatile uint32_t* osRomBase = (void*)0x80000308;
volatile uint32_t* osResetType = (void*)0x8000030C;
volatile uint32_t* osCicId = (void*)0x80000310;
volatile uint32_t* osVersion = (void*)0x80000314;
volatile uint32_t* osMemSize = (void*)0x80000318;
volatile uint32_t* osAppNMIBuffer = (void*)0x8000031C;

void memcpy_32(void* dst, void const* src, size_t len) {
    assertf(((uintptr_t)dst % sizeof(uint32_t)) == 0, "dst misaligned");
    assertf(((uintptr_t)src % sizeof(uint32_t)) == 0, "src misaligned");
    assertf((len % sizeof(uint32_t)) == 0, "len misaligned");

    volatile uint32_t* dst_32 = dst;
    volatile uint32_t const* src_32 = src;

    for (int i = 0; i < (len / sizeof(uint32_t)); i++) {
        dst_32[i] = src_32[i];
    }
}

int main(void) {
    joypad_inputs_t inputs = {0}, prev_inputs;
    #define INPUT(inp) ((inputs.inp) && !(prev_inputs.inp))

    console_init();
    console_set_render_mode(RENDER_MANUAL);

    printf("\n\n");

    joypad_init();
    joypad_poll();
    inputs = joypad_get_inputs(JOYPAD_PORT_1);

    bool has_patch = true;

    if (!debug_init_sdfs("sd:/", -1)) {
        printf("SD initialisation failed\n");
        console_render();

        has_patch = false;
        goto __continue;
    }

    const int f = open("sd:/rom.patch", O_RDONLY);
    if (f == -1) {
        printf("Failed to open patch file\n");
        console_render();

        has_patch = false;
        goto __continue;
    }

    struct stat sb;
    if (fstat(f, &sb) == -1) {
        printf("Failed to stat patch file\n");
        console_render();

        has_patch = false;
        goto __close_patch_file;
    }

    const off_t size = sb.st_size;
    assertf(size >= 0, "size is negative");

    uint8_t *const buf = aligned_alloc(4, size);
    if (buf == NULL) {
        printf("alloc failed\n");
        console_render();

        has_patch = false;
        goto __close_patch_file;
    }

    size_t done = 0;
    while (done < (size_t)size) {
        const size_t left = (size_t)size - done;
        const ssize_t res = read(f, buf + done, left);
        if ((res == 0) || (res == -1)) {
            printf("read failed\n");
            console_render();

            // buffer is never freed, so that it won't be overwritten by anything in libdragon
            has_patch = false;
            goto __close_patch_file;
        }

        done += (size_t)res;
    }

    printf("Read patch file\n");
    console_render();

    data_cache_writeback_invalidate_all();
    inst_cache_invalidate_all();

__close_patch_file:
    close(f);

__continue:

    void reset_callback(void) {
        printf("Reset button pressed, waiting for exception...\n");
        console_render();
    }

    void setup_callback(void) {
        printf("Press the reset button now\n");
        console_render();
    }

    hang_pif(reset_callback, setup_callback);

    printf("Swap now, then press the A button to continue\n");
    console_render();

    while (true) {
        joypad_poll();
        prev_inputs = inputs;
        inputs = joypad_get_inputs(JOYPAD_PORT_1);

        if (INPUT(btn.a)) {
            break;
        }
    }

    while (true) {
        joypad_poll();
        prev_inputs = inputs;
        inputs = joypad_get_inputs(JOYPAD_PORT_1);

        if (!INPUT(btn.a)) {
            break;
        }
    }

    printf("Dumping PIFROM...\n");
    console_render();
    console_render();

    dma_read_async(ipl3_buf, 0x10000040, sizeof(ipl3_buf));

    disable_interrupts();
    memcpy_32(pifrom_buf, VirtualUncachedAddr(0x1FC00000), sizeof(pifrom_buf));
    enable_interrupts();

    dma_wait();

    printf("Dumped PIFROM...\n");
    console_render();
    console_render();

    MEMORY_BARRIER();

    printf("Patching PIFROM...\n");
    console_render();
    console_render();

    assertf(pifrom_buf[0x61C / sizeof(uint32_t)] == 0x04110008, "bal VerifyAndRunIPL3 not found");

    pifrom_buf[0x61C / sizeof(uint32_t)] = 0x03E00008; // jr $ra

    data_cache_hit_writeback(pifrom_buf, sizeof(pifrom_buf));

    MEMORY_BARRIER();

    #define MUL_SEED(seed) (((seed) * 0x6C078965U) + 1)
    #define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

    struct {
        uint32_t seedmul;
        uint64_t expect;
        uint16_t which;
        uint32_t sub;
    } known_ipl3s[] = {
        {
            MUL_SEED(0x3F), 0x45CC'73EE317A, 6101, 0
        },
        {
            MUL_SEED(0x3F), 0xA536'C0F1D859, 7101 /* 6102 */, 0
        },
        {
            MUL_SEED(0x3F), 0x4416'0EC5D9AF, 7102, 0
        },
        {
            MUL_SEED(0x78), 0x586F'D4709867, 7103 /* 6103 */, 0x100000
        },
        {
            MUL_SEED(0x91), 0x8618'A45BC2D3, 7105 /* 6105 */, 0
        },
        {
            MUL_SEED(0x85), 0x2BBA'D4E6EB74, 7106 /* 6106 */, 0x200000
        },
    };

    int which_ipl3 = 0;
    uint64_t calc;

    while (true) {
        for (int i = 0; i < ARRAY_LEN(known_ipl3s); i++) {
            calc = calc_crc(known_ipl3s[i].seedmul, ipl3_buf, sizeof(ipl3_buf), &pifrom_buf[0x258 / sizeof(uint32_t)]);
            /*printf("%016" PRIX64 "\n", calc);
            console_render();*/
            if (calc == known_ipl3s[i].expect) {
                which_ipl3 = i;
                break;
            }
        }

        if (which_ipl3 != 0) {
            printf("Found IPL3 %d\n", known_ipl3s[which_ipl3].which);
            console_render();
            break;
        } else {
            printf("Found unknown IPL3\n");
            printf("Press A to retry\n");
            console_render();

            while (true) {
                joypad_poll();
                prev_inputs = inputs;
                inputs = joypad_get_inputs(JOYPAD_PORT_1);

                if (INPUT(btn.a)) {
                    break;
                }
            }

            while (true) {
                joypad_poll();
                prev_inputs = inputs;
                inputs = joypad_get_inputs(JOYPAD_PORT_1);

                if (!INPUT(btn.a)) {
                    break;
                }
            }

            dma_wait();
            disable_interrupts();
            memcpy_32(ipl3_buf, VirtualUncachedAddr(0x10000040), sizeof(ipl3_buf));
            enable_interrupts();
        }
    }

    uint32_t entrypoint = known_ipl3s[which_ipl3].which == 7102 ? 0x80000480 : io_read(0x10000008) - known_ipl3s[which_ipl3].sub;
    uint32_t crc1 = io_read(0x10000010);
    uint32_t crc2 = io_read(0x10000014);

    dma_read_async((void*)entrypoint, 0x10001000, 1 * 1024 * 1024);
    dma_wait();

    inst_cache_invalidate_all();

    *osTvType = get_tv_type();
    *osRomType = 0;
    *osRomBase = 0xB0000000;
    *osResetType = 0;
    *osCicId = known_ipl3s[which_ipl3].which;
    *osVersion = 15;
    *osMemSize = 0x400000;

    if (has_patch) {
        void* patch_start = (void *)buf + 0x10;
        void (*patch_main)(void*, void*) = patch_start + *(uint32_t*)(buf + 8);

        uint32_t check_crc1 = *(uint32_t*)(buf + 0);
        uint32_t check_crc2 = *(uint32_t*)(buf + 4);

        if ((check_crc1 != crc1) || (check_crc2 != crc2)) {
            printf("CRC mismatch\n");
            console_render();

            while (true) {
                continue;
            }
        }

        printf("Patching...\n");
        console_render();

        patch_main((void*)entrypoint, (void*)patch_start);

        data_cache_writeback_invalidate_all();
        inst_cache_invalidate_all();

        printf("Patched!\n");
        console_render();
    }

    printf("Press A to boot the inserted game\n");
    console_render();

    while (true) {
        joypad_poll();
        prev_inputs = inputs;
        inputs = joypad_get_inputs(JOYPAD_PORT_1);

        if (INPUT(btn.a)) {
            break;
        }
    }

    while (true) {
        joypad_poll();
        prev_inputs = inputs;
        inputs = joypad_get_inputs(JOYPAD_PORT_1);

        if (!INPUT(btn.a)) {
            break;
        }
    }

    /*printf("Locking PIFROM...\n");
    console_render();

    // Lock ROM
    si_ram_write(0x3C, si_ram_read(0x3C) | 0x10);
    wait_ms(10);

    printf("Sending IPL3 checksum...\n");
    console_render();

    // Send checksum
    si_ram_write(0x30, (si_ram_read(0x30) & 0xFFFF0000) | (calc >> 32));
    wait_ms(10);
    si_ram_write(0x34, calc & 0xFFFFFFFF);
    wait_ms(10);

    // Check checksum
    si_ram_write(0x3C, si_ram_read(0x3C) | 0x20);
    wait_ms(10);

    printf("Waiting for checksum ack...\n");
    console_render();

    while ((si_ram_read(0x3C) & 0x80) == 0) {
        printf("%" PRIX32 "\n", si_ram_read(0x3C));
        console_render();
        continue;
    }

    printf("Clearing PIFRAM...\n");
    console_render();

    // Clear PIFRAM
    si_ram_write(0x3C, si_ram_read(0x3C) | 0x40);
    wait_ms(10);*/

    printf("Launching...\n");
    console_render();

    disable_interrupts();

    joypad_close();
    console_close();
    vi_close();
    audio_close();

    MEMORY_BARRIER();

    ((void(*)(void))entrypoint)();
}
