#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include "RTClib.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define DHTPIN 6     // what digital pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define analogInPin A0  // Analog input pin that the potentiometer is attached to
#define analogOutPin 3 // Analog output pin that the LED is attached to

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x3f, 16, 2);
RTC_DS3231 rtc;
OneWire  dsWire(8);
DallasTemperature ds(&dsWire);
DeviceAddress dsAddress;
DateTime now;
DateTime lastTime;

unsigned long lastMillis = 0;
bool powerOn = true;
int screen = 0;
int maxPower = 80;
float kP = 5;
float kI = 0.05;
float kD = 5;
float dsTempC = 0;
float setTempC = 32.0;
float integral = 0;
char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};



void setup() {
  delay(1000);
  
  TCCR2B = TCCR2B & 0b11111000 | 0x01;
  Serial.begin(9600);
  Serial.println("Starting after reboot");
  ds.begin();
  if (!ds.getAddress(dsAddress, 0)) Serial.println("Unable to find address for Device 0"); 
   // set the resolution to 12 bit (9-12)
  ds.setResolution(dsAddress, 12);
  Serial.print("Device 0 Resolution: ");
  Serial.print(ds.getResolution(dsAddress), DEC); 
  Serial.println();
  dht.begin();
  lcd.begin();
  lcd.backlight();
  if (screen == 0)
  {
    lcd.setCursor(0, 0);
    lcd.print("  Snake Heater  ");
    lcd.setCursor(0, 1);
    lcd.print("    v. 0.1    "); 
  }
  now=rtc.now();
  lastMillis = millis();
  integral = (maxPower / 2)/kI;
}

void loop() {

  /*sensorValue = analogRead(analogInPin);
  // map it to the range of the analog out:
  outputValue = map(sensorValue, 0, 1023, 0, 255);
  // change the analog out value:
  analogWrite(analogOutPin, outputValue);*/

  delay(1500);

  // Collecting data from sensors
  now = rtc.now();
  dsTempC = ds.getTempC(dsAddress);
  ds.requestTemperatures(); // Send the command to get temperature from ds18b20
  float h = dht.readHumidity();
  float t = dht.readTemperature();
 

  // Check for errors
   
  if (isnan(h) || isnan(t)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DHT read error!");
    Serial.println("DHT read error!");
    Serial.println();
    screen = 0;
    return;
  }
  if (! rtc.begin()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC error!");
    Serial.println("RTC error!");
    Serial.println();
    analogWrite(analogOutPin, 0);
    delay(3000);
    return;
  }

  if (now.second() < 30 && screen != 1)
  {
    lcd.clear();
    screen = 1;
  }
  if (now.second() >= 30 && screen != 2)
  {
    lcd.clear();
    screen = 2;
  }

  
  switch(screen)
  {
    case 1:
      lcd.setCursor(0, 0);
      if(now.day() < 10) lcd.print('0');
      lcd.print(now.day(), DEC);
      lcd.print('.');
      if(now.month() < 10) lcd.print('0');
      lcd.print(now.month(), DEC);
      lcd.print('.');
      lcd.print(now.year(), DEC);
      lcd.print(" (");
      lcd.print(daysOfTheWeek[now.dayOfTheWeek()]);
      lcd.print(") ");
      lcd.setCursor(0, 1);
      if(now.hour() < 10) lcd.print('0');
      lcd.print(now.hour(), DEC);
      lcd.print(':');
      if(now.minute() < 10) lcd.print('0');
      lcd.print(now.minute(), DEC);
      lcd.print(':');
      if(now.second() < 10) lcd.print('0');
      lcd.print(now.second(), DEC);
      lcd.print(" ");
      lcd.println(dsTempC);
      lcd.setCursor(14, 1);
      lcd.print(" C");
      delay(40);
      break;
    case 2:
      lcd.setCursor(0, 0);
      lcd.print("Hum: ");
      lcd.print(h);
      lcd.print("%");
      lcd.setCursor(0, 1);
      lcd.print("Temp: ");
      lcd.print(t);
      lcd.print(" C");
      delay(40);
      break;
    default:
    lcd.setCursor(0, 0);
    lcd.print("  Snake Heater  ");
    lcd.setCursor(0, 1);
    lcd.print("    v. 0.1    ");
    delay(2000);
    break; 
  }

  if (now.minute() < 30 && !powerOn)
  {
    powerOn = true;
    integral = (maxPower / 2)/kI;
    lastMillis = millis();
  }
  if (now.minute() >= 30 && powerOn) powerOn = false;

  if (!powerOn)
  {
    analogWrite(analogOutPin, 0);
    Serial.print("temp = ");
    Serial.println(dsTempC);
    Serial.println("Night mode, heater off");
    Serial.println();
    return;
  }
  
  unsigned long currentMillis = millis();
  float timeDelta = (currentMillis - lastMillis)/1000.0;
  Serial.println(timeDelta);
  lastMillis = currentMillis;


  
  float dsTempCDelta = setTempC - dsTempC;
  float differential = dsTempCDelta/timeDelta;
  if(kP*dsTempCDelta+kD*differential <= 5) integral += dsTempCDelta*timeDelta;
  int processVariable = kP*dsTempCDelta+kI*integral+kD*differential+0.51;
  Serial.print("time = ");
  Serial.println(timeDelta);
  Serial.print("temp = ");
  Serial.println(dsTempC);
  Serial.print("offset = ");
  Serial.println(dsTempCDelta);
  Serial.print("integral = ");
  Serial.println(integral);
  Serial.print("differential = ");
  Serial.println(differential);
  Serial.print("output = ");
  Serial.println(processVariable);
  Serial.println();
    
  if (processVariable < 0 || dsTempC > setTempC+0.4) analogWrite(analogOutPin, 0);
    else if (processVariable > maxPower) analogWrite(analogOutPin, maxPower);
      else analogWrite(analogOutPin, processVariable);

}




