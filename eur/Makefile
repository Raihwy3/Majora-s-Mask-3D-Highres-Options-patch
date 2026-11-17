CC ?= cc
ARM_CC ?= /opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc
WARN = -std=c11 -Wall -Wextra -Werror -Isrc

.PHONY: check arm clean
check: build/test_display_options
	ASAN_OPTIONS=detect_leaks=0 ./build/test_display_options

build/test_display_options: src/display_options.c src/display_options.h tests/test_display_options.c
	mkdir -p build
	$(CC) $(WARN) -g -fsanitize=address,undefined src/display_options.c tests/test_display_options.c -o $@

arm: build/display_options.arm.o

build/display_options.arm.o: src/display_options.c src/display_options.h
	mkdir -p build
	$(ARM_CC) $(WARN) -Os -march=armv6k -mtune=mpcore -marm -mfloat-abi=hard -mfpu=vfp -ffreestanding -c src/display_options.c -o $@

clean:
	rm -f build/test_display_options build/display_options.arm.o

ARM_FLAGS = $(WARN) -Os -march=armv6k -mtune=mpcore -marm -mfloat-abi=hard -mfpu=vfp -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables
NATIVE_OBJS = build/native-options_motion.o build/native-settings_io.o build/native-ipc.o build/native-cstick.o build/native-bootstrap.o build/native-hires.o build/native-runtime.o build/native-text_stream.o build/native-render_modes.o build/native-output_buffers.o build/native-resolve.o build/native-renderhooks.o build/native-menu.o build/native-options_style.o build/native-options_tabs.o build/display_options.arm.o

.PHONY: native
native: build/native.elf

build/native-%.o: native/%.S
	mkdir -p build
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@

build/native-runtime.o: native/runtime.c src/display_options.h
	mkdir -p build
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@

build/native.elf: $(NATIVE_OBJS) native/module.ld Makefile
	$(ARM_CC) $(ARM_FLAGS) -nostdlib -Wl,-T,native/module.ld,-Map,build/native.map -o $@ $(NATIVE_OBJS) -lgcc
	/opt/devkitpro/devkitARM/bin/arm-none-eabi-objcopy -O binary --only-section=.bootstrap $@ build/bootstrap.bin
	/opt/devkitpro/devkitARM/bin/arm-none-eabi-objcopy -O binary --remove-section=.bootstrap $@ build/module.bin

.PHONY: check-native
check-native: native
	python3 tests/test_bootstrap.py

build/native-text_probe.o: native/text_probe.c
	mkdir -p build
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@

build/test_native_text: native/text_probe.c native/text_stream.c tests/test_native_text.c
	mkdir -p build
	$(CC) $(WARN) -g -fsanitize=address,undefined $^ -o $@

.PHONY: check-text
check-text: build/test_native_text
	ASAN_OPTIONS=detect_leaks=0 ./build/test_native_text

build/native-text_stream.o: native/text_stream.c
	mkdir -p build
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@

build/native-render_modes.o: native/render_modes.c src/display_options.h
	mkdir -p build
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@

build/native-renderhooks.o: native/render_modes.S
	mkdir -p build
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@

build/native-menu.o: native/menu.c src/display_options.h
	mkdir -p build
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@

build/test_render_modes: native/render_modes.c native/output_buffers.c src/display_options.c tests/test_render_modes.c src/display_options.h
	mkdir -p build
	$(CC) $(WARN) -DMM_NATIVE_HOST_TEST -fno-pie -no-pie -g -fsanitize=address,undefined native/render_modes.c native/output_buffers.c src/display_options.c tests/test_render_modes.c -o $@

.PHONY: check-render
check-render: build/test_render_modes
	ASAN_OPTIONS=detect_leaks=0 ./build/test_render_modes

build/test_native_menu: native/menu.c native/render_modes.c native/output_buffers.c native/text_stream.c src/display_options.c tests/test_native_menu.c src/display_options.h
	mkdir -p build
	$(CC) $(WARN) -DMM_NATIVE_HOST_TEST -fno-pie -no-pie -g -fsanitize=address,undefined native/menu.c native/render_modes.c native/output_buffers.c native/text_stream.c src/display_options.c tests/test_native_menu.c -o $@

