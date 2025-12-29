/*
 * stabilizer_final_v3.ino
 *
 * 16-Stage Automatic Voltage Regulator Firmware
 * + TM1680 Display Driver (5 Screens)
 * + RMS Measurement (Peak Detection) for Accuracy
 */

// ================= PINS =================
const int PIN_VOLTAGE_SENSOR = A0;
const int PIN_CURRENT_SENSOR = A1;

// Relays (Binary 1, 2, 4, 8)
const int PIN_RELAY_1 = 2;
const int PIN_RELAY_2 = 3;
const int PIN_RELAY_3 = 4;
const int PIN_RELAY_4 = 5;
const int PIN_RELAY_OUTPUT =
    6; // NEW: Protection Relay (Connect/Disconnect Load)

// ... (address mapping stays same) ...

const int CUTOFF_LIMIT = 250; // Max Allowed Output Voltage

void loop() {
  float vin = readRMS(PIN_VOLTAGE_SENSOR, VOLTAGE_CALIBRATION);
  float amps = readRMS(PIN_CURRENT_SENSOR, CURRENT_CALIBRATION);

  // 1. Calculate best stage
  int bestStage = 0;
  float minError = 1000;
  float bestVout = vin;

  for (int i = 0; i < 16; i++) {
    float correction = 0;
    if (i & 1)
      correction += 10;
    if (i & 2)
      correction += 20;
    if (i & 4)
      correction += 40;
    if (i & 8)
      correction += 80;

    // Apply physical offset
    float estV = vin + correction + BASE_OFFSET;
    float err = abs(TARGET_VOLTAGE - estV);

    if (err < minError) {
      minError = err;
      bestStage = i;
      bestVout = estV;
    }
  }

  // 2. PROTECTION CHECK
  if (bestVout > CUTOFF_LIMIT) {
    // DANGER: Voltage too high! Disconnect Output.
    digitalWrite(PIN_RELAY_OUTPUT, LOW);

    // Show "Hi" or Error on screen
    bestVout = 0;  // Indicate OFF
    bestStage = 0; // Turn off taps if needed, or keep to minimize arc
  } else {
    // Safe: Connect Output
    digitalWrite(PIN_RELAY_OUTPUT, HIGH);
    setRelays(bestStage);
  }

  // 3. Update All 5 Screens
  displayNumber(ADDR_INPUT_V, (int)vin);
  displayNumber(ADDR_OUTPUT_V, (int)bestVout); // Showing 0 means disconnected
  displayNumber(ADDR_CURRENT, (int)amps);
  displayNumber(ADDR_STEP, bestStage);
  displayNumber(ADDR_TIMER, 0);

  delay(100);
}
const int PIN_DIO = A4;
const int PIN_CLK = A5;

// ================= ADDRESS MAPPING =================
const byte ADDR_INPUT_V = 0;
const byte ADDR_OUTPUT_V = 6;
const byte ADDR_CURRENT = 12;
const byte ADDR_STEP = 18;
const byte ADDR_TIMER = 22;

// ================= CONFIG =================
float VOLTAGE_CALIBRATION = 0.50;
float CURRENT_CALIBRATION = 0.15;
const int TARGET_VOLTAGE = 220;
const int BASE_OFFSET = -80; // Changed to -80V (More Reduction Steps)
const int STARTUP_DELAY_SEC = 5;

// TM1680 Config
#define TM1680_SYSON 0x81
const byte DIGITS[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                       0x6D, 0x7D, 0x07, 0x7F, 0x6F};

void setup() {
  Serial.begin(9600);
  pinMode(PIN_RELAY_1, OUTPUT);
  pinMode(PIN_RELAY_2, OUTPUT);
  pinMode(PIN_RELAY_3, OUTPUT);
  pinMode(PIN_RELAY_4, OUTPUT);

  tm1680_init();

  // Custom Startup Screen
  for (int i = STARTUP_DELAY_SEC; i > 0; i--) {
    displayNumber(ADDR_TIMER, i);
    float vin = readRMS(PIN_VOLTAGE_SENSOR, VOLTAGE_CALIBRATION);
    displayNumber(ADDR_INPUT_V, (int)vin);
    displayNumber(ADDR_OUTPUT_V, 0);
    delay(1000);
  }
}

// ================= MEASUREMENT LOGIC =================

