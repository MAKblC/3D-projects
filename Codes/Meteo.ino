#include <Wire.h>
#include "MCP3221.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include "SparkFun_SGP30_Arduino_Library.h"
#include <I2C_graphical_LCD_display.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#define BOT_TOKEN "xxxxxxxxxxx"  // Токен телеграмм бота
#define WIFI_SSID "xxxxxxx"      // Имя сети WI-FI
#define WIFI_PASSWORD "xxxxxxxx" // Пароль сети WI-FI

const unsigned long BOT_MTBS = 1000; 
WiFiClientSecure secured_client; 
UniversalTelegramBot bot(BOT_TOKEN, secured_client); 
unsigned long bot_lasttime; 

TaskHandle_t Task1;
TaskHandle_t Task2;

I2C_graphical_LCD_display lcd;

BH1750 lightMeter; 

MCP3221 mcp3221(0x4D); // Адрес может отличаться

Adafruit_BME280 bme280;

SGP30 mySensor ;

float t;
float h;
float p;
float lux;
float sensorVoltage;
float UV_index;
float tvoc;
float co2;

#define I2C_HUB_ADDR        0x70
#define EN_MASK             0x08
#define DEF_CHANNEL         0x00
#define MAX_CHANNEL         0x08

void setup() {
  Serial.begin(115200);
  Wire.begin();
  bme280.begin();
  delay(512);
  Serial.println();
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); 
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); 
  while (WiFi.status() != WL_CONNECTED) 
  {
    Serial.print(".");
    delay(250);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
 
  setBusChannel(0x04);
  lcd.begin();
  lcd.gotoxy (0, 0);
  lcd.clear (0, 0, 128, 64, 0x00);
  
  setBusChannel(0x06);
  mcp3221.setVinput(VOLTAGE_INPUT_5V);

  setBusChannel(0x07); 
  lightMeter.begin();

  setBusChannel(0x05);
  if (mySensor.begin() == false) {
    Serial.println("No SGP30 Detected. Check connections.");
    while (1);
  }
  mySensor.initAirQuality();

  setBusChannel(0x05);
  for(int i=0;i<=20;i++){
  mySensor.measureAirQuality();
  co2 = mySensor.CO2;
  tvoc = mySensor.TVOC;
  Serial.print("CO2: ");
  Serial.print(mySensor.CO2);
  Serial.print(" ppm\tTVOC: ");
  Serial.print(mySensor.TVOC);
  Serial.println(" ppb");
  delay (1000);
  }

 //создаем задачу, которая будет выполняться на ядре 0 с максимальным приоритетом (1)
  xTaskCreatePinnedToCore(
                    Task1code,   /* Функция задачи. */
                    "Task1",     /* Ее имя. */
                    10000,       /* Размер стека функции */
                    NULL,        /* Параметры */
                    1,           /* Приоритет */
                    &Task1,      /* Дескриптор задачи для отслеживания */
                    1);          /* Указываем пин для данного ядра */                  
  delay(500); 
//Создаем задачу, которая будет выполняться на ядре 1 с наивысшим приоритетом (1)
  xTaskCreatePinnedToCore(
                    Task2code,   /* Функция задачи. */
                    "Task2",     /* Имя задачи. */
                    10000,       /* Размер стека */
                    NULL,        /* Параметры задачи */
                    2,           /* Приоритет */
                    &Task2,      /* Дескриптор задачи для отслеживания */
                    1);          /* Указываем пин для этой задачи */
    delay(500); 
    
}

void Task1code( void * pvParameters ){
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());
  for(;;){
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
  }
}

