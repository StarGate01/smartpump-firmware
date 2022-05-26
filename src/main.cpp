/**
 * @file main.cpp
 * @author Christoph Honal
 * @brief Provides the core logic
 * @version 0.1
 * @date 2022-05-25
 */

#include <Arduino.h>
#include "HardwareConfiguration.h"
#include "NetworkConfiguration.h"

#include "LoRaWan_APP.h" // LoRa WAN
#include "PowerExtender.h" // Relay and sensor board


// Auxiliary hardware drivers
PowerExtender power_extender;

// LoRa network configuration buffers
// These symbols are required by the LoraWan library
uint8_t devEui[8]; //!< Generated from hardware id
uint8_t appEui[] = TTN_APP_EUI;
uint8_t appKey[] = TTN_APP_KEY;
uint8_t nwkSKey[16] = { 0 }; //!< Unused in OTAA mode
uint8_t appSKey[16] = { 0 }; //!< Unused in OTAA mode
uint32_t devAddr = 0; //!< Unused in OTAA mode
// Allow channels 0-7; EU868 uses 0-9 only, CN470 uses 0-95 and so on
uint16_t userChannelsMask[6] = { 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

// Configuration variables, see platformio.ini
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;
DeviceClass_t  loraWanClass = LORAWAN_CLASS;
bool overTheAirActivation = LORAWAN_NETMODE;
bool loraWanAdr = LORAWAN_ADR;
bool keepNet = LORAWAN_NET_RESERVE;
bool isTxConfirmed = LORAWAN_UPLINKMODE;

uint32_t appTxDutyCycle = 15000; //!< Transmission frequency in ms
uint8_t appPort = 2; //!< Application port 
uint8_t confirmedNbTrials = 4; //!< Number of trials to transmit the frame if not acknowledged


// Lora packet structure definition and buffer allocation
// Force packed memory alignment to enable pointer cast to buffer
struct __attribute__ ((packed)) lora_packet_up_t 
{ 
    uint8_t id; 
    float32 current[4];
} static packet_up;

struct __attribute__ ((packed)) lora_packet_down_t 
{ 
    uint8_t relays;
} static packet_down;


// Core logic

void setup() 
{
    // Serial
    Serial.begin(115200);

    // Disable 3.3V auxiliary power output (LED)
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, HIGH); 

    // Setup sensors
    power_extender.begin();

    // Setup LoRa system
    deviceState = DEVICE_STATE_INIT;
	LoRaWAN.ifskipjoin();
}

void loop() 
{
    switch(deviceState)
	{
		case DEVICE_STATE_INIT:
		{
            // Generate device EUI from hardware ID
			LoRaWAN.generateDeveuiByChipID();
			printDevParam();

            // Init LoRa system
			LoRaWAN.init(loraWanClass, loraWanRegion);
			deviceState = DEVICE_STATE_JOIN;
			break;
		}
		case DEVICE_STATE_JOIN:
		{
            // Join LoRa network
			LoRaWAN.join();
			break;
		}
		case DEVICE_STATE_SEND:
		{
            // Read sensors
            packet_up.current[0] = (float32) power_extender.analogReadAsCurrent(PEPIN_AK1);
            packet_up.current[1] = (float32) power_extender.analogReadAsCurrent(PEPIN_AK2);
            packet_up.current[2] = (float32) power_extender.analogReadAsCurrent(PEPIN_AK3);
            packet_up.current[3] = (float32) power_extender.analogReadAsCurrent(PEPIN_AK4);
            
            // Print packet for monitoring
            if(Serial) Serial.printf("Sending packet with id=%d\n", packet_up.id);

            // Blit packet struct into library buffer
            memcpy(&appData, &packet_up, min(sizeof(lora_packet_up_t), LORAWAN_APP_DATA_MAX_SIZE));
			appDataSize = sizeof(lora_packet_up_t);
            LoRaWAN.send();
            packet_up.id++;

			deviceState = DEVICE_STATE_CYCLE;
			break;
		}
		case DEVICE_STATE_CYCLE:
		{
			// Schedule next packet transmission
			txDutyCycleTime = appTxDutyCycle + randr(0, APP_TX_DUTYCYCLE_RND);
			LoRaWAN.cycle(txDutyCycleTime);
			deviceState = DEVICE_STATE_SLEEP;
			break;
		}
		case DEVICE_STATE_SLEEP:
		{
            // Enter deep sleep
			LoRaWAN.sleep();
			break;
		}
		default:
		{
			deviceState = DEVICE_STATE_INIT;
			break;
		}
	}
}

// This function is referenced and required by the LoRa WAN library
void downLinkDataHandle(McpsIndication_t *mcpsIndication)
{
    if(Serial)
    {
        // Print packet meta info
        Serial.printf("Received downlink: %s, RXSIZE %d, PORT %d, DATA: ",
            mcpsIndication->RxSlot ? "RXWIN2":"RXWIN1",
            mcpsIndication->BufferSize,
            mcpsIndication->Port);

        // Print packet data
        for(uint8_t i = 0;i < mcpsIndication->BufferSize; i++) 
        {
            Serial.printf("%02X", mcpsIndication->Buffer[i]);
        }
        Serial.println();

        // Control relays
        if(mcpsIndication->BufferSize >= sizeof(lora_packet_down_t))
        {
            packet_down = *((lora_packet_down_t*) mcpsIndication->Buffer); 
            power_extender.digitalWrite(PEPIN_DOUT_K1, (packet_down.relays & 1)? HIGH:LOW);
            power_extender.digitalWrite(PEPIN_DOUT_K2, (packet_down.relays & 2)? HIGH:LOW);
            power_extender.digitalWrite(PEPIN_DOUT_K3, (packet_down.relays & 4)? HIGH:LOW);
            power_extender.digitalWrite(PEPIN_DOUT_K4, (packet_down.relays & 8)? HIGH:LOW);
        }
    }
}