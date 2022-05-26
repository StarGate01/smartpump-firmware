# smartpump-firmware

Firmware for an off-grid water pumping system

## Hardware overview

 - Board: **CubeCell HTCC-AB02A ASR605x** (ARM Cortex M0+ & SX1262 LoRa)
   - [Documentation](https://heltec.org/project/htcc-ab02a/)
   - [Arduino implementation](https://github.com/HelTecAutomation/CubeCell-Arduino)

 - Pump and sensor interface: **CHRZ power-extender**
   - [Documentation](https://github.com/StarGate01/power-extender)
   - [Arduino library](https://registry.platformio.org/libraries/stargate01/power-extender)

## Development setup

Install **Visual Studio Code** and the **PlatformIO** extension.

Copy `include/NetworkConfiguration.h.example` to `include/NetworkConfiguration.h` and insert your [*The Things Network*](https://www.thethingsnetwork.org) configuration. 

Edit the `platformio.ini` file and change the `board_build.arduino.lorawan.region` to the correct frequency band for your country.

Attach a serial monitor and read the Device EUI after a reset. Then set the device EUI in the TTN console, use OTAA and MAC 1.0.2 .

Use the PlatformIO menu to compile and upload the code.

## TTN configuration

### Uplink Payload Formatter

```javascript
function decodeInt16(bytes, offset)
{
  return (((bytes[offset + 1] & 0xFF) << 8) | (bytes[offset] & 0xFF));
}

function decodeFloat32(bytes, offset)
{
  var d = new DataView(new ArrayBuffer(4));
  for(var i=0; i<4; i++) d.setUint8(i, bytes[offset + i]);
  return d.getFloat32(0, true);
}

function decodeUplink(input) 
{
  return {
    data: {
      bytes: input.bytes,
      id: input.bytes[0],
      current: [
        decodeFloat32(input.bytes, 1),
        decodeFloat32(input.bytes, 5),
        decodeFloat32(input.bytes, 9),
        decodeFloat32(input.bytes, 13)
      ],
      adc: [
        decodeFloat32(input.bytes, 17),
        decodeFloat32(input.bytes, 21),
        decodeFloat32(input.bytes, 25),
        decodeFloat32(input.bytes, 29)
      ]
    },
    warnings: [],
    errors: []
  };
}
```

### Downlink Payload Formatter

```javascript
function encodeDownlink(input) {
  var rel = 0;
  if(input.data.relay[0] === true) rel |= 1;
  if(input.data.relay[1] === true) rel |= 2;
  if(input.data.relay[2] === true) rel |= 4;
  if(input.data.relay[3] === true) rel |= 8;
  return {
    bytes: [ rel ],
    fPort: 1,
    warnings: [],
    errors: []
  };
}

function decodeDownlink(input) {
  return {
    data: {
      bytes: input.bytes,
      relay: [
        input.bytes[0] & 1, 
        input.bytes[0] & 2,
        input.bytes[0] & 4,
        input.bytes[0] & 8
      ]
    },
    warnings: [],
    errors: []
  };
}
```

### Example uplink payload

```json
{
  "id": 1,
  "current": [1.0, 1.0, 1.0, 1.0],
  "adc": [1.0, 1.0, 1.0, 1.0]
}
```

### Example downlink payload

```json
{
  "relay": [true, true, true, true]
}
```