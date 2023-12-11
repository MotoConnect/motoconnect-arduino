#include <Wire.h>
#include <MPU6050.h>
#include "rgb_lcd.h"

MPU6050 mpu;
rgb_lcd lcd;

const int colorR = 255;
const int colorG = 255;
const int colorB = 255;

const float alpha = 0.98;
const float beta = 0.02;

float pitchAngle = 0.0;

float accelAngleY = 0.0;
float gyroOffsetY = 0.0;

void setup() {
  Serial.begin(9600);
  
  Wire.begin();
  mpu.initialize();

  lcd.begin(16, 2);
  
  lcd.setRGB(colorR, colorG, colorB);

  lcd.print("Inclinaison:");
  lcd.setCursor(0, 1);
  lcd.print("Temp:");

  calibrateSensors();
}

void loop() {
  int16_t ax, ay, az, gx, gy, gz, angle;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  int16_t temperature = mpu.getTemperature();

  float accelAngle = atan2(ax - accelAngleY, az) * (180.0 / PI);
  float gyroRate = (gy - gyroOffsetY) / 131.0;

  pitchAngle = alpha * (pitchAngle + gyroRate * 0.01) + beta * accelAngle;

  angle = int(trunc(pitchAngle));
  temperature = int(trunc(temperature / 340.0 + 36.53));

  if (angle < 0){
    angle = angle * -1;
  }
  
  if (angle == 99 || angle == 9) {
    lcd.setCursor(12, 0);
    lcd.print("    ");
  }

  lcd.setCursor(12, 0);
  lcd.print(angle);

  lcd.setCursor(5, 1);
  lcd.print(temperature);
  lcd.print("'C");

  delay(10);
}

void calibrateSensors() {
  const int numSamples = 1000;
  int32_t accelY = 0;
  int32_t gyroY = 0;
  int16_t ax, ay, az, gx, gy, gz;

  for (int i = 0; i < numSamples; ++i) {
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    accelY += ay;
    gyroY += gy;
    delay(1);
  }

  int16_t moyenneAy = accelY / numSamples;
  accelAngleY = atan2(ax, -moyenneAy) * (180.0 / PI);
  gyroOffsetY = gyroY / numSamples;
}
