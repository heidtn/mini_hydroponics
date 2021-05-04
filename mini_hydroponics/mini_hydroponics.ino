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

// bubbler setup
#define BUBBLER_PIN 2

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
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
    input:checked+.slider {background-color: #b30000}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>Mini Hydroponics Server</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="ds-labels">Temperature Celsius</span> 
    <span id="temperaturec">%TEMPERATUREC%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-bolt" style="color:#059e8a;"></i> 
    <span class="ds-labels">TDS in PPM</span>
    <span id="TDS">%TDS%</span>
  </p>

  <label class="switch">
      <input type="checkbox" onchange="toggleCheckbox(this)" id=BUBBLER_STATE %BUBBLER_STATE%>
      <span class="slider"></span>
  </label>
 
</body>
<script>
function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ 
      xhr.open("GET", "/set_bubbler?state=0", true); 
  } else { 
      xhr.open("GET", "/set_bubbler?state=1", true); 
  }
  xhr.send();
}

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperaturec").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperaturec", true);
  xhttp.send();
}, 1000) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("TDS").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/TDS", true);
  xhttp.send();
}, 1000) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      if(this.responseText == "1") {
          document.getElementById("BUBBLER_STATE").checked = false;
      } else {
          document.getElementById("BUBBLER_STATE").checked = true;
      }
    }
  };
  xhttp.open("GET", "/bubbler_state", true);
  xhttp.send();
}, 1000) ;

</script>
</html>

)rawliteral";


float shared_temperature = 0;
float shared_TDS = 0;
bool bubble_status = false;

// ----------------- function prototypes ----------------
void setup_wifi();
float get_TDS();
float get_temperature();
void handle_measurements(float PPM, float temperature);
void handle_air_pump();
void set_bubbler(int state);

// ----------------- function definitions ---------------

void setup() {
  Serial.begin(115200);
  sensors.begin();
  pinMode(BUBBLER_PIN, OUTPUT);
  set_bubbler(LOW);
  setup_wifi();
}

String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATUREC"){
    return String(shared_temperature);
  }
  else if(var == "TDS"){
    return String(shared_TDS);
  } else if(var == "BUBBLER_STATE") {
    if(bubble_status) {
      return String("checked");
    } else {
      return String();
    }
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

  server.on("/temperaturec", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(shared_temperature).c_str());
  });

  server.on("/TDS", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(shared_TDS).c_str());
  });

  server.on("/bubbler_state", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(bubble_status).c_str());
  });

  server.on("/set_bubbler", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("state")) {
      Serial.print("Setting Bubbler to ");
      String state = request->getParam("state")->value();  
      int bubbler_status = state.toInt();
      Serial.println(bubble_status);
      set_bubbler(bubbler_status);
      request->send(200, "text/plain", "OK");
    } 
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
  shared_temperature = temperature;
  shared_TDS = PPM;
}

void handle_air_pump() {

}

void set_bubbler(int state) {
  digitalWrite(BUBBLER_PIN, state);
  bubble_status = state;
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
