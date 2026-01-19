#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// WiFi AP Settings
const char* ssid = "NUCLEA-JAM";
const char* password = "Njam@123";

ESP8266WebServer server(80);

// Pin out for D1 Mini
#define RF_TX_PIN1 D2     // GPIO4 - First RF module
#define RF_TX_PIN2 D3     // GPIO0 - Second RF module
#define BUTTON_PIN D1     // GPIO5 - Single button
#define LED_PIN D4        // GPIO2 - Built-in LED (active LOW)

// RF Module States
bool rf1Active = false;
bool rf2Active = false;
bool systemActive = false;  // ADDED THIS LINE - Global system status
int currentMode = 0;  // 0=433MHz, 1=315MHz, 2=868MHz, 3=2.4GHz

// Frequency Settings
const char* frequencies[] = {"433 MHz", "315 MHz", "868 MHz", "2.4 GHz"};
const int freqValues[] = {433, 315, 868, 2400};

// Button State
bool lastButtonState = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// LED Blink Patterns
enum LEDPattern {
  LED_OFF,
  LED_SOLID,
  LED_SLOW_BLINK,
  LED_FAST_BLINK
};

LEDPattern currentLEDPattern = LED_OFF;
unsigned long lastLEDToggle = 0;
bool ledState = LOW;

// Transmission Parameters
unsigned long transmissionStartTime = 0;
const unsigned long MAX_TRANSMISSION_TIME = 60000; // 1 minute safety limit

// Function declarations
void handleRoot();
void handleStart();
void handleStop();
void handleToggleRF();
void handleSetMode();
void handleStatus();
void handleNotFound();
void updateLED();
void checkButton();
void toggleTransmission();
void checkTransmissionSafety();
void generateSignal();

// HTML Web Interface
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Dual RF Signal Jammer</title>
<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  background: linear-gradient(45deg, #0f0c29, #302b63, #24243e);
  background-size: 400% 400%;
  animation: gradientBG 15s ease infinite;
  min-height: 100vh;
  color: #fff;
  padding: 20px;
}

@keyframes gradientBG {
  0%, 100% { background-position: 0% 50%; }
  50% { background-position: 100% 50%; }
}

.container {
  max-width: 900px;
  margin: 0 auto;
}

.header {
  text-align: center;
  margin-bottom: 30px;
  padding: 25px;
  background: rgba(255, 255, 255, 0.1);
  backdrop-filter: blur(15px);
  border-radius: 20px;
  border: 2px solid rgba(255, 255, 255, 0.2);
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
}

.header h1 {
  font-size: 2.5em;
  background: linear-gradient(45deg, #00f5ff, #ff00ff);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  margin-bottom: 10px;
}

.subtitle {
  color: rgba(255, 255, 255, 0.7);
  font-size: 1.1em;
}

.card {
  background: rgba(255, 255, 255, 0.1);
  backdrop-filter: blur(15px);
  border-radius: 18px;
  padding: 25px;
  margin-bottom: 25px;
  border: 2px solid rgba(255, 255, 255, 0.2);
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
  transition: all 0.3s ease;
}

.card:hover {
  transform: translateY(-5px);
  box-shadow: 0 12px 40px rgba(0, 245, 255, 0.3);
}

.status-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 15px;
  margin-bottom: 25px;
}

@media (max-width: 768px) {
  .status-grid {
    grid-template-columns: repeat(2, 1fr);
  }
}

.status-item {
  background: rgba(0, 0, 0, 0.3);
  padding: 20px;
  border-radius: 12px;
  border: 2px solid rgba(0, 245, 255, 0.3);
  text-align: center;
  transition: all 0.3s ease;
}

.status-item.active {
  border-color: #00ff88;
  background: rgba(0, 255, 136, 0.1);
}

.status-label {
  color: #00f5ff;
  font-size: 1em;
  margin-bottom: 10px;
  text-transform: uppercase;
  letter-spacing: 1px;
}

.status-item.active .status-label {
  color: #00ff88;
}

.status-value {
  font-size: 1.8em;
  font-weight: 700;
  color: #fff;
}

