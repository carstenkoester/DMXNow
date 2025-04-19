/*
  DMXNow_Receiver.cpp - Library for receiving DMX using ESP32 ESP-Now
  Carsten Koester, carsten@ckoester.net, May-2024.
  Released into the public domain.
*/

#include "DMXNow_Receiver.h"
#include <esp_task_wdt.h>

#include "WiFi.h"

/*
 * Static members
 */
unsigned char DMXNow_Receiver::wifiChannel = 1;
esp_task_wdt_user_handle_t DMXNow_Receiver::wdt_handle;
void (*DMXNow_Receiver::_receiveCallback)() = nullptr;
DMXNow_Receiver::ESP_NOW_Peer_Class* DMXNow_Receiver::_peer = nullptr;

unsigned int DMXNow_Receiver::_rxCount = 0;
unsigned int DMXNow_Receiver::_rxInvalid = 0;
unsigned int DMXNow_Receiver::_rxOverruns = 0;
unsigned int DMXNow_Receiver::_rxSeqErrors = 0;

void DMXNow_Receiver::_register_new_peer(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg)
{
  if (!memcmp(info->des_addr, ESP_NOW.BROADCAST_ADDR, 6) == 0) {
    // The receiver will only process broadcast messages
    Serial.printf("Ignoring unicast message from " MACSTR, MAC2STR(info->src_addr));
    return;
  }

  Serial.printf("Unknown peer " MACSTR " sent a broadcast message\n", MAC2STR(info->src_addr));
  if (_peer) {
    Serial.printf("Already have a peer. Ignoring new peer.\n");
    return;
  }
  Serial.println("Registering the peer as a transmitter");

  _peer = new ESP_NOW_Peer_Class(info->src_addr, wifiChannel, WIFI_IF_STA, NULL, (uint8_t *) arg);

  if (!_peer->add_peer()) {
    Serial.println("Failed to register the new peer");
    return;
  }
  esp_err_t r =  esp_now_set_peer_rate_config(info->src_addr, &dmxnow_rate_config);
  if (r != ESP_OK) {
    Serial.printf("Failed to set ESP-Now rate: %d\n", r);
    return;
  }
}

DMXNow_Receiver::DMXNow_Receiver()
{
}

void DMXNow_Receiver::begin(uint8_t channel, void (*receiveCallback)())
{
  uint32_t esp_now_version;

  // Callback
  _receiveCallback = receiveCallback;

  // WiFi
  wifiChannel = channel;
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(wifiChannel);
  if (debug) {
    Serial.printf("Starting WiFi");
  }

  while (!WiFi.STA.started()) {
    delay(100);
    if (debug) {
      Serial.printf(".");
    }
  }
  if (debug) {
    Serial.printf("\n");
  }

  Serial.printf("DMXNow Receiver\n");
  Serial.printf("Wi-Fi parameters:\n");
  Serial.printf("  Mode: STA\n");
  Serial.printf("  MAC Address: %s\n", WiFi.macAddress());
  Serial.printf("  Channel: %d\n", wifiChannel);

  // ESP-Now
  if (!ESP_NOW.begin()) {
    Serial.println("Failed to initialize ESP-NOW");
    Serial.println("Reeboting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

  // Print the ESP-Now version (for debugging purposes)
  esp_err_t r2 = esp_now_get_version(&esp_now_version);
  Serial.printf("ESP-Now version: %d\n", esp_now_version);

  // Register the new peer callback
  ESP_NOW.onNewPeer(_register_new_peer, &dmxBuffer);

  // Clear the DMX buffer
  memset(&dmxBuffer, 0x00, sizeof(dmxBuffer)); // Clear DMX buffer
}