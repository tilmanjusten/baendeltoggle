#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "secrets.h"

#define PIN_SWITCH 2
#define SWITCH_OPEN HIGH
#define SWITCH_CLOSE LOW
#define NUM_DEVICES (sizeof(devices) / sizeof(int))
#define SWITCH_PERIOD 1000
#define DEBOUNCE_PERIOD 50

ESP8266WebServer server(HTTP_PORT);

bool doSwitch = false;
int currentState = SWITCH_OPEN;
int reading;
unsigned long timer = 0;
unsigned long periodTimer = 0;
unsigned long clickTimer = 0;
int clicksCount = 0;

void handleWebRequests404()
{
  String message = "File Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(404, "text/plain", message);

  Serial.println(message);
}

void switchDevice(bool enable, const char *device)
{
  Serial.println("-------------------------------------");
  Serial.print("Device: ");
  Serial.print(device);
  Serial.println();
  Serial.print("Connecting to; ");
  Serial.println(URL_HOST);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;

  if (!client.connect(URL_HOST, HTTP_PORT))
  {
    Serial.println(">>> Connection failed!");

    return;
  }

  // We now create a URI for the request
  String url = URL_PATH;
  url += device;
  url += "/";

  if (enable)
  {
    url += URL_PARAM_ON;
  }
  else
  {
    url += URL_PARAM_OFF;
  }

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + URL_HOST + "\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long timeout = millis();

  while (client.available() == 0)
  {
    if (millis() - timeout > 5000)
    {
      Serial.println(">>> Client Timeout!");
      client.stop();

      return;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  while (client.available())
  {
    String line = client.readStringUntil('\r');

    Serial.print(line);
  }

  Serial.println();
  Serial.println("Closing connection.");

  delay(100);
}

void switchDevices(bool switchMode)
{
  switchDevice(switchMode, "alle-lichter");
}

void handleSwitch()
{
  reading = digitalRead(PIN_SWITCH);

  if (millis() - timer > DEBOUNCE_PERIOD)
  {
    timer = millis();

    // count state changes within period
    // two changes or more = lights off
    // one change = lights on

    // state change happens
    if (reading != currentState)
    {
      currentState = reading;

      // start period
      if (periodTimer == 0)
      {
        periodTimer = millis();
      }

      // check if within period
      if (millis() - periodTimer <= SWITCH_PERIOD)
      {
        // increase clicks counter
        clicksCount += 1;
      }
    }
  }

  // check if elapsed period exists
  if (clicksCount > 0 && millis() - periodTimer > SWITCH_PERIOD)
  {
    // switch lights depending on clicks count
    if (clicksCount > 1)
    {
      switchDevices(false);
    }
    else
    {
      switchDevices(true);
    }

    // resets
    clicksCount = 0;
    periodTimer = 0;
  }
}

void wifiSetup()
{
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void switchSetup()
{
  // Switch initialization
  pinMode(PIN_SWITCH, INPUT);

  delay(100);

  // get current state on start
  //currentState = digitalRead(PIN_SWITCH);
  currentState = SWITCH_OPEN;

  Serial.print("Current state is: ");
  Serial.println(currentState);
}

void setupWebserver()
{
  if (MDNS.begin("Door Station"))
  {
    Serial.println("MDNS responder started");
  }

  // Overview
  server.serveStatic("/", SPIFFS, "/www/index.html");
  server.serveStatic("/index.html", SPIFFS, "/www/index.html");
  server.serveStatic("/styles.css", SPIFFS, "/www/styles.css");
  server.serveStatic("/scripts.js", SPIFFS, "/www/scripts.js");

  // Debug
  server.on("/state", []() {
    server.send(200, "text/plain", (const char *)SWITCH_OPEN);
  });

  // 404
  server.onNotFound(handleWebRequests404);

  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

bool loadFromSpiffs(String path)
{
  String dataType = "text/plain";

  if (path.endsWith("/"))
    path += "index.html";

  if (path.endsWith(".src"))
    path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(".html"))
    dataType = "text/html";
  else if (path.endsWith(".htm"))
    dataType = "text/html";
  else if (path.endsWith(".css"))
    dataType = "text/css";
  else if (path.endsWith(".js"))
    dataType = "application/javascript";
  else if (path.endsWith(".png"))
    dataType = "image/png";
  else if (path.endsWith(".gif"))
    dataType = "image/gif";
  else if (path.endsWith(".jpg"))
    dataType = "image/jpeg";
  else if (path.endsWith(".ico"))
    dataType = "image/x-icon";
  else if (path.endsWith(".xml"))
    dataType = "text/xml";
  else if (path.endsWith(".pdf"))
    dataType = "application/pdf";
  else if (path.endsWith(".zip"))
    dataType = "application/zip";

  File dataFile = SPIFFS.open(path.c_str(), "r");

  if (server.hasArg("download"))
  {
    dataType = "application/octet-stream";
  }

  if (server.streamFile(dataFile, dataType) != dataFile.size())
  {
  }

  dataFile.close();

  return true;
}

void setupOTA()
{
  // Port defaults to 8266
  ArduinoOTA.setPort(OTA_PORT);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("OTA Ready.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);
  delay(10);

  // WiFi
  wifiSetup();

  // OTA
  setupOTA();

  // Switch
  switchSetup();

  // Webserver
  SPIFFS.begin();
  setupWebserver();
}

void loop()
{
  // OTA
  ArduinoOTA.handle();

  // Webserver
  server.handleClient();

  // Switch
  handleSwitch();
}
