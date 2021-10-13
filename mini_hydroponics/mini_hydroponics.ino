#include <DallasTemperature.h>
#include <OneWire.h>

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Hash.h>

#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include <NTPClient.h>

#include "index.h"

// full range at 1000ppm is 2.3v -> 2.3/3.3 = x/1024 or 2.3*1024/3.3 = 713.69
#define FULL_RANGE 2.3
#define FULL_RANGE_RAW (FULL_RANGE * 1024 / 3.3)
#define TDS_TO_PPM (1000.0 / FULL_RANGE_RAW)

// one wire setup
#define ONE_WIRE_BUS 13
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


// pump setup
#define PUMP_PIN 2
#define PUMP_OFF_TIME 3  // how many hours to keep the pump off when toggled
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 60 * 60 * -7; // UTC -8 i.e. PST
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Light setup
byte mac[6];
#define LIGHT_NAME "hydroponics light"
#define LIGHT_VERSION 2.1
#define LIGHT_PIN 12
#define LIGHT_OFFTIME 6 // how many hours the light will turn off for, starting at midnight


// wifi setup
const char *ssid = "NATural20-24g";
const char *password = "Foxland5!";
const char *mdns = "hydro";

AsyncWebServer server(80);

// MQTT setup
#define MQTT_SERV "io.adafruit.com"
#define MQTT_PORT 1883
#define MQTT_NAME "strigusconsilium"
#define MQTT_PASS "key"
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, MQTT_SERV, MQTT_PORT, MQTT_NAME, MQTT_PASS);
Adafruit_MQTT_Subscribe onoff = Adafruit_MQTT_Subscribe(&mqtt, MQTT_NAME "/f/onoff");


float shared_temperature = 0;
float shared_TDS = 0;
bool pump_status = true;
bool light_status = true;
unsigned long pump_disable_time = 0;

int cur_minute_display = 100;

#define NUM_SAMPLES 512
#define SAMPLE_INTERVAL 240
float temperatures[NUM_SAMPLES] = {0};
float TDSs[NUM_SAMPLES] = {0};
int sample_head = 0;

// ----------------- function prototypes ----------------
void setup_wifi();
float get_TDS();
float get_temperature();
void handle_measurements(float PPM, float temperature);
void handle_pump();
void handle_light();
void set_pump(int state);

// ----------------- function definitions ---------------

void setup() {
  Serial.begin(115200);
  sensors.begin();
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, HIGH);
  set_pump(0);
  setup_wifi();
}

String processor(const String &var) {
  if (var == "TEMPERATUREC") {
    return String(shared_temperature);
  } else if (var == "TDS") {
    return String(shared_TDS);
  } else if (var == "PUMP_STATE") {
    if (pump_status) {
      return String("checked");
    } else {
      return String("");
    }
  } else if (var == "LIGHT_STATE") {
    if(light_status) {
      return String("checked");
    } else {
      return String("");
    }
  } else if (var == "TIME") {
    return String(cur_minute_display);
  } else if (var == "TEMPERATURE_ARRAY") {
    String data("[");
    for (int i = 0; i < NUM_SAMPLES; i++) {
      data += String(temperatures[(i + sample_head + 1) % NUM_SAMPLES]);
      if (i != NUM_SAMPLES - 1)
        data += String(',');
    }
    data += "]";
    return data;
  } else if (var == "TDS_ARRAY") {
    String data("[");
    for (int i = 0; i < NUM_SAMPLES; i++) {
      data += String(TDSs[(i + sample_head + 1) % NUM_SAMPLES]);
      if (i != NUM_SAMPLES - 1)
        data += String(',');
    }
    data += "]";
    return data;
  } else if (var == "LABELS") {
    String data("[");
    for (int i = 0; i < NUM_SAMPLES; i++) {
      data += String(i * SAMPLE_INTERVAL);
      if (i != NUM_SAMPLES - 1)
        data += String(',');
    }
    data += "]";
    return data;
  }
  return String();
}

