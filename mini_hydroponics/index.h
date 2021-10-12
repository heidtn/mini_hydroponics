
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
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

			h2 {
				font-size: 3.0rem;
			}

			p {
				font-size: 3.0rem;
			}

			.units {
				font-size: 1.2rem;
			}

			.ds-labels {
				font-size: 1.5rem;
				vertical-align: middle;
				padding-bottom: 15px;
			}

			.switch {
				position: relative;
				display: inline-block;
				width: 120px;
				height: 68px
			}

			.switch input {
				display: none
			}

			.slider {
				position: absolute;
				top: 0;
				left: 0;
				right: 0;
				bottom: 0;
				background-color: #ccc;
				border-radius: 6px
			}

			.slider:before {
				position: absolute;
				content: "";
				height: 52px;
				width: 52px;
				left: 8px;
				bottom: 8px;
				background-color: #fff;
				-webkit-transition: .4s;
				transition: .4s;
				border-radius: 3px
			}

			input:checked + .slider {
				background-color: #b30000
			}

			input:checked + .slider:before {
				-webkit-transform: translateX(52px);
				-ms-transform: translateX(52px);
				transform: translateX(52px)
			}

			.chart-container {
				width: 1000px;
				height: 600px;
			}

			.row {
				display: flex;
			}

			.column {
				flex: 50%;
			}
		</style>
	</head>

	<body>
		<h2>Mini Hydroponics Server</h2>
		<p>
			<i class="fas fa-thermometer-half" style="color:#059e8a;"></i>
			<span class="ds-labels">Temperature Celsius</span>
			<span id="temperaturec">23.00</span>
			<sup class="units">&deg;C</sup>
		</p>
		<p>
			<i class="fas fa-bolt" style="color:#059e8a;"></i>
			<span class="ds-labels">TDS in PPM</span>
			<span id="TDS">927.56</span>
		</p>

		<div class="row">
			<div class="column">
				<p>
					<i class="fas fa-water" style="color:#059e8a;"></i>
				</p>

				<label class="switch">
					<input type="checkbox" onchange="togglePump(this)" id=PUMP_STATE checked>
					<span class="slider"></span>
				</label>
			</div>

			<div class="column">
				<p>
					<i class="fas fa-lightbulb" style="color:#059e8a;"></i>
				</p>
				<label class="switch">
					<input type="checkbox" onchange="toggleLight(this)" id=LIGHT_STATE checked>
					<span class="slider"></span>
				</label>
			</div>
		</div>

		<div>
			<canvas id="chart"></canvas>
		</div>

		<p>
			<h3 id=CLOCK>39</h3>
		</P </body>


  <div>
    <canvas id="chart"></canvas>
  </div>

  <p>
    <h3 id=CLOCK>%TIME%</h3>
  </P
 
</body>
<script>
function togglePump(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ 
      xhr.open("GET", "/set_pump?state=1", true); 
  } else { 
      xhr.open("GET", "/set_pump?state=0", true); 
  }
  xhr.send();
}

function toggleLight(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ 
      xhr.open("GET", "/set_light?state=1", true); 
  } else { 
      xhr.open("GET", "/set_light?state=0", true); 
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
          document.getElementById("PUMP_STATE").checked = true;
      } else {
          document.getElementById("PUMP_STATE").checked = false;
      }
    }
  };
  xhttp.open("GET", "/pump_state", true);
  xhttp.send();
}, 500) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      if(this.responseText == "1") {
          document.getElementById("LIGHT_STATE").checked = true;
      } else {
          document.getElementById("LIGHT_STATE").checked = false;
      }
    }
  };
  xhttp.open("GET", "/light_state", true);
  xhttp.send();
}, 500) ;

</script>
</html>

)rawliteral";