.status-item.active .status-value {
  color: #00ff88;
  animation: pulse 1.5s infinite;
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.7; }
}

.rf-controls {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 20px;
  margin-bottom: 25px;
}

.rf-module {
  background: rgba(0, 0, 0, 0.2);
  padding: 20px;
  border-radius: 12px;
  border: 2px solid rgba(0, 245, 255, 0.2);
  text-align: center;
}

.rf-module.active {
  border-color: #ff0066;
  background: rgba(255, 0, 102, 0.1);
}

.rf-title {
  font-size: 1.4em;
  color: #00f5ff;
  margin-bottom: 15px;
}

.rf-module.active .rf-title {
  color: #ff0066;
}

.rf-toggle-btn {
  background: linear-gradient(135deg, #ff0066, #ff6b00);
  border: none;
  border-radius: 10px;
  padding: 15px 30px;
  color: white;
  font-size: 1.1em;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.3s ease;
  width: 100%;
}

.rf-toggle-btn:hover {
  transform: translateY(-3px);
  box-shadow: 0 8px 25px rgba(255, 0, 102, 0.4);
}

.rf-toggle-btn.off {
  background: linear-gradient(135deg, #00ff88, #00d4ff);
}

.rf-toggle-btn.off:hover {
  box-shadow: 0 8px 25px rgba(0, 255, 136, 0.4);
}

.frequency-selector {
  margin-bottom: 25px;
}

.selector-title {
  font-size: 1.4em;
  color: #00f5ff;
  margin-bottom: 20px;
  display: flex;
  align-items: center;
  gap: 10px;
}

.selector-title::before {
  content: '';
  width: 4px;
  height: 25px;
  background: #00f5ff;
  border-radius: 5px;
}

.modes-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
  gap: 15px;
}

.mode-btn {
  background: rgba(0, 245, 255, 0.15);
  border: 2px solid rgba(0, 245, 255, 0.3);
  border-radius: 12px;
  padding: 20px;
  text-align: center;
  cursor: pointer;
  transition: all 0.3s ease;
  color: #fff;
  font-size: 1.2em;
  font-weight: 600;
}

.mode-btn:hover {
  transform: translateY(-3px);
  background: rgba(0, 245, 255, 0.25);
  border-color: #00f5ff;
  box-shadow: 0 8px 25px rgba(0, 245, 255, 0.4);
}

.mode-btn.active {
  background: rgba(0, 255, 136, 0.25);
  border-color: #00ff88;
  color: #00ff88;
}

.control-buttons {
  display: flex;
  gap: 20px;
  margin-bottom: 25px;
}

.control-btn {
  flex: 1;
  padding: 20px;
  border: none;
  border-radius: 12px;
  font-size: 1.3em;
  font-weight: 700;
  cursor: pointer;
  transition: all 0.3s ease;
  text-transform: uppercase;
  letter-spacing: 1px;
}

.control-btn:active {
  transform: scale(0.95);
}

#startAllBtn {
  background: linear-gradient(135deg, #00ff88, #00d4ff);
  color: #000;
  box-shadow: 0 6px 20px rgba(0, 255, 136, 0.4);
}

#startAllBtn:hover {
  transform: translateY(-3px);
  box-shadow: 0 10px 30px rgba(0, 255, 136, 0.6);
}

#startAllBtn:disabled {
  background: #333;
  color: #666;
  cursor: not-allowed;
  box-shadow: none;
  transform: none;
}

#stopAllBtn {
  background: linear-gradient(135deg, #ff0066, #ff6b00);
  color: #fff;
  box-shadow: 0 6px 20px rgba(255, 0, 102, 0.4);
}

#stopAllBtn:hover {
  transform: translateY(-3px);
  box-shadow: 0 10px 30px rgba(255, 0, 102, 0.6);
}

#stopAllBtn:disabled {
  background: #333;
  color: #666;
  cursor: not-allowed;
  box-shadow: none;
  transform: none;
}

.physical-controls {
  background: rgba(0, 0, 0, 0.2);
  padding: 20px;
  border-radius: 12px;
  border: 2px solid rgba(255, 255, 255, 0.1);
  margin-bottom: 25px;
  text-align: center;
}

