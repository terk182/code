#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoNvs.h>
#include <config.h>
#include <kame.h>
#include <gamepad.h>


Kame robot;
WebServer server(80);

int joy_x = 0;
int joy_y = 0;
int calibration[8] = {0, 0, 0, 0, 0, 0, 0, 0};

String wifi_ssid = SSID;
String wifi_password = PASSWORD;
String wifi_hostname = HOSTNAME;

String setup_html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <title>WiFi Setup</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
    body {
        background-color: #121212;
        color: #f0f0f0;
        font-family: Arial, sans-serif;
        padding: 20px;
        text-align: center;
    }
    h1 { color: #ff3535; }
    .form-box {
        background: #1e1e1e;
        padding: 30px;
        border-radius: 12px;
        box-shadow: 0 4px 12px rgba(0,0,0,0.5);
        max-width: 400px;
        margin: 30px auto;
    }
    .form-group {
        margin: 20px 0;
        text-align: left;
    }
    label {
        display: block;
        margin-bottom: 8px;
        font-size: 1.1em;
    }
    input[type="text"], input[type="password"] {
        width: 100%;
        padding: 12px;
        border: none;
        border-radius: 8px;
        background: #2a2a2a;
        color: #f0f0f0;
        font-size: 1em;
        box-sizing: border-box;
    }
    .btn {
        font-size: 1.2em;
        text-decoration: none;
        padding: 12px 24px;
        margin: 10px;
        background: #ff3535;
        color: white;
        border-radius: 8px;
        transition: background 0.3s;
        display: inline-block;
        cursor: pointer;
        border: none;
    }
    .btn:hover { background: #ff5959; }
    .nav-btn { background: #444; }
    .nav-btn:hover { background: #666; }
    .status {
        margin-top: 20px;
        padding: 15px;
        border-radius: 8px;
        background: #2a2a2a;
    }
    .success { color: #4caf50; }
    .info { color: #2196f3; }
    </style>
</head>
<body>
    <h1>WiFi Setup</h1>
    <a class="btn nav-btn" href="/">Back to Gamepad</a>
    <div class="form-box">
        <div class="form-group">
            <label>SSID (Access Point Name)</label>
            <input type="text" id="ssid" placeholder="Enter SSID">
        </div>
        <div class="form-group">
            <label>Password</label>
            <input type="password" id="password" placeholder="Enter Password (min 8 chars)">
        </div>
        <div class="form-group">
            <label>Hostname</label>
            <input type="text" id="hostname" placeholder="Enter Hostname">
        </div>
        <button class="btn" onclick="saveWifi()">Save & Restart</button>
        <button class="btn nav-btn" onclick="loadWifi()">Load Current</button>
        <div class="status" id="status"></div>
    </div>
    <script>
        function loadWifi() {
            fetch('/wifi_get').then(r=>r.json()).then(d=>{
                document.getElementById('ssid').value = d.ssid;
                document.getElementById('password').value = d.password;
                document.getElementById('hostname').value = d.hostname;
                document.getElementById('status').innerHTML = '<span class="info">Loaded current settings</span>';
            });
        }
        function saveWifi() {
            const ssid = document.getElementById('ssid').value;
            const password = document.getElementById('password').value;
            const hostname = document.getElementById('hostname').value;
            if(ssid.length < 1) {
                document.getElementById('status').innerHTML = '<span style="color:#f44336">SSID cannot be empty</span>';
                return;
            }
            if(password.length < 8) {
                document.getElementById('status').innerHTML = '<span style="color:#f44336">Password must be at least 8 characters</span>';
                return;
            }
            fetch('/wifi_save?ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(password)+'&hostname='+encodeURIComponent(hostname))
            .then(r=>r.json()).then(d=>{
                if(d.success) {
                    document.getElementById('status').innerHTML = '<span class="success">Saved! Restarting in 3 seconds...</span>';
                    setTimeout(()=>{ location.reload(); }, 5000);
                }
            });
        }
        loadWifi();
    </script>
</body>
</html>
)rawliteral";

String calibration_html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <title>Calibration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
    body {
        background-color: #121212;
        color: #f0f0f0;
        font-family: Arial, sans-serif;
        padding: 20px;
        text-align: center;
    }
    h1 {
        color: #ff3535;
    }
    .servo-grid {
        display: grid;
        grid-template-columns: repeat(4, 1fr);
        gap: 20px;
        margin: 30px 0;
    }
    .servo-box {
        background: #1e1e1e;
        padding: 20px;
        border-radius: 12px;
        box-shadow: 0 4px 12px rgba(0,0,0,0.5);
    }
    .servo-title {
        margin-bottom: 10px;
        font-size: 1.1em;
    }
    .value {
        font-size: 1.5em;
        margin: 10px 0;
    }
    .btn {
        font-size: 1.5em;
        text-decoration: none;
        padding: 8px 14px;
        margin: 5px;
        background: #ff3535;
        color: white;
        border-radius: 8px;
        transition: background 0.3s;
        display: inline-block;
        cursor: pointer;
        border: none;
    }
    .btn:hover {
        background: #ff5959;
    }
    pre {
        background: #1e1e1e;
        padding: 10px;
        border-radius: 8px;
        color: #f0f0f0;
        font-size: 1.1em;
    }
    .nav-btn {
        background: #444;
        margin: 10px;
    }
    .nav-btn:hover {
        background: #666;
    }
    </style>
</head>
<body>
    <h1>Calibration</h1>
    <a class="btn nav-btn" href="/">Back to Gamepad</a>
    <div class="servo-grid" id="servo-grid"></div>
    <button class="btn" style="width: 180px; margin: 0 10px;" onclick="loadCal()">Load</button>
    <button class="btn" style="width: 180px; margin: 0 10px;" onclick="saveCal()">Save</button>
    <h3 style="margin-top: 50px;">Generated calibration array</h3>
    <pre id="cal-array">int servo_calibration[8] = {0, 0, 0, 0, 0, 0, 0, 0};</pre>
    <script>
        let cal = [0,0,0,0,0,0,0,0];
        function updateUI() {
            let grid = document.getElementById('servo-grid');
            grid.innerHTML = '';
            for(let i=0; i<8; i++) {
                grid.innerHTML += `
                <div class="servo-box">
                    <div class="servo-title">Servo ${i}</div>
                    <button class="btn" onclick="increase(${i})">&#9650;</button>
                    <div class="value" id="val${i}">${cal[i]}</div>
                    <button class="btn" onclick="decrease(${i})">&#9660;</button>
                </div>`;
            }
            document.getElementById('cal-array').textContent = 
                'int servo_calibration[8] = {' + cal.join(', ') + '};';
        }
        function increase(i) {
            fetch('/cal_increase?i='+i).then(r=>r.json()).then(d=>{ cal=d.cal; updateUI(); });
        }
        function decrease(i) {
            fetch('/cal_decrease?i='+i).then(r=>r.json()).then(d=>{ cal=d.cal; updateUI(); });
        }
        function loadCal() {
            fetch('/cal_load').then(r=>r.json()).then(d=>{ cal=d.cal; updateUI(); });
        }
        function saveCal() {
            fetch('/cal_save').then(r=>r.json()).then(d=>{ cal=d.cal; updateUI(); });
        }
        fetch('/cal_get').then(r=>r.json()).then(d=>{ cal=d.cal; updateUI(); });
    </script>
</body>
</html>
)rawliteral";

float progress = 0;
float period = 600;  // SG90: ปรับช้าลงเพื่อถนอม motor
float leg_spread = 20;
float body_height = 10;
float body_shift = 0;
float step_amplitude = 0;
float step_height = 20;

float phase_linear[] =  {90,  90,  270, 90,  270, 270, 90,  270};
float phase_angular[] = {90,  270, 270, 90,  270, 90,  90,  270};
float phase[] =         {0,   0,   0,   0,   0,   0,   0,   0};


void handleRoot() {
    server.send(200, "text/html", gamepad_html);
}

void handleCalibration() {
    server.send(200, "text/html", calibration_html);
}

void updateServoCalibration(int i) {
    if (i >= 0 && i < 8) {
        robot.setCalibration(calibration);
        robot.setServo(i, 90);
    }
}

void sendCalibrationJson() {
    String json = "{\"cal\":[";
    for (int i = 0; i < 8; i++) {
        json += String(calibration[i]);
        if (i < 7) json += ",";
    }
    json += "]}";
    server.send(200, "application/json", json);
}

void handleCalGet() {
    sendCalibrationJson();
}

void handleCalIncrease() {
    int i = server.arg("i").toInt();
    if (i >= 0 && i < 8) {
        calibration[i]++;
        updateServoCalibration(i);
    }
    sendCalibrationJson();
}

void handleCalDecrease() {
    int i = server.arg("i").toInt();
    if (i >= 0 && i < 8) {
        calibration[i]--;
        updateServoCalibration(i);
    }
    sendCalibrationJson();
}

void handleCalLoad() {
    if (NVS.begin()) {
        for (int i = 0; i < 8; i++) {
            calibration[i] = NVS.getInt("servo" + String(i), 0);
            updateServoCalibration(i);
        }
    }
    sendCalibrationJson();
}

void handleCalSave() {
    if (NVS.begin()) {
        for (int i = 0; i < 8; i++) {
            NVS.setInt("servo" + String(i), calibration[i]);
        }
    }
    sendCalibrationJson();
}

void handleSetup() {
    server.send(200, "text/html", setup_html);
}

void handleWifiGet() {
    String json = "{\"ssid\":\"" + wifi_ssid + "\",";
    json += "\"password\":\"" + wifi_password + "\",";
    json += "\"hostname\":\"" + wifi_hostname + "\"}";
    server.send(200, "application/json", json);
}

void handleWifiSave() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
        wifi_ssid = server.arg("ssid");
        wifi_password = server.arg("password");
        wifi_hostname = server.arg("hostname");
        
        if (NVS.begin()) {
            NVS.setString("wifi_ssid", wifi_ssid);
            NVS.setString("wifi_pass", wifi_password);
            NVS.setString("wifi_host", wifi_hostname);
        }
        
        server.send(200, "application/json", "{\"success\":true}");
        
        delay(3000);
        ESP.restart();
    } else {
        server.send(400, "application/json", "{\"success\":false}");
    }
}

void handleJoystick() {
    if (server.hasArg("x") && server.hasArg("y")) {
        joy_x = server.arg("x").toInt();
        joy_y = server.arg("y").toInt();
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing x or y");
    }
}

void handleButton() {
    if (server.hasArg("label")) {
        String label = server.arg("label");
        Serial.println(label);
        if (label == "A") {
            robot.hello();
        } else if (label == "B") {
            robot.jump();
        } else if (label == "C") {
            robot.pushUp(4, 1000);
        } else if (label == "X") {
            robot.dance(2, 1000);
        } else if (label == "Y") {
            robot.moonwalkL(2, 2000);
        } else if (label == "Z") {
            robot.frontBack(2, 1000);
        } else if (label == "Start") {
            robot.arm();
        } else if (label == "Stop") {
            robot.disarm();
        } else {
            Serial.println("Unknown button: " + label);
        }
        server.send(200, "text/plain", "Button OK");
    } else {
        server.send(400, "text/plain", "Missing label");
    }
}


void setup() {
    Serial.begin(115200);
    
    // โหลดค่าจาก NVS
    if (NVS.begin()) {
        // โหลด WiFi settings
        String saved_ssid = NVS.getString("wifi_ssid");
        String saved_pass = NVS.getString("wifi_pass");
        String saved_host = NVS.getString("wifi_host");
        
        if (saved_ssid.length() > 0) wifi_ssid = saved_ssid;
        if (saved_pass.length() >= 8) wifi_password = saved_pass;
        if (saved_host.length() > 0) wifi_hostname = saved_host;
        
        // โหลด calibration
        for (int i = 0; i < 8; i++) {
            calibration[i] = NVS.getInt("servo" + String(i), 0);
        }
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifi_ssid.c_str(), wifi_password.c_str());
    MDNS.begin(wifi_hostname.c_str());
    
    Serial.println("AP SSID: " + wifi_ssid);
    Serial.println("Hostname: " + wifi_hostname);
    
    robot.init();
    robot.setCalibration(calibration);
    robot.home();

    server.on("/", handleRoot);
    server.on("/joystick", handleJoystick);
    server.on("/button", handleButton);
    server.on("/cal", handleCalibration);
    server.on("/cal_get", handleCalGet);
    server.on("/cal_increase", handleCalIncrease);
    server.on("/cal_decrease", handleCalDecrease);
    server.on("/cal_load", handleCalLoad);
    server.on("/cal_save", handleCalSave);
    server.on("/setup", handleSetup);
    server.on("/wifi_get", handleWifiGet);
    server.on("/wifi_save", handleWifiSave);
    server.begin();
}

void loop() {
    server.handleClient();

    progress += robot.oscillator[0].getPhaseProgress();
    while (progress > 360)
        progress -= 360;

    if (abs(joy_x) > 0.0 || abs(joy_y) > 0.0) {
        if (abs(joy_y) >= abs(joy_x)) {
            // Linear movement
            step_amplitude = joy_y * 0.25;
            body_shift = step_amplitude * 0.8;

            phase[0] = phase_linear[0] + progress;
            phase[1] = phase_linear[1] + progress;
            phase[2] = phase_linear[2] + 2 * progress;
            phase[3] = phase_linear[3] + 2 * progress;
            phase[4] = phase_linear[4] + progress;
            phase[5] = phase_linear[5] + progress;
            phase[6] = phase_linear[6] + 2 * progress;
            phase[7] = phase_linear[7] + 2 * progress;
        } else {
            // Angular movement
            step_amplitude = joy_x * 0.25;
            body_shift = 0.0;

            phase[0] = phase_angular[0] + progress;
            phase[1] = phase_angular[1] + progress;
            phase[2] = phase_angular[2] + 2 * progress;
            phase[3] = phase_angular[3] + 2 * progress;
            phase[4] = phase_angular[4] + progress;
            phase[5] = phase_angular[5] + progress;
            phase[6] = phase_angular[6] + 2 * progress;
            phase[7] = phase_angular[7] + 2 * progress;
        }

        for (int i = 0; i < 8; i++) {
            robot.oscillator[i].setPhase(phase[i]);
            robot.oscillator[i].reset();
        }

        robot.oscillator[0].setAmplitude(step_amplitude);
        robot.oscillator[1].setAmplitude(step_amplitude);
        robot.oscillator[4].setAmplitude(step_amplitude);
        robot.oscillator[5].setAmplitude(step_amplitude);

        robot.oscillator[2].setAmplitude(step_height);
        robot.oscillator[3].setAmplitude(step_height);
        robot.oscillator[6].setAmplitude(step_height);
        robot.oscillator[7].setAmplitude(step_height);

        robot.oscillator[0].setOffset(90 + leg_spread - body_shift);
        robot.oscillator[1].setOffset(90 - leg_spread + body_shift);
        robot.oscillator[4].setOffset(90 - leg_spread - body_shift);
        robot.oscillator[5].setOffset(90 + leg_spread + body_shift);

        robot.oscillator[2].setOffset(90 - body_height);
        robot.oscillator[3].setOffset(90 + body_height);
        robot.oscillator[6].setOffset(90 + body_height);
        robot.oscillator[7].setOffset(90 - body_height);

        robot.oscillator[0].setPeriod(period);
        robot.oscillator[1].setPeriod(period);
        robot.oscillator[4].setPeriod(period);
        robot.oscillator[5].setPeriod(period);

        robot.oscillator[2].setPeriod(period / 2);
        robot.oscillator[3].setPeriod(period / 2);
        robot.oscillator[6].setPeriod(period / 2);
        robot.oscillator[7].setPeriod(period / 2);

        robot.setServo(0, robot.oscillator[0].refresh());
        robot.setServo(1, robot.oscillator[1].refresh());
        robot.setServo(4, robot.oscillator[4].refresh());
        robot.setServo(5, robot.oscillator[5].refresh());

        if (progress < 180) {
            robot.setServo(3, robot.oscillator[3].refresh());
            robot.setServo(6, robot.oscillator[6].refresh());
        } else {
            robot.setServo(2, robot.oscillator[2].refresh());
            robot.setServo(7, robot.oscillator[7].refresh());
        }
    }
    else{
        robot.home();
    }
}
