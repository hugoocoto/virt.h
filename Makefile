CC      ?= cc
CXX     ?= c++
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -Werror -g
SAN     ?= -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all

BUILD = build

all: check

$(BUILD):
	mkdir -p $@

# the example only has to build: running it types into the focused window
$(BUILD)/demo: examples/demo.c virt.h | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/test_events: tests/test_events.c virt.h | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) $< -o $@

$(BUILD)/test_uinput: tests/test_uinput.c virt.h | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/two_tu: tests/two_tu_a.c tests/two_tu_b.c virt.h | $(BUILD)
	$(CC) $(CFLAGS) tests/two_tu_a.c tests/two_tu_b.c -o $@

$(BUILD)/cxx_header.o: tests/cxx_header.cpp virt.h | $(BUILD)
	$(CXX) -std=c++11 -Wall -Wextra -Wpedantic -Werror -c $< -o $@

test: $(BUILD)/test_events $(BUILD)/two_tu
	./$(BUILD)/test_events
	./$(BUILD)/two_tu

# real devices: needs root (or access to /dev/uinput and /dev/input/event*)
test-uinput: $(BUILD)/test_uinput
	./$(BUILD)/test_uinput

check: $(BUILD)/demo $(BUILD)/cxx_header.o $(BUILD)/test_uinput test

clean:
	rm -rf $(BUILD)

.PHONY: all test test-uinput check clean