.physical-controls h3 {
  color: #00f5ff;
  margin-bottom: 15px;
}

.led-status {
  display: inline-flex;
  align-items: center;
  gap: 10px;
  padding: 10px 20px;
  background: rgba(0, 0, 0, 0.3);
  border-radius: 8px;
  margin-top: 10px;
}

.led-indicator {
  width: 20px;
  height: 20px;
  border-radius: 50%;
  background: #333;
  transition: all 0.3s ease;
}

.led-indicator.on {
  background: #00ff88;
  box-shadow: 0 0 10px #00ff88;
}

.led-indicator.blink {
  background: #ff0066;
  animation: blink 1s infinite;
}

@keyframes blink {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.3; }
}

.warning-box {
  background: rgba(255, 0, 102, 0.1);
  border: 2px solid rgba(255, 0, 102, 0.3);
  border-radius: 12px;
  padding: 20px;
  margin-top: 25px;
  color: #ff0066;
  font-size: 0.9em;
}

.warning-box h3 {
  color: #ff0066;
  margin-bottom: 10px;
  display: flex;
  align-items: center;
  gap: 10px;
}

.warning-box ul {
  margin-left: 20px;
  margin-top: 10px;
}

.warning-box li {
  margin-bottom: 8px;
}

.footer {
  text-align: center;
  margin-top: 40px;
  padding: 18px;
  background: rgba(255, 255, 255, 0.1);
  backdrop-filter: blur(10px);
  border-radius: 12px;
  color: rgba(255, 255, 255, 0.7);
}

.notification {
  position: fixed;
  top: 20px;
  right: 20px;
  padding: 18px 25px;
  border-radius: 12px;
  backdrop-filter: blur(15px);
  border: 2px solid;
  font-weight: 600;
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
  z-index: 1000;
  display: none;
  animation: slideIn 0.5s;
}

@keyframes slideIn {
  from { transform: translateX(400px); opacity: 0; }
  to { transform: translateX(0); opacity: 1; }
}

.success {
  background: rgba(0, 255, 136, 0.2);
  border-color: #00ff88;
  color: #00ff88;
}

.error {
  background: rgba(255, 0, 102, 0.2);
  border-color: #ff0066;
  color: #ff0066;
}

.info {
  background: rgba(0, 212, 255, 0.2);
  border-color: #00d4ff;
  color: #00d4ff;
}

@media (max-width: 768px) {
  .rf-controls {
    grid-template-columns: 1fr;
  }
  
  .control-buttons {
    flex-direction: column;
  }
  
  .modes-grid {
    grid-template-columns: 1fr;
  }
}
</style>
</head>
<body>
<div class="container">
<div class="header">
<h1> DUAL RF SIGNAL Jammer</h1>
<div class="subtitle">NUCLEA RF JAMMER</div>
</div>

<div class="card">
<div class="status-grid">
<div class="status-item" id="systemStatus">
<div class="status-label">System Status</div>
<div class="status-value" id="statusValue">STANDBY</div>
</div>
<div class="status-item" id="frequencyStatus">
<div class="status-label">Frequency</div>
<div class="status-value" id="freqValue">433 MHz</div>
</div>
<div class="status-item" id="ledStatus">
<div class="status-label">LED Status</div>
<div class="status-value" id="ledValue">OFF</div>
</div>
<div class="status-item" id="timeStatus">
<div class="status-label">Transmit Time</div>
<div class="status-value" id="timeValue">0s</div>
</div>
</div>

<div class="rf-controls">
<div class="rf-module" id="rfModule1">
<div class="rf-title">📡 RF MODULE 1</div>
<button class="rf-toggle-btn off" onclick="toggleRF(1)">ACTIVATE</button>
</div>
<div class="rf-module" id="rfModule2">
<div class="rf-title">📡 RF MODULE 2</div>
<button class="rf-toggle-btn off" onclick="toggleRF(2)">ACTIVATE</button>
</div>
</div>

<div class="physical-controls">
<h3>🎛️ PHYSICAL CONTROLS</h3>
<p>Button: Toggle RF Modules | LED: System Status</p>
<div class="led-status">
<div class="led-indicator" id="ledIndicator"></div>
<span id="ledDesc">System Ready</span>
</div>
</div>

