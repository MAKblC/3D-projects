#include <Wire.h>
#include "FastLED.h"
#include <PCA9634.h>
#include <BH1750.h>

#define I2C_HUB_ADDR 0x70
#define EN_MASK 0x08
#define DEF_CHANNEL 0x00
#define MAX_CHANNEL 0x08

BH1750 lightMeter;
PCA9634 testModule(0x1C);
float lux;
int luix;
bool bob = 0;
static uint32_t color;
static int sld4;
static int inp1 = 200;
static int inp2 = 400;
static bool sw1;
static bool sw2;
static bool sw3;
uint16_t size = 12;

const char* Icon;

#define LED_PIN1 19  // пин
#define LED_NUM1 3   // количество светодиодов
#define LED_PIN2 23  // пин
#define LED_NUM2 3   // количество светодиодов
CRGB led1[LED_NUM1];
CRGB led2[LED_NUM2];

// логин-пароль от роутера
#define AP_SSID "XXXXX"
#define AP_PASS "XXXXXXXXXXX"

#include <Arduino.h>
#include <GyverHub.h>
GyverHub hub;
// GyverHub hub("MyDevices", "ESP", "");  // можно настроить тут, но без F-строк!

unsigned long t_del = 0;

// обработчик кнопки (см. ниже)
void btn_cb() {
  Serial.println("click 4");
}

// обработчик кнопки с информацией о билде (см. ниже)
void btn_cb_b(gh::Build& b) {
  Serial.print("click 5 from client ID: ");
  Serial.println(b.client.id);
}

