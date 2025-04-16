// If you are using Arduino IDE, the board should be "ESP32S3 Dev Module"
// but you should manually specify a few other board options.
// USB CDC On Boot should be "Enabled",
// Flash Size should be "16MB (128Mb)",
// partition scheme should be "Huge APP (3MB No OTA/1MB SPIFFS)", 
// and PSRAM should be "OPI PSRAM".

#include "SPI.h"
#include "Ethernet.h"
#include "ESP32_NOW.h"
#include "WiFi.h"

#include "sACN.h"
#include "DMXNow.h"

#include <esp_mac.h>  // For the MAC2STR and MACSTR macros

#define ETH_MISO_PIN                    12
#define ETH_MOSI_PIN                    11
#define ETH_SCLK_PIN                    13
#define ETH_CS_PIN                      14
#define ETH_INT_PIN                     10
#define ETH_RST_PIN                     9
#define ETH_ADDR                        1

unsigned long msg_sent = 0;
unsigned long msg_fail = 0;

uint16_t dmxnow_sequence = 0;

unsigned long last_sent_millis = 0;

/* Definitions */

#define ESPNOW_WIFI_CHANNEL 6

/* Classes */

// Creating a new class that inherits from the ESP_NOW_Peer class is required.

esp_now_rate_config_t rate_config = {
  .phymode = WIFI_PHY_MODE_HT20,
  .rate = WIFI_PHY_RATE_MCS1_SGI,
  .ersu = false,
  .dcm = false
};


class ESP_NOW_Broadcast_Peer : public ESP_NOW_Peer {
public:
  // Constructor of the class using the broadcast address
  ESP_NOW_Broadcast_Peer(uint8_t channel, wifi_interface_t iface, const uint8_t *lmk) : ESP_NOW_Peer(ESP_NOW.BROADCAST_ADDR, channel, iface, lmk) {}

  // Destructor of the class
  ~ESP_NOW_Broadcast_Peer() {
    remove();
  }

  // Function to properly initialize the ESP-NOW and register the broadcast peer
  bool begin() {
    if (!ESP_NOW.begin() || !add()) {
      log_e("Failed to initialize ESP-NOW or register the broadcast peer");
      return false;
    }
    esp_err_t r =  esp_now_set_peer_rate_config(ESP_NOW.BROADCAST_ADDR, &rate_config);
      if (r != ESP_OK) {
        Serial.printf("Failed to set ESP-Now rate: %d\n", r);
        Serial.println("Rebooting in 5 seconds...");
        delay(5000);
        ESP.restart();
    } else {
      Serial.println("Rate config set OK");
    }
    return true;
  }



  // Function to send a message to all devices within the network
  bool send_message(const uint8_t* data, size_t len) {
    if (!send(data, len)) {
      log_e("Failed to broadcast message");
      msg_fail++;
      return false;
    }

    msg_sent++;
    return true;
  }
};


// Create a broadcast peer object
ESP_NOW_Broadcast_Peer broadcast_peer(ESPNOW_WIFI_CHANNEL, WIFI_IF_STA, NULL);

uint8_t mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x14, 0x48}; // MAC Adress of your device



EthernetUDP sacn;
Receiver recv(sacn); // universe 1

void debugDumpPacket(const void* data, const unsigned int len) {
  const unsigned char* buf = (const unsigned char*) data;

  Serial.printf("PKT len %d:", len);
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

void receiveDmx() {
  broadcastDmx(true);
}

void broadcastDmx(bool updated)
{
  dmxnow_packet_t dmxnow;
  unsigned char* dmx;

  dmx = recv.dmx();

  dmxnow.magic = dmxnow_magic;
  dmxnow.version = 1;
  dmxnow.priority = 100;
  dmxnow.universe = 6; // FIXME
  dmxnow.type = 0;

  // First segment
  dmxnow.sequence = dmxnow_sequence++;
  dmxnow.flags = dmxnow_flag_t::DMXNOW_FLAG_FIRST;
  dmxnow.segments = (2 << 4);
  dmxnow.length = 172;
  dmxnow.offset = 0;
  dmxnow.payload[0] = 0x00; // Start code
  memcpy(&dmxnow.payload[1], dmx, 171);
  broadcast_peer.send_message((uint8_t*) &dmxnow, DMXNOW_HEADER_SIZE+172);

  // Second segment
  dmxnow.sequence = dmxnow_sequence++;
  dmxnow.flags = dmxnow_flag_t::DMXNOW_FLAG_NONE;
  dmxnow.segments = (2 << 4) + 1;
  dmxnow.offset = 172;
  memcpy(&dmxnow.payload, (dmx + 171), 172);
  broadcast_peer.send_message((uint8_t*) &dmxnow, DMXNOW_HEADER_SIZE+172);

  // Third segment
  dmxnow.sequence = dmxnow_sequence++;
  dmxnow.flags = dmxnow_flag_t::DMXNOW_FLAG_LAST;
  dmxnow.segments = (2 << 4) + 2;
  dmxnow.length = 169;
  dmxnow.offset = 344;
  memcpy(&dmxnow.payload, (dmx + 343), 169);
  broadcast_peer.send_message((uint8_t*) &dmxnow, DMXNOW_HEADER_SIZE+169);

  // Keep timestamp of when we sent this
  last_sent_millis = millis();
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
	Serial.begin(9600);
	delay(2000);
	Serial.println("Program start");
#ifdef ETH_POWER_PIN
  pinMode(ETH_POWER_PIN, OUTPUT);
  digitalWrite(ETH_POWER_PIN, HIGH);
#endif


//  Ethernet.init(14);
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
    Serial.println("Reebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

	recv.callbackDMX(receiveDmx);
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
    broadcastDmx(false);
  }
}