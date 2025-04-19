#include "DMXNow_Receiver.h"
#include <esp_task_wdt.h>

#include "WiFi.h"

uint16_t DMXNow_Receiver::ESP_NOW_Peer_Class::_expected_sequence = 0;

static void debugDumpPacket(const char* title, const void* data, const unsigned int len) {
  const unsigned char* buf = (const unsigned char*) data;

  Serial.printf("%s len %d:", title, len);
  for (int i=0; i<len; i++) {
    if (!((i)%32)) {
      Serial.printf("\n%04x:", i);
    }

    if (!((i)%8)) {
      Serial.printf(" ");
    }

    Serial.printf(" %02x", buf[i]);
  }
  Serial.printf("\n");
}

DMXNow_Receiver::ESP_NOW_Peer_Class::ESP_NOW_Peer_Class(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface, const uint8_t *lmk, uint8_t* dmxBuffer) : ESP_NOW_Peer(mac_addr, channel, iface, lmk)
{
  _dmxBuffer = dmxBuffer;
}

void DMXNow_Receiver::ESP_NOW_Peer_Class::onReceive(const uint8_t *data, size_t len, bool broadcast)
{
  // debugDumpPacket("DMXNOW RX", data, len);

  dmxnow_packet_t* dmxnow;
  dmxnow = (dmxnow_packet_t *) data;

  if (dmxnow->magic.magic != dmxnow_magic.magic) {
    // Invalid magic number, ignore
    _rxInvalid++;
    return;
  }
  if (dmxnow->version != 1) {
    // Invalid version, ignore
    _rxInvalid++;
    return;
  }
  if (dmxnow->length > (len - DMXNOW_HEADER_SIZE)) {
    // Packet payload size exceeds outer packet size, ignore
    _rxInvalid++;
    return;
  }

  if (dmxnow->sequence != _expected_sequence) {
    _rxSeqErrors++;
    if (dmxnow->sequence < _expected_sequence) {
      Serial.printf("exp %d got %d delta %d\n",  _expected_sequence, dmxnow->sequence, dmxnow->sequence - _expected_sequence);
    }
  }

  memcpy(_dmxBuffer+dmxnow->offset, dmxnow->payload+1, dmxnow->length-1);

  // Housekeeping: Invoke callback function, reset WDT, increment statistics
  if (_receiveCallback) {
    _receiveCallback();
  }
  esp_task_wdt_reset_user(wdt_handle);
  _rxCount++;
  _expected_sequence = dmxnow->sequence + 1;
}

bool DMXNow_Receiver::ESP_NOW_Peer_Class::add_peer()
{
  if (!add()) {
    log_e("Failed to register the broadcast peer");
    return false;
  }
  esp_task_wdt_add_user("DMXNow_Broadcast_Receiver", &wdt_handle);
  Serial.printf("Added WDT user\n");
  return true;
}