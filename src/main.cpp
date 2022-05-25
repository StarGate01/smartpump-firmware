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
#include <Wire.h>


// Auxiliary hardware drivers


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


// Sensor buffers
static char temp_str[8], humid_str[8], temp_str2[8], humid_str2[8];


// Lora packet structure definition and buffer allocation
// Force packed memory alignment to enable pointer cast to buffer
struct __attribute__ ((packed)) lora_packet_t 
{ 
    uint8_t id; 
    int16_t battery;
} static packet;


// Core logic

void setup() 
{
    // Serial
    Serial.begin(115200);

    // Disable 3.3V auxiliary power output (LED)
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, HIGH); 

    // Setup sensors

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
            // Read battery voltage
            packet.battery = getBatteryVoltage();

            // Read sensors

            
            // Print packet for monitoring
            if(Serial)
            {
                Serial.printf("Sending packet with id=%d, battery=%d\n", 
                    packet.id, packet.battery);
            }

            // Blit packet struct into library buffer
            memcpy(&appData, &packet, min(sizeof(lora_packet_t), LORAWAN_APP_DATA_MAX_SIZE));
			appDataSize = sizeof(lora_packet_t);
            LoRaWAN.send();
            packet.id++;

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
    }
}