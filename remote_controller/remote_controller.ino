#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

/* --- Pin Definitions --- */
#define CE_PIN    7
#define CSN_PIN   8
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

struct __attribute__((packed)) RC_Packet_t {
  uint16_t joy1_x;
  uint16_t joy1_y;
  uint16_t joy2_x;
  uint16_t joy2_y;

  uint8_t  fly_mode;          
  uint8_t  altitudeHold_flag;
  uint8_t  failsafe_flag;
  uint16_t packet_id;

  uint8_t  reserved[2]; 
  uint8_t  checksum;
};

struct __attribute__((packed)) ACK_Payload_t {
  uint8_t packet_status; 
  uint8_t battery;
  uint8_t sig_strength;
  uint8_t flags; 
};

// Global instances
RF24 radio(CE_PIN, CSN_PIN);
const byte address[5] = {0xE7, 0xE7, 0xFF, 0x11, 0xE2};
int center_j1x, center_j1y, center_j2x, center_j2y;
uint16_t id_count = 0;

/* --- Functions --- */

uint8_t compute_checksum(uint8_t *data, uint16_t length) {
  uint8_t checksum = 0;
  for (uint16_t i = 0; i < length; i++) {
    checksum ^= data[i];
  }
  return checksum;
}

void calibrateJoysticks() {
  long sum1x = 0, sum1y = 0, sum2x = 0, sum2y = 0;
  for (int i = 0; i < 50; i++) {
    sum1x += analogRead(PIN_J1X);
    sum1y += analogRead(PIN_J1Y);
    sum2x += analogRead(PIN_J2X);
    sum2y += analogRead(PIN_J2Y);
    delay(5);
  }
  center_j1x = sum1x / 50;
  center_j1y = sum1y / 50;
  center_j2x = sum2x / 50;
  center_j2y = sum2y / 50;
}

int applyDeadzone(int value, int center) {
  return (abs(value - center) <= 8) ? center : value; 
}

void parse_packet(RC_Packet_t &pkt) {
  pkt.joy1_x = map(applyDeadzone(analogRead(PIN_J1X), center_j1x), 0, 1023, 1000, 2000);
  pkt.joy1_y = map(applyDeadzone(analogRead(PIN_J1Y), center_j1y), 0, 1023, 1000, 2000);
  pkt.joy2_x = map(applyDeadzone(analogRead(PIN_J2X), center_j2x), 0, 1023, 1000, 2000);
  pkt.joy2_y = map(applyDeadzone(analogRead(PIN_J2Y), center_j2y), 0, 1023, 1000, 2000);

  // Arming Logic: LOW = Switch Flipped = ARMED
  pkt.fly_mode = (digitalRead(PIN_ARM_SWITCH) == LOW) ? FLY_MODE : 0;

  pkt.altitudeHold_flag = 0;
  pkt.failsafe_flag = 0;
  pkt.packet_id = id_count++;

  pkt.checksum = compute_checksum((uint8_t*)&pkt, sizeof(RC_Packet_t) - 1);
}

void sync_drone() {
  Serial.println(F("Searching for Drone..."));
  bool connected = false;
  while (!connected) {
    RC_Packet_t sync_packet;
    parse_packet(sync_packet); 

    if (radio.write(&sync_packet, sizeof(sync_packet))) {
      if (radio.isAckPayloadAvailable()) {
        connected = true;
        Serial.println(F("Drone Link Established!"));
      }
    } else {
      Serial.println(F("No Reply"));
    }
    delay(100); 
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_ARM_SWITCH, INPUT_PULLUP);
  calibrateJoysticks();

  // Robust Radio Initialization
  while (!radio.begin()) {
    Serial.println(F("nRF24 Error: Check wiring/power!"));
    delay(1000);
  }

  radio.setChannel(100);
  radio.setDataRate(RF24_250KBPS); 
  radio.setPALevel(RF24_PA_MAX);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.setRetries(5, 15);
  radio.openWritingPipe(address);
  radio.stopListening();

  sync_drone();

  // Safety Boot Check
  while (digitalRead(PIN_ARM_SWITCH) == LOW) {
    Serial.println(F("SAFETY: Move Arm Switch to DISARMED position!"));
    delay(500);
  }

  Serial.println(F("Transmitter Ready..."));
}

void loop() {
  RC_Packet_t rc_packet;
  parse_packet(rc_packet);

  ACK_Payload_t ack;
  if (radio.write(&rc_packet, sizeof(RC_Packet_t))) {
    if (radio.isAckPayloadAvailable()) {
      radio.read(&ack, sizeof(ack));
      /* Debug Output */
      Serial.print(F("ID: ")); Serial.print(rc_packet.packet_id);
      Serial.print(F(" | Arm: ")); Serial.print(rc_packet.fly_mode ? "YES" : "NO");
      Serial.print(F(" | Thr: ")); Serial.println(rc_packet.joy2_y); 
      Serial.print(F(" | ACK: ")); Serial.println(ack.battery); 
      // Telemetry processing happens here
    }
  } else {
    Serial.println(F("LOST LINK"));
  }

  delay(20); 
}
