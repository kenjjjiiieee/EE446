#include <Arduino_HS300x.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_APDS9960.h>

// thresholds
float humidThresh  = 3.0;
float tempThresh   = 0.4;
float magThresh    = 20.0;
int   lightThresh  = 80;

// cooldown
int cooldownMs = 4000;
unsigned long lastTrigger = 0;
String lastLabel = "";

// baseline values
float baseRH    = 0;
float baseTemp  = 0;
float baseMag   = 0;
int   baseClear = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);

  if (!HS300x.begin()) {
    Serial.println("Failed to initialize humidity/temperature sensor.");
    while (1);
  }

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU.");
    while (1);
  }

  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960 sensor.");
    while (1);
  }

  Serial.println("Collecting baseline, keep environment still...");

  float sumRH = 0, sumTemp = 0, sumMag = 0;
  int sumClear = 0;
  int n = 10;

  for (int i = 0; i < n; i++) {
    sumRH   += HS300x.readHumidity();
    sumTemp += HS300x.readTemperature();

    float x, y, z;
    if (IMU.magneticFieldAvailable()) {
      IMU.readMagneticField(x, y, z);
      sumMag += sqrt(x*x + y*y + z*z);
    }

    int r, g, b, c;
    if (APDS.colorAvailable()) {
      APDS.readColor(r, g, b, c);
      if (c > 0) sumClear += c;
    }

    Serial.print("warmup sample ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.println(n);

    delay(1000);
  }

  baseRH    = sumRH   / n;
  baseTemp  = sumTemp / n;
  baseMag   = sumMag  / n;
  baseClear = sumClear / n;

  Serial.print("baseline set - rh=");   Serial.print(baseRH, 2);
  Serial.print(", temp=");              Serial.print(baseTemp, 2);
  Serial.print(", mag=");               Serial.print(baseMag, 2);
  Serial.print(", clear=");             Serial.println(baseClear);
}

void loop() {

  float rh   = HS300x.readHumidity();
  float temp = HS300x.readTemperature();

  float x, y, z;
  float magMag = 0;
  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(x, y, z);
    magMag = sqrt(x*x + y*y + z*z);
  }

  static int lastClear = 500;
  int r, g, b, c;
  c = 0;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, c);
    if (c > 0) lastClear = c;
  }
  int clearVal = lastClear;

  float dRH    = rh      - baseRH;
  float dTemp  = temp    - baseTemp;
  float dMag   = abs(magMag   - baseMag);
  int   dClear = abs(clearVal - baseClear);

  // binary flags
  int humid_jump           = (dRH    > humidThresh)  ? 1 : 0;
  int temp_rise            = (dTemp  > tempThresh)   ? 1 : 0;
  int mag_shift            = (dMag   > magThresh)    ? 1 : 0;
  int light_or_color_change = (dClear > lightThresh) ? 1 : 0;

  String label = "BASELINE_NORMAL";

  if (mag_shift) {
    label = "MAGNETIC_DISTURBANCE_EVENT";
  } else if (light_or_color_change) {
    label = "LIGHT_OR_COLOR_CHANGE_EVENT";
  } else if (humid_jump || temp_rise) {
    label = "BREATH_OR_WARM_AIR_EVENT";
  }

  unsigned long now = millis();
  if (label != "BASELINE_NORMAL") {
    if (label == lastLabel && (now - lastTrigger) < cooldownMs) {
    } else {
      lastTrigger = now;
      lastLabel   = label;
    }
  } else {
    baseRH    += 0.01 * (rh      - baseRH);
    baseTemp  += 0.01 * (temp    - baseTemp);
    baseMag   += 0.01 * (magMag  - baseMag);
    baseClear += (int)(0.01 * (clearVal - baseClear));
    lastLabel = "";
  }

  Serial.print("raw,rh=");   Serial.print(rh, 2);
  Serial.print(",temp=");    Serial.print(temp, 2);
  Serial.print(",mag=");     Serial.print(magMag, 2);
  Serial.print(",r=");       Serial.print(r);
  Serial.print(",g=");       Serial.print(g);
  Serial.print(",b=");       Serial.print(b);
  Serial.print(",clear=");   Serial.println(clearVal);

  Serial.print("flags,humid_jump=");        Serial.print(humid_jump);
  Serial.print(",temp_rise=");              Serial.print(temp_rise);
  Serial.print(",mag_shift=");              Serial.print(mag_shift);
  Serial.print(",light_or_color_change=");  Serial.println(light_or_color_change);

  Serial.print("event,"); Serial.println(label);
  Serial.println();

  delay(500);
}