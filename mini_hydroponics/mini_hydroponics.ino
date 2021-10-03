#include <DallasTemperature.h>
#include <OneWire.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Hash.h>

#include <ArduinoOTA.h>

#include <NTPClient.h>

// full range at 1000ppm is 2.3v -> 2.3/3.3 = x/1024 or 2.3*1024/3.3 = 713.69
#define FULL_RANGE 2.3
#define FULL_RANGE_RAW (FULL_RANGE * 1024 / 3.3)
#define TDS_TO_PPM (1000.0 / FULL_RANGE_RAW)

// one wire setup
#define ONE_WIRE_BUS 13
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);



// pump setup
#define BUBBLER_PIN 2
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 60 * 60 * -7; // UTC -8 i.e. PST
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Hue Light setup
byte mac[6];

// wifi setup
const char *ssid = "NATural20-24g";
const char *password = "Foxland5!";
const char *mdns = "hydro";

AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
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
    .chart-container {width: 1000px; height: 600px;}
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

  <div>
    <canvas id="chart"></canvas>
  </div>

  <p>
    <h3 id=CLOCK>%TIME%</h3>
  </P
 
</body>
<script>
function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ 
      xhr.open("GET", "/set_pump?state=1", true); 
  } else { 
      xhr.open("GET", "/set_pump?state=0", true); 
  }
  xhr.send();
}

const data = {
  labels: %LABELS%,
  datasets: [{
    label: 'Temperatures',
    backgroundColor: 'rgb(255, 99, 132)',
    borderColor: 'rgb(255, 99, 0)',
    yAxisID:'A',
    data: %TEMPERATURE_ARRAY%,
  }, {
    label: 'TDSs',
    backgroundColor: 'rgb(255, 99, 132)',
    borderColor: 'rgb(255, 99, 132)',
    yAxisID:'B',
    data: %TDS_ARRAY%,
  }]
};

const config = {
  type:'line',
  data:data,
  options: {    
    responsive: true,
    //maintainAspectRatio: false,
    scales: {
      yAxes: [{
        id: 'A',
        type: 'linear',
        position: 'left',
      }, {
        id: 'B',
        type: 'linear',
        position: 'right',
      }]
    }
  }
};

var chart = new Chart(
  document.getElementById('chart'),
  config
);

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
          document.getElementById("BUBBLER_STATE").checked = true;
      } else {
          document.getElementById("BUBBLER_STATE").checked = false;
      }
    }
  };
  xhttp.open("GET", "/pump_state", true);
  xhttp.send();
}, 1000) ;

</script>
</html>

)rawliteral";

float shared_temperature = 0;
float shared_TDS = 0;
bool pump_status = true;
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
void handle_air_pump();
void set_pump(int state);

// ----------------- function definitions ---------------

void setup() {
  Serial.begin(115200);
  sensors.begin();
  pinMode(BUBBLER_PIN, OUTPUT);
  set_pump(0);
  setup_wifi();
}

String processor(const String &var) {
  if (var == "TEMPERATUREC") {
    return String(shared_temperature);
  } else if (var == "TDS") {
    return String(shared_TDS);
  } else if (var == "BUBBLER_STATE") {
    if (pump_status) {
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

void handle_air_pump() {
  timeClient.update();
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
    if(((timeClient.getEpochTime() - pump_disable_time) % 86400L) / 3600 > 8) {
      pump_status = 1;
    }
  }

}

void set_pump(int state) {
  digitalWrite(BUBBLER_PIN, !state); // control is inverted
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
