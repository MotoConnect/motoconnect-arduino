#include <Wire.h>
#include <MPU6050.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <rgb_lcd.h>

MPU6050 mpu;
TinyGPSPlus gps;
rgb_lcd lcd;

const int colorR = 255;
const int colorG = 255;
const int colorB = 255;

const float alpha = 0.98;
const float beta = 0.02;

const float timeInterval = 0.1;

float pitchAngle = 0.0;

float accelAngleY = 0.0;
float gyroOffsetY = 0.0;

float accelerationY = 0.0;

float velocityY = 0.0;

int velocity_km = 0;

void setup() {
  Serial.begin(9600);
  Serial3.begin(9600);
  
  Wire.begin();
  mpu.initialize();

  lcd.begin(16, 2);
  
  // Configurer la couleur de l'écran
  lcd.setRGB(colorR, colorG, colorB);

  // Affichage par défaut de l'écran LCD
  lcd.print("Inclinaison:");
  lcd.setCursor(0, 1);
  lcd.print("Temp:");

  calibrateSensors();
}

void loop() {
  int16_t ax, ay, az, gx, gy, gz, angle;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  
  // Initialisation de la température
  int16_t temperature = mpu.getTemperature();

  float accelAngle = atan2(ax - accelAngleY, az) * (180.0 / PI);
  float gyroRate = (gy - gyroOffsetY) / 131.0;

  // Calcul de l'inclinaison
  pitchAngle = alpha * (pitchAngle + gyroRate * 0.01) + beta * accelAngle;

  // Créer un format adéquat pour l'affichage
  angle = int(trunc(pitchAngle)) - 90;
  temperature = int(trunc(temperature / 340.0 + 36.53));

  // Mise à l'échelle du monde réel de la valeur de l'accélération
  accelerationY = ay / 16384.0;

  // Calcul de la vitesse
  velocityY = accelerationY * timeInterval;

  // Convertir en km/h
  velocity_km = int(trunc(velocityY * 3.6));

  // Eviter les valeurs négatives
  if (angle < 0){
    angle = angle * -1;
  }
  if (velocity_km < 0){
    velocity_km = velocity_km * -1;
  }
  
  // Eviter le bug de l'affichage d'un zéro à la fin
  if (angle == 99 || angle == 9) {
    lcd.setCursor(12, 0);
    lcd.print("    ");
  }

  // Affichage de l'inclinaison de la moto
  lcd.setCursor(12, 0);
  lcd.print(angle);

  // Affichage de la temperature 
  lcd.setCursor(5, 1);
  lcd.print(temperature);
  lcd.print("'C");

  // Affichage des infos gps
  if(Serial3.available()) {
    if(gps.encode(Serial3.read())) {
      // Affichage de la vitesse
      Serial.print("Velocity : ");
      Serial.print(velocity_km);
      Serial.print(" km/h ");
      displayInfo();
    }
  }

  delay(15);
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

void displayInfo() {
	Serial.print(F("Location: ")); 
	if (gps.location.isValid()) {
		Serial.print(gps.location.lat(), 6);
		Serial.print(F(","));
		Serial.print(gps.location.lng(), 6);
	} else {
		Serial.print(F("INVALID"));
	}

	Serial.print(F("  Date/Time: "));
	if (gps.date.isValid()) {
		Serial.print(gps.date.month());
		Serial.print(F("/"));
		Serial.print(gps.date.day());
		Serial.print(F("/"));
		Serial.print(gps.date.year());
	} else {
		Serial.print(F("INVALID"));
	}

	Serial.print(F(" "));
	if (gps.time.isValid()) {
		if (gps.time.hour() < 10) Serial.print(F("0"));
		Serial.print(gps.time.hour());
		Serial.print(F(":"));
		if (gps.time.minute() < 10) Serial.print(F("0"));
		Serial.print(gps.time.minute());
		Serial.print(F(":"));
		if (gps.time.second() < 10) Serial.print(F("0"));
		Serial.print(gps.time.second());
		Serial.print(F("."));
		if (gps.time.centisecond() < 10) Serial.print(F("0"));
		Serial.print(gps.time.centisecond());
	} else {
		Serial.print(F("INVALID"));
	}

	Serial.println();
}
