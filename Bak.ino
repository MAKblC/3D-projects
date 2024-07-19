
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#define WIFI_SSID "XXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXXXXXX"
#define BOT_TOKEN "XXXXXXXXXXXXXXXXXXXXXXX"

const unsigned long BOT_MTBS = 1000;  

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  

#include <Wire.h>
#include <VL53L0X.h>
VL53L0X lox;

TaskHandle_t Task1;
TaskHandle_t Task2;

volatile int flow_frequency;    // измеряет частоту
int l_min;                      // рассчитанные литр/час
unsigned char flowsensor = 19;  // Вход сенсора
unsigned long currentTime;
unsigned long cloopTime;

void IRAM_ATTR flow()  // функция прерывания
{
  flow_frequency++;
}

bool pumpF = false;
int sumL = 0;
int target = 0;  // Объём который требуется набрать в мЛ

float dist;
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

    if (text == "/Volume") {
      float vol;
      vol = map(dist, 150, 50, 0, 100);
      bot.sendMessage(chat_id, "Бак заполнен на: "+ String(vol, 1) + "%", "");
    }

    if (text == "/SetFlow") {
      sumL = 0;
      pumpF = true;
      bot.sendMessage(chat_id, "Введите кол-во миллилитров:", "");
    }

    if (pumpF == true) {
      target = text.toInt();
      if (target != NULL) {
        digitalWrite(16, 1);

        while (sumL < target) {
          currentTime = millis();
          // Каждую секунду рассчитываем и выводим на экран литры в час
          if (currentTime >= (cloopTime + 100)) {
            cloopTime = currentTime;  // Обновление cloopTime
            // Частота импульсов (Гц) = 7.5Q, Q - это расход в л/мин.
            l_min = (flow_frequency / 15 * 630);  // (Частота x 60 мин) / 7.5Q = расход в л/час ////7.5 * 1070
            flow_frequency = 0;                     // Сбрасываем счетчик
            Serial.print(l_min, DEC);               // Отображаем л/час
            Serial.println(" mL/m ");
            sumL += l_min / 60 / 10;
            Serial.print(sumL);  // Отображаем мл/час
            Serial.println("ml");
          }
        }
        digitalWrite(16, 0);

        bot.sendMessage(chat_id, "Нужное кол-во набрано", "");
        bot.sendMessage(chat_id, "Налили: " + String(target) + " ml", "");
      }
    }

    if (text == "/start") {
      String welcome = "Привет, " + from_name + ", я контроллер йотик32, это проект Умный бак.\n";
      welcome += "Тут ты можешь управлять умным баком:\n\n";
      welcome += "/SetFlow : Налить требуемок кол-во\n";
      welcome += "/Volume : Узнать заполненность бака\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}


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

  delay(1000);
  Wire.begin();

  lox.init();
  lox.setTimeout(500);
  // параметры для режима высокой точности
  lox.setMeasurementTimingBudget(200000);

  pinMode(16, OUTPUT);
  digitalWrite(16, 0);
  pinMode(flowsensor, INPUT);
  digitalWrite(flowsensor, HIGH);
  attachInterrupt(flowsensor, flow, RISING);  // настраиваем прерывания
  //sei(); // активируем прерывания
  currentTime = millis();
  cloopTime = currentTime;

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
    dist = lox.readRangeSingleMillimeters();
    // Вывод измеренных значений в терминал
    Serial.println("Distance  = " + String(dist, 0) + " mm  ");
    delay(250);
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