// билдер
void build(gh::Builder& b) {
  // =================== КОНТЕЙНЕРЫ ===================
  // =================== ШИРИНА ===================
  // ширина виджетов задаётся в "долях" - отношении их ширины друг к другу
  // виджеты займут пропорциональное место во всю ширину контейнера
  // если ширина не указана - она принимается за 1
  if (b.beginRow()) {
    b.Label_(F("jon")).label(" ").value("Умная лампа MG bot");
    b.endRow();
  }

  // =============== ПОДКЛЮЧЕНИЕ ПЕРЕМЕННОЙ ===============

  if (b.beginRow()) {
    Icon = "f011";
    b.SwitchIcon_("sw", &sw2).label("Выкл/Вкл").icon(Icon).size(2).click();
    //if (b.Button().label("icon").click()) hub.update("sw")(rndIcon());
    // библиотека позволяет подключить к активному виджету переменную для чтения и записи
    // я создам статические переменные для ясности. Они могут быть глобальными и так далее
    // таким образом изменения останутся при перезагрузке страницы
    b.endRow();
  }
  if (b.beginRow()) {

    // для подключения нужно передать переменную по адресу
    // библиотека сама определит тип переменной и будет брать из неё значение и записывать при действиях
    // библиотека поддерживает все стандартные типы данных, а также некоторые свои (Pairs, Pos, Button, Log...)
    b.Label_(F("Jon")).label(" ").value("Границы работы автояркости:").fontSize(size).size(2);
    b.Input(&inp1).label("Мин.").size(1);
    b.Input(&inp2).label("Макс.").size(1);
    b.Label_(F("label")).label("Освещённость").size(1).value("__");  // с указанием стандартного значения

    // внутри обработки действия переменная уже будет иметь новое значение:
  b.endRow();
  }

if (b.beginRow()) {
    b.Label_(F("bon")).label(" ").value("Выбор цвта:").fontSize(size).size(2);
    if (b.Switch(&sw1).label("Выкл/вкл").size(2).click()) {
      Serial.print("switch: ");
      Serial.println(sw1);
    }

    b.Color(&color).label("Цвет").size(2);
    b.endRow();
  }

  if (b.beginRow()) {
    b.Label_(F("don")).label(" ").value("Управление яркостью:").fontSize(size).size(2);
    b.Switch(&sw3).label("Вкл/Выкл").size(2).click();
    b.Slider(&sld4).label("Яркость").size(2);  
    b.endRow();
  }

  // =================== ИНФО О БИЛДЕ ===================

  // можно получить информацию о билде и клиенте для своих целей
  // поставь тут 1, чтобы включить вывод =)
  if (0) {
    // запрос информации о виджетах
    if (b.build.isUI()) {
      Serial.println("=== UI BUILD ===");
    }

    // действие с виджетом
    if (b.build.isSet()) {
      Serial.println("=== SET ===");
      Serial.print("name: ");
      Serial.println(b.build.name);
      Serial.print("value: ");
      Serial.println(b.build.value);
    }

    Serial.print("client from: ");
    Serial.println(gh::readConnection(b.build.client.connection()));
    Serial.print("ID: ");
    Serial.println(b.build.client.id);
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  FastLED.addLeds< WS2812, LED_PIN1, GRB>(led1, LED_NUM1);
  FastLED.addLeds< WS2812, LED_PIN2, GRB>(led2, LED_NUM2);
  testModule.begin();
  if (AP_SSID == "xxxxx") {
    bob = 1;
  }
  for (int channel = 0; channel < testModule.channelCount(); channel++) {
    testModule.setLedDriverMode(channel, PCA9634_LEDOFF);  // выключить все светодиоды в режиме 0/1
  }
  setBusChannel(0x07);
  lightMeter.begin();
#ifdef GH_ESP_BUILD
/*
  // подключение к роутеру
  WiFi.mode(WIFI_STA);
  WiFi.begin(AP_SSID, AP_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println(WiFi.localIP());

  // если нужен MQTT - подключаемся
  hub.mqtt.config("test.mosquitto.org", 1883);
  // hub.mqtt.config("test.mosquitto.org", 1883, "login", "pass");
*/
  // ИЛИ

  // режим точки доступа
  WiFi.mode(WIFI_AP);
  WiFi.softAP("SmartLampMGBot");
  Serial.println(WiFi.softAPIP());    // по умолч. 192.168.4.1
#endif

  // указать префикс сети, имя устройства и иконку
  hub.config(F("MyDevices"), F("SamrtLamp"), F(""));

  // подключить билдер
  hub.onBuild(build);

  // запуск!
  hub.begin();
  testModule.write1(0, 0);
  testModule.write1(6, 0);
  testModule.write1(2, 0);
  testModule.write1(5, 0);
  testModule.write1(3, 0);
}

void loop() {

  // =================== ТИКЕР ===================
  // вызываем тикер в главном цикле программы
  // он обеспечивает работу связи, таймаутов и прочего
  hub.tick();

  // =========== ОБНОВЛЕНИЯ ПО ТАЙМЕРУ ===========
  // в библиотеке предусмотрен удобный класс асинхронного таймера
  static gh::Timer tmr(10);  // период 1 секунда

  // каждую секунду будем обновлять заголовок
  if (tmr) {
    hub.update(F("title")).value(millis());
  }
  for (int channel = 0; channel < testModule.channelCount(); channel++) {
    testModule.setLedDriverMode(channel, PCA9634_LEDPWM);  // установка режима ШИМ (0-255)
  }

  if (millis() - t_del > 100) {
    t_del = millis();

    setBusChannel(0x07);
    lux = lightMeter.readLightLevel();  // считывание освещенности
    Serial.print("Освещенность: ");
    Serial.print(lux);
    Serial.println(" Люкс");
  }

  luix = lux;
  hub.update(F("label")).value(luix);
 
  if (sw1 == 0) {
    if (sw2 == 1 or bob == 1) {
      led1[0].setRGB(255, 255, 255);
      led1[1].setRGB(255, 255, 255);
      led1[2].setRGB(255, 255, 255);
      testModule.write1(2, 0);
      testModule.write1(5, 0);
      testModule.write1(3, 0);
      int c = map(lux, inp2, inp1, 0, 255);
      c = constrain(c, 0, 255);
      FastLED.setBrightness(c);
      FastLED.show();
      led2[0].setRGB(255, 255, 255);
      led2[1].setRGB(255, 255, 255);
      led2[2].setRGB(255, 255, 255);
      c = constrain(c, 0, 255);
      FastLED.setBrightness(c);
      FastLED.show();
      int b = map(lux, inp2, inp1, 0, 90);
      b = constrain(b, 0, 90);
      testModule.write1(0, b);
      testModule.write1(6, b);
    }
    if (sw2 == 0) {
      led2[0].setRGB(0, 0, 0);
      led2[1].setRGB(0, 0, 0);
      led2[2].setRGB(0, 0, 0);
      FastLED.setBrightness(0);
      FastLED.show();
      led1[0].setRGB(0, 0, 0);
      led1[1].setRGB(0, 0, 0);
      led1[2].setRGB(0, 0, 0);
      FastLED.setBrightness(0);
      FastLED.show();
      testModule.write1(0, 0);
      testModule.write1(6, 0);
      testModule.write1(3, 0);
      testModule.write1(2, 0);
      testModule.write1(5, 0);
    }
  }
  if (sw1 == 1) {
    testModule.write1(0, 0);
    testModule.write1(6, 0);
    if (sw2 == 1) {
      int lit = sld4;
      int a = map(lit, 100, 0, 90, 0);
      int e = map(lit, 100, 0, 255, 0);

      e = constrain(e, 0, 255);
      int c = map(lux, inp2, inp1, 0, 255);
      c = constrain(c, 0, 255);

      int red = color >> 16;

      int green = (color & 0x00ff00) >> 8;

      int blue = (color & 0x0000ff);

      if (sw3 == 1) {
        int rd = red * sld4 / 100;
        int gn = green * sld4 / 100;
        int be = blue * sld4 / 100;
        testModule.write1(3, rd);
        testModule.write1(2, gn);
        testModule.write1(5, be);
      }
      if (sw3 == 0) {
        int bq1 = map(lux, inp2, inp1, 0, 100);
        bq1 = constrain(bq1, 0, 100);
        int bq = bq1;
        int re = red * bq / 100;
        int gr = green * bq / 100;
        int bl = blue * bq / 100;
        testModule.write1(3, re);
        testModule.write1(2, gr);
        testModule.write1(5, bl);
      }

      if (sw3 == 0) {
        led1[0] = color;
        led1[1] = color;
        led1[2] = color;
        FastLED.setBrightness(c);
        FastLED.show();
      }
      if (sw3 == 1) {
        led1[0] = color;
        led1[1] = color;
        led1[2] = color;
        FastLED.setBrightness(e);
        FastLED.show();
      }

      if (sw3 == 0) {
        led2[0] = color;
        led2[1] = color;
        led2[2] = color;
        FastLED.setBrightness(c);
        FastLED.show();
      }
      if (sw3 == 1) {
        led2[0] = color;
        led2[1] = color;
        led2[2] = color;
        FastLED.setBrightness(e);
        FastLED.show();
      }
    }
    if (sw2 == 0) {
      led2[0].setRGB(0, 0, 0);
      led2[1].setRGB(0, 0, 0);
      led2[2].setRGB(0, 0, 0);
      FastLED.setBrightness(0);
      FastLED.show();
      led1[0].setRGB(0, 0, 0);
      led1[1].setRGB(0, 0, 0);
      led1[2].setRGB(0, 0, 0);
      FastLED.setBrightness(0);
      FastLED.show();
      testModule.write1(0, 0);
      testModule.write1(6, 0);
      testModule.write1(3, 0);
      testModule.write1(2, 0);
      testModule.write1(5, 0);
    }
  }
}

// Функция установки нужного выхода I2C
bool setBusChannel(uint8_t i2c_channel) {
  if (i2c_channel >= MAX_CHANNEL) {
    return false;
  } else {
    Wire.beginTransmission(I2C_HUB_ADDR);
    Wire.write(0x01 << i2c_channel);
    // Wire.write(i2c_channel | EN_MASK); // для микросхемы PCA9547
    Wire.endTransmission();
    return true;
  }
}
