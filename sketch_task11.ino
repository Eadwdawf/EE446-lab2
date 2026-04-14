#include <Arduino_HS300x.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_APDS9960.h>
#include <math.h>

float baselineHumidity = 0.0;
float baselineTemperature = 0.0;
float baselineMag = 0.0;
int baselineClear = 0;

// store last valid sensor readings
int clearVal = 0;
float magMetric = 0.0;

bool baselineReady = false;
unsigned long startTime = 0;
unsigned long lastEventTime = 0;
const unsigned long cooldownMs = 3000;

// confirmation counters
int lightCount = 0;
int magCount = 0;
const int confirmCount = 2;

void setup() {
  Serial.begin(115200);
  delay(1500);

  if (!HS300x.begin()) {
    Serial.println("Failed to initialize HS300x!");
    while (1);
  }

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960!");
    while (1);
  }

  startTime = millis();

  Serial.println("Task 11 event detector started");
  Serial.println("Leave the board in normal room conditions for 5 seconds...");
}

void loop() {
  // humidity and temperature 
  float rh = HS300x.readHumidity();
  float temp = HS300x.readTemperature();

  // magnetometer
  float mx = 0.0, my = 0.0, mz = 0.0;
  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(mx, my, mz);
    magMetric = sqrt(mx * mx + my * my + mz * mz);
  }

  // light
  int rDummy, gDummy, bDummy;
  if (APDS.colorAvailable()) {
    APDS.readColor(rDummy, gDummy, bDummy, clearVal);
  }

  // capture baseline after 5 seconds
  if (!baselineReady) {
    if (millis() - startTime < 5000) {
      Serial.print("Waiting for baseline... clear=");
      Serial.print(clearVal);
      Serial.print(" mag=");
      Serial.println(magMetric, 2);
      delay(500);
      return;
    }

    baselineHumidity = rh;
    baselineTemperature = temp;
    baselineMag = magMetric;
    baselineClear = clearVal;
    baselineReady = true;

    Serial.println("Baseline captured.");
    Serial.print("baseline rh=");
    Serial.print(baselineHumidity, 2);
    Serial.print(" temp=");
    Serial.print(baselineTemperature, 2);
    Serial.print(" mag=");
    Serial.print(baselineMag, 2);
    Serial.print(" clear=");
    Serial.println(baselineClear);
    Serial.println();

    delay(1000);
    return;
  }

  // event indicators
  bool humidJump = (rh - baselineHumidity > 8.0);
  bool tempRise = (temp - baselineTemperature > 1.5);

  // magnetic change: lowered threshold
  bool magShiftRaw = (fabs(magMetric - baselineMag) > 5.0);

  // light change: only based on clear channel
  bool lightChangeRaw = (fabs(clearVal - baselineClear) > 180);

  // confirmation logic
  if (magShiftRaw) {
    magCount++;
  } else {
    magCount = 0;
  }

  if (lightChangeRaw) {
    lightCount++;
  } else {
    lightCount = 0;
  }

  bool magShift = (magCount >= confirmCount);
  bool lightOrColorChange = (lightCount >= confirmCount);

  // cooldown
  bool inCooldown = (millis() - lastEventTime < cooldownMs);

  String finalLabel = "BASELINE_NORMAL";

  if (!inCooldown) {
    if (humidJump || tempRise) {
      finalLabel = "BREATH_OR_WARM_AIR_EVENT";
      lastEventTime = millis();
    } else if (magShift) {
      finalLabel = "MAGNETIC_DISTURBANCE_EVENT";
      lastEventTime = millis();
    } else if (lightOrColorChange) {
      finalLabel = "LIGHT_OR_COLOR_CHANGE_EVENT";
      lastEventTime = millis();
    }
  }

  // serial output
  Serial.print("rh=");
  Serial.print(rh, 2);
  Serial.print(" temp=");
  Serial.print(temp, 2);
  Serial.print(" mag=");
  Serial.print(magMetric, 2);
  Serial.print(" clear=");
  Serial.println(clearVal);

  Serial.print("humid_jump=");
  Serial.print(humidJump);
  Serial.print(" temp_rise=");
  Serial.print(tempRise);
  Serial.print(" mag_shift=");
  Serial.print(magShift);
  Serial.print(" light_or_color_change=");
  Serial.println(lightOrColorChange);

  Serial.print("FINAL_LABEL=");
  Serial.println(finalLabel);
  Serial.println();

  delay(500);
}