#ifndef DMXNow_h
#define DMXNow_h

#define DMXNOW_HEADER_SIZE 16

typedef union {
  uint8_t magic_char[4];
  uint32_t magic;
} dmxnow_magic_t;

const dmxnow_magic_t dmxnow_magic = {0x44, 0x6D, 0x58, 0x4E};

enum class dmxnow_flag_t : uint8_t {
  DMXNOW_FLAG_NONE          = 0x00,
  DMXNOW_FLAG_RESET         = 0x01,
  DMXNOW_FLAG_NEW           = 0x02,
  DMXNOW_FLAG_FIRST         = 0x04,
  DMXNOW_FLAG_LAST          = 0x08
};

typedef struct {
    dmxnow_magic_t magic;      // "DmXN"
    uint8_t version;
    uint8_t priority;
    uint16_t universe;
    uint16_t sequence;
    uint8_t type;
    dmxnow_flag_t flags;
    uint8_t segments;          // 4-bit "expected segments"; 4-bit "segment ID"
    uint8_t length;
    uint16_t offset;

    uint8_t payload[234];
} dmxnow_packet_t;

#endif
