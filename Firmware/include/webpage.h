#pragma once

const char *htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Master Station</title>
  <style>
    body {font-family: Arial, sans-serif;text-align: center;background-color: #f4f4f4;}
    h1 {color: #4CAF50;}
    .container { width: 100%; margin: auto; background: white; border-radius: 10px;box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1);}
    .station { border: 2px solid #4CAF50; padding: 15px;  border-radius: 10px; background: #fff;margin-top: 20px; }
    .slider { width: 400px;}
    table {width: 100%; border-collapse: collapse; }
    td {  border: 1px solid #ddd; padding: 10px;text-align: center; font-size: 18px;}
    button {  padding: 10px 20px; margin: 10px; font-size: 16px; border: none; border-radius: 5px;cursor: pointer; }
    .btn-on { background-color: #4CAF50;color: white;}
    .btn-off {background-color: #f44336;color: white;}
    input[type='time'] { padding: 5px;font-size: 18px;}
  </style>
</head>
<body>
  <div class="container">
    <h1>GRADUATION PROJECT</h1>
    <h1>LIGHTING CONTROL AND MONITORING LORA APPLICATION AND WEBSERVER</h1>
    <div id="clock" style="position: absolute; top: 20px; left: 10px; font-size: 20px; font-weight: bold; background: white; padding: 5px; border-radius: 5px; box-shadow: 0px 0px 5px rgba(0,0,0,0.2);">Loading...</div>
    <div class='station'>
      <h2>GATEWAY</h2>
      <table>
        <tr>
          <td><strong>I1</strong><br><span id='sensor1'>--</span> A</td>
          <td><strong>I2</strong><br><span id='sensor2'>--</span> A</td>
          <td><strong>I3</strong><br><span id='sensor3'>--</span> A</td>
        </tr>
        <tr>
          <td><strong>V1</strong><br><span id='sensor4'>--</span> V</td>
          <td><strong>V2</strong><br><span id='sensor5'>--</span> V</td>
          <td><strong>V3</strong><br><span id='sensor6'>--</span> V</td>
        </tr>
      </table>
      <button class="btn-on" onclick="toggleAll('on')">Turn ON All</button>
      <button class="btn-off" onclick="toggleAll('off')">Turn OFF All</button>
      <p><label>Set ON Time: </label><input type='time' id='onTime'></p>
      <p><label>Set OFF Time: </label><input type='time' id='offTime'></p>
      <button onclick="setSchedule()">Set Schedule</button>
      <button onclick="resetSchedule()">Clear Schedule</button>
    </div>
  </div>
  %SLAVE_STATIONS%
  <script>
    // JavaScript functions (unchanged)
    function toggleAll(state) {
        fetch(`/toggleAll?state=${state}`)
        .then(response => response.text())
        .then(data => {
            for (let i = 1; i <= 10; i++)
            {
                document.getElementById(`status${i}`).innerText = (state === 'on') ? "ON" : "OFF";
                let newSliderValue = (state === 'on') ? 80 : 0;
                document.getElementById(`sliderValue${i}`).innerText = newSliderValue;
                document.getElementById(`slider${i}`).value = newSliderValue;
                fetch(`/slider?id=${i}&value=${newSliderValue}`).then(response => response.text());
            }
        });
    }
    function toggleStation(id, state) {
        fetch(`/toggle?id=${id}&state=${state}`).then(response => response.text())
        .then(data => {
            document.getElementById(`status${id}`).innerText = (state === 'on') ? "ON" : "OFF";
        });
    }
    function updateSlider(id, value) {
        document.getElementById(`sliderValue${id}`).innerText = value;
        fetch(`/slider?id=${id}&value=${value}`)
        .then(response => response.text())
        .then(() => {
            if (value > 80) {
                document.getElementById(`status${id}`).innerText = "ON";
            } else if (value < 10) {
                document.getElementById(`status${id}`).innerText = "OFF";
            }
        });
    }
    function setSchedule() {
        onTime = document.getElementById('onTime').value;
        offTime = document.getElementById('offTime').value;
        fetch(`/schedule?onTime=${onTime}&offTime=${offTime}`)
        .then(response => response.text())
        .then(data => {
            console.log("Schedule set: ON at " + onTime + ", OFF at " + offTime);
        });
    }
    function resetSchedule() {
        fetch('/resetSchedule')
        .then(response => response.json())
        .then(data => {
            document.getElementById('onTime').value = data.onTime;
            document.getElementById('offTime').value = data.offTime;
            console.log("Schedule reset successfully.");
        });
    }
    function fetchSlaveData() {
        fetch(`/getSlaveData`).then(response => response.json()).then(data => {
            data.forEach(slave => {
                document.getElementById(`status${slave.id}`).innerText = slave.isOn ? "ON" : "OFF";
                document.getElementById(`sliderValue${slave.id}`).innerText = slave.sliderValue;
                document.getElementById(`slider${slave.id}`).value = slave.sliderValue;
                document.getElementById(`time${slave.id}`).innerText = slave.timeString;
                document.getElementById(`temperature${slave.id}`).innerText = slave.temperature;
                document.getElementById(`connection${slave.id}`).innerText = slave.isConnected ? "Connected" : "Disconnected";
            });
        });
    }
    setInterval(fetchSlaveData, 2000);
    function fetchSensorData() {
        fetch(`/getSensorData`).then(response => response.json()).then(data => {
            document.getElementById("sensor1").innerText = data[0];
            document.getElementById("sensor2").innerText = data[1];
            document.getElementById("sensor3").innerText = data[2];
            document.getElementById("sensor4").innerText = data[3];
            document.getElementById("sensor5").innerText = data[4];
            document.getElementById("sensor6").innerText = data[5];
        });
    }
    setInterval(fetchSensorData, 2000);
    function updateClock() {
        fetch(`/getTime`).then(response => response.json()).then(data => {
            let timeString = `${data.hour.toString().padStart(2, '0')}:${data.minute.toString().padStart(2, '0')}:${data.second.toString().padStart(2, '0')} - ${data.day.toString().padStart(2, '0')}/${data.month.toString().padStart(2, '0')}/${data.year}`;
            document.getElementById("clock").innerText = timeString;
        }).catch(error => console.error("Lỗi khi lấy dữ liệu từ RTC:", error));
    }
    updateClock();
    setInterval(updateClock, 1000);
  </script>
</body>
</html>
)rawliteral";