<div class="frequency-selector">
<div class="selector-title">📡 SELECT FREQUENCY</div>
<div class="modes-grid" id="modeButtons">
<button class="mode-btn active" onclick="setMode(0)">433 MHz</button>
<button class="mode-btn" onclick="setMode(1)">315 MHz</button>
<button class="mode-btn" onclick="setMode(2)">868 MHz</button>
<button class="mode-btn" onclick="setMode(3)">2.4 GHz</button>
</div>
</div>

<div class="control-buttons">
<button class="control-btn" id="startAllBtn" onclick="startAll()">▶ START ALL</button>
<button class="control-btn" id="stopAllBtn" onclick="stopAll()" disabled>⏹ STOP ALL</button>
</div>

<div class="warning-box">
<h3>⚠️ IMPORTANT WARNING</h3>
<ul>
<li>For EDUCATIONAL and RESEARCH use only</li>
<li>Ensure you are in a shielded environment</li>
<li>Check local RF regulations before operation</li>
<li>Never aim at sensitive electronic equipment</li>
<li>Dual module operation increases power output</li>
</ul>
</div>
</div>

<div class="footer">
<p>Created by Nihal MP | NUCLEA RF JAMMER</p>
<p>WiFi: NUCLEA-JAM | IP: 192.168.4.1 | Button </p>
</div>
</div>

<div id="notification" class="notification"></div>

<script>
let currentMode = 0;
let rf1Active = false;
let rf2Active = false;
let systemActive = false;
let transmitTime = 0;
let transmitTimer = null;

function showNotification(message, type) {
const notification = document.getElementById('notification');
notification.textContent = message;
notification.className = 'notification ' + type;
notification.style.display = 'block';
setTimeout(() => {
notification.style.display = 'none';
}, 3000);
}

function setMode(mode) {
currentMode = mode;
const buttons = document.querySelectorAll('.mode-btn');
buttons.forEach((btn, index) => {
if(index === mode) {
btn.classList.add('active');
} else {
btn.classList.remove('active');
}
});
updateFrequencyDisplay();
fetch('/setmode?mode=' + mode)
.then(response => response.text())
.then(data => {
showNotification('Frequency set to ' + getFrequencyName(mode), 'info');
})
.catch(error => {
showNotification('Failed to set frequency', 'error');
});
}

function getFrequencyName(mode) {
const frequencies = ['433 MHz', '315 MHz', '868 MHz', '2.4 GHz'];
return frequencies[mode] || 'Unknown';
}

function updateFrequencyDisplay() {
document.getElementById('freqValue').textContent = getFrequencyName(currentMode);
}

function updateRFModuleDisplay() {
const rf1 = document.getElementById('rfModule1');
const rf2 = document.getElementById('rfModule2');
const btn1 = rf1.querySelector('.rf-toggle-btn');
const btn2 = rf2.querySelector('.rf-toggle-btn');
if(rf1Active) {
rf1.classList.add('active');
btn1.textContent = 'DEACTIVATE';
btn1.classList.remove('off');
} else {
rf1.classList.remove('active');
btn1.textContent = 'ACTIVATE';
btn1.classList.add('off');
}
if(rf2Active) {
rf2.classList.add('active');
btn2.textContent = 'DEACTIVATE';
btn2.classList.remove('off');
} else {
rf2.classList.remove('active');
btn2.textContent = 'ACTIVATE';
btn2.classList.add('off');
}
}

function updateSystemStatus() {
const statusItem = document.getElementById('systemStatus');
const timeItem = document.getElementById('timeStatus');
const statusValue = document.getElementById('statusValue');
const timeValue = document.getElementById('timeValue');
const startBtn = document.getElementById('startAllBtn');
const stopBtn = document.getElementById('stopAllBtn');
if(systemActive) {
statusItem.classList.add('active');
statusValue.textContent = 'ACTIVE';
startBtn.disabled = true;
stopBtn.disabled = false;
} else {
statusItem.classList.remove('active');
statusValue.textContent = 'STANDBY';
startBtn.disabled = false;
stopBtn.disabled = true;
}
timeValue.textContent = transmitTime + 's';
}

