# DMXNow protocol and library

**This is a draft and work-in-progress impmementation!**

DMX transmission using the ESP8266/ESP32 [ESP-Now](https://www.espressif.com/en/solutions/low-power-solutions/esp-now) protocol


## Packet format

TODO - Update for ESP-NOW V2 with larger payload:

  - https://github.com/espressif/esp-now/blob/master/User_Guide.md
  - https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html#frame-format

This spec now requires v2.

ESP-Now supports a maximum of 250-byte payload, therefore we need to split a 512-byte DMX payload into a maximum of three frames.

This protocol is designed to be able to carry an entire payload, but at the same time, flexible enough to carry only a subset of the payload (ie. a smaller number of DMX slots) if desired.

```
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |               Magic String (Protocol Identifier)              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     Version     |   Priority   |        DMX Universe ID       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                               |                 | | | | | |N|R|
   |         Sequence Number       |      Type       | | | | | |E|S|
   |                               |                 | | | | | |W|T|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            Offset             |            Length             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                   Payload (max. 234 bytes)                    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

##### Magic String (Protocol Identifier)

24-bit, always [0x44, 0x6D, 0x58, 0x4E] which is equivalent to ASCII `DmXN`.

#### Version identifier

8-bit. Currently always version 0x01.

#### Priority

8-bit. Vaguely inspired by E1.31, a future version of this protocol may consider scenarios where multiple transmitters exist on the network, and receivers may chose one source over another based on the priority value. At of now, the priority value SHOULD be at a fixed value of 100.

#### DMX Universe ID

16-bit. Similar to E1.31 (sACN), Universe numbers between 64000 and 65535 are reserved.

#### Sequence Number

16-bit. To detect duplicate, lost, or out-of-order packets.

Sequence numbering happens per-source (obviously) and per-universe. That is, a source transmitting multiple universes in parallel would maintain separate sequence numbers for each universe it transmits.

Also, sequence numbering happens for individual transmissions, regardless of segmentation (see below). If a 513-byte DMX buffer is split into multiple segments for transmission, then each segment will increment the transmission's sequence number.

#### Type

8-bit. Indicates the type of payload.

In a future version, the remainder of the packet may differ depending on the type. Currently, a single type value is supported:

Type 0x00: DMX data packet

Potential future uses could be other packet types for universe discovery, synchronization purposes, or to disseminate control or administrative data such as source names, starting/stopping of streams to indicate to receivers that they may go into "sleep" mode, etc.

#### Flags

8-bit bit mask

  - `RST` (`RESET`), 0x01: Reset. This shall be set on the first ten (10) packets after a transmitter has restarted or otherwise reset its transmit sequence number. Upon receipt of a packet containing this bit set, a receiver shall ignore any sequence number errors and re-synchronize its expected receive sequence number to the sequence number received.

  - `NEW`, 0x02: New payload. A receiver MUST set this bit to 1 if the contents of the DMX buffer differ from a previous transmission for this same universe and segment, or if it is unknown (ie. if the transmitter did not compare or analyze whether the payload differs from a previous transmission). A transmitter MAY set this bit to 0 if this is a periodic re-transmission of a previously sent payload. A receiver MAY chose to ignore processing of a packet that has the `NEW` bit set to 0.



#### Offset

The offset into the DMX buffer including the start code.

For a full buffer transmission, this value would be zero.

#### Length

The number of bytes of DMX payload. The maximum size is 513 bytes for a full buffer of start code followed by 512 DMX slots.


### Receiver

A receiver shall discard a received packet if:

- The packet does not start with the magic string "DmXN". This SHOULD not be considered an error, as the packet may be a legitimate packet of a different transmission happening in ESP-Now. This MAY be counted as an "Unsupported protocol" condition.

- The version number is not supported. A receiver MAY choose to support multiple versions and differentiate receive packet processing based on this version.

- The packet is not on a supported universe

- The reserved fields are anything other than 0x00.


## References

Other DMX over ESP-Now libraries:

  - <https://github.com/Blinkinlabs/esp-now-dmx>
  - <https://github.com/kripton/DMXoverNOW>
