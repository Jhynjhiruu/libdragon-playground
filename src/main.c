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

#define READ_SIZE (8 * 1024 * 1024)
#define BANK_SIZE (32 * 1024)

#define NUM_BANKS (READ_SIZE / BANK_SIZE)

    char const *const path = "sd:/dump.pak";

    const int f = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (f == -1) {
        printf("Failed to create %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
        console_render();
        while (true) {
            continue;
        }
    }

    uint8_t buf[BANK_SIZE];

    for (uint bank = 0; bank < NUM_BANKS; bank++) {
        printf("Reading bank %d\n", bank);
        console_render();

        joypad_accessory_error_t res = joypad_controller_pak_set_bank(JOYPAD_PORT_1, bank);

        if (res != JOYPAD_ACCESSORY_ERROR_NONE) {
            printf("Error setting bank %d: %d\n", bank, res);
            console_render();
            goto __close_file;
        }

        res = joypad_accessory_xfer(JOYPAD_PORT_1, JOYPAD_ACCESSORY_XFER_READ, 0x0000, buf, sizeof(buf));

        if (res != JOYPAD_ACCESSORY_ERROR_NONE) {
            printf("Error reading bank %d: %d\n", bank, res);
            console_render();
            goto __close_file;
        }

        size_t done = 0;
        while (done < BANK_SIZE) {
            const size_t left = BANK_SIZE - done;
            const ssize_t res = write(f, buf + done, left);
            if ((res == 0) || (res == -1)) {
                printf("Failed to write %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
                console_render();
                goto __close_file;
            }

            done += (size_t)res;
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
