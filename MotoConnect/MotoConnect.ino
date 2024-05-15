#include <Wire.h>
#include <MPU6050.h>
#include <TinyGPSPlus.h>
#include <rgb_lcd.h>
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

#define WIFI_SSID "Redmi Hotspot"
#define WIFI_PASSWORD "0775_BSA"

#define API_KEY "AIzaSyD3fi827lOpQ_xF124BntwaY_oQhdDA9AU"

#define FIREBASE_PROJECT_ID "motoconnect-c75e4"

#define USER_EMAIL "motoconnect@arduino.com"
#define USER_PASSWORD "motoarduino"

#define ID "3"

const int colorR = 255;
const int colorG = 255;
const int colorB = 255;

const float alpha = 0.98;
const float beta = 0.02;

const float timeInterval = 0.1;

int16_t ax, ay, az, gx, gy, gz, angle;

float pitchAngle = 0.0;

float accelAngleY = 0.0;
float gyroOffsetY = 0.0;

float velocityY = 0.0;

int velocity_km = 0;

FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long dataMillis = 0;

bool taskcomplete = false;
String startDateTime = "";
String lastDateTime = "";

MPU6050 mpu;
TinyGPSPlus gps;
rgb_lcd lcd;

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(".");
      delay(300);
    }
  Serial.println();
  Serial.println("Connected !");
  
  Wire.begin();
  mpu.initialize();

  lcd.begin(16, 2);
  
  // Configurer la couleur de l'écran
  lcd.setRGB(colorR, colorG, colorB);

  // Affichage par défaut de l'écran LCD
  lcd.print("Inclinaison:");
  lcd.setCursor(0, 1);
  lcd.print("Temp:");

  config.api_key = API_KEY;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;

  Firebase.reconnectNetwork(true);

  fbdo.setBSSLBufferSize(4096, 1024);

  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);

  calibrateSensors();
}

void loop() {
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

  // Eviter les valeurs négatives
  if (angle < 0){
    angle = angle * -1;
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

  while (Serial2.available() > 0) {
		if (gps.encode(Serial2.read())) {
      if (gps.location.isValid() && gps.date.isValid() && gps.time.isValid() && gps.speed.isValid()) {
        updateFirebase();
      }
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
	if (gps.location.isValid() && gps.date.isValid() && gps.time.isValid() && gps.speed.isValid()) {
    Serial.print("Velocity : ");
    Serial.print(int(gps.speed.kmph()));
    Serial.print(" km/h ");

    Serial.print(F("Location: ")); 
		Serial.print(gps.location.lat(), 6);
		Serial.print(F(","));
		Serial.print(gps.location.lng(), 6);
    
    Serial.print(F("  Date/Time: "));
    Serial.print(gps.date.month());
		Serial.print(F("/"));
		Serial.print(gps.date.day());
		Serial.print(F("/"));
		Serial.print(gps.date.year());

    Serial.print(F(" "));
    if (gps.time.hour() < 10) Serial.print(F("0"));
		Serial.print(gps.time.hour());
		Serial.print(F(":"));
		if (gps.time.minute() < 10) Serial.print(F("0"));
		Serial.print(gps.time.minute());
		Serial.print(F(":"));
		if (gps.time.second() < 10) Serial.print(F("0"));
		Serial.print(gps.time.second());
	}

	Serial.println();
}

void updateFirebase() {
  if (Firebase.ready() && (millis() - dataMillis > 10000 || dataMillis == 0)) {
      dataMillis = millis();

      FirebaseJson content;

      String devicePath = "devices/";
      devicePath.concat(ID);

      content.clear();
      content.set("fields/currentMotoPosition/geoPointValue/latitude", gps.location.lat());
      content.set("fields/currentMotoPosition/geoPointValue/longitude", gps.location.lng());

      Serial.print("Mise à jour du document... ");

      if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", devicePath.c_str(), content.raw(), "currentMotoPosition"))
        Serial.println("Done");
      else
        Serial.println(fbdo.errorReason());
      
      
      if (int(gps.speed.kmph()) > 10) {
        devicePath.concat("/journeys/");

        content.clear();

        if (!taskcomplete) {
          taskcomplete = true;

          startDateTime = createDateTime(
            gps.date.year(),
            gps.date.day(),
            gps.date.month(),
            gps.time.hour(),
            gps.time.minute(),
            gps.time.second()
            );
          
          devicePath.concat(startDateTime);

          content.set("fields/startDateTime/timestampValue", startDateTime);

          Serial.print("Creation d'un document... ");

          if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", devicePath.c_str(), content.raw()))
            Serial.println("Done");
          else
            Serial.println(fbdo.errorReason());
        } else {
          devicePath.concat(startDateTime);
          content.set("fields/startDateTime/timestampValue", startDateTime);
        }

        lastDateTime = createDateTime(
            gps.date.year(),
            gps.date.day(),
            gps.date.month(),
            gps.time.hour(),
            gps.time.minute(),
            gps.time.second()
            );

        content.set("fields/endDateTime/timestampValue", lastDateTime);

        Serial.print("Mise à jour du document... ");

        if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", devicePath.c_str(), content.raw(), "startDateTime,endDateTime"))
          Serial.println("Done");
        else
          Serial.println(fbdo.errorReason());


        devicePath.concat("/points/");
        devicePath.concat(lastDateTime);

        content.clear();

        content.set("fields/time/timestampValue", createDateTime(
          gps.date.year(),
          gps.date.day(),
          gps.date.month(),
          gps.time.hour(),
          gps.time.minute(),
          gps.time.second()
          )
        );
        content.set("fields/tilt/integerValue", angle);
        content.set("fields/speed/integerValue", int(gps.speed.kmph()));
        content.set("fields/geoPoint/geoPointValue/latitude", gps.location.lat());
        content.set("fields/geoPoint/geoPointValue/longitude", gps.location.lng());

        Serial.print("Creation d'un document... ");

        if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", devicePath.c_str(), content.raw()))
          Serial.println("Done");
        else
          Serial.println(fbdo.errorReason());
      }
  }
}

String createDateTime(int year, int day, int month, int hour, int min, int sec) {
  String date = String(year);
  date.concat("-");
  date.concat(month);
  date.concat("-");
  date.concat(day);
  date.concat("T");
  date.concat(hour);
  date.concat(":");
  date.concat(min);
  date.concat(":");
  date.concat(sec);
  date.concat("Z");
  return date;
}
