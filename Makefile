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
# Override with `make ota HOST=<ip>` if resolution fails but you know the IP.
HOSTNAME := $(NODE).local
HOST ?= $(shell ping -c1 $(HOSTNAME) 2>/dev/null | sed -nE 's/^PING [^ ]+ \(([0-9.]+)\).*/\1/p')

.PHONY: help secrets validate build flash status kill-port ota ip logs logs-usb clean

help:
	@echo "esp-sprinkler -- ESPHome $(ESPHOME_VERSION), node '$(NODE)'"
	@echo ""
	@echo "  make secrets    Create secrets.yaml from the template, generating fresh keys"
	@echo "  make validate   Parse the YAML and check component schemas (no compile)"
	@echo "  make build      Validate, then fully compile the firmware"
	@echo "  make flash      Build, then flash over USB   (PORT=$(PORT))"
	@echo "  make logs-usb   Stream the serial log over USB -- use before Wi-Fi works"
	@echo "  make status     Query the attached board: chip type, revision, MAC"
	@echo "  make kill-port  Free the serial port if something is holding it"
	@echo "  make ota        Build, then push over the network (needs Wi-Fi up)"
	@echo "  make logs       Stream the log over the native API (needs Wi-Fi up)"
	@echo "  make ip         Resolve $(HOSTNAME) via mDNS"
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
	@test -n "$(HOST)" || (echo "Could not resolve $(HOSTNAME) -- is the device on Wi-Fi and reachable? Pass HOST=<ip> to override." && exit 1)
	$(ESPHOME) upload $(CONFIG) --device $(HOST)

# Resolves the node's current IP via mDNS (same lookup `ota` uses).
ip:
	@test -n "$(HOST)" || (echo "Could not resolve $(HOSTNAME) -- is the device on Wi-Fi and reachable? Pass HOST=<ip> to override." && exit 1)
	@echo "$(HOSTNAME) -> $(HOST)"

# Streams the device's live log over Wi-Fi (native API, port 6053) -- no USB
# needed. Ctrl-C to stop.
logs:
	@test -n "$(HOST)" || (echo "Could not resolve $(HOSTNAME) -- is the device on Wi-Fi and reachable? Pass HOST=<ip> to override." && exit 1)
	docker run --rm -it -v "$(CURDIR):/config" ghcr.io/esphome/esphome:$(ESPHOME_VERSION) logs $(CONFIG) --device $(HOST) --states

# Serial log over USB. Needed during bootstrap, before Wi-Fi credentials are
# in place -- `make logs` can't work until the device has joined the network.
# Ctrl-A then K to quit screen.
logs-usb:
	@test -n "$(PORT)" || (echo "No ESP32 serial port found." && exit 1)
	@echo "Attaching to $(PORT) at 115200 baud. Ctrl-A then K to quit."
	screen $(PORT) 115200

clean:
	rm -rf .esphome
