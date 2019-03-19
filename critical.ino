#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
#include "icon.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "FS.h"

// Initialize the OLED display using brzo_i2c
// D3 -> SDA
// D5 -> SCL
// D0 -> reset
//d1 - verde
//d2 amarelo
//d7 -vermelho
//RESET 4

int notice = 0;
int warning = 0;
int error = 0;
String project;

void(* resetFunc) (void) = 0;

// Initialize the OLED display using Wire library
SSD1306Wire  display(0x3c, D3, D5);
ESP8266WebServer server(80);
int row = 0;
String rows[6];


const char* ssidAccessPoint     = "CriticalAP";
const char* passwordAccessPoint = "cri123cal";

const char* ssid;
const char* password;
String url;


int interval = 5;
int  user_id;
int count = 0;
bool configMode = false;
bool flag = false;



void printDisplay(String text, int type)
{

  if (type == 2 || type == 3) {
    Serial.println(text);
  }
  if (type == 1 || type == 3)
  {
    if (row == 5  && flag )
    {
      for (int x = 1 ; x <= 5; x++)
      {
        rows[x - 1] = rows[x];
      }
    }
    if (text.indexOf("\n") >= 0 )
    {
      rows[row] = text;
      if (row == 5) flag = true;
      if (row < 5) row++;
    }
    else
    {
      rows[row] = String(rows[row] + text);
    }

    // PRINT DISPLAY
    display.clear();
    for (int x = 0 ; x <= 5; x++)
    {
      display.drawString(0, x * 10, rows[x]);
    }
    display.display();
  }
}







bool saveConfig(String ssid, String password, String user_id, String url, String interval ) {
  StaticJsonBuffer<256> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  json["ssid"] = ssid;
  json["password"] = password;
  json["user_id"] = user_id;
  json["url"] = url;
  json["interval"] = interval;


  File configFile = SPIFFS.open("/config1.json", "w");
  if (!configFile) {
    printDisplay("Failed to writing\n", 3);
    return false;
  }

  json.printTo(configFile);
  return true;
}



bool loadConfig()
{
  File configFile = SPIFFS.open("/config1.json", "r");
  if (!configFile) {
    printDisplay("Failed to open\n", 3);
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    printDisplay("Config too large\n", 3);
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<256> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    printDisplay("Failed to parse! \n", 3);
    printDisplay("Re-maker json file.\n", 3);
    resetConfig();
    delay(1000);
    printDisplay("Reboot...\n", 3);
    resetFunc();
    return false;
  }

  ssid = json["ssid"].as<char*>();
  password = json["password"].as<char*>();
  user_id = json["user_id"];
  url = json["url"].as<char*>();
  interval = json["interval"];


  printDisplay(String("ssid: " + String(ssid)), 2);
  printDisplay(String("password: " + String(password)), 2);
  printDisplay(String("user_id: " + String(user_id)), 2);
  printDisplay(String("url: " + String(url)), 2);
  printDisplay(String("interval: " + String(interval)), 2);


  return true;
}





void receiveData()
{
  if ((WiFi.status() == WL_CONNECTED )) {

    HTTPClient http;

    http.begin(url);

    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        printDisplay(String("Data: " + payload ), 2);

        StaticJsonBuffer<256> jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(payload);

        notice = json["notice"];
        warning = json["warning"];
        error = json["error"];
        project = json["project"].as<char*>();

        mainDisplay();

        digitalWrite(D1, 0);
        digitalWrite(D2, 0);
        digitalWrite(D7, 0);

        if (error == 0 && warning == 0)
        {
          digitalWrite(D1, 1);
        } else if ( warning > 0 && error == 0) {
          digitalWrite(D2, 1);
        } else {

          digitalWrite(D7, 1);
        }

        delay(interval * 1000);
      }
    } else {
      digitalWrite(D1, 1);
      digitalWrite(D2, 1);
      digitalWrite(D7, 1);
      delay(500);
      digitalWrite(D1, 0);
      digitalWrite(D2, 0);
      digitalWrite(D7, 0);
      delay(500);

      notice = error = warning = 0;
      mainDisplay();

      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
}


void page_form()
{
  String html = "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"></head><body><fieldset>";
  html += "<legend>Config</legend>";
  html += "<form method=\"post\" >";
  html += "SSID:<input type=\"text\" name=\"ssid\"><br>";
  html += "PWD:<input type=\"text\" name=\"password\"><br>";
  html += "USER:<input type=\"text\" name=\"user_id\"><br>";
  html += "URL:<input type=\"text\" name=\"url\" value=\"https://zeus.servicesdigital.com.br/critical\"><br>";
  html += "INTERVAL:<input type=\"text\" name=\"interval\" VALUE=\"5\"><br>";
  html += "<button type=\"submit\">Save</button>";
  html += "</form>";
  html += "</fieldset></body></html>";
  printDisplay("Cliente conected.", 3);
  server.send(200, "text/html", html);
}

void page_save()
{
  server.send(200, "text/html", "<h1>Config saved successfully.</h1>");

  saveConfig(server.arg("ssid"), server.arg("password"), server.arg("user_id"), server.arg("url"), server.arg("interval"));

  printDisplay("Config saved successfully.\n", 3);

  digitalWrite(D1, 1);
  delay(200);
  digitalWrite(D1, 0);
  delay(200);

  printDisplay("Reboot...", 3);
  resetFunc();
}