function updateLEDStatus(ledState, ledPattern) {
const ledIndicator = document.getElementById('ledIndicator');
const ledDesc = document.getElementById('ledDesc');
const ledValue = document.getElementById('ledValue');
ledIndicator.className = 'led-indicator';
if(ledPattern === 'off') {
ledValue.textContent = 'OFF';
ledDesc.textContent = 'System Ready';
} else if(ledPattern === 'solid') {
ledIndicator.classList.add('on');
ledValue.textContent = 'ON';
ledDesc.textContent = 'Transmitting';
} else if(ledPattern === 'blink') {
ledIndicator.classList.add('blink');
ledValue.textContent = 'BLINKING';
ledDesc.textContent = 'Jamming Active';
}
}

function toggleRF(module) {
fetch('/togglerf?module=' + module)
.then(response => response.json())
.then(data => {
rf1Active = data.rf1;
rf2Active = data.rf2;
systemActive = data.system;
updateRFModuleDisplay();
updateSystemStatus();
showNotification('RF Module ' + module + ' toggled', 'info');
})
.catch(error => {
showNotification('Failed to toggle RF module', 'error');
});
}

function startAll() {
fetch('/start')
.then(response => response.json())
.then(data => {
rf1Active = data.rf1;
rf2Active = data.rf2;
systemActive = data.system;
updateRFModuleDisplay();
updateSystemStatus();
showNotification('All RF modules activated', 'success');
})
.catch(error => {
showNotification('Failed to start transmission', 'error');
});
}

function stopAll() {
fetch('/stop')
.then(response => response.json())
.then(data => {
rf1Active = false;
rf2Active = false;
systemActive = false;
transmitTime = 0;
updateRFModuleDisplay();
updateSystemStatus();
showNotification('All RF modules deactivated', 'info');
})
.catch(error => {
showNotification('Failed to stop transmission', 'error');
});
}

function loadStatus() {
fetch('/status')
.then(response => response.json())
.then(data => {
currentMode = data.mode;
rf1Active = data.rf1;
rf2Active = data.rf2;
systemActive = data.system;
transmitTime = data.transmitTime;
updateFrequencyDisplay();
updateRFModuleDisplay();
updateSystemStatus();
updateLEDStatus(data.ledState, data.ledPattern);
const buttons = document.querySelectorAll('.mode-btn');
buttons.forEach((btn, index) => {
if(index === currentMode) {
btn.classList.add('active');
} else {
btn.classList.remove('active');
}
});
})
.catch(error => {
console.error('Failed to load status:', error);
});
}

// Start transmit timer
function startTransmitTimer() {
if(transmitTimer) clearInterval(transmitTimer);
transmitTimer = setInterval(() => {
if(systemActive) {
transmitTime++;
updateSystemStatus();
}
}, 1000);
}

// Stop transmit timer
function stopTransmitTimer() {
if(transmitTimer) {
clearInterval(transmitTimer);
transmitTimer = null;
}
}

// Initial load
loadStatus();
startTransmitTimer();
// Auto-refresh every 2 seconds
setInterval(loadStatus, 2000);
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

void handleStart() {
  rf1Active = true;
  rf2Active = true;
  systemActive = true;
  transmissionStartTime = millis();
  currentLEDPattern = LED_FAST_BLINK;
  
  Serial.println("=== ALL RF MODULES ACTIVATED ===");
  Serial.print("Frequency: ");
  Serial.println(frequencies[currentMode]);
  Serial.println("Both modules transmitting!");
  
  String json = "{";
  json += "\"rf1\":true,";
  json += "\"rf2\":true,";
  json += "\"system\":true";
  json += "}";
  server.send(200, "application/json", json);
}

void handleStop() {
  rf1Active = false;
  rf2Active = false;
  systemActive = false;
  digitalWrite(RF_TX_PIN1, LOW);
  digitalWrite(RF_TX_PIN2, LOW);
  currentLEDPattern = LED_OFF;
  
  Serial.println("=== ALL RF MODULES DEACTIVATED ===");
  
  String json = "{";
  json += "\"rf1\":false,";
  json += "\"rf2\":false,";
  json += "\"system\":false";
  json += "}";
  server.send(200, "application/json", json);
}

