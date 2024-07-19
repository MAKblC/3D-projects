#include <Wire.h>
#include <WiFi.h> // библиотека для Wi-Fi
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h> // библиотека Телеграм

#define WIFI_SSID "xxxxx" // логин Wi-Fi
#define WIFI_PASSWORD "xxxxx" // пароль Wi-Fi
#define BOT_TOKEN "xxxxx" // токен телеграмм бота 7015495636:AAGm0HaRi7QvQSLG_JrEtzWraZtj0betrME

const unsigned long BOT_MTBS = 1000; // время обновления
WiFiClientSecure secured_client; // клиент SSL (TLS)
UniversalTelegramBot bot(BOT_TOKEN, secured_client); // экземпляр бота
unsigned long bot_lasttime; // счетчик времени
#include "Adafruit_APDS9960.h"
#include <ESP32_Servo.h>
Adafruit_APDS9960 apds9960;
Servo myservo1; //Сервопривод шлагбаума. Его нужно установить с углом в 5 градусов в закрытом положении руки шлагбаума 
Servo myservo2; //Сервопривод подвижного ската. Его нужно установить с углов в 0 градусов с направленим откидывания в ячейку 1.



int detal1_red; // Переменные для запоминания показаний для детали 1
int detal1_blue;
int detal1_green;

int detal2_red; // Переменные для запоминания показаний для детали 2
int detal2_blue;
int detal2_green;

int detal3_red; // Переменные для запоминания показаний для детали 3
int detal3_blue;
int detal3_green;

int detal1_kolvo =0; // Переменные для запоминания количества деталей
int detal2_kolvo =0;
int detal3_kolvo =0;

uint16_t red_data;
uint16_t green_data; 
uint16_t blue_data;  
uint16_t clear_data; 
uint16_t prox_data;  
int auto1 = 0;
String keyboardJson2 = "[[\"/start\",\"/options\"]]"; // панель для команды /menu 
String keyboardJson1 = "[[\"/reg_1\", \"/reg_2\", \"/reg_3\"],[\"/kolvo_1\", \"/kolvo_2\", \"/kolvo_3\"],[\"/reset\", \"/menu\"]]"; // панель для команды /options
void setup() {
  // Инициализация последовательного порта
  Serial.begin(115200);
  // Инициализация датчика
  if (!apds9960.begin()) {
    Serial.println("Failed to initialize device!");
  }
  // Инициализация режимов работы датчика
  apds9960.enableColor(true);
  apds9960.enableProximity(true);
  myservo1.attach(19);     // пин сервомотора
  myservo2.attach(23);     // пин сервомотора
 
  myservo1.write(100);
  myservo2.write(0);
  
  if (BOT_TOKEN == "xxxxx")
  {
  auto1 = 1;
  }
  Serial.println();
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // подключение
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // сертификат
  while (WiFi.status() != WL_CONNECTED) // проверка подключения
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  autov();
}

void autov() 
{
  bool a=0;
  while (auto1 == 1 && a == 0)
  {
    Serial.print("Поднесите деталь. Введите 1, 2 или 3 для записи цвета детали.\n");
    delay(2000);
    apds9960.getColorData(&red_data, &green_data, &blue_data, &clear_data);
    // Определение близости препятствия
    prox_data = apds9960.readProximity();
    if (Serial.available()) {
    int val = Serial.parseInt();

      if (val == 1)
      {
      detal1_red= red_data;
      detal1_blue= blue_data;   
      detal1_green= green_data;
      Serial.print("Цвет записан. Уберите деталь.");
      }
    
      if (val == 2)
      {
      detal2_red= red_data;
      detal2_blue= blue_data;   
      detal2_green= green_data; 
      Serial.print("Цвет записан. Уберите деталь.");
      }

      if (val == 3)
      {
      detal3_red= red_data;
      detal3_blue= blue_data;   
      detal3_green= green_data; 
      Serial.print("Цвет записан. Уберите деталь.");
      }
      if (detal1_red>0 &&  detal2_blue>0 && detal3_green>0)
      a=1;
    }
  }
}

void loop() {
  
  // Определение цвета
  while (!apds9960.colorDataReady()) {
    delay(5);
  }
  apds9960.getColorData(&red_data, &green_data, &blue_data, &clear_data);
  // Определение близости препятствия
  prox_data = apds9960.readProximity();
  // Вывод измеренных значений в терминал
  Serial.println("RED   = " + String(red_data));
  Serial.println("GREEN = " + String(green_data));
  Serial.println("BLUE  = " + String(blue_data));
  Serial.println("CLEAR = " + String(clear_data));
  Serial.println("PROX  = " + String(prox_data));
  delay(350);

  if (millis() - bot_lasttime > BOT_MTBS) // периодическая проверка
  {
    // постановка нового сообщения в очередь обработки
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    // если сообщение пришло
    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    // отслеживание периодичности опроса
    bot_lasttime = millis();
  }





if (detal1_red>0 &&  detal1_blue>0 && detal1_green>0)
{
  if(detal1_red+5 >=red_data && red_data >= detal1_red-5 && detal1_green+5 >= green_data && green_data >= detal1_green-5 && detal1_blue+5 >= blue_data && blue_data >= detal1_blue-5 && prox_data >=100) 
  {
    Serial.println("Detal_1");
    myservo2.write(0);
    delay(1000);
     for (int i = 100; i > 5; i--) {
    myservo1.write(i);
    delay(10);
    }
    delay(1000);
    detal1_kolvo=detal1_kolvo+1;
  }
}
if( detal2_red>0 and detal2_blue>0 and detal2_green>0)
{
  if(detal2_red+5 >=red_data && red_data >= detal2_red-5 && detal2_green+5 >= green_data && green_data >= detal2_green-5 && detal2_blue+5 >= blue_data && blue_data >= detal2_blue-5 && prox_data >=100)
  {
    Serial.println("Detal_2");
     for (int i = 0; i <35; i++) {
    myservo2.write(i);
    delay(25);
    }
    delay(1000);
     for (int i = 100; i > 5; i--) {
    myservo1.write(i);
    delay(10);
    }
    delay(1000);
     for (int i = 35; i >0; i--) {
    myservo2.write(i);
    delay(25);
    }
    detal2_kolvo=detal2_kolvo+1;
  }
}
if( detal3_red>0 and detal3_blue>0 and detal3_green>0)
{
  if(detal3_red+5 >=red_data && red_data >= detal3_red-5 && detal3_green+5 >= green_data && green_data >= detal3_green-5 && detal3_blue+5 >= blue_data && blue_data >= detal3_blue-5 && prox_data >=100)
  {
    Serial.println("Detal_3");
    for (int i = 0; i <71; i++) {
    myservo2.write(i);
    delay(25);
    }
    delay(1000);
     for (int i = 100; i > 5; i--) {
    myservo1.write(i);
    delay(10);
    }
    delay(1000);
     for (int i = 71; i >0; i--) {
    myservo2.write(i);
    delay(25);
    }
    detal3_kolvo=detal3_kolvo+1;
  }
}
myservo1.write(100);

}

