#ifndef DMXNow_Transmitter_h
#define DMXNow_Transmitter_h

#include <mutex>

#include "Arduino.h"
#include "DMXNow.h"
#include "ESP32_NOW.h"

#include <esp_task_wdt.h>

#define DMX_BUFSIZE                   512   // Total number of slots in a DMX universe, exluding start code
#define DMX_BUFSIZE_WITH_START_CODE   513   // DMX start code plus total number of slots in a DMX universe

class DMXNow_Transmitter
{
  class ESP_NOW_Broadcast_Peer_Class : public ESP_NOW_Peer {
    public:
      ESP_NOW_Broadcast_Peer_Class(uint8_t channel, wifi_interface_t iface, const uint8_t *lmk);
      ~ESP_NOW_Broadcast_Peer_Class();

      bool begin();
      bool send_message(const uint8_t* data, size_t len);
  };
  
  public:
    DMXNow_Transmitter();

    void begin(uint8_t channel, uint16_t universe, void (*transmitCallback)() = nullptr);

    const uint8_t getValue(unsigned int address) const { return (dmxNowPacket.payload[address]); };
    const void getValues(unsigned int startAddress, unsigned int length, void* buffer) const { memcpy(buffer, &dmxNowPacket.payload[startAddress], length); };

    bool setValue(unsigned int address, unsigned char value);                     // Return true if changed
    bool setValues(unsigned int startAddress, unsigned int length, void* buffer); // Return true if changed

    const unsigned int txCount() const { return (_txCount); };
    const unsigned int txFail() const { return (_txFail); };

    bool debug;
    dmxnow_packet_t dmxNowPacket;
    uint8_t* dmxBuffer;

    static unsigned char wifiChannel;

  private:
    static void _register_new_peer(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg);
    bool _transmit(bool changed);

    static ESP_NOW_Broadcast_Peer_Class *_peer;

    static unsigned int _txCount;         // Number of frames sent
    static unsigned int _txFail;          // Number of send failures
    static unsigned long _txMillis;       // Timestamp of last transmission

    static esp_task_wdt_user_handle_t wdt_handle;
    static void (*_transmitCallback)();

    void _dmxTransmitLoop();
    static void _startDMXTransmitThread(void*);

    uint16_t dmxnow_sequence;
    TaskHandle_t _dmxTransmitTask;
    std::mutex _transmitLock;
};

#endif