void handleToggleRF() {
  if(server.hasArg("module")) {
    int module = server.arg("module").toInt();
    if(module == 1) {
      rf1Active = !rf1Active;
      Serial.print("RF Module 1: ");
      Serial.println(rf1Active ? "ACTIVATED" : "DEACTIVATED");
    } else if(module == 2) {
      rf2Active = !rf2Active;
      Serial.print("RF Module 2: ");
      Serial.println(rf2Active ? "ACTIVATED" : "DEACTIVATED");
    }
    
    // Update system status
    systemActive = rf1Active || rf2Active;
    if(systemActive) {
      transmissionStartTime = millis();
      if(rf1Active && rf2Active) {
        currentLEDPattern = LED_FAST_BLINK;
      } else {
        currentLEDPattern = LED_SLOW_BLINK;
      }
    } else {
      currentLEDPattern = LED_OFF;
    }
    
    String json = "{";
    json += "\"rf1\":" + String(rf1Active ? "true" : "false") + ",";
    json += "\"rf2\":" + String(rf2Active ? "true" : "false") + ",";
    json += "\"system\":" + String(systemActive ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
  } else {
    server.send(400, "text/plain", "Missing module parameter");
  }
}

void handleSetMode() {
  if(server.hasArg("mode")) {
    int mode = server.arg("mode").toInt();
    if(mode >= 0 && mode < 4) {
      currentMode = mode;
      Serial.print("Frequency changed to: ");
      Serial.println(frequencies[currentMode]);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid mode");
    }
  } else {
    server.send(400, "text/plain", "Missing mode parameter");
  }
}

void handleStatus() {
  // Calculate transmission time
  unsigned long transmitTime = 0;
  if(systemActive) {
    transmitTime = (millis() - transmissionStartTime) / 1000;
  }
  
  // Determine LED pattern description
  String ledPatternStr;
  switch(currentLEDPattern) {
    case LED_OFF: ledPatternStr = "off"; break;
    case LED_SOLID: ledPatternStr = "solid"; break;
    case LED_SLOW_BLINK: ledPatternStr = "blink"; break;
    case LED_FAST_BLINK: ledPatternStr = "blink"; break;
  }
  
  String json = "{";
  json += "\"rf1\":" + String(rf1Active ? "true" : "false") + ",";
  json += "\"rf2\":" + String(rf2Active ? "true" : "false") + ",";
  json += "\"system\":" + String(systemActive ? "true" : "false") + ",";
  json += "\"mode\":" + String(currentMode) + ",";
  json += "\"frequency\":\"" + String(frequencies[currentMode]) + "\",";
  json += "\"transmitTime\":" + String(transmitTime) + ",";
  json += "\"ledState\":" + String(ledState ? "true" : "false") + ",";
  json += "\"ledPattern\":\"" + ledPatternStr + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void generateSignal() {
  if(!systemActive) return;
  
  int delayMicros = 1;
  
  switch(currentMode) {
    case 0: delayMicros = 1; break;    // 433 MHz
    case 1: delayMicros = 2; break;    // 315 MHz
    case 2: delayMicros = 1; break;    // 868 MHz
    case 3: delayMicros = 1; break;    // 2.4 GHz
  }
  
  // Generate burst on active modules
  for(int i = 0; i < 100; i++) {
    if(rf1Active) digitalWrite(RF_TX_PIN1, HIGH);
    if(rf2Active) digitalWrite(RF_TX_PIN2, HIGH);
    delayMicroseconds(delayMicros);
    
    if(rf1Active) digitalWrite(RF_TX_PIN1, LOW);
    if(rf2Active) digitalWrite(RF_TX_PIN2, LOW);
    delayMicroseconds(delayMicros);
  }
  
  delay(5);  // Small gap between bursts
}

void updateLED() {
  unsigned long currentMillis = millis();
  
  switch(currentLEDPattern) {
    case LED_OFF:
      digitalWrite(LED_PIN, HIGH);  // D1 Mini LED is active LOW
      ledState = LOW;
      break;
      
    case LED_SOLID:
      digitalWrite(LED_PIN, LOW);
      ledState = HIGH;
      break;
      
    case LED_SLOW_BLINK:
      if(currentMillis - lastLEDToggle >= 500) {  // 0.5 second interval
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? LOW : HIGH);
        lastLEDToggle = currentMillis;
      }
      break;
      
    case LED_FAST_BLINK:
      if(currentMillis - lastLEDToggle >= 100) {  // 0.1 second interval
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? LOW : HIGH);
        lastLEDToggle = currentMillis;
      }
      break;
  }
}

