// If you are using Arduino IDE, the board should be "ESP32S3 Dev Module"
// but you should manually specify a few other board options.
// USB CDC On Boot should be "Enabled",
// Flash Size should be "16MB (128Mb)",
// partition scheme should be "Huge APP (3MB No OTA/1MB SPIFFS)", 
// and PSRAM should be "OPI PSRAM".

// Wired Ethernet related
#include "SPI.h"
#include "Ethernet.h"
#include "sACN.h"

// WiFi/ESP-Now
#include "ESP32_NOW.h"
#include "WiFi.h"
#include "DMXNow.h"
#include <esp_mac.h>                    // For the MAC2STR and MACSTR macros

// Status LED
#include <Adafruit_NeoPixel.h>          // For status LED only


/*
 * Definitions
*/
#define ETH_MISO_PIN                    12
#define ETH_MOSI_PIN                    11
#define ETH_SCLK_PIN                    13
#define ETH_CS_PIN                      14
#define ETH_INT_PIN                     10
#define ETH_RST_PIN                     9
#define ETH_ADDR                        1

#define ESPNOW_WIFI_CHANNEL             11
#define NEOPIXEL_STATUS_LED_PIN         21

uint8_t mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x14, 0x48}; // MAC Adress of your device

/*
 * Runtime
 */

unsigned long msg_sent = 0;
unsigned long msg_fail = 0;
uint16_t dmxnow_sequence = 0;

unsigned long last_sent_millis = 0;

Adafruit_NeoPixel status_led = Adafruit_NeoPixel(1, NEOPIXEL_STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

EthernetUDP sacn;
Receiver recv(sacn);


class ESP_NOW_Broadcast_Peer : public ESP_NOW_Peer {
public:
  ESP_NOW_Broadcast_Peer(uint8_t channel, wifi_interface_t iface, const uint8_t *lmk) : ESP_NOW_Peer(ESP_NOW.BROADCAST_ADDR, channel, iface, lmk) {}
  ~ESP_NOW_Broadcast_Peer() { remove(); }

  bool begin() {
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

  bool send_message(const uint8_t* data, size_t len) {
    return (send(data, len));
  }
};


// Create a broadcast peer object
ESP_NOW_Broadcast_Peer broadcast_peer(ESPNOW_WIFI_CHANNEL, WIFI_IF_STA, NULL);


void receiveSACN() {
  broadcastDmxNow(true);
}

void broadcastDmxNow(bool updated)
{
  dmxnow_packet_t dmxnow;
  unsigned char* dmx;

  dmx = recv.dmx();

  dmxnow.magic = dmxnow_magic;
  dmxnow.version = 1;
  dmxnow.priority = 100;
  dmxnow.universe = 6; // FIXME
  dmxnow.type = 0;

  dmxnow.sequence = dmxnow_sequence++;
  dmxnow.flags = dmxnow_flag_t::DMXNOW_FLAG_NONE;
  dmxnow.offset = 0;
  dmxnow.length = 513;
  dmxnow.payload[0] = 0x00; // Start code
  memcpy(&dmxnow.payload[1], dmx, 512);

  if (broadcast_peer.send_message((uint8_t*) &dmxnow, DMXNOW_HEADER_SIZE+513)) {
    msg_sent++;
    last_sent_millis = millis();
  } else {
    msg_fail++;
  }
}

void newSource() {
	Serial.print("new soure name: ");
	Serial.println(recv.name());
}

void framerate() {
//  Serial.print("Framerate fps: ");
//  Serial.println(recv.framerate());
}

void timeOut() {
	Serial.println("Timeout!");
}

unsigned long last_status;


void setup() {
	Serial.begin(115200);
	delay(100);
	Serial.println("Program start");

  SPI.begin(ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);
  Ethernet.init(ETH_CS_PIN);

	Ethernet.begin(mac);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found. Sorry, can't run without hardware. :(");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
	} else if (Ethernet.hardwareStatus() == EthernetW5100) {
    Serial.println("W5100 Ethernet controller detected.");
  } else if (Ethernet.hardwareStatus() == EthernetW5200) {
    Serial.println("W5200 Ethernet controller detected.");
  } else if (Ethernet.hardwareStatus() == EthernetW5500) {
    Serial.println("W5500 Ethernet controller detected.");
  }

  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }
 
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());

  // Initialize the Wi-Fi module
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) {
    delay(100);
  }

  Serial.println("ESP-NOW Example - Broadcast Master");
  Serial.println("Wi-Fi parameters:");
  Serial.println("  Mode: STA");
  Serial.println("  MAC Address: " + WiFi.macAddress());
  Serial.printf("  Channel: %d\n", ESPNOW_WIFI_CHANNEL);

  // Register the broadcast peer
  if (!broadcast_peer.begin()) {
    Serial.println("Failed to initialize broadcast peer");
    Serial.println("Rebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

	recv.callbackDMX(receiveSACN);
	recv.callbackSource(newSource);
	recv.callbackFramerate(framerate);
	recv.callbackTimeout(timeOut);
	recv.begin(6);
	Serial.println("sACN start");

  last_status = millis();

}


void loop() {
	recv.update();

  if (millis() > last_status+1000) {
    printf("Sent %d fail %d (%0.03f%%)\n", msg_sent, msg_fail, float(msg_fail)/float(msg_sent));
    last_status = millis();
  }

  if (millis() > last_sent_millis+1000) {
    broadcastDmxNow(false);
  }
}