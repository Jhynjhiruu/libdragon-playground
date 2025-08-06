#include <../../src/joypad_accessory.h>
#include <libdragon.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    console_init();
    console_set_render_mode(RENDER_MANUAL);

    joypad_init();

    printf("Controller pak dumper\n");
    console_render();

    if (!debug_init_sdfs("sd:/", -1)) {
        printf("SD card not found\n");
        console_render();
        while (true) {
            continue;
        }
    }

#define READ_SIZE (128 * 1024)
#define BANK_SIZE (32 * 1024)

#define NUM_BANKS (READ_SIZE / BANK_SIZE)

    char const *const path = "sd:/write.pak";

    const int f = open(path, O_RDONLY);
    if (f == -1) {
        printf("Failed to open %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
        console_render();
        while (true) {
            continue;
        }
    }

    uint8_t buf[BANK_SIZE];

    for (uint bank = 0; bank < NUM_BANKS; bank++) {
        printf("Writing bank %d\n", bank);
        console_render();

        size_t done = 0;
        while (done < sizeof(buf)) {
            const size_t left = sizeof(buf) - done;
            const ssize_t res = read(f, buf + done, left);
            if (res == 0) {
                memset(buf + done, 0, left);
                done += left;
            } else if (res == -1) {
                printf("Failed to read %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
                console_render();

                goto __close_file;
            } else {
                done += (size_t)res;
            }
        }

        joypad_accessory_error_t res = joypad_controller_pak_set_bank(JOYPAD_PORT_1, bank);

        if (res != JOYPAD_ACCESSORY_ERROR_NONE) {
            printf("Error setting bank %d: %d\n", bank, res);
            console_render();
            goto __close_file;
        }

        res = joypad_accessory_xfer(JOYPAD_PORT_1, JOYPAD_ACCESSORY_XFER_WRITE, 0x0000, buf, sizeof(buf));

        if (res != JOYPAD_ACCESSORY_ERROR_NONE) {
            printf("Error writing bank %d: %d\n", bank, res);
            console_render();
            goto __close_file;
        }
    }

__close_file:
    if (close(f) == -1) {
        printf("Failed to close %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
        console_render();
    }

    printf("Done!\n");
    console_render();

    while (true) {
        ;
    }
}
