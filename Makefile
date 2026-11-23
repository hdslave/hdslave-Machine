# hdslave Machine: a CLAP synth for the Akai MPC Sample (64-bit ARM Linux, Cortex-A55).
# Build inside a Linux arm64 image with ./build.sh, which runs `make all test`.
#   make            build/Machine.clap
#   make test       the checks against the built plug-in (tests/host.cpp) and the sound's fingerprint
#                   (tests/fingerprint.cpp: the output to the bit, against tests/fingerprint.txt)
#   make sanitize   the host tests against a build with AddressSanitizer and UBSan
#   make bench      build/bench: renders busy patches; tools/bench.sh counts its instructions
# The screen (ui/) builds on its own: make -C ui.
CXX ?= g++
OUT := build
WARN := -Wall -Wextra -Werror
# The MPC's cores. -ffp-contract=fast lets the compiler fuse multiply-adds; nothing here relies on IEEE errno, traps
# or exceptions.
OPT := -O3 -mcpu=cortex-a55 -ffp-contract=fast -fno-math-errno -fno-trapping-math
CXXFLAGS := -std=c++17 $(OPT) $(WARN) -fno-exceptions -fno-rtti -fvisibility=hidden -fPIC -Ithird_party/clap/include -Isrc
# Only clap_entry is exported; libstdc++ and libgcc are linked in so the MPC needs nothing beyond libc and libm.
LDFLAGS := -shared -s -static-libgcc -static-libstdc++ -Wl,--gc-sections -Wl,-z,now -Wl,-O1 -Wl,--version-script=src/exports.map

SOURCES := src/machine.cpp
HEADERS := $(wildcard src/*.h)
# The screen (ui/): built in when its sources are there, MACHINE_SCREEN on. UI_DIR names another copy.
UI_DIR ?= ui
UI_SOURCES := $(wildcard $(UI_DIR)/machine_ui.cpp $(UI_DIR)/context.cpp)
ifneq ($(UI_SOURCES),)
SCREEN := -DMACHINE_SCREEN=1 -I$(UI_DIR)
UI_OBJECTS := $(patsubst $(UI_DIR)/%.cpp,$(OUT)/ui/%.o,$(UI_SOURCES))
endif

.PHONY: all test bench clean fingerprint-update sanitize
all: $(OUT)/Machine.clap

$(OUT)/Machine.clap: $(SOURCES) $(HEADERS) $(UI_OBJECTS) src/exports.map Makefile
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) $(SCREEN) $(SOURCES) $(UI_OBJECTS) $(LDFLAGS) -o $@

$(OUT)/ui/%.o: $(UI_DIR)/%.cpp $(wildcard $(UI_DIR)/*.h) Makefile
	@mkdir -p $(OUT)/ui
	$(CXX) -std=c++17 $(OPT) -fno-exceptions -fno-rtti -fvisibility=hidden -fPIC -Wall -Wextra -I$(UI_DIR) -c $< -o $@

$(OUT)/host: tests/host.cpp tests/wav.h Makefile
	@mkdir -p $(OUT)
	$(CXX) -std=c++17 -O2 $(WARN) -Ithird_party/clap/include tests/host.cpp -ldl -o $@

$(OUT)/fingerprint: tests/fingerprint.cpp tests/fingerprint_scenarios.h Makefile
	@mkdir -p $(OUT)
	$(CXX) -std=c++17 -O2 $(WARN) -Ithird_party/clap/include tests/fingerprint.cpp -ldl -o $@

test: $(OUT)/Machine.clap $(OUT)/host $(OUT)/fingerprint
	$(OUT)/host $(OUT)/Machine.clap $(OUT)
	$(OUT)/fingerprint $(OUT)/Machine.clap tests/fingerprint.txt > /dev/null

# The sound's hashes after a change meant to change the sound (see tests/fingerprint.cpp).
fingerprint-update: $(OUT)/Machine.clap $(OUT)/fingerprint
	$(OUT)/fingerprint $(OUT)/Machine.clap > tests/fingerprint.txt

SAN := -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g -O1
SAN_ENV := ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1:abort_on_error=1
sanitize: $(SOURCES) $(HEADERS) $(UI_SOURCES) tests/host.cpp tests/wav.h Makefile
	@mkdir -p $(OUT)/san
	$(CXX) -std=c++17 $(SAN) -mcpu=cortex-a55 $(WARN) -fPIC -shared -Ithird_party/clap/include -Isrc $(SCREEN) $(SOURCES) $(UI_SOURCES) -o $(OUT)/san/Machine.clap
	$(CXX) -std=c++17 $(SAN) $(WARN) -Ithird_party/clap/include tests/host.cpp -ldl -o $(OUT)/san/host
	$(SAN_ENV) $(OUT)/san/host $(OUT)/san/Machine.clap $(OUT)/san

$(OUT)/bench: tests/bench.cpp $(SOURCES) $(HEADERS) Makefile
	@mkdir -p $(OUT)
	$(CXX) -std=c++17 $(OPT) $(WARN) -g -Ithird_party/clap/include -Isrc tests/bench.cpp -o $@

bench: $(OUT)/bench

clean:
	rm -rf $(OUT)