.PHONY: check-menu
check-menu: build/test_native_menu
	ASAN_OPTIONS=detect_leaks=0 ./build/test_native_menu

build/native-options_style.o: native/options_style.c src/display_options.h native/options_motion.h
	mkdir -p build
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@


.PHONY: check-rect
check-rect: native
	python3 tests/test_copy_rect.py

build/test_options_style: native/options_style.c native/text_stream.c src/display_options.c tests/test_options_style.c src/display_options.h native/options_motion.h
	mkdir -p build
	$(CC) $(WARN) -DMM_NATIVE_HOST_TEST -fno-pie -no-pie -g -fsanitize=address,undefined native/options_style.c native/text_stream.c src/display_options.c tests/test_options_style.c -o $@

.PHONY: check-style
check-style: build/test_options_style
	ASAN_OPTIONS=detect_leaks=0 ./build/test_options_style

.PHONY: check-depth
check-depth: native
	python3 tests/test_stereo_strength.py




build/native-options_tabs.o: native/options_tabs.c
	mkdir -p build
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@

build/test_options_tabs: native/options_tabs.c tests/test_options_tabs.c
	mkdir -p build
	$(CC) $(WARN) -g -fsanitize=address,undefined $^ -o $@

.PHONY: check-tabs
check-tabs: build/test_options_tabs
	ASAN_OPTIONS=detect_leaks=0 ./build/test_options_tabs
	python3 build_options_tabs.py
	python3 tests/test_options_tab_assets.py
	python3 tests/test_message_window_calc.py


build/native-output_buffers.o: native/output_buffers.c
	mkdir -p build
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@

.PHONY: check-scanout
check-scanout: native
	python3 tests/test_scanout_pointer.py
	python3 tests/test_transfer_commands.py

build/native-resolve.o: native/resolve.c
	mkdir -p build
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@


build/test_resolve: native/resolve.c tests/test_resolve.c
	mkdir -p build
	$(CC) $(WARN) -DMM_NATIVE_HOST_TEST -fno-pie -no-pie -g -fsanitize=address,undefined $^ -o $@

.PHONY: check-resolve
check-resolve: build/test_resolve
	ASAN_OPTIONS=detect_leaks=0 ./build/test_resolve
	ASAN_OPTIONS=detect_leaks=0 ./build/test_resolve allocation-failure


build/native-settings_io.o: native/settings_io.c src/display_options.h
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@
build/native-cstick.o: native/cstick.c src/display_options.h
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@

build/test_settings_io: native/settings_io.c src/display_options.c tests/test_settings_io.c src/display_options.h
	$(CC) $(WARN) -DMM_NATIVE_HOST_TEST -fno-pie -no-pie -g -fsanitize=address,undefined native/settings_io.c src/display_options.c tests/test_settings_io.c -o $@
build/test_cstick: native/cstick.c src/display_options.c tests/test_cstick.c src/display_options.h
	$(CC) $(WARN) -g -fsanitize=address,undefined native/cstick.c src/display_options.c tests/test_cstick.c -o $@
.PHONY: check-settings
check-settings: build/test_settings_io build/test_cstick
	ASAN_OPTIONS=detect_leaks=0 python3 tests/test_settings_journal.py
	ASAN_OPTIONS=detect_leaks=0 ./build/test_cstick

build/native-options_motion.o: native/options_motion.c native/options_motion.h
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@

build/test_options_motion: native/options_motion.c native/options_motion.h tests/test_options_motion.c
	$(CC) $(WARN) -g -fsanitize=address,undefined native/options_motion.c tests/test_options_motion.c -o $@
.PHONY: check-motion
check-motion: build/test_options_motion
	ASAN_OPTIONS=detect_leaks=0 ./build/test_options_motion
	python3 tests/test_options_motion_assets.py
