/*
  DMXNow_Receiver.cpp - Library for receiving DMX using a Wireless module
  Carsten Koester, ckoester@cisco.com, May-2024.
  Released into the public domain.
*/

#include "DMXNow_Receiver.h"
#include <esp_task_wdt.h>

#include "WiFi.h"

unsigned char DMXNow_Receiver::wifiChannel = 1;
std::vector<DMXNow_Receiver::ESP_NOW_Peer_Class> DMXNow_Receiver::_peers;

unsigned int DMXNow_Receiver::_rxCount = 0;
unsigned int DMXNow_Receiver::_rxSeqErrors = 0;
uint16_t DMXNow_Receiver::_last_sequence_number = 0;

esp_now_rate_config_t rate_config = {
  .phymode = WIFI_PHY_MODE_HT20,    
  .rate = WIFI_PHY_RATE_MCS1_SGI,     
  .ersu = false,                     
  .dcm = false                       
};
  

void DMXNow_Receiver::ESP_NOW_Peer_Class::onReceive(const uint8_t *data, size_t len, bool broadcast) {
  dmxnow_packet_t* dmxnow;

  dmxnow = (dmxnow_packet_t *) data;

  Serial.printf("Received a message from master " MACSTR " (%s)\n", MAC2STR(addr()), broadcast ? "broadcast" : "unicast");
  Serial.printf("univ %d seq %d seg 0x%02x len %d minus hdr %d\n", dmxnow->universe, dmxnow->sequence, dmxnow->segments, len, len-16);

  if (dmxnow->sequence != _last_sequence_number + 1) {
    _rxSeqErrors++;
  }

/*
  if ((dmxnow->flags & dmxnow_flag_t::DMXNOW_FLAG_FIRST) == dmxnow_flag_t::DMXNOW_FLAG_FIRST) {
    memcpy(_dmxBuffer, (dmxnow->payload)+1, dmxnow->length-1); // Don't copy the start code
  } else {
    memcpy(_dmxBuffer+dmxnow->offset-1, dmxnow->payload, dmxnow->length); // Offset -1 to adjust for the start code
  }
*/
  memcpy(_dmxBuffer, (dmxnow->payload)+1, len-16);
  _rxCount++;
  _last_sequence_number = dmxnow->sequence;
}

bool DMXNow_Receiver::ESP_NOW_Peer_Class::add_peer()
{
  if (!add()) {
    log_e("Failed to register the broadcast peer");
    return false;
  }
  return true;
}

void DMXNow_Receiver::_register_new_peer(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg)
{
  if (memcmp(info->des_addr, ESP_NOW.BROADCAST_ADDR, 6) == 0) {
    Serial.printf("Unknown peer " MACSTR " sent a broadcast message\n", MAC2STR(info->src_addr));
    Serial.println("Registering the peer as a transmitter");

    ESP_NOW_Peer_Class new_peer(info->src_addr, wifiChannel, WIFI_IF_STA, NULL, (uint8_t *) arg);

    _peers.push_back(new_peer);
    if (!_peers.back().add_peer()) {
      Serial.println("Failed to register the new peer");
      return;
    }
    esp_err_t r =  esp_now_set_peer_rate_config(info->src_addr, &rate_config);
    if (r != ESP_OK) {
      Serial.printf("Failed to set ESP-Now rate: %d\n", r);
      return;
    } else {
      Serial.println("Rate config set OK");
    }
  } else {
    // The slave will only receive broadcast messages
    Serial.printf("Received a unicast message from " MACSTR, MAC2STR(info->src_addr));
    Serial.printf("Igorning the message");
  }
  uint32_t espver;
  esp_err_t r2 = esp_now_get_version(&espver);
  if (r2 != ESP_OK) {
    Serial.printf("Failed to get ESP-Now version: %d\n", r2);
  } else {
    Serial.printf("ESP-Now version: %d\n", espver);
  }

}

DMXNow_Receiver::DMXNow_Receiver(int statusLEDPin)
{
  _statusLEDPin = statusLEDPin;
}

void DMXNow_Receiver::begin(uint8_t channel)
{
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

  // Initialize the ESP-NOW protocol
  if (!ESP_NOW.begin()) {
    Serial.println("Failed to initialize ESP-NOW");
    Serial.println("Reeboting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

  // Register the new peer callback
  ESP_NOW.onNewPeer(_register_new_peer, &dmxBuffer);

  // Clear the DMX buffer
  memset(&dmxBuffer, 0x00, sizeof(dmxBuffer)); // Clear DMX buffer

//  esp_task_wdt_add(_dmxReceiveTask);
//  esp_err_t esp_task_wdt_add_user(const char *user_name, esp_task_wdt_user_handle_t *user_handle_ret)
}