void setup_wifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected! ");
  Serial.println(WiFi.localIP());

  if (!MDNS.begin(mdns)) {
    Serial.println("Unable to setup the MDNS responder!");
  }
  Serial.print("MDNS Broadcasting at: ");
  Serial.println(mdns);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/temperaturec", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(shared_temperature).c_str());
  });

  server.on("/TDS", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(shared_TDS).c_str());
  });

  server.on("/pump_state", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(pump_status).c_str());
  });

  server.on("/light_state", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(light_status).c_str());
  });

  server.on("/set_pump", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("state")) {
      Serial.print("Setting Bubbler to ");
      String state = request->getParam("state")->value();
      int pump_status_value = state.toInt();
      pump_status = pump_status_value;
      if(pump_status == 0) {
        pump_disable_time = timeClient.getEpochTime();        
      }
      Serial.println(pump_status);
      set_pump(pump_status_value);
      request->send(200, "text/plain", "OK");
    }
  });

  server.on("/set_light", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      int light_status_value = state.toInt();
      light_status = (bool)light_status_value;
      digitalWrite(LIGHT_PIN, light_status_value);
      request->send(200, "text/plain", "OK");
    }
  });

  server.on("/data_chart", HTTP_GET, [](AsyncWebServerRequest *request) {
    String data("{\"temperatures\": [");
    for (int i = 0; i < NUM_SAMPLES; i++) {
      data += String(temperatures[(i + sample_head + 1) % NUM_SAMPLES]);
      if (i != NUM_SAMPLES - 1)
        data += String(',');
    }
    data += String("], \"TDSs\": [");
    for (int i = 0; i < NUM_SAMPLES; i++) {
      data += String(TDSs[(i + sample_head + 1) % NUM_SAMPLES]);
      if (i != NUM_SAMPLES - 1)
        data += String(',');
    }
    data += String("]}");
    request->send_P(200, "text/plain", data.c_str());
  });

  server.begin();

  mqtt.subscribe(&onoff);


  ArduinoOTA.setHostname("hydro");

  ArduinoOTA.onStart([]() { Serial.println("Start"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
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
  Serial.println("OTA ready");
}

float get_TDS() {
  int raw_conductivity = analogRead(0);
  float PPM = raw_conductivity * TDS_TO_PPM;
  return PPM;
}

float get_temperature() {
  float temperature = sensors.getTempCByIndex(0);
  return temperature;
}

void handle_measurements(float PPM, float temperature) {
  static int count = 0;
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(", PPM: ");
  Serial.println(PPM);
  shared_temperature = temperature;
  shared_TDS = PPM;
  if (count == SAMPLE_INTERVAL) {
    temperatures[sample_head] = temperature;
    TDSs[sample_head] = PPM;
    sample_head++;
    sample_head = sample_head % NUM_SAMPLES;
    count = 0;
  }
  count++;
}

void handle_light() {
  static int last_hour = timeClient.getHours();
  int cur_hour = timeClient.getHours();

  if((last_hour == (LIGHT_OFFTIME - 1)) && (cur_hour == LIGHT_OFFTIME)) {
    digitalWrite(LIGHT_PIN, HIGH);
    light_status = true;
  } else if(last_hour == 23 && cur_hour == 0) {
    digitalWrite(LIGHT_PIN, LOW);
    light_status = false;
  }
  last_hour = cur_hour;
}

void handle_pump() {
  Serial.print("Current time is: ");
  Serial.print(timeClient.getHours());
  Serial.print(":");
  Serial.println(timeClient.getMinutes());

  int cur_hour = timeClient.getHours();
  int cur_minute = timeClient.getMinutes();

  cur_minute_display = cur_minute;
  if(pump_status) {
    if(cur_minute >= 0 && cur_minute < 15) {
      set_pump(1);
    } else if(cur_minute >= 15 && cur_minute < 30) {
      set_pump(0);
    } else if(cur_minute >=30 && cur_minute < 45) {
      set_pump(1);
    } else {
      set_pump(0);
    }
  } else {
    if(((timeClient.getEpochTime() - pump_disable_time) % 86400L) / 3600 > PUMP_OFF_TIME) {
      pump_status = 1;
    }
  }

}

void set_pump(int state) {
  digitalWrite(PUMP_PIN, !state); // control is inverted
}

void MQTT_connect() 
{
  int8_t ret;
  // Stop if already connected
  if (mqtt.connected())
  {
    return;
  }

  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) // connect will return 0 for connected
  { 
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(1000);  // wait 5 seconds
    retries--;
    if (retries == 0) 
    {
      // basically die and wait for WDT to reset me
      break;
    }
  }
  Serial.println("MQTT Connected!");
}

void mqtt_handle() {
  MQTT_connect();

  //Read from our subscription queue until we run out, or
  //wait up to 5 seconds for subscription to update
  Adafruit_MQTT_Subscribe * subscription;
  if ((subscription = mqtt.readSubscription(1000)))
  {
    //If we're in here, a subscription updated...
    if (subscription == &onoff)
    {
      //Print the new value to the serial monitor
      Serial.print("onoff: ");
      Serial.println((char*) onoff.lastread);

      //If the new value is  "ON", turn the light on.
      //Otherwise, turn it off.
      if (!strcmp((char*) onoff.lastread, "ON"))
      {
        //active low logic
        digitalWrite(LIGHT_PIN, HIGH);
      }
      else
      {
        digitalWrite(LIGHT_PIN, LOW);
      }
    }
  }

  // ping the server to keep the mqtt connection alive
  if (!mqtt.ping()) {
    mqtt.disconnect();
  }
}

void loop() {
  float conductivity = get_TDS();
  float temperature = get_temperature();
  handle_measurements(conductivity, temperature);
  handle_pump();
  timeClient.update();
  sensors.requestTemperatures();
  ArduinoOTA.handle();
  mqtt_handle();
  delay(100);
}
