# Build test harness for ESPHome Dehumidifier component.
# Uses mock ESPHome headers so the component code compiles with host g++/clang++.

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -g -O0

# Include paths: the mock headers shadow the real ESPHome includes
#    -I.          → mock_esphome.h, esphome forwarding headers
#    -I..         → component source files (midea_dehum.h, etc.)
INCLUDES := -I. -I..

# Compile flags — enable V1 + V2 + V3 + all optional features we want to test
DEFINES  := -DMIDEA_PROTOCOL_V1 -DMIDEA_PROTOCOL_V2 -DMIDEA_PROTOCOL_V3 \
            -DMIDEA_PROTOCOL_AUTO               \
            -DUSE_MIDEA_DEHUM_HANDSHAKE           \
            -DUSE_MIDEA_DEHUM_ERROR               \
            -DUSE_MIDEA_DEHUM_BEEP                \
            -DUSE_MIDEA_DEHUM_ION                 \
            -DUSE_MIDEA_DEHUM_PUMP                \
            -DUSE_MIDEA_DEHUM_SLEEP               \
            -DUSE_MIDEA_DEHUM_LIGHT               \
            -DUSE_MIDEA_DEHUM_SWING               \
            -DUSE_MIDEA_DEHUM_HORIZONTAL_SWING    \
            -DUSE_MIDEA_DEHUM_FILTER              \
            -DUSE_MIDEA_DEHUM_FILTER_BUTTON       \
            -DUSE_MIDEA_DEHUM_RESET_WATER_LEVEL   \
            -DUSE_MIDEA_DEHUM_TIMER               \
            -DUSE_MIDEA_DEHUM_CAPABILITIES

# All component source files (except __init__.py)
COMPONENT_SRC := ../midea_dehum.cpp          \
                 ../midea_dehum_uart.cpp      \
                 ../midea_dehum_features.cpp  \
                 ../midea_dehum_state.cpp     \
                 ../midea_dehum_protocol_v1.cpp \
                 ../midea_dehum_protocol_v2.cpp \
                 ../midea_dehum_protocol_v3.cpp \
                 ../midea_dehum_protocol_auto.cpp

COMPONENT_OBJS := $(notdir $(COMPONENT_SRC:.cpp=.o))
OBJS := test_harness.o $(COMPONENT_OBJS)
DEPFLAGS := -MMD -MP

.PHONY: all clean run-v1 run-v2 run-v3 run-all test test-all test-asan standalone

all: test_harness

# ── Combined harness ────────────────────────────────────────────────────

test_harness: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Generated dependencies include fixtures, included test suites, and headers.
# Rebuild existing objects once when these build rules change.
$(OBJS): Makefile

test_harness.o: test_harness.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) $(DEFINES) $(DEPFLAGS) -c -o $@ $<

$(COMPONENT_OBJS): %.o: ../%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) $(DEFINES) $(DEPFLAGS) -c -o $@ $<

run-v1: test_harness
	./test_harness v1

run-v2: test_harness
	./test_harness v2

run-v3: test_harness
	./test_harness v3

run-all: test_harness
	./test_harness all

# ── CI targets ──────────────────────────────────────────────────────────

test test-all: run-all

# ASAN: rebuild with address sanitizer and run
test-asan: clean
	$(MAKE) CXXFLAGS="-std=c++17 -fsanitize=address -g -O0"
	./test_harness all

# ── Standalone single-suite executables ─────────────────────────────────

STANDALONE_OBJS := $(COMPONENT_OBJS)

standalone: test_handshake_solo test_commands_solo test_e2e_solo test_system_solo

test_%_solo: test_%.cpp fixtures.h mock_esphome.h $(STANDALONE_OBJS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) $(DEFINES) $(DEPFLAGS) $(LDFLAGS) -o $@ $< $(STANDALONE_OBJS) $(LDLIBS)

clean:
	rm -f test_harness *.o *.d *_solo

-include $(OBJS:.o=.d) $(wildcard *_solo.d)
