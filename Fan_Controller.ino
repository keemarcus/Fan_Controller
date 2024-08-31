// Include the library
#include <FanController.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Adafruit_SGP40.h"
#include "Adafruit_SHT31.h"

Adafruit_SGP40 sgpInternal;
Adafruit_SHT31 sht31Internal;

Adafruit_SGP40 sgpExternal;
Adafruit_SHT31 sht31External;

#define TCAADDR 0x70

int32_t voc_index_internal;
int32_t voc_index_external;

LiquidCrystal_I2C lcd(0x27, 16, 4);

const int buttonPin = 5;
int buttonState = 0;

const long buttonWait = 500;
unsigned long lastButtonPress = 0;

const long checkFanSpeedWait = 1000;
unsigned long lastFanSpeedCheck = 0;

const int fanOffSetting = 0;
const int fanLowSetting = 33;
const int fanMedSetting = 66;
const int fanHighSetting = 100;
const int fanAutoSetting = -1;
int currentFanSetting;

// Sensor wire is plugged into port 2 on the Arduino.
// For a list of available pins on your board,
// please refer to: https://www.arduino.cc/en/Reference/AttachInterrupt
#define SENSOR_PIN_1 1
#define SENSOR_PIN_2 0

// Choose a threshold in milliseconds between readings.
// A smaller value will give more updated results,
// while a higher value will give more accurate and smooth readings
#define SENSOR_THRESHOLD 1000

// PWM pin (4th on 4 pin fans)
#define PWM_PIN 9

// Initialize library

FanController fan1(SENSOR_PIN_1, SENSOR_THRESHOLD, PWM_PIN);
FanController fan2(SENSOR_PIN_2, SENSOR_THRESHOLD, PWM_PIN);

/*
   The setup function. We only start the library here
*/
void setup(void) {
  Serial.begin(9600);
  //while (!Serial){
    //delay(10);
  //}
  Wire.begin();

  pinMode(buttonPin, INPUT);
  currentFanSetting = fanOffSetting;
  //pinMode(SENSOR_PIN, INPUT_PULLUP);
  // start serial port
  

  // Start up the library
  fan1.begin();
  fan2.begin();

  tcaselect(2);
  if (!sgpInternal.begin()) {
    Serial.println("Internal SGP40 sensor not found :(");
  }

  if (!sht31Internal.begin(0x44)) {  // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find Internal SHT31");
  }

  tcaselect(1);
  if (!sgpExternal.begin()) {
    Serial.println("External SGP40 sensor not found :(");
  }

  if (!sht31External.begin(0x44)) {  // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find External SHT31");
  }

  tcaselect(0);
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("I-VOC:");
  lcd.setCursor(10, 0);
  lcd.print("E-VOC:");
  lcd.setCursor(0, 1);
  lcd.print("Fan Setting:");
  lcd.setCursor(0, 2);
  lcd.print("Fan 1 RPM:");
  lcd.setCursor(0, 3);
  lcd.print("Fan 2 RPM:");


  currentFanSetting = ChangeFanSetting(-2);
  fan1.setDutyCycle(currentFanSetting);
  fan2.setDutyCycle(currentFanSetting);
}

/*
   Main function, get and show the temperature
*/
void loop(void) {
  unsigned long currentMillis = millis();
  buttonState = digitalRead(buttonPin);

  if (currentMillis - lastFanSpeedCheck >= checkFanSpeedWait) {
    // check the voc readings
    tcaselect(2);
    float ti = sht31Internal.readTemperature();
    float hi = sht31Internal.readHumidity();
    voc_index_internal = sgpInternal.measureVocIndex(ti, hi);

    tcaselect(1);
    float te = sht31External.readTemperature();
    float he = sht31External.readHumidity();
    voc_index_external = sgpExternal.measureVocIndex(te, he);

    tcaselect(0);
    lcd.setCursor(6, 0);
    lcd.print(FormatVOC(voc_index_internal));
    lcd.setCursor(16, 0);
    lcd.print(FormatVOC(voc_index_external));

    if(currentFanSetting == fanAutoSetting){
      Serial.print("Auto Fan Setting: ");
      int autoFanSetting;
      switch(voc_index_internal){
        case 0 ... 110:
          autoFanSetting = 0;
          break;
        case 111 ... 150:
          autoFanSetting = fanLowSetting;
          break;
        case 151 ... 199:
          autoFanSetting = fanMedSetting;
          break;
        default:
          autoFanSetting = fanHighSetting;
          break;
      }
      Serial.println(autoFanSetting);
      fan1.setDutyCycle(autoFanSetting);
      fan2.setDutyCycle(autoFanSetting);
    }

    // Call fan.getSpeed() to get fan RPM.
    Serial.print("Current speed: ");
    unsigned int fan1RPM = fan1.getSpeed();
    unsigned int fan2RPM = fan2.getSpeed();
    String formattedRPM1 = FormatRPM(fan1RPM);
    String formattedRPM2 = FormatRPM(fan2RPM);
    Serial.print(formattedRPM1 + " - " + formattedRPM2);
    Serial.println(" RPM");
    lcd.setCursor(11, 2);
    lcd.print(formattedRPM1);
    lcd.setCursor(11, 3);
    lcd.print(formattedRPM2);
    lastFanSpeedCheck = millis();
  }

  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (buttonState == HIGH && (currentMillis - lastButtonPress >= buttonWait)) {
    lastButtonPress = millis();
    currentFanSetting = ChangeFanSetting(currentFanSetting);
    Serial.print("Changing Fan Setting : ");
    if(currentFanSetting == fanAutoSetting){
      Serial.println("Auto");
    } else{
      Serial.println(currentFanSetting);
      fan1.setDutyCycle(currentFanSetting);
      fan2.setDutyCycle(currentFanSetting);
    }
  }
}

int ChangeFanSetting(int currentSetting) {
  lcd.setCursor(13, 1);
  switch (currentSetting) {
    case fanOffSetting:
      lcd.print("LOW ");
      return fanLowSetting;
    case fanLowSetting:
      lcd.print("MED ");
      return fanMedSetting;
    case fanMedSetting:
      lcd.print("HIGH");
      return fanHighSetting;
    case fanHighSetting:
      lcd.print("AUTO");
      return fanAutoSetting;
    default:
      lcd.print("OFF ");
      return fanOffSetting;
  }
}

String FormatRPM(int rawRPM) {
  String rawRPMString = String(rawRPM);
  String formattedRPM = "";
  for (int i = 0; i < 5; i++) {
    if (rawRPMString.charAt(i) == NULL) {
      formattedRPM += ' ';
    } else {
      formattedRPM += rawRPMString.charAt(i);
    }
  }

  return formattedRPM;
}

String FormatVOC(int rawVOC) {
  String rawVOCString = String(rawVOC);
  String formattedRPM = "";
  for (int i = 0; i < 4; i++) {
    if (rawVOCString.charAt(i) == NULL) {
      formattedRPM += ' ';
    } else {
      formattedRPM += rawVOCString.charAt(i);
    }
  }

  return formattedRPM;
}

void tcaselect(uint8_t i) {
  if (i > 7) return;

  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}