
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "FastLED.h"
#include <WiFi.h>
#include <Wire.h>
#include <BH1750.h>
BH1750 lightMeter;

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme280;

#include <PCA9634.h>
PCA9634 testModule(0x1C);

#define WIFI_SSID "XXXXX"              // название вашего Wi-Fi
#define WIFI_PASSWORD "XXXXXXXXXX"  // пароль вашего Wi-Fi

#define BOT_TOKEN "XXXX:XXXXXXXXXXX"  // Сюда введите ваш бот токен для Telegram

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;

TaskHandle_t Task1;  // подпрограмма для первого ядра
TaskHandle_t Task2;  // подпрограмма для второго ядра

#define I2C_HUB_ADDR 0x70
#define EN_MASK 0x08
#define DEF_CHANNEL 0x00
#define MAX_CHANNEL 0x08

/*
  I2C порт 0x07 - выводы GP16 (SDA), GP17 (SCL)
  I2C порт 0x06 - выводы GP4 (SDA), GP13 (SCL)
  I2C порт 0x05 - выводы GP14 (SDA), GP15 (SCL)
  I2C порт 0x04 - выводы GP5 (SDA), GP23 (SCL)
  I2C порт 0x03 - выводы GP18 (SDA), GP19 (SCL)
*/

#define PUMP 19

#define SOIL_MOISTURE 34     // A6
#define SOIL_TEMPERATURE 35  // A7
// откалиброванные значения АЦП
const float air_value = 1587.0;
const float water_value = 800.0;
const float moisture_0 = 0.0;
const float moisture_100 = 100.0;

const float k = 6.27;  // коэффициент поправки напряжения АЦП ESP32 для ~4.45В

float t;
float h;
float p;
float lux;
float adc0;
float adc1;
float ts;
float hs;
bool flag_auto = true;
bool flag_svet = false;

void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/auto") {
      flag_auto = !flag_auto;
      bot.sendMessage(chat_id, "Автополив переключен на " + String(flag_auto), "");
    }

    if (text == "/polivOn") {
      digitalWrite(PUMP, 1);
      bot.sendMessage(chat_id, "Полив включен", "");
    }

    if (text == "/polivOff") {
      digitalWrite(PUMP, 0);
      bot.sendMessage(chat_id, "Полив выключен", "");
    }

    if (text == "/svet") {
      flag_svet = !flag_svet;
      setBusChannel(0x04);
      if (flag_svet) {
        testModule.write1(3, 0xFF);
        testModule.write1(2, 0xFF);
        testModule.write1(5, 0xFF);
      } else {
        testModule.write1(3, 0x00);
        testModule.write1(2, 0x00);
        testModule.write1(5, 0x00);
      }
      bot.sendMessage(chat_id, " Свет переключен в режим " + String(flag_auto), "");
    }

    if (text == "/datchiki") {
      String welcome = "Показания датчиков:\n-------------------------------------------\n";
      welcome += "🌡 Температура воздуха: " + String(t, 1) + " °C\n";
      welcome += "💧 Влажность воздуха: " + String(h, 0) + " %\n";
      welcome += "☁ Атмосферное давление: " + String(p, 0) + " мм рт.ст.\n";
      welcome += "☀ Освещенность: " + String(lux) + " Лк\n\n";
      welcome += "🌱 Температура почвы: " + String(ts, 0) + " °C\n";
      welcome += "🌱 Влажность почвы: " + String(hs, 0) + " %\n\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }

    if (text == "/start") {
      String welcome = from_name + ", привет - это умный цветник ЙоТик32!" + ".\n";
      welcome += "Это тестовый код для управления цветником.\n\n";
      welcome += "/polivOn : Включить полив\n";
      welcome += "/polivOff : Выключить полив\n";
      welcome += "/svet : Выключить/выключить освещение\n";
      welcome += "/auto : Выключить/выключить авто полив\n";
      welcome += "/datchiki : Вернёт данные с датчиков \n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  pinMode(PUMP, OUTPUT);

  Wire.begin();
  setBusChannel(0x05);
  lightMeter.begin();
  setBusChannel(0x04);
  testModule.begin();
  for (int channel = 0; channel < testModule.channelCount(); channel++) {
    testModule.setLedDriverMode(channel, PCA9634_LEDOFF);  // выключить все светодиоды в режиме 0/1
  }
  for (int channel = 0; channel < testModule.channelCount(); channel++) {
    testModule.setLedDriverMode(channel, PCA9634_LEDPWM);  // установка режима ШИМ (0-255)
  }
  bool bme_status = bme280.begin();
  if (!bme_status) {
    Serial.println("Не найден по адресу 0х77, пробую другой...");
    bme_status = bme280.begin(0x76);
    if (!bme_status)
      Serial.println("Датчик не найден, проверьте соединение");
  }

  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  if (WIFI_SSID != "логин")
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
    }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org");
  time_t now = time(nullptr);
  if (WIFI_SSID != "логин")
    while (now < 24 * 3600) {
      Serial.print(".");
      delay(100);
      now = time(nullptr);
    }
  Serial.println(now);

  //создаем задачу, которая будет выполняться на ядре 0 с максимальным приоритетом (1)
  xTaskCreatePinnedToCore(
    Task1code, /* Функция задачи. */
    "Task1",   /* Ее имя. */
    10000,     /* Размер стека функции */
    NULL,      /* Параметры */
    1,         /* Приоритет */
    &Task1,    /* Дескриптор задачи для отслеживания */
    1);        /* Указываем пин для данного ядра */
  delay(500);

  //создаем задачу, которая будет выполняться на ядре 0 с максимальным приоритетом (1)
  xTaskCreatePinnedToCore(
    Task2code, /* Функция задачи. */
    "Task2",   /* Ее имя. */
    10000,     /* Размер стека функции */
    NULL,      /* Параметры */
    1,         /* Приоритет */
    &Task2,    /* Дескриптор задачи для отслеживания */
    1);        /* Указываем пин для данного ядра */
  delay(500);
}

