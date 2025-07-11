all: playground.z64
.PHONY: all

BUILD_DIR = build
SOURCE_DIR = src
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/main.o

N64_CFLAGS += -std=gnu23 -Os -G0

playground.z64: N64_ROM_TITLE = "Libdragon playground"

$(BUILD_DIR)/playground.elf: $(OBJS)

# Clean code
clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
