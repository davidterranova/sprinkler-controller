#pragma once
//
//  CRC32 over the persisted records.
//
//  ESPHome's own preference layer already checksums, but it also calls
//  nvs_flash_erase() when NVS fails to open -- i.e. **a corrupt NVS silently
//  wipes every preference** [V, docs/07-firmware.md]. That is why the schedule
//  record carries its own version and CRC and is written to two banks: so the
//  firmware can tell "never written" from "written and damaged", and fall back
//  to the older intact bank rather than to a blank schedule.
//
#include <cstddef>
#include <cstdint>

namespace irrigation_core {

// Bitwise CRC-32 (IEEE 802.3, reflected). No 1 KiB table: this runs a few
// hundred bytes a minute at most, and flash is the scarcer resource.
inline uint32_t crc32(const void *data, size_t len, uint32_t seed = 0xFFFFFFFFu) {
  const uint8_t *p = static_cast<const uint8_t *>(data);
  uint32_t crc = seed;
  for (size_t i = 0; i < len; i++) {
    crc ^= p[i];
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = (crc >> 1) ^ (0xEDB88320u & (~(crc & 1u) + 1u));
  }
  return ~crc;
}

// A record wrapper: {magic, version, sequence, payload, crc}. The bank with the
// higher sequence number wins, provided its CRC checks out.
template<typename T> struct Record {
  uint32_t magic{0x49525247u};  // "IRRG"
  uint16_t version{0};
  uint16_t reserved{0};
  uint32_t sequence{0};
  T payload{};
  uint32_t crc{0};

  void seal() { this->crc = crc32(this, offsetof(Record, crc)); }
  bool valid(uint16_t expect_version) const {
    return this->magic == 0x49525247u && this->version == expect_version &&
           this->crc == crc32(this, offsetof(Record, crc));
  }
};

}  // namespace irrigation_core
