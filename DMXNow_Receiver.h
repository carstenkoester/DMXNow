#ifndef DMXNow_Receiver_h
#define DMXNow_Receiver_h

#include "Arduino.h"
#include "DMXNow.h"
#include "ESP32_NOW.h"

#include <esp_mac.h>  // For the MAC2STR and MACSTR macros
#include <vector>

#define DMX_BUFSIZE                   512   // Total number of slots in a DMX universe

class DMXNow_Receiver
{
  class ESP_NOW_Peer_Class : public ESP_NOW_Peer {
    public:
      // Constructor of the class
      ESP_NOW_Peer_Class(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface, const uint8_t *lmk, uint8_t* dmxBuffer) : ESP_NOW_Peer(mac_addr, channel, iface, lmk)
      {
        _dmxBuffer = dmxBuffer;
      }
      
      ~ESP_NOW_Peer_Class() {}
      
      bool add_peer();
      void onReceive(const uint8_t *data, size_t len, bool broadcast);
    
    private:
      uint8_t* _dmxBuffer;
  };

  public:
    DMXNow_Receiver(int statusLEDPin);

    void begin(uint8_t channel);

    uint8_t getValue(unsigned int address) const { return (dmxBuffer[address-1]); };
    void getValues(unsigned int startAddress, unsigned int length, void* buffer) const { memcpy(buffer, &dmxBuffer[startAddress-1], length); };

    unsigned int rxCount() const { return (_rxCount); };
    unsigned int rxInvalid() const { return (_rxInvalid); };
    unsigned int rxOverruns() const { return (_rxOverruns); };
    unsigned int rxSeqErrors() const { return (_rxSeqErrors); };

    uint8_t dmxBuffer[DMX_BUFSIZE];
    bool debug;

    static unsigned char wifiChannel;

  private:
    // FIXME: We should either make the entire class static, or find a way to manage this better in instances.
    // Realistically we will NOT run two instances of ESP-Now on one ESP32....
    static void _register_new_peer(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg);
    static std::vector<ESP_NOW_Peer_Class> _peers;

    static unsigned int _rxCount;     // Number of frames received
    unsigned int _rxInvalid = 0;   // Number of frames with invalid header
    unsigned int _rxOverruns = 0;  // Number of times RF24 returned FifoFull when we were processing a frame 
    static unsigned int _rxSeqErrors; // Number of times we detected a gap in sequence numbers

    static uint16_t _last_sequence_number;

    int _statusLEDPin;
};

#endif