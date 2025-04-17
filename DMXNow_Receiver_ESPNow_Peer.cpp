#include "DMXNow_Receiver.h"
#include <esp_task_wdt.h>

#include "WiFi.h"

DMXNow_Receiver::ESP_NOW_Peer_Class::ESP_NOW_Peer_Class(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface, const uint8_t *lmk, uint8_t* dmxBuffer) : ESP_NOW_Peer(mac_addr, channel, iface, lmk)
{
  _dmxBuffer = dmxBuffer;
}

void DMXNow_Receiver::ESP_NOW_Peer_Class::setReceiveCallback(std::function<void()> receiveCallback)
{
  // _receiveCallback = receiveCallback;
}

void DMXNow_Receiver::ESP_NOW_Peer_Class::onReceive(const uint8_t *data, size_t len, bool broadcast)
{
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

  if (dmxnow->sequence != _last_sequence_number + 1) {
    _rxSeqErrors++;
    if (dmxnow->sequence <= _last_sequence_number) {
      Serial.printf("exp %d got %d delta %d\n",  _last_sequence_number + 1, dmxnow->sequence, dmxnow->sequence - _last_sequence_number + 1);
    }
  }

  memcpy(_dmxBuffer+dmxnow->offset, dmxnow->payload+1, dmxnow->length-1);

  // Housekeeping: Invoke callback function, reset WDT, increment statistics
  if (_receiveCallback) {
    _receiveCallback();
  }
  esp_task_wdt_reset_user(wdt_handle);
  _rxCount++;
  _last_sequence_number = dmxnow->sequence;
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