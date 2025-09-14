#include "DMXNow_Transmitter.h"
#include <esp_task_wdt.h>

#include "WiFi.h"

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

DMXNow_Transmitter::ESP_NOW_Broadcast_Peer_Class::ESP_NOW_Broadcast_Peer_Class(uint8_t channel, wifi_interface_t iface, const uint8_t *lmk) : 
  ESP_NOW_Peer(ESP_NOW.BROADCAST_ADDR, channel, iface, lmk)
{
}

DMXNow_Transmitter::ESP_NOW_Broadcast_Peer_Class::~ESP_NOW_Broadcast_Peer_Class()
{
  remove();
}

bool DMXNow_Transmitter::ESP_NOW_Broadcast_Peer_Class::begin()
{
  esp_err_t esp_err;
  uint32_t espnow_version;

  if (!ESP_NOW.begin() || !add()) {
    Serial.printf("Failed to initialize ESP-NOW or register the broadcast peer\n");
    return(false);
  }

  esp_err =  esp_now_set_peer_rate_config(ESP_NOW.BROADCAST_ADDR, &dmxnow_rate_config);
  if (esp_err != ESP_OK) {
    Serial.printf("Failed to set ESP-Now rate\n");
    return(false);
  }

  esp_now_get_version(&espnow_version);
  Serial.printf("ESP-Now version %d\n", espnow_version);
    
  return true;
}

bool DMXNow_Transmitter::ESP_NOW_Broadcast_Peer_Class::send_message(const uint8_t* data, size_t len) {
  // debugDumpPacket("TX PKT", &data, len);
  return (send(data, len));
}