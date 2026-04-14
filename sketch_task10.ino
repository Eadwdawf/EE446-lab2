#include <PDM.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_APDS9960.h>

short sampleBuffer[256];
volatile int samplesRead = 0;
int micLevel = 0;

// state variables
bool darkState = false;
bool nearState = false;
bool movingState = false;

// counters for proximity stability
int nearCount = 0;
int farCount = 0;

// previous accelerometer values
float lastAx = 0.0, lastAy = 0.0, lastAz = 1.0;
bool firstAccelRead = true;

// keep last valid sensor readings
int clearVal = 120;
int proximity = 240;
int r = 0, g = 0, b = 0;

// optional timeout tracking
unsigned long lastColorUpdate = 0;
unsigned long lastProxUpdate = 0;

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  // Microphone
  PDM.onReceive(onPDMdata);
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }

  // IMU
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  // APDS9960
  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960!");
    while (1);
  }

  Serial.println("Task 10 situation classifier started");
}

void loop() {
  // ---------------- microphone ----------------
  if (samplesRead) {
    long sum = 0;
    for (int i = 0; i < samplesRead; i++) {
      sum += abs(sampleBuffer[i]);
    }
    micLevel = sum / samplesRead;
    samplesRead = 0;
  }

  bool soundState = (micLevel > 300);

  // ---------------- light ----------------
  // only update when new valid data is available
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, clearVal);
    lastColorUpdate = millis();
  }

  // dark hysteresis
  const int darkOnThreshold = 40;
  const int darkOffThreshold = 100;

  if (!darkState && clearVal < darkOnThreshold) {
    darkState = true;
  } else if (darkState && clearVal > darkOffThreshold) {
    darkState = false;
  }

  // ---------------- proximity ----------------
  // only update when new valid data is available
  if (APDS.proximityAvailable()) {
    proximity = APDS.readProximity();
    lastProxUpdate = millis();
  }

  const int nearOnThreshold = 80;
  const int nearOffThreshold = 180;
  const int confirmCount = 3;

  if (proximity < nearOnThreshold) {
    nearCount++;
    farCount = 0;
  } else if (proximity > nearOffThreshold) {
    farCount++;
    nearCount = 0;
  } else {
    nearCount = 0;
    farCount = 0;
  }

  if (nearCount >= confirmCount) {
    nearState = true;
  }

  if (farCount >= confirmCount) {
    nearState = false;
  }

  // ---------------- motion ----------------
  float ax = 0.0, ay = 0.0, az = 1.0;
  float motionMetric = 0.0;

  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(ax, ay, az);

    if (firstAccelRead) {
      lastAx = ax;
      lastAy = ay;
      lastAz = az;
      firstAccelRead = false;
    }

    motionMetric = abs(ax - lastAx) + abs(ay - lastAy) + abs(az - lastAz);

    lastAx = ax;
    lastAy = ay;
    lastAz = az;
  }

  const float movingOnThreshold = 0.18;
  const float movingOffThreshold = 0.08;

  if (!movingState && motionMetric > movingOnThreshold) {
    movingState = true;
  } else if (movingState && motionMetric < movingOffThreshold) {
    movingState = false;
  }

  // ---------------- final label ----------------
  String finalLabel = "UNMATCHED";

  if (!soundState && !darkState && !movingState && !nearState) {
    finalLabel = "QUIET_BRIGHT_STEADY_FAR";
  } else if (soundState && !darkState && !movingState && !nearState) {
    finalLabel = "NOISY_BRIGHT_STEADY_FAR";
  } else if (!soundState && darkState && !movingState && nearState) {
    finalLabel = "QUIET_DARK_STEADY_NEAR";
  } else if (soundState && !darkState && movingState && nearState) {
    finalLabel = "NOISY_BRIGHT_MOVING_NEAR";
  }

  // ---------------- serial output ----------------
  Serial.print("mic=");
  Serial.print(micLevel);
  Serial.print(" clear=");
  Serial.print(clearVal);
  Serial.print(" motion=");
  Serial.print(motionMetric, 3);
  Serial.print(" prox=");
  Serial.println(proximity);

  Serial.print("sound=");
  Serial.print(soundState);
  Serial.print(" dark=");
  Serial.print(darkState);
  Serial.print(" moving=");
  Serial.print(movingState);
  Serial.print(" near=");
  Serial.print(nearState);
  Serial.print(" nearCount=");
  Serial.print(nearCount);
  Serial.print(" farCount=");
  Serial.println(farCount);

  Serial.print("FINAL_LABEL=");
  Serial.println(finalLabel);
  Serial.println();

  delay(250);
}