void handleNewMessages(int numNewMessages)
{
  
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);
  // обработка сообщений
  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id; // ID чата
    String text = bot.messages[i].text; // текст сообщения
    //text.toLowerCase(); // преобразовать текст в нижний регистр Ff
    String from_name = bot.messages[i].from_name; // имя пользователя
    if (from_name == "")
      from_name = "Guest";

    if (text == "/THP")
    {
      String welcome = "Показания датчиков:\n";
      welcome += "🌡 Температура воздуха: " + String(t, 1) + " °C\n";
      welcome += "💧 Влажность воздуха: " + String(h, 0) + " %\n";
      welcome += "☁ Атмосферное давление: " + String(p, 0) + " мм рт.ст.\n";
      bot.sendMessage(chat_id, welcome, "");
      
    }
  
    if (text == "/illumination")
    {
      String welcome = "Показания датчиков:\n";
      welcome += "☀ Освещенность: " + String(lux, 0) + " Лк\n";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/UV")
    {
      String welcome = "Показания датчиков:\n";
      welcome += "📊 Уровень УФ: " + String(sensorVoltage, 0) + " mV\n";
      welcome += "🔆 Индекс УФ: " + String(UV_index, 0) + " \n";
      bot.sendMessage(chat_id, welcome, "");
    }
    
    if (text == "/CO2")
    {
      String welcome = "Показания датчиков:\n";
      welcome += " Уровень углекислого газа: " + String(co2, 0) + " ppm\n";
      welcome += " Концентрация TVOC: " + String(tvoc, 0) + " ppb\n";
      bot.sendMessage(chat_id, welcome, "");
    }
    

  
    if (text == "/start")
    {
      bot.sendMessage(chat_id, "Привет, " + from_name + "!", "");
      bot.sendMessage(chat_id, "Я контроллер Йотик 32. Я принимаю следующие команды:", "");
      String sms =  "/illumination - считать данные об освещённости.";
      String sms1 =" /THP - вывод температуры, влажности и давления.";
      String sms2 =" /UV - вывод выходного напряжения и индекса ультрафиолетового излучения.";
      String sms3 =" /CO2 - вывод количества CO2 и TVOC.";
      bot.sendMessage(chat_id, sms, "");
      bot.sendMessage(chat_id, sms1, "");
      bot.sendMessage(chat_id, sms2, "");
      bot.sendMessage(chat_id, sms3, "");
    }
  }
}

