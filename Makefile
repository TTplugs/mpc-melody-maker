CC ?= cc
CPPFLAGS += -Iinclude -Isrc/engine
ENGINE_SOURCES = src/engine/mm_candidate_engine.c src/engine/mutation3.c src/engine/mutation5.c
ENGINE_FLAGS = -DMM_FIFO_INPUT -DMM_CANDIDATE_ENGINE -ffunction-sections -fdata-sections -Wl,--gc-sections
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += -lm -ldl

.PHONY: all clean
all: build/mm-generate build/mpc_melody_maker-host.so

build/mm-generate: src/mm_core.c src/mm_crypto.c src/mm_fx.c src/mm_creation.c tools/mm_cli.c include/mm_core.h include/mm_crypto.h include/mm_fx.h include/mm_creation.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/mm_core.c src/mm_crypto.c src/mm_fx.c src/mm_creation.c tools/mm_cli.c -o $@ $(LDLIBS)

clean:
	rm -f build/mm-generate

build/mm-generate-arm: src/mm_core.c src/mm_crypto.c src/mm_fx.c src/mm_creation.c tools/mm_cli.c include/mm_core.h include/mm_crypto.h include/mm_fx.h include/mm_creation.h
	mkdir -p build
	arm-linux-gnueabihf-gcc $(CPPFLAGS) $(CFLAGS) src/mm_core.c src/mm_crypto.c src/mm_fx.c src/mm_creation.c tools/mm_cli.c -o $@ $(LDLIBS)

build/mm_params.h: tools/gen_parameters.py
	python3 tools/gen_parameters.py

build/mpc_melody_maker-host.so: src/mm_core.c src/mm_crypto.c src/mm_fx.c src/mm_creation.c src/mm_vst.c $(ENGINE_SOURCES) include/mm_core.h include/mm_crypto.h include/mm_fx.h include/mm_creation.h build/mm_params.h
	$(CC) $(CPPFLAGS) -Ibuild $(CFLAGS) -fPIC $(ENGINE_FLAGS) -DMM_HOST_TEST -shared src/mm_core.c src/mm_crypto.c src/mm_fx.c src/mm_creation.c src/mm_vst.c $(ENGINE_SOURCES) -o $@ -ldl -lpthread $(LDLIBS)

build/mpc_melody_maker.so: src/mm_core.c src/mm_crypto.c src/mm_fx.c src/mm_creation.c src/mm_vst.c $(ENGINE_SOURCES) include/mm_core.h include/mm_crypto.h include/mm_fx.h include/mm_creation.h build/mm_params.h
	arm-linux-gnueabihf-gcc $(CPPFLAGS) -Ibuild $(CFLAGS) -fPIC $(ENGINE_FLAGS) -fvisibility=hidden -shared src/mm_core.c src/mm_crypto.c src/mm_fx.c src/mm_creation.c src/mm_vst.c $(ENGINE_SOURCES) -o $@ -ldl -lpthread $(LDLIBS) -Wl,--no-undefined
