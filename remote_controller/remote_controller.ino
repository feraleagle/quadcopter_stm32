#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "printf.h"

/* =========================================================
 *  RC PACKET STRUCT [16 Bytes]
 * ========================================================= */
typedef struct __attribute__((packed)) {
        uint16_t joy1_x;
        uint16_t joy1_y;
        uint16_t joy2_x;
        uint16_t joy2_y;
        uint8_t fly_mode;
        uint8_t altitudeHold_flag;
        uint8_t failsafe_flag;
        uint32_t packet_id;
        uint8_t reserved;
} RC_Packet_t;

/* =========================================================
 *  RC ACK PACKET STRUCT [6 Bytes]
 * ========================================================= */
typedef struct __attribute__((packed)) {
        uint16_t battery;
        uint32_t packet_id;
        uint8_t sig_strength;
        uint8_t flags;
} ACK_Packet_t;

/* =========================================================
 *  Definitions & Masks
 * ========================================================= */
/* --- Pin Definitions --- */
#define CE_PIN    9
#define CSN_PIN   10
#define PIN_ARM_SWITCH 2  

/* --- Joystick Pins --- */
#define PIN_J1X  A0
#define PIN_J1Y  A1
#define PIN_J2X  A2
#define PIN_J2Y  A3

/* --- FLAGS Bit-Mask --- */
#define FLY_MODE          0b10000000
#define ALTITUDEHOLD_FLAG 0b01000000
#define FAILSAFE_FLAG     0b00100000

/* --- Global instances --- */
RF24 radio(CE_PIN, CSN_PIN);
const byte address[5] = {0xE7, 0xE7, 0xFF, 0x11, 0xE2};

int tick = 0; // For Timeout  
uint32_t packet_id = 100000000;
int lost = 0;

RC_Packet_t rc_packet;
ACK_Packet_t ack_packet;


/* =========================================================
 *  FUNCTION: Synchronize Drone
 * ========================================================= */
void sync_drone() {
  Serial.println(F("Searching for Drone..."));
  bool connected = false;

  Serial.println(F("Waiting To Establish Link!"));

  while (!connected) {
    if (radio.write(&rc_packet, sizeof(rc_packet))) {
      if (radio.isAckPayloadAvailable()) {
        connected = true;
      }
    }
    delay(10); 
  }
}

/* =========================================================
 *  FUNCTION: Setup Arduino Core FUNCTION
 * ========================================================= */
void setup() {
  Serial.begin(115200);
  pinMode(PIN_ARM_SWITCH, INPUT_PULLUP);

  // Robust Radio Initialization
  while (!radio.begin()) {
    Serial.println(F("nRF24 Error: Check wiring/power!"));
    delay(1000);
  }

  radio.setChannel(100);
  radio.setDataRate(RF24_250KBPS); 
  radio.setPALevel(RF24_PA_MAX);
  radio.enableDynamicPayloads();
  radio.enableAckPayload();
  radio.setRetries(5, 15);
  radio.openWritingPipe(address);
  radio.stopListening();

  printf_begin();             // needed only once for printing details
  sync_drone();

  // Safety Boot Check
  // while (digitalRead(PIN_ARM_SWITCH) == LOW) {
  //   Serial.println(F("SAFETY: Move Arm Switch to DISARMED position!"));
  //   delay(500);
  // }

  Serial.println(F("Transmitter Ready..."));
  rc_packet.packet_id = 1000000;
}


/* =========================================================
 *  FUNCTION: Loop Arduino Core FUNCTION
 * ========================================================= */
void loop() {  
  rc_packet.packet_id = packet_id++;

  if (radio.write(&rc_packet, sizeof(RC_Packet_t))) {
    if (radio.isAckPayloadAvailable()) {
      radio.read(&ack_packet, sizeof(ack_packet));
      /* Debug Output */
      Serial.print(F("\r Packet ID: ")); Serial.print(ack_packet.packet_id);
      tick = 0;
    }
  } else if (tick > 25){
      Serial.println(F("LOST LINK"));
      lost++;
  } else {
      tick++;
      lost++;
  }
  delay(5); 
}
