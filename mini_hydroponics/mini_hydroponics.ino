#include <OneWire.h> 
#include <DallasTemperature.h>

#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>



// full range at 1000ppm is 2.3v -> 2.3/3.3 = x/1024 or 2.3*1024/3.3 = 713.69
#define FULL_RANGE 2.3
#define FULL_RANGE_RAW (FULL_RANGE*1024/3.3)
#define TDS_TO_PPM (1000.0/FULL_RANGE_RAW)

// one wire setup
#define ONE_WIRE_BUS 13
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);

// wifi setup
const char* ssid = "";
const char* password = "";
const char* mdns = "hydro";

AsyncWebServer server(80);


// ----------------- function prototypes ----------------
void setup_wifi();
float get_TDS();
float get_temperature();
void handle_measurements(float PPM, float temperature);
void handle_air_pump();

// ----------------- function definitions ---------------

void setup() {
  Serial.begin(115200);
  sensors.begin();
  setup_wifi();
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
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(", PPM: ");
  Serial.println(PPM);
}

void handle_air_pump() {

}

void loop() {
  float conductivity = get_TDS();
  float temperature = get_temperature();
  handle_measurements(conductivity, temperature);
  handle_air_pump();
  sensors.requestTemperatures(); 
  delay(1000);
}
