
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>


#define WIFI_SSID "XXXXXX"
#define WIFI_PASSWORD "XXXXXXXXXXXXX"

#define BOT_TOKEN "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

const unsigned long BOT_MTBS = 1000;  

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  

#include <Wire.h>

#include <Wire.h>
#include <VL53L0X.h>
VL53L0X lox;

#include <ESP32_Servo.h>
Servo servoD;
Servo servoC;

// Режим работы датчика
//#define LONG_RANGE
#define HIGH_SPEED
//#define HIGH_ACCURACY

TaskHandle_t Task1;
TaskHandle_t Task2;

bool flagS = false;
String chatid = "";

void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;

    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/SeveMe") {
      chatid = chat_id;
      bot.sendMessage(chat_id, "Теперь, вам будут приходить уведомления о тревоге!", "");
    }
    if (text == "/SecurityOn") {
      flagS = 1;
      bot.sendMessage(chat_id, "Режим охраны ВКЛ", "");
    }
    if (text == "/SecurityOff") {
      flagS = 0;
      bot.sendMessage(chat_id, "Режим охраны ВЫКЛ", "");
    }
    if (text == "/ControlPanel") {
      String keyboardJson = "[[\"DoorOpen\", \"DoorClose\", \"Status\"], [\"/SecurityOn\", \"/SecurityOff\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, "Выберите команду", "", keyboardJson, true);
    }
    if (text == "DoorOpen") {
      if (flagS == false) {
        bot.sendMessage(chat_id, "Открываю дверь...", "");
        openDoor();
      } else {
        bot.sendMessage(chat_id, "Для начала снимите сейф с сигнализации!", "");
      }
      if (servoD.read() < 3) bot.sendMessage(chat_id, "Дверь открыта!", "");
    }
    if (text == "DoorClose") {
      if (servoC.read() < 80) {
        bot.sendMessage(chat_id, "Закрываю дверь...", "");
        closeDoor();
      }
      bot.sendMessage(chat_id, "Дверь закрыта!", "");
    }
    if (text == "Status") {
      if (flagS) {
        bot.sendMessage(chat_id, "Сейф в режиме охраны!", "");
      } else {
        bot.sendMessage(chat_id, "Режим охраны деактивирован!", "");
      }
      if (servoD.read() < 3) {
        bot.sendMessage(chat_id, "Дверь открыта!", "");
      } else {
        bot.sendMessage(chat_id, "Дверь закрыта!", "");
      }
    }

    if (text == "/start") {
      String welcome = "Привет, " + from_name + ", я контроллер йотик32, этоп роект умный сейф.\n";
      welcome += "Тут ты можешь управлять сейфом\n\n";
      welcome += "/SecurityOn : Переключает сигнализацию на Вкл\n";
      welcome += "/SecurityOff : Переключать сигнализацию на Выкл\n";
      welcome += "/ControlPanel : Вызывает панель управления\n";
      welcome += "/SeveMe : Сейф запомнит вас как владельца\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}

int angleDoorClose = 100;
int angleCloserClose = 90;

void setup() {
  Serial.begin(115200);
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);  // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  delay(1000);
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

  servoD.attach(23);  // пин сервомотора двери
  servoC.attach(19);  // пин сервомотора замка

  servoD.write(angleDoorClose);
  delay(1500);
  servoC.write(0);
  delay(1000);

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
    if (flagS) {
      digitalWrite(18, 1);
      delay(250);
      digitalWrite(18, 0);
      delay(250);
    } else {
      digitalWrite(18, 1);
    }
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

      if (flagS) {
        float dist = lox.readRangeSingleMillimeters();
        Serial.println("Distance  = " + String(dist, 0) + " mm  ");
        if (dist > 200) {
          if (chatid != "") {
            Serial.println("Сейф взломали!");
            bot.sendMessage(chatid, "Сейф взломали!", "");
          }
        }
      }
    }
  }
}

void closeDoor() {

  servoC.write(0);
  delay(1000);

  for (int i = 0; i < angleDoorClose; i++) {
    servoD.write(i);
    delay(15);
  }

  delay(1000);

  for (int i = 0; i < angleCloserClose; i++) {
    servoC.write(i);
    delay(15);
  }
}

void openDoor() {
  servoC.write(0);
  delay(1000);

  for (int i = angleDoorClose; i > 0; i--) {
    servoD.write(i);
    delay(15);
  }
}