void Task2code( void * pvParameters ){
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());
  for(;;){
  t = bme280.readTemperature();
  h = bme280.readHumidity();
  p = bme280.readPressure() / 100.0F;
  // Вывод измеренных значений в терминал
  Serial.println("Air temperature = " + String(t, 1) + " *C");
  Serial.println("Air humidity = " + String(h, 1) + " %");
  Serial.println("Air pressure = " + String(p, 1) + " hPa"); // 1 mmHg = 1 hPa / 1.33
  
  setBusChannel(0x04);
  lcd.clear (0, 0, 128, 64, 0x00);
  lcd.gotoxy (40, 53); // устанавливаем курсор в координату (40,53)
  lcd.string ("Temperature, C" , false); // пишем фразу для температуры
  char buf[8]; // создаем переменную типа char
  sprintf(buf, "%d", (int)t); // отправляем в нее данные о температуре
  lcd.gotoxy (10, 53); // устанавливаем курсор в координату (10,53)
  lcd.string (buf, false); // Пишем значение температуры
  lcd.gotoxy (40, 39); // устанавливаем курсор в координату (40,39)
  lcd.string ("Humidity, %" , false);// пишем фразу для влажности
  char buf1[8]; // создаем переменную типа char
  sprintf(buf1, "%d", (int)h); // отправляем в нее данные о влажности
  lcd.gotoxy (10, 39); // устанавливаем курсор в координату (10,39)
  lcd.string (buf1, false); // Пишем значение температуры
  lcd.gotoxy (40, 22); // устанавливаем курсор в координату (40,22)
  lcd.string ("Pressure, hPa" , false); // пишем фразу для давления
  char buf2[8]; // создаем переменную типа char
  sprintf(buf2, "%d", (int)p); // отправляем в нее данные о давлении
  lcd.gotoxy (10, 22); // устанавливаем курсор в координату (10,22)
  lcd.string (buf2, false); // Пишем значение давления
  delay(2000);
  lcd.clear (0, 0, 128, 64, 0x00);
  
  // Считывание датчиков
  setBusChannel(0x07);
  lux = lightMeter.readLightLevel(); // считывание освещенности
  Serial.print("Освещенность: ");
  Serial.print(lux);
  Serial.println(" Люкс");

  float sensorValue;
  setBusChannel(0x06);
  sensorValue = mcp3221.getVoltage();
  sensorVoltage = 1000 * (sensorValue / 4096 * 5.0); // напряжение на АЦП
  UV_index = 370 * sensorVoltage / 200000; // Индекс УФ (эмпирическое измерение)
  Serial.println("Выходное напряжение = " + String(sensorVoltage, 1) + "мВ");
  Serial.println("UV index = " + String(UV_index, 1));
  
  setBusChannel(0x04);
  lcd.gotoxy (40, 53); // устанавливаем курсор в координату (40,53)
  lcd.string ("illumin,Lux" , false);
  char buf3[8]; // создаем переменную типа char
  sprintf(buf3, "%d", (int)lux); // отправляем в нее данные об ОСВЕЩЁННОСТИ
  lcd.gotoxy (10, 53); // устанавливаем курсор в координату (10,53)
  lcd.string (buf3, false); // Пишем значение температуры
  lcd.gotoxy (40, 39); // устанавливаем курсор в координату (40,39)
  lcd.string ("output volt,mV" , false);// пишем фразу для влажности
  char buf4[8]; // создаем переменную типа char
  sprintf(buf4, "%d", (int)sensorVoltage); // отправляем в нее данные о Вальтаже
  lcd.gotoxy (10, 39); // устанавливаем курсор в координату (10,39)
  lcd.string (buf4, false); // Пишем значение температуры
  lcd.gotoxy (40, 22); // устанавливаем курсор в координату (40,22)
  lcd.string ("UV ind., V/cm2" , false); // пишем фразу для давления
  char buf5[8]; // создаем переменную типа char
  sprintf(buf5, "%d", (int)UV_index); // отправляем в нее данные о УФ
  lcd.gotoxy (10, 22); // устанавливаем курсор в координату (10,22)
  lcd.string (buf5, false); // Пишем значение УФ
  delay(2000);
  lcd.clear (0, 0, 128, 64, 0x00);

  setBusChannel(0x05);

  mySensor.measureAirQuality();
  co2 = mySensor.CO2;
  tvoc = mySensor.TVOC;
  Serial.print("CO2: ");
  Serial.print(mySensor.CO2);
  Serial.print(" ppm\tTVOC: ");
  Serial.print(mySensor.TVOC);
  Serial.println(" ppb");
  
  setBusChannel(0x04);
  lcd.gotoxy (40, 53); // устанавливаем курсор в координату (40,53)
  lcd.string ("CO2, ppm" , false);
  char buf6[8]; // создаем переменную типа char
  sprintf(buf6, "%d", (int)mySensor.CO2); // отправляем в нее данные об CO2
  lcd.gotoxy (10, 53); // устанавливаем курсор в координату (10,53)
  lcd.string (buf6, false); // Пишем значение температуры
  lcd.gotoxy (40, 39); // устанавливаем курсор в координату (40,39)
  lcd.string ("TVOC, ppb" , false);// пишем фразу для влажности
  char buf7[8]; // создаем переменную типа char
  sprintf(buf7, "%d", (int)mySensor.TVOC); // отправляем в нее данные о TVOC
  lcd.gotoxy (10, 39); // устанавливаем курсор в координату (10,39)
  lcd.string (buf7, false); // Пишем значение температуры
  delay(2000);
  lcd.clear (0, 0, 128, 64, 0x00);
  
}
}
// Функция установки нужного выхода I2C
bool setBusChannel(uint8_t i2c_channel)
{
  if (i2c_channel >= MAX_CHANNEL)
  {
    return false;
  }
  else
  {
    Wire.beginTransmission(I2C_HUB_ADDR);
   Wire.write(0x01 << i2c_channel); 
	 
    Wire.endTransmission();
    return true;
  }
}

void loop() { }






