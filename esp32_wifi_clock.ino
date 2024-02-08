#include <NTPClient.h>
//#include <ESP8266WiFi.h>
#include <WiFi.h>  // for ESP32 WiFi shield
#include <WiFiUdp.h>
#include "GyverFilters.h"
#include <GyverMAX7219.h>
#define AM_W 32  // 4 матрицы (32 точки)
#define AM_H 8   // 1 матрица (8 точек)


const int sensorLight = 36;  // вывод фоторезистора
const char* ssid = "XXXX";   // имя сети WI-FI
const char* password = "XXXXXXXX"; // пароль сети WI-FI

// дисплей 4х1, пин CS на D19, остальные на аппаратный SPI
MAX7219< 4, 1, 19 > mtrx;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.vniiftri.ru", 14400, 60000);  // 14400 = UTC+4 Самара

int errors = 0;
int sec = 0;
int hours = 0;
int minutes = 0;
int sensorValue = 0;
bool onChange = true;
bool expiredTime = false;
hw_timer_t* Clock_timer = NULL;

void IRAM_ATTR onTimer() {
  sec++;
  onChange = true;  // раз в секунду обновляем показания на дисплее
  if (sec == 60) {
    sec = 0;
    minutes++;    
  }
  if (minutes == 60) {
    minutes = 0;
    hours++;
  }
  if (hours == 24) {
    hours = 0;    
    expiredTime = true;   // раз в сутки обновляем время
  }
}

void setup() {
  delay(500);
  WiFi.begin(ssid, password);  
  delay(500);
  mtrx.begin();
  int y0 = 15;
  int y1 = 16;
  while (!WiFi.isConnected()) {
    mtrx.fastLineH(3, y0, y1);
    mtrx.fastLineH(4, y0, y1);
    mtrx.update();
    y0--;
    y1++;
    if (y0 == 0) {
      y0 = 15;
      y1 = 16;
      mtrx.clear();
    }
    delay(100);
  }
  displayText("Wi-Fi");
  delay(100);
  Clock_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(Clock_timer, &onTimer, true);
  timerAlarmWrite(Clock_timer, 1000000, true);
  timerAlarmEnable(Clock_timer);

  if (!timeUpdate()) {
    displayText("Error");
    ESP.restart();
  }
}

void loop() {
  if (onChange) {    
    mtrx.setBright(analogRead(sensorLight) / 273); // 4095/15=273 (12bit/0..15)
    display();
    onChange = false;
  }
  if (expiredTime) {
    timeUpdate();
    expiredTime = false;
  }
  if (errors > 30) {  // так как время уже месяц не обновлялось перезагружаем систему
    ESP.restart();  
  }
}

void displayText(String s) {
  mtrx.clear();
  mtrx.setCursor(2, 0);
  mtrx.print(s);
  mtrx.update();
}

void display() {
  mtrx.clear();
  mtrx.setCursor(2, 0);
  if (hours < 10) {
    mtrx.print(" ");
  }
  mtrx.print(hours);
  mtrx.setCursor(18, 0);
  if (minutes < 10) {
    mtrx.print(0);
  }
  mtrx.print(minutes);
  if (sec % 2 > 0) {
    mtrx.dot(15, 1);
    mtrx.dot(15, 5);
  }
  mtrx.update();  // показать
}

bool timeUpdate() {
  bool result = true;
  if (!WiFi.isConnected()) {
    WiFi.reconnect();
  }
  timeClient.begin();
  if (timeClient.update()) {   
    sec = timeClient.getSeconds(); 
    hours = timeClient.getHours();
    minutes = timeClient.getMinutes(); 
    errors = 0; // так как время обновилось - сбрасываем счетчик ошибок
  } else {    
    result = false;
    errors++; // так как время не обновилось - инкременируем счетчик ошибок
  }
  timeClient.end();
  WiFi.disconnect();
  return result;
}
