#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#define WIFI_SSID "XXXXXX"
#define WIFI_PASSWORD "XXXXXXXXXXXX"
#define BOT_TOKEN "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

const unsigned long BOT_MTBS = 1000;  
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  

#include <FastLED.h> 
#define NUM_LEDS 30     
CRGB leds[NUM_LEDS];     
#define LED_PIN 18       
#define COLOR_ORDER GRB 
#define CHIPSET WS2812  

#include <Wire.h>

#include "SparkFun_SGP30_Arduino_Library.h"
SGP30 mySensor;

#include <MGS_FR403.h>
MGS_FR403 Fire;

TaskHandle_t Task1;
TaskHandle_t Task2;

bool flagS = false;
bool led = true;
bool stop = false;
String chatid = "";
int co2, tvoc;
float fIR = 0;

// Константы для I2C шилда
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

void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;

    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/LEDOnOff") {
      led = !led;
      bot.sendMessage(chat_id, "Светодиоды в режиме: " + String(led), "");
    }

    if (text == "/SaveMe") {
      chatid = chat_id;
      bot.sendMessage(chat_id, "Теперь, вам будут приходить уведомления о тревоге!", "");
    }

    if (text == "/Stop" || text == "/stop" ) {
      stop = true;
      bot.sendMessage(chat_id, "Тревога принудительно отключена.", "");
    }

    if (text == "/reset"){
      bot.sendMessage(chat_id, "Перезагрузка", "");
      ESP.restart();
    }

    if (text == "/start") {
      stop = false;
      String welcome = "Привет, " + from_name + ", я контроллер йотик32, этоп роект умная сигнализация.\n";
      welcome += "Тут ты можешь управлять сигнализацией\n\n";
      welcome += "/LEDOnOff : Переключает сигнализацию на Вкл.Выкл\n";
      welcome += "/SaveMe : Сигнализация запомнит вас как владельца\n";
      welcome += "Сигнализация в режиме охраны\n";
      welcome += "/Stop : принудительно отключить оповещения\n";
      welcome += "/reset : перезагрузка контроллера\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}


void setup() {
  Serial.begin(115200);

  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);  
  FastLED.setBrightness(90);                           

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
  Wire.begin();
  setBusChannel(0x07);
  if (mySensor.begin() == false) {
    Serial.println("No SGP30 Detected. Check connections.");
  }
  mySensor.initAirQuality();

  Fire.begin();

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
    0);        /* Указываем пин для данного ядра */
  delay(500);
}

void loop() {
}

void Task1code(void* pvParameters) {
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    setBusChannel(0x07);
    mySensor.measureAirQuality();
    Serial.print("CO2: ");
    co2 = mySensor.CO2;
    Serial.print(co2);
    Serial.print(" ppm\tTVOC: ");
    tvoc = mySensor.TVOC;
    Serial.print(tvoc);
    Serial.println(" ppb");

    Fire.get_ir_and_vis();
    fIR = Fire.ir_data;
    Serial.print("ИК: ");
    Serial.println(String(Fire.ir_data, 1));
    Serial.print("Видимый: ");
    Serial.println(String(Fire.vis_data, 1));

    if (!led) {
      fill_solid(leds, NUM_LEDS, CHSV(0, 0, 0));
      FastLED.show();
    }

    if (fIR > 500) {
      flagS = true;
      if (led) {
        fill_solid(leds, NUM_LEDS, CHSV(0, 255, 255));  // заполнить всю матрицу выбранным цветом
        FastLED.show();
        delay(500);
        fill_solid(leds, NUM_LEDS, CHSV(0, 0, 0));  // заполнить всю матрицу выбранным цветом
        FastLED.show();
        delay(500);
      }

    } else {
      flagS = false;
      if (led) {
        static int ton;
        ton = map(co2, 400, 600, 100, 0);
        ton = constrain(ton, 0, 100);
        fill_solid(leds, NUM_LEDS, CHSV(ton, 255, 255));  // заполнить всю матрицу выбранным цветом
        FastLED.show();
      }
    }

    Serial.println(flagS);

    delay(500);
  }
}

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
    
    if (fIR > 500 && !stop) bot.sendMessage(chatid, "Тревога, пожар!", "");
  }
}

// Функция установки нужного выхода I2C
bool setBusChannel(uint8_t i2c_channel) {
  if (i2c_channel >= MAX_CHANNEL) {
    return false;
  } else {
    Wire.beginTransmission(I2C_HUB_ADDR);
    Wire.write(i2c_channel | EN_MASK);  // для микросхемы PCA9547
                                        // Wire.write(0x01 << i2c_channel); // Для микросхемы PW548A
    Wire.endTransmission();
    return true;
  }
}