void Task1code(void* pvParameters) {
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {

    if (millis() - bot_lasttime > 1000) {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

      while (numNewMessages) {
        Serial.println("got response");
        handleNewMessages(numNewMessages);
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      }

      bot_lasttime = millis();
    }
  }
}

//Task2code:
void Task2code(void* pvParameters) {
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    setBusChannel(0x05);
    lux = lightMeter.readLightLevel();  // считывание освещенности
    Serial.print("Освещенность: ");
    Serial.print(lux);
    Serial.println(" Люкс");
    delay(1000);

    if (flag_auto) {
      int power = 255 - lux;
      setBusChannel(0x04);
      if (power < 0) {
        testModule.write1(3, 0x00);
        testModule.write1(2, 0x00);
        testModule.write1(5, 0x00);
      } else {
        if (power > 255) {
          testModule.write1(3, 0xFF);
          testModule.write1(2, 0xFF);
          testModule.write1(5, 0xFF);
        } else {
          testModule.write1(3, power);
          testModule.write1(2, power);
          testModule.write1(5, power);
        }
      }

      if (hs < 20) {
        digitalWrite(PUMP, 1);
      } else {
        if (hs > 80) digitalWrite(PUMP, 0);
      }
    }


    // Измерение
    t = bme280.readTemperature();
    h = bme280.readHumidity();
    p = bme280.readPressure() / 100.0F;
    // Вывод измеренных значений в терминал
    Serial.println("Air temperature = " + String(t, 1) + " *C");
    Serial.println("Air humidity = " + String(h, 1) + " %");
    Serial.println("Air pressure = " + String(p, 1) + " hPa");  // 1 mmHg = 1 hPa / 1.33

    adc0 = analogRead(SOIL_MOISTURE);
    adc1 = analogRead(SOIL_TEMPERATURE);
    ts = ((adc1 / 4095.0 * k) - 0.5) * 100.0;  // АЦП разрядность (12) = 4095
    hs = map(adc0, air_value, water_value, moisture_0, moisture_100);
    // Вывод измеренных значений в терминал
    Serial.println("Soil humidity = " + String(hs, 1) + " %");
    Serial.println("Soil temperature = " + String(ts, 1) + " *C");
  }
}

void loop() {
}

bool setBusChannel(uint8_t i2c_channel) {
  if (i2c_channel >= MAX_CHANNEL) {
    return false;
  } else {
    Wire.beginTransmission(I2C_HUB_ADDR);
    //Wire.write(i2c_channel | EN_MASK); // для микросхемы PCA9547
    Wire.write(0x01 << i2c_channel);  // Для микросхемы PW548A
    Wire.endTransmission();
    return true;
  }
}
