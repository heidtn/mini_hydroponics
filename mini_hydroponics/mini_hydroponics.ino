#include <OneWire.h> 
#include <DallasTemperature.h>


// full range at 1000ppm is 2.3v -> 2.3/3.3 = x/1024 or 2.3*1024/3.3 = 713.69
#define TDS_TO_PPM (1000.0/713.69)

#define ONE_WIRE_BUS 13
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);

// ----------------- function prototypes ----------------
void setup_wifi();

void setup() {
  Serial.begin(115200);
  sensors.begin();
}

void setup_wifi() {

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
