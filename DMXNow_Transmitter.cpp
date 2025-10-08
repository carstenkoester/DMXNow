/*
  DMXNow_Transmitter.cpp - Library for transmitting DMX using ESP32 ESP-Now
  Carsten Koester, carsten@ckoester.net, Apr-2025.
  Released into the public domain.
*/

#include "DMXNow_Transmitter.h"
#include <esp_task_wdt.h>

#include "WiFi.h"

/*
 * Static members
 */
unsigned char DMXNow_Transmitter::wifiChannel = 1;
// esp_task_wdt_user_handle_t DMXNow_Transmitter::wdt_handle;
void (*DMXNow_Transmitter::_transmitCallback)() = nullptr;
DMXNow_Transmitter::ESP_NOW_Broadcast_Peer_Class* DMXNow_Transmitter::_peer = nullptr;

unsigned int DMXNow_Transmitter::_txCount = 0;
unsigned int DMXNow_Transmitter::_txFail = 0;
unsigned long DMXNow_Transmitter::_txMillis;


DMXNow_Transmitter::DMXNow_Transmitter()
{
}

void DMXNow_Transmitter::begin(uint8_t channel, uint16_t universe, void (*transmitCallback)())
{
  uint8_t mac[6];

  // Callback
  _transmitCallback = transmitCallback;

  // WiFi
  wifiChannel = channel;

  // Initialize the Wi-Fi module
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

  WiFi.macAddress(mac);
  Serial.printf("DMXNow Transmitter\n");
  Serial.printf("Wi-Fi parameters:\n");
  Serial.printf("  Mode: STA\n");
  Serial.printf("  MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("  Channel: %d\n", wifiChannel);

  // Register the broadcast peer
  _peer = new ESP_NOW_Broadcast_Peer_Class(wifiChannel, WIFI_IF_STA, NULL);

  if (!_peer->begin()) {
    Serial.println("Failed to initialize broadcast peer");
    Serial.println("Rebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

  // Initialize the static components of the DMX-Now packet
  dmxNowPacket.magic = dmxnow_magic;
  dmxNowPacket.version = 1;
  dmxNowPacket.priority = 100;
  dmxNowPacket.universe = universe;
  dmxNowPacket.type = 0;

  // In current version of code, we will always be transmitting complete buffers
  dmxNowPacket.offset = 0;
  dmxNowPacket.length = DMX_BUFSIZE_WITH_START_CODE;

  memset(&dmxNowPacket.payload, 0x00, DMX_BUFSIZE_WITH_START_CODE);

  // Pointer to the actual DMX slots
  dmxBuffer = &dmxNowPacket.payload[1];

  // Start the output task
  xTaskCreatePinnedToCore(
    _startDMXTransmitThread, /* Function to implement the task */
    "DMX Transmit Thread",   /* Name of the task */
    10000,                   /* Stack size in words */
    this,                    /* Task input parameter */
    0,                       /* Priority of the task */
    &_dmxTransmitTask,       /* Task handle. */
    0                        /* Core where the task should run */
  );
}

bool DMXNow_Transmitter::setValue(unsigned int address, uint8_t value) {
  if (address > 512) {
    return false;
  }

  if (dmxNowPacket.payload[address] == value) {
    return false;
  }

  dmxNowPacket.payload[address] = value;
  _transmit(true);
  return true;
}

bool DMXNow_Transmitter::setValues(unsigned int startAddress, unsigned int length, void* buffer) {
  if (startAddress + length - 1 > 512) {
    return false;
  }
  if (memcmp(&dmxNowPacket.payload[startAddress], buffer, length) == 0) {
    return false;
  }

  memcpy(&dmxNowPacket.payload[startAddress], buffer, length);
  _transmit(true);
  return true;
}

bool DMXNow_Transmitter::_transmit(bool changed)
{
  // (Try to) make this thread safe in case both the periodic update and the ad-hoc update occur at the same time
  std::lock_guard<std::mutex> lock(_transmitLock);

  dmxNowPacket.sequence = dmxnow_sequence++;

  if (changed) {
    dmxNowPacket.flags = dmxnow_flag_t::DMXNOW_FLAG_NEW;
  } else {
    dmxNowPacket.flags = dmxnow_flag_t::DMXNOW_FLAG_NONE;
  }

  if (!_peer->send_message((uint8_t*) &dmxNowPacket, DMXNOW_HEADER_SIZE+DMX_BUFSIZE_WITH_START_CODE)) {
    _txFail++;
    return(false);
  }

  _txCount++;
  _txMillis = millis();
  
  if (_transmitCallback) {
    _transmitCallback();
  }

  return(true);
}


void DMXNow_Transmitter::_startDMXTransmitThread(void* _this)
{
  ((DMXNow_Transmitter*)_this)->_dmxTransmitLoop();
}

void DMXNow_Transmitter::_dmxTransmitLoop()
{
  while (true) {
    if (millis() >= _txMillis + 250) {
      _transmit(false);
    }
  }
}