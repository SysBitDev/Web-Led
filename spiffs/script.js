function sendRequest(url) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
    }
  };
  xhttp.open('GET', url, true);
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
    xhttp.send();
  }
}
function restartBoard() {
  if (confirm('Are you sure you want to restart the board?')) {
    sendRequest('/restart');
  }
}
function toggleIgnoreSun() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
            var response = JSON.parse(this.responseText);
            var ignoreSunButton = document.getElementById('ignoreSunButton');
            if (response.ignore_sun) {
                ignoreSunButton.textContent = 'Ignore Sun: ON';
            } else {
                ignoreSunButton.textContent = 'Ignore Sun: OFF';
            }
            alert(response.message);
        }
    };
    xhttp.open('GET', '/toggle-ignore-sun', true);
    xhttp.send();
}

function fetchCurrentTime() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
          try {
              var response = JSON.parse(this.responseText);
              document.getElementById('currentTime').textContent = response.current_time;
          } catch (e) {
              console.error("Failed to parse JSON:", e);
          }
      }
  };
  xhttp.open('GET', '/current-time', true);
  xhttp.send();
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

        var ignoreSunButton = document.getElementById('ignoreSunButton');
        if (settings.ignore_sun) {
            ignoreSunButton.textContent = 'Ignore Sun: ON';
        } else {
            ignoreSunButton.textContent = 'Ignore Sun: OFF';
        }
      }
    };
    xhttp.open('GET', '/get-settings', true);
    xhttp.send();
}


window.onload = function() {
  fetchSettings();
  fetchCurrentTime();
  setInterval(fetchCurrentTime, 1000);
};
