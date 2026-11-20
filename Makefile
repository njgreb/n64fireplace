BUILD_DIR = build
include $(N64_INST)/include/n64.mk

N64_ROM_TITLE = "HEARTH 64"

SRC = src/main.c src/scene.c src/fire.c src/turkey.c
OBJS = $(SRC:%.c=$(BUILD_DIR)/%.o)

TEX_RGBA16 = stone brick wood_mantle floor bark log_end coals rug iron painting
TEX_IA8    = flame glow spark

AUDIO_LOOPS = fire_bed rain_loop ambient_loop
AUDIO_SFX   = crackle_0 crackle_1 crackle_2 crackle_3 crackle_4 crackle_5 stoke log_thud gobble ribbit ding magic meow

ASSETS = \
	$(TEX_RGBA16:%=filesystem/%.sprite) \
	$(TEX_IA8:%=filesystem/%.sprite) \
	$(AUDIO_LOOPS:%=filesystem/%.wav64) \
	$(AUDIO_SFX:%=filesystem/%.wav64)

N64_CFLAGS += -O2 -Wall -Wextra

all: hearth64.z64

filesystem/%.sprite: assets_raw/textures/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE RGBA16] $@"
	@$(N64_MKSPRITE) --format RGBA16 --compress 1 -o filesystem $<

filesystem/flame.sprite: assets_raw/textures/flame.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE IA8] $@"
	@$(N64_MKSPRITE) --format IA8 --compress 1 -o filesystem $<

filesystem/glow.sprite: assets_raw/textures/glow.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE IA8] $@"
	@$(N64_MKSPRITE) --format IA8 --compress 1 -o filesystem $<

filesystem/spark.sprite: assets_raw/textures/spark.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE IA8] $@"
	@$(N64_MKSPRITE) --format IA8 --compress 1 -o filesystem $<

filesystem/fire_bed.wav64: assets_raw/audio/fire_bed.wav
	@mkdir -p $(dir $@)
	@echo "    [AUDIO LOOP] $@"
	@$(N64_AUDIOCONV) --wav-compress 1 --wav-loop true -o filesystem $<

filesystem/rain_loop.wav64: assets_raw/audio/rain_loop.wav
	@mkdir -p $(dir $@)
	@echo "    [AUDIO LOOP] $@"
	@$(N64_AUDIOCONV) --wav-compress 1 --wav-loop true -o filesystem $<

filesystem/ambient_loop.wav64: assets_raw/audio/ambient_loop.wav
	@mkdir -p $(dir $@)
	@echo "    [AUDIO LOOP] $@"
	@$(N64_AUDIOCONV) --wav-compress 1 --wav-loop true -o filesystem $<

filesystem/%.wav64: assets_raw/audio/%.wav
	@mkdir -p $(dir $@)
	@echo "    [AUDIO SFX] $@"
	@$(N64_AUDIOCONV) --wav-compress 1 -o filesystem $<

$(BUILD_DIR)/hearth64.dfs: $(ASSETS)
$(BUILD_DIR)/hearth64.elf: $(OBJS)

hearth64.z64: N64_ROM_TITLE = "HEARTH 64"
hearth64.z64: $(BUILD_DIR)/hearth64.dfs

clean:
	rm -rf $(BUILD_DIR) filesystem hearth64.z64

-include $(wildcard $(BUILD_DIR)/src/*.d)

.PHONY: all clean
