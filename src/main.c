#include <libdragon.h>

static uint64_t *addr = (uint64_t *)0xA0200000;

#define REPEAT_BYTES (8 * 3 - 4)
#define TEST_VALUE 0x1122334455667788

uint32_t left_before, left_during, left_after;
uint32_t right_before, right_during, right_after;

static void writeRepeatSDL(void *addr, uint64_t testVal) {
    uint32_t miVal = MI_WMODE_SET_REPEAT | (REPEAT_BYTES - 1);
    left_before = *MI_MODE;
    asm volatile("sw %1, 0(%0)\n" ::"r"(MI_MODE), "r"(miVal));
    left_during = *MI_MODE;
    asm volatile("sdl %0, 0(%1)\n" ::"r"(testVal), "r"(addr));
    left_after = *MI_MODE;
}

static void writeRepeatSDR(void *addr, uint64_t testVal) {
    uint32_t miVal = MI_WMODE_SET_REPEAT | (REPEAT_BYTES - 1);
    right_before = *MI_MODE;
    asm volatile("sw %1, 0(%0)\n" ::"r"(MI_MODE), "r"(miVal));
    right_during = *MI_MODE;
    asm volatile("sdr %0, 0(%1)\n" ::"r"(testVal), "r"(addr));
    right_after = *MI_MODE;
}

void printMem(int values) {
    for (int i = 0; i < values; ++i) {
        printf("%016llX ", addr[i]);
    }
    printf("\n");
    memset(addr, 0xEE, 128);
}

int main(void) {
    console_init();
    console_set_render_mode(RENDER_MANUAL);

    printMem(0);
    printf("Before: ");
    printMem(3);
    printf("\nValue: %08llX | Repeat bytes: %d\n\n", TEST_VALUE, REPEAT_BYTES);

    console_render();

    disable_interrupts();

    for (int i = 0; i < 8; ++i) {
        writeRepeatSDL((char *)addr + i, TEST_VALUE);
        printf("SDL +%d: ", i);
        printMem(3);
    }

    printf("\n");
    for (int i = 0; i < 8; ++i) {
        writeRepeatSDR((char *)addr + i, TEST_VALUE);
        printf("SDR +%d: ", i);
        printMem(3);
    }

    enable_interrupts();

    printf("initial MI_MODE: %08lX %08lX\n", left_before, right_before);
    printf("during  MI_MODE: %08lX %08lX\n", left_during, right_during);
    printf("after   MI_MODE: %08lX %08lX\n", left_after, right_after);
    console_render();

    console_render();
}