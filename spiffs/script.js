function sendRequest(url) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
    }
  };
  xhttp.open('GET', url, true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function updateBrightness(value) {
  document.getElementById('brightnessInput').value = value;
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
    }
  };
  xhttp.open('GET', '/set-brightness?value=' + value, true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function updateBrightnessInput(value) {
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  document.getElementById('brightnessSlider').value = value;
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
    }
  };
  xhttp.open('GET', '/set-brightness?value=' + value, true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function updateStairsSpeed(value) {
  document.getElementById('stairsSpeedInput').value = value;
  if (value < 10) value = 10;
  if (value > 100) value = 100;
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
    }
  };
  xhttp.open('GET', '/set-stairs-speed?value=' + value, true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function updateStairsSpeedInput(value) {
  if (value < 10) value = 10;
  if (value > 100) value = 100;
  document.getElementById('stairsSpeedSlider').value = value;
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
    }
  };
  xhttp.open('GET', '/set-stairs-speed?value=' + value, true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function updateLedCount(value) {
  document.getElementById('ledCountInput').value = value;
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
    }
  };
  xhttp.open('GET', '/set-led-count?value=' + value, true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function updateLedCountInput(value) {
  if (value < 1) value = 1;
  if (value > 1000) value = 1000;
  document.getElementById('ledCountSlider').value = value;
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
    }
  };
  xhttp.open('GET', '/set-led-count?value=' + value, true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function updateColor(value) {
  var r = parseInt(value.substr(1, 2), 16);
  var g = parseInt(value.substr(3, 2), 16);
  var b = parseInt(value.substr(5, 2), 16);
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
    }
  };
  xhttp.open('GET', '/set-color?r=' + r + '&g=' + g + '&b=' + b, true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function updateStairsGroupSize(value) {
  var buttons = document.querySelectorAll('.group-buttons button');
  buttons.forEach(function(btn) {
    btn.style.backgroundColor = '#333333';
    btn.style.color = '#00ffcc';
  });
  buttons[value - 1].style.backgroundColor = '#00ffcc';
  buttons[value - 1].style.color = '#1c1c1c';
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
    }
  };
  xhttp.open('GET', '/set-stairs-group-size?value=' + value, true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function saveParameters() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      alert(this.responseText);
    }
  };
  xhttp.open('GET', '/save-parameters', true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function eraseNetworkData() {
  if (confirm('Are you sure you want to erase network data?')) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        alert(this.responseText);
      }
    };
    xhttp.open('GET', '/erase-network-data', true);
    xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
    xhttp.send();
  }
}

function restartBoard() {
  if (confirm('Are you sure you want to restart the board?')) {
    sendRequest('/restart');
  }
}

function fetchSettings() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var settings = JSON.parse(this.responseText);
      document.getElementById('brightnessSlider').value = settings.brightness;
      document.getElementById('brightnessInput').value = settings.brightness;
      document.getElementById('stairsSpeedSlider').value = settings.stairs_speed;
      document.getElementById('stairsSpeedInput').value = settings.stairs_speed;
      document.getElementById('ledCountSlider').value = settings.led_count;
      document.getElementById('ledCountInput').value = settings.led_count;
      var rHex = ('0' + settings.color.r.toString(16)).slice(-2);
      var gHex = ('0' + settings.color.g.toString(16)).slice(-2);
      var bHex = ('0' + settings.color.b.toString(16)).slice(-2);
      var colorHex = '#' + rHex + gHex + bHex;
      document.getElementById('colorPicker').value = colorHex;
      updateStairsGroupSize(settings.stairs_group_size);
    }
  };
  xhttp.open('GET', '/get-settings', true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function fetchSunTimes() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
          var sunTimes = JSON.parse(this.responseText);
          document.getElementById('sunriseTime').innerText = sunTimes.sunrise;
          document.getElementById('sunsetTime').innerText = sunTimes.sunset;
      }
  };
  xhttp.open('GET', '/get-sun-times', true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function updateTime() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
          var response = JSON.parse(this.responseText);
          document.getElementById('currentTime').innerText = response.current_time;
          fetchSunTimes();
      }
  };
  xhttp.open('GET', '/get-time', true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function updateRegion() {
  var select = document.getElementById('regionSelect');
  var selectedOption = select.options[select.selectedIndex];
  var region = selectedOption.value;
  var timezone = selectedOption.getAttribute('data-timezone');

  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
          console.log(this.responseText);
          fetchTime();
          fetchSunTimes();
      }
  };
  xhttp.open('GET', '/set-regions?regions=' + encodeURIComponent(region) + '&timezone=' + encodeURIComponent(timezone), true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

function fetchRegions() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
          var regions = JSON.parse(this.responseText);
          var select = document.getElementById('regionSelect');
          select.innerHTML = '';
          regions.forEach(function(region) {
              var option = document.createElement('option');
              option.value = region.region;
              option.setAttribute('data-timezone', region.timezone);
              option.text = region.region;
              select.add(option);
          });
          for (var i = 0; i < regions.length; i++) {
              if (regions[i].region === 'Europe/Kyiv') {
                  select.selectedIndex = i;
                  break;
              }
          }
      }
  };
  xhttp.open('GET', '/get-regions', true);
  xhttp.setRequestHeader('Authorization', 'Basic YWRtaW46cGFzc3dvcmQ=');
  xhttp.send();
}

window.onload = function() {
  fetchSettings();
  fetchRegions();
  fetchSunTimes();
  updateTime();
  setInterval(updateTime, 1000);
};
