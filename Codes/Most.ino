#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#define WIFI_SSID "XXXX"
#define WIFI_PASSWORD "XXXXX"
#define BOT_TOKEN "XXXXXX"

const unsigned long BOT_MTBS = 1000;  

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  

#include <FastLED.h>
#include <VL53L0X.h>
VL53L0X lox;
#define HIGH_SPEED

#include <Wire.h>

#include <ESP32_Servo.h>
Servo servoL;
Servo servoR;

TaskHandle_t Task1;
TaskHandle_t Task2;

bool flagS = false;
String chatid = "";

#include <FastLED.h>  
#define NUM_LEDS 3       
CRGB leds1[NUM_LEDS];   
CRGB leds2[NUM_LEDS];    
#define LED_PIN1 5      
#define LED_PIN2 15      
#define COLOR_ORDER GRB  
#define CHIPSET WS2812   

void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;

    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/ControlPanel") {
      String keyboardJson = "[[\"MostOpen\", \"MostClose\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, "Выберите команду", "", keyboardJson, true);
    }

    if (text == "MostOpen") {
      openMost();
      bot.sendMessage(chat_id, "Мост открыт!", "Markdown");
    }
    if (text == "MostClose") {
      closeMost();
      bot.sendMessage(chat_id, "Мост закрыт!", "Markdown");
    }

    if (text == "/start") {
      String welcome = "Привет, " + from_name + ", я контроллер йотик32, это проект умный мост.\n";
      welcome += "Тут ты можешь управлять мостом\n\n";
      welcome += "/ControlPanel : Вызывает панель управления\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}

int angleMostOpen = 70;
int angleMostClose = 0;

void setup() {
  Serial.begin(115200);
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

  FastLED.addLeds<NEOPIXEL, LED_PIN1>(leds1, NUM_LEDS);  
  FastLED.addLeds<NEOPIXEL, LED_PIN2>(leds2, NUM_LEDS);  
  FastLED.setBrightness(90);                            
  FastLED.show();

  delay(1000);
  Wire.begin();
  lox.init();
  lox.setTimeout(500);
#if defined LONG_RANGE
  lox.setSignalRateLimit(0.1);
  lox.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  lox.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
#endif
#if defined HIGH_SPEED
  lox.setMeasurementTimingBudget(20000);
#elif defined HIGH_ACCURACY
  lox.setMeasurementTimingBudget(200000);
#endif

  servoL.attach(23);  // пин сервомотора двери
  servoR.attach(19);  // пин сервомотора замка

  servoL.write(0);
  servoR.write(70);
  delay(2000);
  servoL.write(70);
  delay(1000);
  servoR.write(0);
  fill_solid(leds1, NUM_LEDS, CRGB(0, 255, 0));  
  fill_solid(leds2, NUM_LEDS, CRGB(0, 255, 0));  
  FastLED.show();

  pinMode(18, OUTPUT);
  digitalWrite(18, 0);

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

      float dist = lox.readRangeSingleMillimeters();
      Serial.println("Distance  = " + String(dist, 0) + " mm  ");
      if (dist < 100 && servoL.read() > 65) {
        closeMost();
        delay(2000);
        openMost();
      }
    }
  }
}

void openMost() {
  fill_solid(leds1, NUM_LEDS, CRGB(255, 0, 0));  // заполнить всю матрицу выбранным цветом
  fill_solid(leds2, NUM_LEDS, CRGB(255, 0, 0));  // заполнить всю матрицу выбранным цветом
  FastLED.show();
  for (int i = servoL.read(); i > 0; i--) {
    servoL.write(i);
    servoR.write(70 - servoL.read());
    delay(15);
  }
}

void closeMost() {  
  fill_solid(leds1, NUM_LEDS, CRGB(0, 255, 0));  // заполнить всю матрицу выбранным цветом
  fill_solid(leds2, NUM_LEDS, CRGB(0, 255, 0));  // заполнить всю матрицу выбранным цветом
  FastLED.show();
  for (int i = servoL.read(); i < angleMostOpen; i++) {
    servoL.write(i);
    servoR.write(70 - i);
    delay(15);
  }
}
