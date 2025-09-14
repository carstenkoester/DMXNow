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
#include "DMXNow_Transmitter.h"

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

#define UNIVERSE                        15

/*
 * Runtime
 */

// Adafruit_NeoPixel status_led = Adafruit_NeoPixel(1, NEOPIXEL_STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);
uint8_t mac[6];

DMXNow_Transmitter transmitter;
unsigned long SACNRxCount = 0;

EthernetUDP sacn;
Receiver recv(sacn);

void receiveSACN() {
  SACNRxCount++;
  transmitter.setValues(1, DMX_BUFSIZE, recv.dmx());
}

void newSource() {
  Serial.print("new soure name: ");
  Serial.println(recv.name());
}

void framerate() {
  Serial.print("Framerate fps: ");
  Serial.println(recv.framerate());
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

  transmitter.begin(ESPNOW_WIFI_CHANNEL, UNIVERSE);

  // Get the MAC address of the Ethernet interface
  esp_read_mac(mac, ESP_MAC_ETH);
  Serial.printf("Ethernet MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

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

  recv.callbackDMX(receiveSACN);
  recv.callbackSource(newSource);
//  recv.callbackFramerate(framerate);
  recv.callbackTimeout(timeOut);
  recv.begin(UNIVERSE);
  Serial.println("sACN start");

  last_status = millis();
}


void loop() {
  recv.update();

  if (millis() > last_status+1000) {
    Serial.printf("SACN Rx %d Sent %d fail %d (%0.03f%%)\n", SACNRxCount, transmitter.txCount(), transmitter.txFail(), float(transmitter.txFail())/float(transmitter.txCount()));
    last_status = millis();
  }
}