
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "FastLED.h"  // подключаем библиотеку фастлед
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <VL53L0X.h>
VL53L0X lox;

#define WIFI_SSID "логин" //название вашего Wi-Fi
#define WIFI_PASSWORD "пароль" //пароль вашего Wi-Fi

#define BOT_TOKEN "XXXXXX:XXXXXXXXXXXXXXXXXXXXX" // Сюда введите ваш бот токен для Telegram

// Режим работы датчика
//#define LONG_RANGE
#define HIGH_SPEED
//#define HIGH_ACCURACY

const unsigned long BOT_MTBS = 1000;  // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done

TaskHandle_t Task1;
TaskHandle_t Task2;

const int ledPin = 16;
int ledStatus = 0;
byte bright = 250;  // яркость LED светодиодов
byte baza = 0;      // изменение оттенка LED
#define NUM_LEDS 31
#define PIN 4
CRGB leds[NUM_LEDS];

const char* mqtt_server = "xxxx";  //ip или http адрес
int mqtt_port = 1234;                   //порт
const char* mqtt_login = "xxxxxxx";     //логин
const char* mqtt_pass = "xxxxxx";      //пароль

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;



void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    ledStatus = true;
    digitalWrite(ledPin, ledStatus);
  } else {
    ledStatus = false;
    digitalWrite(ledPin, ledStatus);
  }
}

void reconnect() {
  if(mqtt_server != "xxxx")
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_login, mqtt_pass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("fontanPower");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/fontanOn") {
      digitalWrite(ledPin, 1);  // turn the LED on (HIGH is the voltage level)
      ledStatus = 1;
      bot.sendMessage(chat_id, "Fontan is ON", "");
    }

    if (text == "/fontanOff") {
      ledStatus = 0;
      digitalWrite(ledPin, 0);  // turn the LED off (LOW is the voltage level)
      bot.sendMessage(chat_id, "Fontan is OFF", "");
    }

    if (text == "/status") {
      if (ledStatus) {
        bot.sendMessage(chat_id, "Fontan is ON", "");
      } else {
        bot.sendMessage(chat_id, "Fontan is OFF", "");
      }
    }

    if (text == "/start") {
      String welcome = from_name + ", привет - это фонтан ЙоТик32!" + ".\n";
      welcome += "Это тестовый код для управления фонтаном.\n\n";
      welcome += "/fontanOn : Включить фонтан\n";
      welcome += "/fontanOff : Выключить фонтан\n";
      welcome += "/status : Вернёт статус \n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}


void setup() {
  FastLED.addLeds<WS2812, PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(bright);

  Serial.begin(115200);
  Serial.println();

  pinMode(ledPin, OUTPUT);  // initialize digital ledPin as an output.
  delay(10);
  digitalWrite(ledPin, LOW);  // initialize pin as off (active LOW)

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

  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);  // Add root certificate for api.telegram.org
  if(WIFI_SSID != "логин")
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org");  // get UTC time via NTP
  time_t now = time(nullptr);
if(WIFI_SSID != "логин") 
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

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void Task1code(void* pvParameters) {
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    float dist = lox.readRangeSingleMillimeters();

    if (millis() - bot_lasttime > BOT_MTBS) {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

      while (numNewMessages) {
        Serial.println("got response");
        handleNewMessages(numNewMessages);
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      }

      bot_lasttime = millis();
    }
    
    if (dist < 150 && dist != 0) {
      ledStatus = !ledStatus;
      digitalWrite(ledPin, ledStatus);
      delay(1000);
    }
    
    Serial.println("Distance  = " + String(dist, 0) + " mm  ");
  }
}

//Task2code: мигает светодиодом раз в 0,7 секунды
void Task2code(void* pvParameters) {
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();

    if (ledStatus) {
      fadeToBlackBy(leds, NUM_LEDS, 2);
      int pos = beatsin16(13, 0, NUM_LEDS - 1);
      leds[pos] += CHSV(baza++, 255, 192);
      FastLED.setBrightness(bright);
      FastLED.show();
    } else {
      fadeToBlackBy(leds, NUM_LEDS, 2);
      int pos = beatsin16(13, 0, NUM_LEDS - 1);
      leds[pos] += CRGB(0, 0, 0);
      FastLED.setBrightness(bright);
      FastLED.show();
    }
  }
}

void loop() {
}
