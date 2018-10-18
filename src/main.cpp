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

ESP8266WebServer server(HTTP_PORT);

bool doSwitch = false;
int currentState = SWITCH_OPEN;
int previousState = SWITCH_CLOSE;
int reading;
long timer = 0;
long debounceTimer = 1000;

void handleWebRequests()
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

void switchDevices()
{
  bool switchMode = currentState == SWITCH_OPEN;

  doSwitch = false;

  switchDevice(switchMode, "alle-lichter");

  //  Serial.println("+++++++++++++++++++++++++++++++++++++");
  //  Serial.print("Switch Mode: ");
  //  Serial.println(switchMode);
  //
  //  for (int i = 0; i < NUM_DEVICES; i++) {
  //    switchDevice(switchMode, devices[i]);
  //  }
}

void handleSwitch()
{
  reading = digitalRead(PIN_SWITCH);

  if (millis() - timer > debounceTimer && doSwitch == false)
  {
    if (reading != currentState)
    {
      Serial.print("Change state: ");

      doSwitch = true;

      if (currentState == SWITCH_OPEN)
      {
        Serial.println("Lights off!");
        currentState = SWITCH_CLOSE;
      }
      else
      {
        Serial.println("Lights on!");
        currentState = SWITCH_OPEN;
      }
    }

    timer = millis();
  }

  previousState = reading;

  if (doSwitch)
  {
    switchDevices();
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
  previousState = currentState == SWITCH_OPEN ? SWITCH_CLOSE : SWITCH_OPEN;

  Serial.print("Current state is: ");
  Serial.println(currentState);
}

void setupWebserver()
{
  if (MDNS.begin("Door Station"))
  {
    Serial.println("MDNS responder started");
  }

  server.serveStatic("/", SPIFFS, "/www/");

  server.on("/state", []() {
    String state = currentState == SWITCH_OPEN ? "on" : "off";

    Serial.print("Handle request: ");
    Serial.println("/state");

    server.send(200, "text/plain", state);
  });

  server.on("/switch/alle-lichter/on", []() {
    Serial.print("Handle request: ");
    Serial.println("/switch/alle-lichter/on");

    server.send(200, "text/plain", "on");

    switchDevice(true, "alle-lichter");
  });

  server.on("/switch/alle-lichter/off", []() {
    Serial.print("Handle request: ");
    Serial.println("/switch/alle-lichter/off");

    server.send(200, "text/plain", "off");

    switchDevice(false, "alle-lichter");
  });

  server.onNotFound(handleWebRequests);

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

  wifiSetup();
  setupOTA();
  switchSetup();
  setupWebserver();
}

void loop()
{
  ArduinoOTA.handle();
  server.handleClient();
  handleSwitch();
}
