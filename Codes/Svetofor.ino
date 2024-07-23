
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Логин и пароль от WiFi
#define WIFI_SSID "XXXXXX"
#define WIFI_PASSWORD "XXXXXXX"
// Токен
#define BOT_TOKEN "XXXXX:XXXXXXXXXXXXXXX"

const unsigned long BOT_MTBS = 1000;  // время обновления сообщения

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  

#include <FastLED.h>

#include <Wire.h>
#include <Adafruit_MCP4725.h>
Adafruit_MCP4725 buzzer;

#include <Wire.h>
#include <VL53L0X.h>
VL53L0X lox;

// Режим работы датчика
//#define LONG_RANGE
#define HIGH_SPEED
//#define HIGH_ACCURACY

#define LED_PIN1 15
#define LED_PIN2 14
#define LED_PIN3 13
#define LED_PIN4 4

#define NUM_LEDS 3
#define BRIGHTNESS 250
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];
CRGB leds3[NUM_LEDS];
CRGB leds4[NUM_LEDS];

// Константы для I2C шилда
#define I2C_HUB_ADDR 0x70
#define EN_MASK 0x08
#define DEF_CHANNEL 0x00
#define MAX_CHANNEL 0x08

TaskHandle_t Task1;
TaskHandle_t Task2;

int vol1 = 3000;
int vol2 = 100;  // разница значений = громкость
int ton;

int wait = 5;
int waitGrean = 5;

bool flagAuto = 1;
byte colorHand;

