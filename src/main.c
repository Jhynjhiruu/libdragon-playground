#include <libdragon.h>

#include "pif.h"

volatile uint32_t* osTvType = (void*)0x80000300;
volatile uint32_t* osRomType = (void*)0x80000304;
volatile uint32_t* osRomBase = (void*)0x80000308;
volatile uint32_t* osResetType = (void*)0x8000030C;
volatile uint32_t* osCicId = (void*)0x80000310;
volatile uint32_t* osVersion = (void*)0x80000314;
volatile uint32_t* osMemSize = (void*)0x80000318;
volatile uint32_t* osAppNMIBuffer = (void*)0x8000031C;
#define NMI_FLAGS (15)

static inline void set_nmi_flag(uint32_t index) {
    osAppNMIBuffer[NMI_FLAGS] |= index;
}

static inline bool get_nmi_flag(uint32_t index) {
    return (osAppNMIBuffer[NMI_FLAGS] & index) != 0;
}

int main(void) {
    joypad_inputs_t inputs = {0}, prev_inputs;
    #define INPUT(inp) ((inputs.inp) && !(prev_inputs.inp))

    console_init();
    console_set_render_mode(RENDER_MANUAL);

    joypad_init();
    joypad_poll();
    inputs = joypad_get_inputs(JOYPAD_PORT_1);

    void reset_callback(void) {
        printf("Reset button pressed, waiting for exception...\n");
        console_render();
    }

    void setup_callback(void) {
        printf("Press the reset button now\n");
        console_render();
    }

    hang_pif(reset_callback, setup_callback);

    printf("Swap to Animal Forest now, then press the A button to continue\n");
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
    *osTvType = 1;
    *osRomType = 0;
    *osRomBase = 0xB0000000;
    *osResetType = 1;
    *osCicId = 0;
    *osVersion = 15;
    *osMemSize = 0x800000;

    set_nmi_flag(1);
    set_nmi_flag(2);
    set_nmi_flag(4);
    set_nmi_flag(8);
    set_nmi_flag(16);
    set_nmi_flag(32);
    set_nmi_flag(64);
    set_nmi_flag(128);
    set_nmi_flag(256);
    set_nmi_flag(512);

    disable_interrupts();

    joypad_close();
    console_close();
    vi_close();
    audio_close();

    MEMORY_BARRIER();

    dma_read((void *)0x80025C00, 0x10001000, 0x100000);
    MEMORY_BARRIER();

    asm __volatile__ (
        "li $t3, 0x80025C00\n"
        "jr $t3\n"
        "nop\n"
    );
}
