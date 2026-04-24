#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

/* --- Pin Definitions --- */
#define CE_PIN   7
#define CSN_PIN  8

/* --- Joystick/Switch Pins --- */
#define PIN_J1X  A0
#define PIN_J1Y  A1
#define PIN_J2X  A2
#define PIN_J2Y  A3

#define PIN_SW1  2
#define PIN_SW2  3
#define PIN_SW3  4
#define PIN_SW4  5

/* --- The 16-Byte Struct (Matches STM32 EXACTLY) --- */
struct __attribute__((packed)) RC_Packet_t {
    uint8_t joy1_x;
    uint8_t joy1_y;
    uint8_t joy2_x;
    uint8_t joy2_y;
    uint8_t sw1;
    uint8_t sw2;
    uint8_t sw3;
    uint8_t sw4;
    uint16_t packet_id;
    uint8_t failsafe_flag;
    uint8_t reserved[5]; 
};

RF24 radio(CE_PIN, CSN_PIN);
RC_Packet_t tx_data;
uint16_t id_count = 0;
const byte address[5] = {0xE7, 0xE7, 0xFF, 0x11, 0xE2};

void setup() {
    Serial.begin(115200);
    
    /* Configure Switches with Internal Pull-ups */
    pinMode(PIN_SW1, INPUT_PULLUP);
    pinMode(PIN_SW2, INPUT_PULLUP);
    pinMode(PIN_SW3, INPUT_PULLUP);
    pinMode(PIN_SW4, INPUT_PULLUP);

    if (!radio.begin()) {
        Serial.println("nRF24 Error!");
        while (1);
    }

    radio.setChannel(100);
    radio.setDataRate(RF24_250KBPS);
    radio.setCRCLength(RF24_CRC_16);
    radio.setPayloadSize(16);
    radio.setAutoAck(false); 
    radio.openWritingPipe(address);
    radio.stopListening();
    
    Serial.println("Transmitter Ready...");
}

void loop() {
    /* 1. Read Joysticks (0-1023) and map to (0-255) */
    tx_data.joy1_x = map(analogRead(PIN_J1X), 0, 1023, 0, 255);
    tx_data.joy1_y = map(analogRead(PIN_J1Y), 0, 1023, 0, 255);
    tx_data.joy2_x = map(analogRead(PIN_J2X), 0, 1023, 0, 255);
    tx_data.joy2_y = map(analogRead(PIN_J2Y), 0, 1023, 0, 255);

    /* 2. Read Switches (Invert because of PULLUP: Pressed = 0, so ! makes it 1) */
    tx_data.sw1 = !digitalRead(PIN_SW1);
    tx_data.sw2 = !digitalRead(PIN_SW2);
    tx_data.sw3 = !digitalRead(PIN_SW3);
    tx_data.sw4 = !digitalRead(PIN_SW4);

    /* 3. System Data */
    tx_data.packet_id = id_count++;
    tx_data.failsafe_flag = 0;

    /* 4. Send Packet */
    radio.write(&tx_data, sizeof(tx_data));

    /* 5. Serial Debug (Optional - slow down to 10Hz) */
    static uint32_t last_debug = 0;
    if (millis() - last_debug > 200) {
        Serial.print("J1: "); Serial.print(tx_data.joy1_x);
        Serial.print(","); Serial.print(tx_data.joy1_y);
        Serial.print(" | SW1: "); Serial.println(tx_data.sw1);
        last_debug = millis();
    }

    delay(20); // Send at ~50Hz for smooth flight/control
}