void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/AutoOn") {
      flagAuto = 1;
      bot.sendMessage(chat_id, "Автоматический режим ВКЛ", "");
    }
    if (text == "/AutoOff") {
      flagAuto = 0;
      bot.sendMessage(chat_id, "Автоматический режим ВЫКЛ", "");
    }
    if (text == "/Color") {
      String keyboardJson = "[[\"Red\", \"Yellow\", \"Green\", \"off\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, "Выберите цвет", "", keyboardJson, true);
    }
    if (text == "off") {
      if(flagAuto != 0) {bot.sendMessage(chat_id, "Сначала выключите авто режим", ""); return;}
      colorHand = 0;
      bot.sendMessage(chat_id, "ВЫКЛ все функции", "");
    }
    if (text == "Red") {
      if(flagAuto != 0) {bot.sendMessage(chat_id, "Сначала выключите авто режим", ""); return;}
      colorHand = 1;
      bot.sendMessage(chat_id, "ВКЛ красный", "");
    }
    if (text == "Yellow") {
      if(flagAuto != 0) {bot.sendMessage(chat_id, "Сначала выключите авто режим", ""); return;}
      colorHand = 2;
      bot.sendMessage(chat_id, "ВКЛ желтый", "");
    }
    if (text == "Green") {
      if(flagAuto != 0) {bot.sendMessage(chat_id, "Сначала выключите авто режим", ""); return;}
      colorHand = 3;
      bot.sendMessage(chat_id, "ВКЛ зеленый", "");
    }

    if (text == "/start") {
      String welcome = "Привет, я контроллер йотик32, этоп роект умный светофор, " + from_name + ".\n";
      welcome += "Тут ты можешь управлять некоторыми аспектами светофора\n\n";
      welcome += "/AutoOn : Переключать ручного управления на Вкл\n";
      welcome += "/AutoOff : Переключать ручного управления на Выкл\n";
      welcome += "/Color : Включить светофор цветом\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}

void setup() {
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  delay(1000);

  Serial.begin(115200);
  // Инициализация датчика
  Wire.begin();
  lox.init();
  lox.setTimeout(500);
#if defined LONG_RANGE
  // lower the return signal rate limit (default is 0.25 MCPS)
  lox.setSignalRateLimit(0.1);
  // increase laser pulse periods (defaults are 14 and 10 PCLKs)
  lox.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  lox.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
#endif
#if defined HIGH_SPEED
  // reduce timing budget to 20 ms (default is about 33 ms)
  lox.setMeasurementTimingBudget(20000);
#elif defined HIGH_ACCURACY
  // increase timing budget to 200 ms
  lox.setMeasurementTimingBudget(200000);
#endif

  setBusChannel(0x07);
  buzzer.begin(0x60);           // Без перемычки адрес будет 0x61
  buzzer.setVoltage(0, false);  // выключение звука

  FastLED.addLeds<LED_TYPE, LED_PIN1, COLOR_ORDER>(leds1, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, LED_PIN2, COLOR_ORDER>(leds2, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, LED_PIN3, COLOR_ORDER>(leds3, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, LED_PIN4, COLOR_ORDER>(leds4, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

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

void loop() {
}


void Task1code(void* pvParameters) {
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    if (flagAuto == true) {
      float dist = lox.readRangeSingleMillimeters();
      // Вывод измеренных значений в терминал
      Serial.println("Distance  = " + String(dist, 0) + " mm  ");
      delay(100);

      if (dist < 150) {
        setBusChannel(0x07);
        buzzer.setVoltage(0, false);  // выключение звука
        note(3, 450);
        note(5, 150);
        note(6, 450);  // пример нескольких нот
        delay(500);

        for (int i = 0; i < 3; i++) {
          leds1[i] = CRGB(255, 0, 0);
          leds2[i] = CRGB(255, 0, 0);
          leds3[i] = CRGB(255, 0, 0);
          leds4[i] = CRGB(255, 0, 0);
          FastLED.show();
          delay(wait * 1000 / 9);
        }
        FastLED.clear();
        FastLED.show();

        for (int i = 0; i < 3; i++) {
          leds1[i] = CRGB(255, 255, 0);
          leds2[i] = CRGB(255, 255, 0);
          leds3[i] = CRGB(255, 255, 0);
          leds4[i] = CRGB(255, 255, 0);
          FastLED.show();
          delay(wait * 1000 / 9);
        }
        FastLED.clear();
        FastLED.show();

        setBusChannel(0x07);
        buzzer.setVoltage(0, false);  // выключение звука
        note(6, 450);                 // пример нескольких нот
        note(5, 150);
        note(3, 450);
        delay(500);

        for (int i = 0; i < 3; i++) {
          leds1[i] = CRGB(0, 255, 0);
          leds2[i] = CRGB(0, 255, 0);
          leds3[i] = CRGB(0, 255, 0);
          leds4[i] = CRGB(0, 255, 0);
          FastLED.show();
          delay(wait * 1000 / 9);
        }
        delay(waitGrean);
        FastLED.clear();
        FastLED.show();

      } else {
        for (int i = 0; i < 3; i++) {
          leds1[i] = CRGB(255, 200, 0);
          leds2[i] = CRGB(255, 200, 0);
          leds3[i] = CRGB(255, 200, 0);
          leds4[i] = CRGB(255, 200, 0);
          delay(500);
          FastLED.show();
        }

        delay(1000);
        FastLED.clear();
        FastLED.show();
        delay(500);
      }
    } else {  //если не в автоматичском режиме

      if (colorHand == 0)
        for (int i = 0; i < 3; i++) {
          leds1[i] = CRGB(0, 0, 0);
          leds2[i] = CRGB(0, 0, 0);
          leds3[i] = CRGB(0, 0, 0);
          leds4[i] = CRGB(0, 0, 0);
          delay(500);
          FastLED.show();
        }

      if (colorHand == 1)
        for (int i = 0; i < 3; i++) {
          leds1[i] = CRGB(255, 0, 0);
          leds2[i] = CRGB(255, 0, 0);
          leds3[i] = CRGB(255, 0, 0);
          leds4[i] = CRGB(255, 0, 0);
          delay(500);
          FastLED.show();
        }

      if (colorHand == 2)
        for (int i = 0; i < 3; i++) {
          leds1[i] = CRGB(255, 255, 0);
          leds2[i] = CRGB(255, 255, 0);
          leds3[i] = CRGB(255, 255, 0);
          leds4[i] = CRGB(255, 255, 0);
          delay(500);
          FastLED.show();
        }

      if (colorHand == 3)
        for (int i = 0; i < 3; i++) {
          leds1[i] = CRGB(0, 255, 0);
          leds2[i] = CRGB(0, 255, 0);
          leds3[i] = CRGB(0, 255, 0);
          leds4[i] = CRGB(0, 255, 0);
          delay(500);
          FastLED.show();
        }

      delay(wait*1000);
      FastLED.clear();
      FastLED.show();
      delay(500);
    }
  }
}

//Task2code: мигает светодиодом раз в 0,7 секунды
void Task2code(void* pvParameters) {
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    if (millis() - bot_lasttime > BOT_MTBS) {
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

void note(int type, int duration) {  // нота (нота, длительность)
  switch (type) {
    case 1: ton = 1000; break;
    case 2: ton = 860; break;
    case 3: ton = 800; break;
    case 4: ton = 700; break;
    case 5: ton = 600; break;
    case 6: ton = 525; break;
    case 7: ton = 450; break;
    case 8: ton = 380; break;
    case 9: ton = 315; break;
    case 10: ton = 250; break;
    case 11: ton = 190; break;
    case 12: ton = 130; break;
    case 13: ton = 80; break;
    case 14: ton = 30; break;
    case 15: ton = 1; break;
  }
  delay(10);  // воспроизведение звука с определенной тональностью и длительностью
  for (int i = 0; i < duration; i++) {
    setBusChannel(0x07);
    buzzer.setVoltage(vol1, false);
    buzzer.setVoltage(vol2, false);
    delayMicroseconds(ton);
  }
}

// Функция установки нужного выхода I2C
bool setBusChannel(uint8_t i2c_channel) {
  if (i2c_channel >= MAX_CHANNEL) {
    return false;
  } else {
    Wire.beginTransmission(I2C_HUB_ADDR);
    // для микросхемы PCA9547
    // Wire.write(i2c_channel | EN_MASK);  
    Wire.write(0x01 << i2c_channel); // Для микросхемы PW548A
    Wire.endTransmission();
    return true;
  }
}
