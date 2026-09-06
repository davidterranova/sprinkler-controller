CONFIG := sprinkler.yaml
NODE := sprinkler-controller
ESPHOME_VERSION := 2026.8.2
ESPHOME := docker run --rm -v "$(CURDIR):/config" ghcr.io/esphome/esphome:$(ESPHOME_VERSION)
BUILD_DIR := .esphome/build/$(NODE)/build

# Discovered rather than hardcoded, so an ESP-IDF upgrade doesn't silently
# break `make flash`. Override with ESPTOOL=/path/to/esptool if needed.
ESPTOOL ?= $(shell ls -d $(HOME)/.espressif/tools/python/*/venv/bin/esptool 2>/dev/null | tail -n1)

# Auto-detects the board's USB-serial port; override if you have more than
# one such device plugged in, e.g. `make flash PORT=/dev/cu.usbserial-XXXX`.
PORT ?= $(shell ls /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART* /dev/cu.wchusbserial* /dev/cu.usbmodem* 2>/dev/null | head -n1)

# Resolved on the host (mDNS via macOS's built-in Bonjour) rather than inside
# the container -- Docker Desktop's networking doesn't reliably pass mDNS
# multicast traffic through, so we hand `esphome upload` a plain IP instead.
#
# Three names are tried, first answer wins, because a rename is circular: a
# node renamed in the YAML is still on the network under its OLD name, and it
# cannot be reached at the new `<name>.local` until the firmware carrying that
# name has been uploaded -- which is the thing that needs to reach it. Set
# `OLD_NODE` for that one OTA and the circle breaks:
#
#     make ota OLD_NODE=arrosage
#
# `<node>.lan` is the UniFi local DNS record for the node's static lease. It
# comes from the controller rather than from the device, so it keeps working
# across a rename and does not depend on mDNS crossing the IoT VLAN.
# Override with `make ota HOST=<ip>` if you know the IP and want none of this.
OLD_NODE ?=
NODE_NAMES := $(NODE).local $(NODE).lan $(if $(OLD_NODE),$(OLD_NODE).local)
HOST ?= $(shell for h in $(NODE_NAMES); do \
	ip=$$(ping -c1 -t2 $$h 2>/dev/null | sed -nE 's/^PING [^ ]+ \(([0-9.]+)\).*/\1/p'); \
	[ -n "$$ip" ] && echo $$ip && break; \
	done)

# Repeated by ota, ip and logs -- the three targets that need the device.
NO_HOST := echo "Could not resolve any of: $(NODE_NAMES). Is the device on Wi-Fi and reachable? If you have just renamed the node, its firmware still answers to the old name -- pass OLD_NODE=<previous name>, or HOST=<ip>."

.PHONY: help secrets test purity-gate flash-write-gate ci validate build flash status \
        kill-port ota ip logs logs-usb clean

help:
	@echo "esp-sprinkler -- ESPHome $(ESPHOME_VERSION), node '$(NODE)'"
	@echo ""
	@echo "  make secrets    Create secrets.yaml from the template, generating fresh keys"
	@echo "  make test       Build and run the host unit tests (no device, no Docker)"
	@echo "  make ci         Everything CI runs: test + gates + validate"
	@echo "  make validate   Parse the YAML and check component schemas (no compile)"
	@echo "  make build      Validate, then fully compile the firmware"
	@echo "  make flash      Build, then flash over USB   (PORT=$(PORT))"
	@echo "  make logs-usb   Stream the serial log over USB -- use before Wi-Fi works"
	@echo "  make status     Query the attached board: chip type, revision, MAC"
	@echo "  make kill-port  Free the serial port if something is holding it"
	@echo "  make ota        Build, then push over the network (needs Wi-Fi up)"
	@echo "  make logs       Stream the log over the native API (needs Wi-Fi up)"
	@echo "  make ip         Resolve the node's address (mDNS, then UniFi DNS)"
	@echo "  make clean      Remove the build tree"

# Creates secrets.yaml and generates the two crypto values, so the only thing
# left to fill in by hand is Wi-Fi. Refuses to clobber an existing file.
secrets:
	@test ! -f secrets.yaml || (echo "secrets.yaml already exists -- refusing to overwrite it." && exit 1)
	@cp secrets.yaml.template secrets.yaml
	@key=$$(python3 -c "import os,base64; print(base64.b64encode(os.urandom(32)).decode())"); \
	 ota=$$(python3 -c "import secrets; print(secrets.token_urlsafe(24))"); \
	 sed -i '' "s|\"API_KEY\"|\"$$key\"|" secrets.yaml; \
	 sed -i '' "s|\"OTA_PASSWORD\"|\"$$ota\"|" secrets.yaml
	@sed -i '' 's|"TIMEZONE".*|"Europe/Paris"|' secrets.yaml
	@echo "Created secrets.yaml with a fresh API key and OTA password."
	@echo "Now fill in wifi_ssid, wifi_password, and lat/long if you want them."

# -----------------------------------------------------------------------------
#  Host tests and the two source gates
#
#  These need no Docker, no toolchain and no network -- just a C++17 compiler.
#  That is deliberate: the DST table is the thing most worth being able to run
#  in two seconds, and a test suite that needs a container to start is a test
#  suite that gets run less often.
# -----------------------------------------------------------------------------
CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O1
TEST_BIN := .test-build
CORE_SRC := $(wildcard components/irrigation_core/*.cpp)
TESTS := $(patsubst test/%_test.cpp,%,$(wildcard test/*_test.cpp))

test:
	@mkdir -p $(TEST_BIN)
	@fail=0; \
	for t in $(TESTS); do \
		$(CXX) $(CXXFLAGS) -o $(TEST_BIN)/$$t test/$${t}_test.cpp $(CORE_SRC) || exit 1; \
		$(TEST_BIN)/$$t || fail=1; \
	done; \
	exit $$fail

# The purity gate. irrigation_core must stay compilable on a host with nothing
# but a standard library -- that is what makes the resolver and the state
# machine testable against a table of awkward dates instead of against a
# garden. One stray ESPHome include and the whole property is gone, silently.
purity-gate:
	@if grep -rn 'esphome' components/irrigation_core --include='*.h' --include='*.cpp' \
	     | grep -v '^\S*: *[/*]' | grep -v 'ESPHome'; then \
		echo "PURITY GATE FAILED: irrigation_core references ESPHome outside a comment."; \
		exit 1; \
	fi
	@echo "purity gate: irrigation_core has no ESPHome dependency."

# The flash-write gate. docs/07-firmware.md quantifies the trap exactly: at
# 476 pulses/L and 500 L/day, saving the total on every turbine pulse is
# 238 000 writes/day and destroys the flash in ~52 days [V]. There is no pulse
# path yet -- this gate exists so that the day one arrives, it cannot quietly
# carry a save() with it.
flash-write-gate:
	@if grep -rn 'save(' components --include='*.cpp' \
	     | grep -iE 'pulse|isr|count_cb|on_pulse'; then \
		echo "FLASH WRITE GATE FAILED: a save() appears in a pulse path."; \
		exit 1; \
	fi
	@echo "flash-write gate: no save() in any pulse path."

ci: test purity-gate flash-write-gate validate

# Parses the YAML and checks component schemas, without compiling any C++.
validate:
	$(ESPHOME) config $(CONFIG)

# Validates, then fully compiles the firmware (catches errors inside lambdas
# too, which `validate` alone can't since those are raw C++).
build:
	$(ESPHOME) compile $(CONFIG)

# Flashes over UART via esptool (bundled with the ESP-IDF install), not through
# Docker: Docker Desktop on macOS can't pass a host USB-serial device into the
# container. Offsets come from the build's own flasher_args.json.
flash: build
	@test -n "$(PORT)" || (echo "No ESP32 serial port found. Plug in the board or pass PORT=/dev/cu.xxxx explicitly." && exit 1)
	@test -n "$(ESPTOOL)" || (echo "esptool not found under ~/.espressif -- pass ESPTOOL=/path/to/esptool." && exit 1)
	$(ESPTOOL) --chip esp32 --port $(PORT) --baud 460800 \
		--before default-reset --after hard-reset write-flash \
		--flash-mode dio --flash-size 4MB --flash-freq 40m \
		0x1000 $(BUILD_DIR)/bootloader/bootloader.bin \
		0x8000 $(BUILD_DIR)/partition_table/partition-table.bin \
		0x9000 $(BUILD_DIR)/ota_data_initial.bin \
		0x10000 $(BUILD_DIR)/$(NODE).bin

# Queries the connected board over UART: chip type, revision, MAC, flash size.
status:
	@test -n "$(PORT)" || (echo "No ESP32 serial port found. Plug in the board or pass PORT=/dev/cu.xxxx explicitly." && exit 1)
	$(ESPTOOL) --port $(PORT) flash-id

# Kills whatever process (screen, monitor, another esptool run, ...) is
# holding the serial port open, so flash/status can grab it.
kill-port:
	@test -n "$(PORT)" || (echo "No ESP32 serial port found." && exit 1)
	@pids="$$(lsof -t $(PORT) 2>/dev/null)"; \
	if [ -n "$$pids" ]; then \
		echo "Killing process(es) holding $(PORT): $$pids"; \
		kill $$pids; \
	else \
		echo "$(PORT) is free."; \
	fi

# Pushes a rebuilt firmware over the network (port 3232) instead of USB --
# only works once the device has already joined Wi-Fi.
ota: build
	@test -n "$(HOST)" || ($(NO_HOST) && exit 1)
	$(ESPHOME) upload $(CONFIG) --device $(HOST)

# Resolves the node's current IP (the same lookup `ota` and `logs` use).
ip:
	@test -n "$(HOST)" || ($(NO_HOST) && exit 1)
	@echo "$(NODE) -> $(HOST)"

# Streams the device's live log over Wi-Fi (native API, port 6053) -- no USB
# needed. Ctrl-C to stop.
logs:
	@test -n "$(HOST)" || ($(NO_HOST) && exit 1)
	docker run --rm -it -v "$(CURDIR):/config" ghcr.io/esphome/esphome:$(ESPHOME_VERSION) logs $(CONFIG) --device $(HOST) --states

# Serial log over USB. Needed during bootstrap, before Wi-Fi credentials are
# in place -- `make logs` can't work until the device has joined the network.
# Ctrl-A then K to quit screen.
logs-usb:
	@test -n "$(PORT)" || (echo "No ESP32 serial port found." && exit 1)
	@echo "Attaching to $(PORT) at 115200 baud. Ctrl-A then K to quit."
	screen $(PORT) 115200

clean:
	rm -rf .esphome $(TEST_BIN)