// функция обработки нового сообщения
void handleNewMessages(int numNewMessages)
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);
  // обработка сообщений
  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id; // ID чата
    String text = bot.messages[i].text; // текст сообщения
    text.toLowerCase(); // преобразовать текст в нижний регистр
    String from_name = bot.messages[i].from_name; // имя пользователя
    if (from_name == "")
      from_name = "Guest";
    // обработка команд
    if (text == "/reg_1")
    {
      detal1_red= red_data;
      detal1_blue= blue_data;   
      detal1_green= green_data; 
       
      bot.sendMessage(chat_id, "показания записаны", "");
    }
    if (text == "/reg_2")
    {
      detal2_red= red_data;
      detal2_blue= blue_data;   
      detal2_green= green_data; 
      bot.sendMessage(chat_id, "показания записаны", "");
    }
    if (text == "/reg_3")
    {
      detal3_red= red_data;
      detal3_blue= blue_data;   
      detal3_green= green_data; 
       
      bot.sendMessage(chat_id, "показания записаны", "");
    }
      if (text == "/kolvo_1")
    {
      int kolvo_1=detal1_kolvo;
      bot.sendMessage(chat_id, String(kolvo_1), "");
    }
        if (text == "/kolvo_2")
    {
      int kolvo_2=detal2_kolvo;
      bot.sendMessage(chat_id, String(kolvo_2), "");
    }
      
        if (text == "/kolvo_3")
    {
      int kolvo_3=detal3_kolvo;
      bot.sendMessage(chat_id, String(kolvo_3), "");
    }
    if (text == "/reset")
    {
      detal1_red= 0;
      detal1_blue= 0;
      detal1_green=0;
      detal2_red= 0;
      detal2_blue= 0;
      detal2_green=0;
      detal3_red= 0;
      detal3_blue= 0;
      detal3_green=0;
      detal1_kolvo=0;
      detal2_kolvo=0;
      detal3_kolvo=0;
      bot.sendMessage(chat_id, "сброшено", "");
    }
    if (text == "/options") {
      // отображение панели
      bot.sendMessageWithReplyKeyboard(chat_id, "Выберите команду", "", keyboardJson1, true);
    }
     if (text == "/menu") {
      // отображение панели
      bot.sendMessageWithReplyKeyboard(chat_id, "Выберите команду", "", keyboardJson2, true);
    }
    if (text == "/help")
    {
      String sms1 ="Руководство пользователя.\n";
      sms1 += "Перед началом работы надо откалибровать датчик: /reg_1 - команда для запоминания цвета детали 1. /reg_2 - команда для запоминания цвета детали 2. /reg_3 - команда для запоминания цвета детали 3. Детали отправляются в номер ячейки соотвтствующий номеру детали.\n";
      sms1 += "В любой момент работы объекта можно подать команды которые выводят количество изделий отправленных в ячейку. В 1 ячейку - /kolvo_1.Во 2 ячейку - /kolvo_2.В 3 ячейку - /kolvo_3.\n";
      sms1 += "Перед началом работы с новыми изделиями слодует сбросить запомненные данные о прошлых изделиях и их подсчитанное количество. Для этого есть команда /reset.\n";
      bot.sendMessage(chat_id, sms1, "");
    }
    if (text == "/start")
    {
      bot.sendMessage(chat_id, "Я контроллер Йотик 32. Я принимаю следующие команды:", "");
      String sms = "/reg_1 - Запомнить первую деталь. Подайте эту команду чтобы запомнить цвет детали. Уберите деталь при получении что деталь записана.\n";
      sms += "/reg_2 - Запомнить вторую деталь. Подайте эту команду чтобы запомнить цвет детали. Уберите деталь при получении что деталь записана.\n";
      sms += "/reg_3 - Запомнить третью деталь. Подайте эту команду чтобы запомнить цвет детали. Уберите деталь при получении что деталь записана.\n";
      sms += "/kolvo_1 - Вывести количество первых деталей.\n";
      sms += "/kolvo_2 - Вывести количество вторых деталей.\n";
      sms += "/kolvo_3 - Вывести количество третьих деталей.\n";
      sms += "/reset - сброс запомненных деталей и их количества.\n";
      sms += "/options - Вызов удобного меню.\n";
      sms += "/help - Вызов руководства пользователя.\n";
      bot.sendMessage(chat_id, sms, "");
    }
  }
}