void mainDisplay()
{
  display.clear();
  display.drawRect(0, 0, 128, 20);
  display.drawString(5, 5, project);
  // hori, ver , larg, alt
  display.drawRect(0, 25, 39, 39);
  display.drawRect(44, 25, 39, 39);
  display.drawRect(88, 25, 39, 39);

  display.setFont(ArialMT_Plain_24);

  display.drawString(7, 30, leftZero(notice));
  display.drawString(51, 30, leftZero(warning)); //20
  display.drawString(93, 30, leftZero(error)); // 25
  display.display();
  display.setFont(ArialMT_Plain_10);

  Serial.println(notice);
}
String leftZero(int num)
{
  if (num >= 100)
    return "99";
  else
  {
    if (num < 10)
      return String("0" + String(num));
    else
      return String(num);
  }
}

void setup() {
  //start
  Serial.begin(115200);
  SPIFFS.begin();
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  pinMode(D1, OUTPUT);  //verd
  pinMode(D2, OUTPUT);  //amarelo
  pinMode(D7, OUTPUT);  //vermelho
  pinMode(D4, INPUT);   //reset



  display.setFont(ArialMT_Plain_10);

  //show logo
  display.clear();
  display.drawXbm(1, 1, icon_width, icon_height, icon_bits);
  display.display();
  digitalWrite(D1, 1);
  delay(500);
  digitalWrite(D1, 0);

  digitalWrite(D2, 1);
  delay(500);
  digitalWrite(D2, 0);

  digitalWrite(D7, 1);
  delay(500);
  digitalWrite(D7, 0);
  delay(500);

  digitalWrite(D1, 1);
  digitalWrite(D2, 1);
  digitalWrite(D7, 1);
  delay(100);
  digitalWrite(D1, 0);
  digitalWrite(D2, 0);
  digitalWrite(D7, 0);
  delay(100);

  digitalWrite(D1, 1);
  digitalWrite(D2, 1);
  digitalWrite(D7, 1);
  delay(100);
  digitalWrite(D1, 0);
  digitalWrite(D2, 0);
  digitalWrite(D7, 0);

  delay(100);

  digitalWrite(D1, 1);
  digitalWrite(D2, 1);
  digitalWrite(D7, 1);
  delay(100);
  digitalWrite(D1, 0);
  digitalWrite(D2, 0);
  digitalWrite(D7, 0);
  delay(100);

  digitalWrite(D1, 1);
  digitalWrite(D2, 1);
  digitalWrite(D7, 1);
  delay(100);
  digitalWrite(D1, 0);
  digitalWrite(D2, 0);
  digitalWrite(D7, 0);

  display.clear();



  //load config
  loadConfig();

  if (strcmp(ssid, "")  == 0)  configMode = true;

  if (configMode) {
    printDisplay("**** CONFIG MODE ****\n", 3);
    WiFi.softAP(ssidAccessPoint, passwordAccessPoint);
    printDisplay(String("AP: " + String(ssidAccessPoint) + "\n"), 3);
    printDisplay(String("Password: " + String(passwordAccessPoint) + "\n"), 3);
    printDisplay(String("IP address: " + WiFi.softAPIP().toString() + "\n"), 3);
    server.on("/", HTTP_GET, page_form);
    server.on("/", HTTP_POST, page_save);
    server.begin();




  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    printDisplay("Conecting WiFi.", 3);
    while (WiFi.status() != WL_CONNECTED) {
      printDisplay(".", 3);
      digitalWrite(D2, 1);
      delay(1000);
      digitalWrite(D2, 0);
      delay(1000);
      if (count > 10)  {
        printDisplay("Error\n", 3);
        delay(1000);
        printDisplay("Reboot...\n", 3);
        resetFunc();
        delay(1000);
      }
      count++;
      if (digitalRead(D4) == 0)
      {
        resetConfig();
        printDisplay("Reset factory.", 3);
        resetFunc();
        delay(2000);
      }
    }
    //printDisplay("OK\n", 3);
    //printDisplay(String("IP address:" + WiFi.localIP().toString() + "\n" ), 3);
    //delay(2000);
    mainDisplay();
  }




}

void resetConfig()
{
  digitalWrite(D1, 1);
  digitalWrite(D2, 1);
  digitalWrite(D7, 1);

  saveConfig("", "empty", "empty", "empty", "empty");
}

void loop() {
  // wait for WiFi connection
  if (!configMode )
  {
    receiveData();
    delay(3000);
    if (digitalRead(D4) == 0)
    {
      resetConfig();
      printDisplay("Reset factory.", 3);
      resetFunc();
      delay(2000);
    }


  } else {
    server.handleClient();
    if (WiFi.softAPgetStationNum() == 0)
    {
      digitalWrite(D7, 1);
      delay(200);
      digitalWrite(D7, 0);
      delay(200);
    } else {
      digitalWrite(D2, 1);
      delay(200);
      digitalWrite(D2, 0);
      delay(200);
    }

  }

}
