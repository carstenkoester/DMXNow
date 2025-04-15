# DMXNow protocol and library

**This is a draft and work-in-progress impmementation!**

DMX transmission using the ESP8266/ESP32 [ESP-Now](https://www.espressif.com/en/solutions/low-power-solutions/esp-now) protocol



## Packet format

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
   |                                 |               | | | |L|F|N|R|
   |         Sequence Number         |     Type      | | | |S|S|E|S|
   |                                 |               | | | |T|T|W|T|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Expected|Segment|             Offset            |   Length    |
   | Segments|  ID   |                               |             |
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

  - `FST` (`FIRST`), 0x04: This is the first transmission in a series of segmented DMX payload. In other words, this transmission is the first segment of any one segmented DMX payload.

  - `LST` (`LAST`), 0x08: This is the last transmission in a series of segmented DMX payload. In other words, this transmission is the last segment of any one segmented DMX payload. If a transmitter opts to transmit only a subset of a DMX universe, up to 234 slots, then segmentation may not be necessary and a packet would have both the `FST` and `LST` bit set.


#### Expected segments

For the DMX payload currently being transmitted, the highest segment ID expected for a complete DMX buffer, as zero-based number.

Assuming the sender is transmitting an entire DMX buffer of 513 bytes (start code + 512 slots), at the maximum size considering ESP-Now's 250-byte limit (and for this protocol, accounting for overhead, a maximum payload size of 234 bytes), then any one DMX buffer would be split into three segments. As the segment IDs are zero-based, the expected segment IDs would be 0x0, 0x1, 0x2, with 0x2 being the expected highest segment ID.

This field MUST be the same value for all segments that are part of the same DMX buffer. For example, if the transmitter does segment a 512-byte payload into three segments, then this field must have value 0x2 for *all* three segments.


#### Segment ID

For the DMX payload currently being transmitted, this is the segment ID of the current transmission.

The Segment ID MUST be reset to zero when the `FST` bit is set on a packet.

The Segment ID MUST be identical to the "Expected segments" value when the `LST` bit is set on a packet.


#### Offset

For the current segment, the offset into the DMX buffer including the start code.

#### Length

The number of bytes of DMX payload

Assuming a 513-byte DMX buffer (start code + 512 slots), and assuming a source splits this buffer into three equal-size segments of 171 bytes each, then a transmitter could typically chose to transmit three segments as follows:

| Segment ID | FST | LST | Offset | Length | Remarks                          |
|------------|-----|-----|--------|--------|----------------------------------|
| 0          |  X  |     | 0      | 171    | DMX Start code + DMX slots 1-170 |
| 1          |     |     | 171    | 171    | DMX slots 171-341                |
| 2          |     |  X  | 342    | 171    | DMX slots 342-512                |

However, to optimize for aligning with the processor's native 32-bit boundary, a transmitter MAY also chose something like

| Segment ID | FST | LST | Offset | Length | Remarks                          |
|------------|-----|-----|--------|--------|----------------------------------|
| 0          |  X  |     | 0      | 172    | DMX Start code + DMX slots 1-171 |
| 1          |     |     | 172    | 172    | DMX slots 172-343                |
| 2          |     |  X  | 344    | 169    | DMX slots 344-512                |

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
