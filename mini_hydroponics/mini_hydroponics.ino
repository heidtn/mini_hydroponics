#include <OneWire.h> 
#include <DallasTemperature.h>

#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>

#include <ArduinoOTA.h>



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


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .ds-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>ESP DS18B20 Server</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="ds-labels">Temperature Celsius</span> 
    <span id="temperaturec">%TEMPERATUREC%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="ds-labels">Temperature Fahrenheit</span>
    <span id="temperaturef">%TEMPERATUREF%</span>
    <sup class="units">&deg;F</sup>
  </p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperaturec").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperaturec", true);
  xhttp.send();
}, 10000) ;
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperaturef").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperaturef", true);
  xhttp.send();
}, 10000) ;
</script>
</html>)rawliteral";


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

String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATUREC"){
    return String("1.0");
  }
  else if(var == "TEMPERATUREF"){
    return String("2.0");
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

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.begin();

  ArduinoOTA.setHostname("hydro");

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
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
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
  ArduinoOTA.handle();

}
