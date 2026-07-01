#include <libdragon.h>

#include <inttypes.h>

volatile uint32_t* const SI_CTRL = VirtualUncachedAddr(0x0480000C);
volatile uint32_t* const SI_CONFIG = VirtualUncachedAddr(0x0480001C);

int main(void) {
    console_init();
    console_set_render_mode(RENDER_MANUAL);

    printf("si_config: %" PRIx32 "\n", *SI_CONFIG);
    console_render();

    while (true) {
        ;
    }
}