void checkButton() {
  int reading = digitalRead(BUTTON_PIN);
  
  if(reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if((millis() - lastDebounceTime) > debounceDelay) {
    if(reading != buttonState) {
      buttonState = reading;
      
      if(buttonState == LOW) {  // Button pressed (assuming pull-up)
        toggleTransmission();
      }
    }
  }
  
  lastButtonState = reading;
}

void toggleTransmission() {
  if(!systemActive) {
    // Activate both modules
    rf1Active = true;
    rf2Active = true;
    systemActive = true;
    transmissionStartTime = millis();
    currentLEDPattern = LED_FAST_BLINK;
    
    Serial.println("=== BUTTON: TRANSMISSION STARTED ===");
    Serial.println("Both RF modules activated via button");
  } else {
    // Deactivate both modules
    rf1Active = false;
    rf2Active = false;
    systemActive = false;
    digitalWrite(RF_TX_PIN1, LOW);
    digitalWrite(RF_TX_PIN2, LOW);
    currentLEDPattern = LED_OFF;
    
    Serial.println("=== BUTTON: TRANSMISSION STOPPED ===");
  }
}

void checkTransmissionSafety() {
  if(systemActive && (millis() - transmissionStartTime > MAX_TRANSMISSION_TIME)) {
    // Safety timeout reached
    rf1Active = false;
    rf2Active = false;
    systemActive = false;
    digitalWrite(RF_TX_PIN1, LOW);
    digitalWrite(RF_TX_PIN2, LOW);
    currentLEDPattern = LED_SLOW_BLINK;  // Alert pattern
    
    Serial.println("⚠️ SAFETY TIMEOUT: Transmission automatically stopped");
    Serial.println("Maximum transmission time (60s) exceeded");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  
  Serial.println("\n========================================");
  Serial.println("  DUAL RF SIGNAL GENERATOR");
  Serial.println("  ESP8266 D1 Mini with Two RF Modules");
  Serial.println("  Physical Button + LED Status");
  Serial.println("  Created by Nihal MP");
  Serial.println("========================================");
  
  // Initialize pins
  pinMode(RF_TX_PIN1, OUTPUT);
  pinMode(RF_TX_PIN2, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  // Ensure all outputs are off
  digitalWrite(RF_TX_PIN1, LOW);
  digitalWrite(RF_TX_PIN2, LOW);
  digitalWrite(LED_PIN, HIGH);  // LED off (active LOW)
  
  Serial.println("\nPin Configuration:");
  Serial.println("RF Module 1: GPIO4 (D2)");
  Serial.println("RF Module 2: GPIO0 (D3)");
  Serial.println("Button:      GPIO5 (D1) - Pull-up");
  Serial.println("LED:         GPIO2 (D4) - Active LOW");
  
  // Start WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("\nWiFi AP: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("IP Address: ");
  Serial.println(IP);
  Serial.println("Physical Button: Toggle RF Transmission");
  Serial.println("LED: OFF=Standby, SLOW BLINK=One Module, FAST BLINK=Both Modules");
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/togglerf", handleToggleRF);
  server.on("/setmode", handleSetMode);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("\nWeb server started!");
  Serial.println("Open: http://192.168.4.1");
  Serial.println("\nSystem ready - awaiting commands...\n");
}

void loop() {
  server.handleClient();
  checkButton();
  updateLED();
  checkTransmissionSafety();
  
  // Generate signal if active
  if(systemActive) {
    generateSignal();
  }
  
  delay(1);  // Small delay for stability
}