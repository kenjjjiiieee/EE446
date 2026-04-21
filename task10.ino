#include <PDM.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_APDS9960.h>

int microphone    = 300;
int light  = 150;
float motion = 0.15;
int prox   = 240;  

short sampleBuffer[256];
volatile int samplesRead = 0;

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  PDM.onReceive(onPDMdata);
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM microphone.");
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

  Serial.println("Workspace classifier started");
}

void loop() {

  int level = 0;
  if (samplesRead) {
    long sum = 0;
    for (int i = 0; i < samplesRead; i++) {
      sum += abs(sampleBuffer[i]);
    }
    level = sum / samplesRead;
    samplesRead = 0;
  }

  static int lastClear = 500;  
  int r, g, b, c;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, c);
    if (c > 0) lastClear = c;  
  }
  int clearVal = lastClear;


  static float prevX = 0, prevY = 0, prevZ = 0;
  float x, y, z;
  float motionVal = 0;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    float dx = x - prevX;
    float dy = y - prevY;
    float dz = z - prevZ;
    motionVal = sqrt(dx*dx + dy*dy + dz*dz);
    prevX = x; prevY = y; prevZ = z;
  }

  int proximity = 0;
  if (APDS.proximityAvailable()) {
    proximity = APDS.readProximity();
  }

  // binary flags
  int sound  = (level     > microphone)    ? 1 : 0;
  int dark   = (clearVal  < light)  ? 1 : 0;
  int moving = (motionVal > motion) ? 1 : 0;
  int near   = (proximity > prox)   ? 1 : 0;

  String state;

  if (!sound && !dark && !moving && !near) {
    state = "QUIET_BRIGHT_STEADY_FAR";
  } else if (sound && !dark && !moving && !near) {
    state = "NOISY_BRIGHT_STEADY_FAR";
  } else if (!sound && dark && !moving && near) {
    state = "QUIET_DARK_STEADY_NEAR";
  } else if (sound && !dark && moving && near) {
    state = "NOISY_BRIGHT_MOVING_NEAR";
  } else {
    if (sound && moving && near)   state = "NOISY_BRIGHT_MOVING_NEAR";
    else if (dark && near)         state = "QUIET_DARK_STEADY_NEAR";
    else if (dark)                 state = "QUIET_DARK_STEADY_NEAR";
    else if (sound)                state = "NOISY_BRIGHT_STEADY_FAR";
    else                           state = "QUIET_BRIGHT_STEADY_FAR";
  }

  Serial.print("raw,mic=");     Serial.print(level);
  Serial.print(",clear=");      Serial.print(clearVal);
  Serial.print(",motion=");     Serial.print(motionVal, 3);
  Serial.print(",prox=");       Serial.println(proximity);

  Serial.print("flags,sound="); Serial.print(sound);
  Serial.print(",dark=");       Serial.print(dark);
  Serial.print(",moving=");     Serial.print(moving);
  Serial.print(",near=");       Serial.println(near);

  Serial.print("state,");       Serial.println(state);
  Serial.println();

  delay(500);
}