float readRMS(int pin, float calibration) {
  int maxVal = 0;
  unsigned long start_time = millis();

  // Sample for 40ms (2 full cycles at 50Hz)
  while (millis() - start_time < 40) {
    int val = analogRead(pin);
    if (val > maxVal) {
      maxVal = val;
    }
  }

  // For a biased signal (centered at 2.5V/512),
  // Peak is distance from center.
  // But usually simple sensor circuits just rectify.
  // Assuming Simple Peak Detect here:
  return (float)maxVal * calibration;
}

void loop() {
  float vin = readRMS(PIN_VOLTAGE_SENSOR, VOLTAGE_CALIBRATION);
  float amps = readRMS(PIN_CURRENT_SENSOR, CURRENT_CALIBRATION);

  int bestStage = 0;
  float minError = 1000;
  float bestVout = vin;

  for (int i = 0; i < 16; i++) {
    float correction = 0;
    if (i & 1)
      correction += 10;
    if (i & 2)
      correction += 20;
    if (i & 4)
      correction += 40;
    if (i & 8)
      correction += 80;

    float estV = vin + correction + BASE_OFFSET;
    float err = abs(TARGET_VOLTAGE - estV);

    if (err < minError) {
      minError = err;
      bestStage = i;
      bestVout = estV;
    }
  }

  setRelays(bestStage);

  displayNumber(ADDR_INPUT_V, (int)vin);
  displayNumber(ADDR_OUTPUT_V, (int)bestVout);
  displayNumber(ADDR_CURRENT, (int)amps);
  displayNumber(ADDR_STEP, bestStage);
  displayNumber(ADDR_TIMER, 0);

  delay(100);
}

void setRelays(int stage) {
  digitalWrite(PIN_RELAY_1, (stage & 1));
  digitalWrite(PIN_RELAY_2, (stage & 2));
  digitalWrite(PIN_RELAY_3, (stage & 4));
  digitalWrite(PIN_RELAY_4, (stage & 8));
}

// ================= TM1680 DRIVER =================
void tm1680_init() {
  pinMode(PIN_CLK, OUTPUT);
  pinMode(PIN_DIO, OUTPUT);
  tm1680_writeCmd(TM1680_SYSON);
  tm1680_writeCmd(0x20);
}

void displayNumber(byte startAddr, int num) {
  if (num > 999)
    num = 999;
  int d1 = num / 100;
  int d2 = (num % 100) / 10;
  int d3 = num % 10;

  if (num >= 100) {
    tm1680_sendData(startAddr, DIGITS[d1]);
    tm1680_sendData(startAddr + 2, DIGITS[d2]);
    tm1680_sendData(startAddr + 4, DIGITS[d3]);
  } else if (num >= 10) {
    tm1680_sendData(startAddr, 0x00);
    tm1680_sendData(startAddr + 2, DIGITS[d2]);
    tm1680_sendData(startAddr + 4, DIGITS[d3]);
  } else {
    tm1680_sendData(startAddr, 0x00);
    tm1680_sendData(startAddr + 2, 0x00);
    tm1680_sendData(startAddr + 4, DIGITS[d3]);
  }
}

void tm1680_writeCmd(byte cmd) {
  tm1680_start();
  tm1680_sendByte(cmd);
  tm1680_stop();
}

void tm1680_sendData(byte addr, byte data) {
  tm1680_start();
  tm1680_sendByte(0xC0 | addr);
  tm1680_sendByte(data);
  tm1680_stop();
}

void tm1680_start() {
  digitalWrite(PIN_DIO, HIGH);
  digitalWrite(PIN_CLK, HIGH);
  delayMicroseconds(2);
  digitalWrite(PIN_DIO, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_CLK, LOW);
}

void tm1680_stop() {
  digitalWrite(PIN_CLK, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_DIO, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_CLK, HIGH);
  delayMicroseconds(2);
  digitalWrite(PIN_DIO, HIGH);
}

void tm1680_sendByte(byte data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(PIN_CLK, LOW);
    digitalWrite(PIN_DIO, (data & 1) ? HIGH : LOW);
    data >>= 1;
    delayMicroseconds(2);
    digitalWrite(PIN_CLK, HIGH);
    delayMicroseconds(2);
  }
  digitalWrite(PIN_CLK, LOW);
  digitalWrite(PIN_DIO, HIGH);
  digitalWrite(PIN_CLK, HIGH);
  digitalWrite(PIN_CLK, LOW);
}
