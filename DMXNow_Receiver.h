#ifndef DMXNow_Receiver_h
#define DMXNow_Receiver_h

#include "Arduino.h"
#include "DMXNow.h"
#include "ESP32_NOW.h"

#include <esp_task_wdt.h>

#define DMX_BUFSIZE                   512   // Total number of slots in a DMX universe

const uint8_t INVALID_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 

class DMXNow_Receiver
{
  class ESP_NOW_Peer_Class : public ESP_NOW_Peer {
    public:
      ESP_NOW_Peer_Class(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface, const uint8_t *lmk, uint8_t* dmxBuffer);    
      ~ESP_NOW_Peer_Class() {}
      
      bool add_peer();
      void onReceive(const uint8_t *data, size_t len, bool broadcast);
    
    private:
      uint8_t* _dmxBuffer;
      static uint16_t _expected_sequence;
  };

  public:
    DMXNow_Receiver();

    void begin(uint8_t channel, uint16_t universe, void (*receiveCallback)());

    uint8_t getValue(unsigned int address) const { return (dmxBuffer[address-1]); };
    void getValues(unsigned int startAddress, unsigned int length, void* buffer) const { memcpy(buffer, &dmxBuffer[startAddress-1], length); };

    const bool isLocked() const { return((_peer != nullptr) && (_rxCount > 3)); }; // Wait for at least three valid packets before declaring victory
    const unsigned char* getXmtr() const { return (_peer ? _peer->addr() : (const unsigned char*) &INVALID_MAC); };

    unsigned int rxCount() const { return (_rxCount); };
    unsigned int rxInvalid() const { return (_rxInvalid); };
    unsigned int rxOverruns() const { return (_rxOverruns); };
    unsigned int rxSeqErrors() const { return (_rxSeqErrors); };
    unsigned int rxWrongUniverse() const { return (_rxWrongUniverse); };


    uint8_t dmxBuffer[DMX_BUFSIZE];
    bool debug;

    static unsigned char wifiChannel;
    static uint16_t dmxUniverse;

  
  private:
    // FIXME: We should either make the entire class static, or find a way to manage this better in instances.
    // Realistically we will NOT run two instances of ESP-Now on one ESP32....
    static void _register_new_peer(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg);
    static ESP_NOW_Peer_Class *_peer;


    static unsigned int _rxCount;         // Number of frames received
    static unsigned int _rxInvalid;       // Number of frames with invalid header
    static unsigned int _rxOverruns;      // Number of times RF24 returned FifoFull when we were processing a frame 
    static unsigned int _rxSeqErrors;     // Number of times we detected a gap in sequence numbers
    static unsigned int _rxWrongUniverse; // Number of packets received on a different DMX universe

    static esp_task_wdt_user_handle_t wdt_handle;
    static void (*_receiveCallback)();
};

#endif