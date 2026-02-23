#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <LoRa.h>

// WiFi credentials
const char* ssid = "Alamak";
const char* password = "Alamak12e1";

// Web server
WebServer server(80);

// LoRa settings
const int csPin = 5;          // LoRa radio chip select
const int resetPin = 14;      // LoRa radio reset
const int irqPin = 2;         // LoRa hardware interrupt pin

// Conversion factor (approximate for sunlight)
const float LUX_TO_WM2 = 0.0126;

// Error thresholds
const float MIN_VOLTAGE = 0.1;
const float MAX_VOLTAGE = 20.0;
const float MIN_CURRENT = -500.0;
const float MAX_CURRENT = 5000.0;
const float MIN_POWER = -500.0;
const float MAX_POWER = 10000.0;

// Battery capacity estimation (adjust these based on your battery)
const float BATTERY_CAPACITY_MAH = 3600.0;  
const float BATTERY_FULL_VOLTAGE = 12.8;     // Full charge voltage
const float BATTERY_EMPTY_VOLTAGE = 10.5;    // Empty voltage
const float BATTERY_NOMINAL_VOLTAGE = 12.0;  // Nominal voltage

// Data history for graphs
const int MAX_DATA_POINTS = 50;
float solarVoltageHistory[MAX_DATA_POINTS] = {0};
float solarCurrentHistory[MAX_DATA_POINTS] = {0};
float solarPowerHistory[MAX_DATA_POINTS] = {0};
float batteryVoltageHistory[MAX_DATA_POINTS] = {0};
float batteryCurrentHistory[MAX_DATA_POINTS] = {0};
float batteryPowerHistory[MAX_DATA_POINTS] = {0};
float batteryCapacityHistory[MAX_DATA_POINTS] = {0};
float irradianceHistory[MAX_DATA_POINTS] = {0};
int rssiHistory[MAX_DATA_POINTS] = {0};
float snrHistory[MAX_DATA_POINTS] = {0};
unsigned long timestampHistory[MAX_DATA_POINTS] = {0}; // Add timestamp history
int dataIndex = 0;
bool historyInitialized = false;

struct SensorData {
  float solar_voltage = 0;
  float solar_current = 0;
  float solar_power = 0;
  float battery_voltage = 0;
  float battery_current = 0;
  float battery_power = 0;
  float light_level = 0;
  float solar_irradiance = 0;
  bool relay_state = false;
  int rssi = 0;
  float snr = 0;
  
  // ADD THESE NEW FIELDS FOR RF ANALYSIS
  int tx_power = 0;           // Transmitter power from the packet
  
  // Battery capacity estimates
  float battery_capacity_mah = 0;
  float battery_capacity_percent = 0;
  float battery_remaining_hours = 0;
  
  bool solar_voltage_error = false;
  bool solar_current_error = false;
  bool solar_power_error = false;
  bool battery_voltage_error = false;
  bool battery_current_error = false;
  bool battery_power_error = false;
  bool light_level_error = false;
  
  unsigned long last_update = 0;
  bool data_received = false;
};

SensorData sensorData;

// Function declarations
void processLoRaPacket();
void calculateBatteryCapacity();
void updateHistory();
void resetErrorFlags();
bool isValidVoltage(float voltage);
bool isValidCurrent(float current);
bool isValidPower(float power);
void displaySensorData();
String getLastUpdateString();
String getExactTimeString();
bool hasAnyError();
void handleRoot();
void handleData();
void handleHistory();
void handleStatus();

void setup() {
  Serial.begin(115200);
  
  // Initialize LoRa
  LoRa.setPins(csPin, resetPin, irqPin);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (1);
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/history", handleHistory);
  server.on("/status", handleStatus);
  
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    processLoRaPacket();
  }
}

void processLoRaPacket() {
  String received = "";
  while (LoRa.available()) {
    received += (char)LoRa.read();
  }
  
  sensorData.rssi = LoRa.packetRssi();
  sensorData.snr = LoRa.packetSnr();
  
  int index = 0;
  int nextIndex;
  
  resetErrorFlags();
  
  // Parse solar voltage
  nextIndex = received.indexOf(',', index);
  sensorData.solar_voltage = received.substring(index, nextIndex).toFloat();
  sensorData.solar_voltage_error = !isValidVoltage(sensorData.solar_voltage);
  index = nextIndex + 1;
  
  // Parse solar current
  nextIndex = received.indexOf(',', index);
  sensorData.solar_current = received.substring(index, nextIndex).toFloat();
  sensorData.solar_current_error = !isValidCurrent(sensorData.solar_current);
  index = nextIndex + 1;
  
  // Parse solar power
  nextIndex = received.indexOf(',', index);
  sensorData.solar_power = received.substring(index, nextIndex).toFloat();
  sensorData.solar_power_error = !isValidPower(sensorData.solar_power);
  index = nextIndex + 1;
  
  // Parse battery voltage
  nextIndex = received.indexOf(',', index);
  sensorData.battery_voltage = received.substring(index, nextIndex).toFloat();
  sensorData.battery_voltage_error = !isValidVoltage(sensorData.battery_voltage);
  index = nextIndex + 1;
  
  // Parse battery current
  nextIndex = received.indexOf(',', index);
  sensorData.battery_current = received.substring(index, nextIndex).toFloat();
  sensorData.battery_current_error = !isValidCurrent(sensorData.battery_current);
  index = nextIndex + 1;
  
  // Parse battery power
  nextIndex = received.indexOf(',', index);
  sensorData.battery_power = received.substring(index, nextIndex).toFloat();
  sensorData.battery_power_error = !isValidPower(sensorData.battery_power);
  index = nextIndex + 1;
  
  // Parse light level (lux)
  nextIndex = received.indexOf(',', index);
  sensorData.light_level = received.substring(index, nextIndex).toFloat();
  sensorData.light_level_error = (sensorData.light_level < 0 || sensorData.light_level > 100000);
  
  // Convert lux to W/m² (solar irradiance)
  sensorData.solar_irradiance = sensorData.light_level * LUX_TO_WM2;
  index = nextIndex + 1;
  
  // Parse relay state (light ON/OFF)
 sensorData.relay_state = !received.substring(index).toInt();
  
  // Calculate battery capacity estimates
  calculateBatteryCapacity();
  
  sensorData.last_update = millis();
  sensorData.data_received = true;
  
  // Update history for graphs
  updateHistory();
  
  displaySensorData();
}

void calculateBatteryCapacity() {
  // Method 1: Voltage-based SOC estimation (simple linear model)
  float voltage_soc = constrain(
    (sensorData.battery_voltage - BATTERY_EMPTY_VOLTAGE) / 
    (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE) * 100.0, 
    0.0, 100.0
  );
  
  // Method 2: Current-based capacity tracking (simplified)
  // This would normally require integration over time, but we'll do a simple estimate
  static float estimated_capacity_mah = BATTERY_CAPACITY_MAH * 0.8; // Start with 80% assumption
  
  // Simple discharge/charge estimation based on current
  if (sensorData.battery_current < 0) {
    // Discharging (current is negative in typical convention)
    estimated_capacity_mah += sensorData.battery_current * (10.0 / 3600.0); // Rough estimate for 10s interval
  } else if (sensorData.battery_current > 0) {
    // Charging
    estimated_capacity_mah += sensorData.battery_current * (10.0 / 3600.0); // Rough estimate for 10s interval
  }
  
  // Keep capacity within bounds
  estimated_capacity_mah = constrain(estimated_capacity_mah, 0, BATTERY_CAPACITY_MAH);
  
  // Use weighted average of both methods
  sensorData.battery_capacity_percent = (voltage_soc * 0.7) + ((estimated_capacity_mah / BATTERY_CAPACITY_MAH) * 100.0 * 0.3);
  sensorData.battery_capacity_percent = constrain(sensorData.battery_capacity_percent, 0.0, 100.0);
  
  sensorData.battery_capacity_mah = (sensorData.battery_capacity_percent / 100.0) * BATTERY_CAPACITY_MAH;
  
  // Calculate remaining hours based on current discharge rate
  if (sensorData.battery_current < 0 && sensorData.battery_capacity_mah > 0) {
    sensorData.battery_remaining_hours = sensorData.battery_capacity_mah / abs(sensorData.battery_current) / 1000.0;
  } else {
    sensorData.battery_remaining_hours = 0;
  }
}

void updateHistory() {
  solarVoltageHistory[dataIndex] = sensorData.solar_voltage;
  solarCurrentHistory[dataIndex] = sensorData.solar_current;
  solarPowerHistory[dataIndex] = sensorData.solar_power;
  batteryVoltageHistory[dataIndex] = sensorData.battery_voltage;
  batteryCurrentHistory[dataIndex] = sensorData.battery_current;
  batteryPowerHistory[dataIndex] = sensorData.battery_power;
  batteryCapacityHistory[dataIndex] = sensorData.battery_capacity_percent;
  irradianceHistory[dataIndex] = sensorData.solar_irradiance;
  rssiHistory[dataIndex] = sensorData.rssi;
  snrHistory[dataIndex] = sensorData.snr;
  timestampHistory[dataIndex] = millis(); // Store actual timestamp
  
  dataIndex = (dataIndex + 1) % MAX_DATA_POINTS;
  if (!historyInitialized && dataIndex == 0) historyInitialized = true;
}

void resetErrorFlags() {
  sensorData.solar_voltage_error = false;
  sensorData.solar_current_error = false;
  sensorData.solar_power_error = false;
  sensorData.battery_voltage_error = false;
  sensorData.battery_current_error = false;
  sensorData.battery_power_error = false;
  sensorData.light_level_error = false;
}

bool isValidVoltage(float voltage) {
  return (voltage >= MIN_VOLTAGE && voltage <= MAX_VOLTAGE);
}

bool isValidCurrent(float current) {
  return (current >= MIN_CURRENT && current <= MAX_CURRENT);
}

bool isValidPower(float power) {
  return (power >= MIN_POWER && power <= MAX_POWER);
}

void displaySensorData() {
  Serial.println("=== New Data Received ===");
  Serial.print("Solar Voltage: "); Serial.print(sensorData.solar_voltage, 2); 
  Serial.println(sensorData.solar_voltage_error ? " V ⚠️" : " V");
  
  Serial.print("Solar Current: "); Serial.print(sensorData.solar_current, 2); 
  Serial.println(sensorData.solar_current_error ? " mA ⚠️" : " mA");
  
  Serial.print("Solar Power: "); Serial.print(sensorData.solar_power, 2); 
  Serial.println(sensorData.solar_power_error ? " mW ⚠️" : " mW");
  
  Serial.print("Battery Voltage: "); Serial.print(sensorData.battery_voltage, 2); 
  Serial.println(sensorData.battery_voltage_error ? " V ⚠️" : " V");
  
  Serial.print("Battery Current: "); Serial.print(sensorData.battery_current, 2); 
  Serial.println(sensorData.battery_current_error ? " mA ⚠️" : " mA");
  
  Serial.print("Battery Power: "); Serial.print(sensorData.battery_power, 2); 
  Serial.println(sensorData.battery_power_error ? " mW ⚠️" : " mW");
  
  Serial.print("Battery Capacity: "); Serial.print(sensorData.battery_capacity_percent, 1); 
  Serial.print("% ("); Serial.print(sensorData.battery_capacity_mah, 0); Serial.println(" mAh)");
  
  Serial.print("Remaining Time: "); Serial.print(sensorData.battery_remaining_hours, 1); Serial.println(" hours");
  
  Serial.print("Light Level: "); Serial.print(sensorData.light_level, 2); 
  Serial.println(sensorData.light_level_error ? " lux ⚠️" : " lux");
  
  Serial.print("Solar Irradiance: "); Serial.print(sensorData.solar_irradiance, 2); Serial.println(" W/m²");
  
  Serial.print("Relay State: "); Serial.println(sensorData.relay_state ? "ON" : "OFF");
  
  Serial.print("RSSI: "); Serial.print(sensorData.rssi); Serial.println(" dBm");
  Serial.print("SNR: "); Serial.print(sensorData.snr, 1); Serial.println(" dB");
  Serial.print("Tx Power: "); Serial.print(sensorData.tx_power); Serial.println(" dBm");
  Serial.println("=========================");
}

String getLastUpdateString() {
  if (!sensorData.data_received) {
    return "Never";
  }
  
  unsigned long secondsAgo = (millis() - sensorData.last_update) / 1000;
  
  if (secondsAgo < 60) {
    return String(secondsAgo) + " seconds ago";
  } else if (secondsAgo < 3600) {
    return String(secondsAgo / 60) + " minutes ago";
  } else {
    return String(secondsAgo / 3600) + " hours ago";
  }
}

String getExactTimeString() {
  if (!sensorData.data_received) {
    return "Never";
  }
  
  unsigned long secondsAgo = (millis() - sensorData.last_update) / 1000;
  
  if (secondsAgo < 60) {
    return String(secondsAgo) + "s ago";
  } else if (secondsAgo < 3600) {
    return String(secondsAgo / 60) + "m " + String(secondsAgo % 60) + "s ago";
  } else {
    return String(secondsAgo / 3600) + "h " + String((secondsAgo % 3600) / 60) + "m ago";
  }
}

bool hasAnyError() {
  return sensorData.solar_voltage_error || sensorData.solar_current_error || 
         sensorData.solar_power_error || sensorData.battery_voltage_error ||
         sensorData.battery_current_error || sensorData.battery_power_error ||
         sensorData.light_level_error;
}

void handleRoot() {
  String html = R"=====(
<!DOCTYPE html><html><head>
<title>Solar Light Monitoring</title>
<meta http-equiv='refresh' content='10'>
<meta charset='UTF-8'>
<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { 
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  min-height: 100vh;
  color: #333;
}
.header {
  background: rgba(255,255,255,0.95);
  backdrop-filter: blur(10px);
  padding: 1rem 2rem;
  border-bottom: 1px solid rgba(255,255,255,0.2);
  box-shadow: 0 4px 6px rgba(0,0,0,0.1);
}
.header h1 {
  color: #2d3748;
  font-size: 1.8rem;
  font-weight: 700;
}
.header h2 {
  color: #4a5568;
  font-size: 0.9rem;
  font-weight: 400;
  letter-spacing: 2px;
}
.dashboard {
  padding: 2rem;
  max-width: 1400px;
  margin: 0 auto;
}
.welcome-card {
  background: rgba(255,255,255,0.95);
  border-radius: 16px;
  padding: 2rem;
  margin-bottom: 2rem;
  box-shadow: 0 8px 32px rgba(0,0,0,0.1);
  backdrop-filter: blur(10px);
  border: 1px solid rgba(255,255,255,0.2);
}
.welcome-card h3 {
  color: #2d3748;
  margin-bottom: 0.5rem;
  font-size: 1.4rem;
}
.welcome-card p {
  color: #718096;
  font-size: 1rem;
}
.stats-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: 1.5rem;
  margin-bottom: 2rem;
}
.stat-card {
  background: rgba(255,255,255,0.95);
  border-radius: 16px;
  padding: 1.5rem;
  box-shadow: 0 8px 32px rgba(0,0,0,0.1);
  backdrop-filter: blur(10px);
  border: 1px solid rgba(255,255,255,0.2);
  transition: transform 0.3s ease;
}
.stat-card:hover {
  transform: translateY(-5px);
}
.stat-card h4 {
  color: #4a5568;
  font-size: 0.9rem;
  margin-bottom: 0.5rem;
  text-transform: uppercase;
  letter-spacing: 1px;
}
.stat-value {
  color: #2d3748;
  font-size: 2rem;
  font-weight: 700;
  margin-bottom: 0.5rem;
}
.stat-unit {
  color: #718096;
  font-size: 0.9rem;
}
.battery-info {
  margin-top: 0.5rem;
  padding-top: 0.5rem;
  border-top: 1px solid #e2e8f0;
}
.battery-percentage {
  font-size: 1.5rem;
  font-weight: bold;
  color: #2d3748;
}
.battery-bar {
  width: 100%;
  height: 8px;
  background: #e2e8f0;
  border-radius: 4px;
  margin: 0.5rem 0;
  overflow: hidden;
}
.battery-fill {
  height: 100%;
  background: linear-gradient(90deg, #f56565, #48bb78);
  border-radius: 4px;
  transition: width 0.3s ease;
}
.relay-status {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-top: 0.5rem;
}
.status-badge {
  padding: 0.5rem 1rem;
  border-radius: 20px;
  font-weight: 600;
  font-size: 0.9rem;
}
.status-on {
  background: #4CAF50;
  color: white;
}
.status-off {
  background: #F44336;
  color: white;
}
.graphs-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(400px, 1fr));
  gap: 1.5rem;
  margin-bottom: 2rem;
}
.graph-card {
  background: rgba(255,255,255,0.95);
  border-radius: 16px;
  padding: 1.5rem;
  box-shadow: 0 8px 32px rgba(0,0,0,0.1);
  backdrop-filter: blur(10px);
  border: 1px solid rgba(255,255,255,0.2);
}
.graph-card h4 {
  color: #2d3748;
  margin-bottom: 1rem;
  font-size: 1.1rem;
}
.chart-container {
  position: relative;
  height: 250px;
  width: 100%;
}
.error-badge {
  background: #fed7d7;
  color: #c53030;
  padding: 0.25rem 0.75rem;
  border-radius: 20px;
  font-size: 0.8rem;
  font-weight: 600;
  display: inline-block;
  margin-left: 0.5rem;
}
.last-update {
  background: rgba(255,255,255,0.9);
  border-radius: 12px;
  padding: 1rem;
  text-align: center;
  font-size: 0.9rem;
  color: #4a5568;
  margin-top: 1rem;
}
@media (max-width: 768px) {
  .dashboard { padding: 1rem; }
  .graphs-grid { grid-template-columns: 1fr; }
  .stats-grid { grid-template-columns: 1fr; }
}
</style>
</head>
<body>
<div class='header'>
  <h1>HELFSHEIM</h1>
  <h2>SMART_LIGHT_MONITORING</h2>
</div>

<div class='dashboard'>
  <div class='welcome-card'>
    <h3>🌞 Monitoring Dashboard</h3>
    <p>Real-time PJU-TS monitoring system</p>
  </div>

  <div class='stats-grid'>
    <div class='stat-card'>
      <h4>Solar Voltage</h4>
      <div class='stat-value' id='solarVoltage'>0.00</div>
      <div class='stat-unit'>Volts</div>
    </div>
    <div class='stat-card'>
      <h4>Solar Current</h4>
      <div class='stat-value' id='solarCurrent'>0.00</div>
      <div class='stat-unit'>mA</div>
    </div>
    <div class='stat-card'>
      <h4>Solar Power</h4>
      <div class='stat-value' id='solarPower'>0.00</div>
      <div class='stat-unit'>mW</div>
    </div>
    <div class='stat-card'>
      <h4>Battery Voltage</h4>
      <div class='stat-value' id='batteryVoltage'>0.00</div>
      <div class='stat-unit'>Volts</div>
    </div>
    <div class='stat-card'>
      <h4>Battery Current</h4>
      <div class='stat-value' id='batteryCurrent'>0.00</div>
      <div class='stat-unit'>mA</div>
    </div>
    <div class='stat-card'>
      <h4>Battery Power</h4>
      <div class='stat-value' id='batteryPower'>0.00</div>
      <div class='stat-unit'>mW</div>
    </div>
    <div class='stat-card'>
      <h4>Battery Capacity</h4>
      <div class='battery-percentage' id='batteryPercent'>0%</div>
      <div class='battery-bar'>
        <div class='battery-fill' id='batteryFill' style='width: 0%'></div>
      </div>
      <div class='battery-info'>
        <div class='stat-unit' id='batteryCapacity'>0 mAh</div>
        <div class='stat-unit' id='batteryRemaining'>0.0 hours remaining</div>
      </div>
    </div>
    <div class='stat-card'>
      <h4>Light Intensity</h4>
      <div class='stat-value' id='lightLevel'>0.00</div>
      <div class='stat-unit'>lux</div>
    </div>
    <div class='stat-card'>
      <h4>Solar Irradiance</h4>
      <div class='stat-value' id='solarIrradiance'>0.00</div>
      <div class='stat-unit'>W/m²</div>
    </div>
    <div class='stat-card'>
      <h4>Light Relay</h4>
      <div class='stat-value'>💡</div>
      <div class='relay-status'>
        <span>State:</span>
        <span class='status-badge' id='relayState'>OFF</span>
      </div>
    </div>
    <div class='stat-card'>
      <h4>LoRa RSSI</h4>
      <div class='stat-value' id='rssi'>0</div>
      <div class='stat-unit'>dBm</div>
    </div>
    <div class='stat-card'>
      <h4>LoRa SNR</h4>
      <div class='stat-value' id='snr'>0.0</div>
      <div class='stat-unit'>dB</div>
    </div>
  </div>

  <div class='graphs-grid'>
    <!-- Solar Panel Graphs -->
    <div class='graph-card'>
      <h4>🔋 Solar Panel Voltage</h4>
      <div class='chart-container'>
        <canvas id='solarVoltageChart'></canvas>
      </div>
    </div>
    <div class='graph-card'>
      <h4>⚡ Solar Panel Current</h4>
      <div class='chart-container'>
        <canvas id='solarCurrentChart'></canvas>
      </div>
    </div>
    <div class='graph-card'>
      <h4>🔆 Solar Panel Power</h4>
      <div class='chart-container'>
        <canvas id='solarPowerChart'></canvas>
      </div>
    </div>
    
    <!-- Battery Graphs -->
    <div class='graph-card'>
      <h4>🔋 Battery Voltage</h4>
      <div class='chart-container'>
        <canvas id='batteryVoltageChart'></canvas>
      </div>
    </div>
    <div class='graph-card'>
      <h4>⚡ Battery Current</h4>
      <div class='chart-container'>
        <canvas id='batteryCurrentChart'></canvas>
      </div>
    </div>
    <div class='graph-card'>
      <h4>🔆 Battery Power</h4>
      <div class='chart-container'>
        <canvas id='batteryPowerChart'></canvas>
      </div>
    </div>
    
    <!-- Signal Quality Graphs -->
    <div class='graph-card'>
      <h4>📡 LoRa RSSI Signal Strength</h4>
      <div class='chart-container'>
        <canvas id='rssiChart'></canvas>
      </div>
    </div>
    <div class='graph-card'>
      <h4>📶 LoRa SNR Signal Quality</h4>
      <div class='chart-container'>
        <canvas id='snrChart'></canvas>
      </div>
    </div>
    <div class='graph-card'>
      <h4>🔋 Battery Capacity</h4>
      <div class='chart-container'>
        <canvas id='capacityChart'></canvas>
      </div>
    </div>
  </div>

  <div class='last-update'>
    Last Update: <span id='lastUpdate'>Never</span>
    <span id='errorIndicator'></span>
  </div>
</div>

<script>
let solarVoltageChart, solarCurrentChart, solarPowerChart;
let batteryVoltageChart, batteryCurrentChart, batteryPowerChart;
let rssiChart, snrChart, capacityChart;

function initCharts() {
  // Solar Voltage Chart
  solarVoltageChart = new Chart(document.getElementById('solarVoltageChart'), {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { 
          label: 'Solar Voltage (V)', 
          data: [], 
          borderColor: '#4CAF50', 
          backgroundColor: 'rgba(76,175,80,0.1)', 
          tension: 0.4,
          fill: true
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: { 
        y: { 
          beginAtZero: true,
          title: { display: true, text: 'Voltage (V)' }
        }
      }
    }
  });

  // Solar Current Chart
  solarCurrentChart = new Chart(document.getElementById('solarCurrentChart'), {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { 
          label: 'Solar Current (mA)', 
          data: [], 
          borderColor: '#2196F3', 
          backgroundColor: 'rgba(33,150,243,0.1)', 
          tension: 0.4,
          fill: true
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: { 
        y: { 
          beginAtZero: true,
          title: { display: true, text: 'Current (mA)' }
        }
      }
    }
  });

  // Solar Power Chart
  solarPowerChart = new Chart(document.getElementById('solarPowerChart'), {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { 
          label: 'Solar Power (mW)', 
          data: [], 
          borderColor: '#FF9800', 
          backgroundColor: 'rgba(255,152,0,0.1)', 
          tension: 0.4,
          fill: true
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: { 
        y: { 
          beginAtZero: true,
          title: { display: true, text: 'Power (mW)' }
        }
      }
    }
  });

  // Battery Voltage Chart
  batteryVoltageChart = new Chart(document.getElementById('batteryVoltageChart'), {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { 
          label: 'Battery Voltage (V)', 
          data: [], 
          borderColor: '#9C27B0', 
          backgroundColor: 'rgba(156,39,176,0.1)', 
          tension: 0.4,
          fill: true
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: { 
        y: { 
          beginAtZero: true,
          title: { display: true, text: 'Voltage (V)' }
        }
      }
    }
  });

  // Battery Current Chart
  batteryCurrentChart = new Chart(document.getElementById('batteryCurrentChart'), {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { 
          label: 'Battery Current (mA)', 
          data: [], 
          borderColor: '#191970', 
          backgroundColor: 'rgba(25,25,112,0.1)', 
          tension: 0.4,
          fill: true
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: { 
        y: { 
          beginAtZero: true,
          title: { display: true, text: 'Current (mA)' }
        }
      }
    }
  });

  // Battery Power Chart
  batteryPowerChart = new Chart(document.getElementById('batteryPowerChart'), {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { 
          label: 'Battery Power (mW)', 
          data: [], 
          borderColor: '#F44336', 
          backgroundColor: 'rgba(244,67,54,0.1)', 
          tension: 0.4,
          fill: true
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: { 
        y: { 
          beginAtZero: true,
          title: { display: true, text: 'Power (mW)' }
        }
      }
    }
  });

  // RSSI Chart
  rssiChart = new Chart(document.getElementById('rssiChart'), {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { 
          label: 'RSSI (dBm)', 
          data: [], 
          borderColor: '#607D8B', 
          backgroundColor: 'rgba(96,125,139,0.1)', 
          tension: 0.4,
          fill: true
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: { 
        y: { 
          reverse: false,
          suggestedMin: -120,
          suggestedMax: -40,
          title: { display: true, text: 'Signal Strength (dBm)' },
          ticks: {
            callback: function(value) {
              return value + ' dBm';
            }
          }
        }
      }
    }
  });

  // SNR Chart
  snrChart = new Chart(document.getElementById('snrChart'), {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { 
          label: 'SNR (dB)', 
          data: [], 
          borderColor: '#795548', 
          backgroundColor: 'rgba(121,85,72,0.1)', 
          tension: 0.4,
          fill: true
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: { 
        y: { 
          suggestedMin: -20,
          suggestedMax: 20,
          title: { display: true, text: 'Signal Quality (dB)' },
          ticks: {
            callback: function(value) {
              return value + ' dB';
            }
          }
        }
      }
    }
  });

  // Capacity Chart
  capacityChart = new Chart(document.getElementById('capacityChart'), {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { 
          label: 'Capacity (%)', 
          data: [], 
          borderColor: '#48bb78', 
          backgroundColor: 'rgba(72,187,120,0.1)', 
          tension: 0.4,
          fill: true
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: { 
        y: { 
          beginAtZero: true,
          max: 100,
          title: { display: true, text: 'State of Charge (%)' }
        }
      }
    }
  });
}

function updateCharts(history) {
  // Generate real time labels based on the current time and data points
  const now = new Date();
  const labels = [];
  
  // Create time labels going backwards from current time
  for (let i = history.solar_voltage.length - 1; i >= 0; i--) {
    const timeAgo = (history.solar_voltage.length - 1 - i) * 10; // Assuming 10-second intervals
    const time = new Date(now.getTime() - timeAgo * 1000);
    labels.unshift(time.toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'}));
  }
  
  // Update Solar Charts
  solarVoltageChart.data.labels = labels;
  solarVoltageChart.data.datasets[0].data = history.solar_voltage;
  solarVoltageChart.update('none');
  
  solarCurrentChart.data.labels = labels;
  solarCurrentChart.data.datasets[0].data = history.solar_current;
  solarCurrentChart.update('none');
  
  solarPowerChart.data.labels = labels;
  solarPowerChart.data.datasets[0].data = history.solar_power;
  solarPowerChart.update('none');
  
  // Update Battery Charts
  batteryVoltageChart.data.labels = labels;
  batteryVoltageChart.data.datasets[0].data = history.battery_voltage;
  batteryVoltageChart.update('none');
  
  batteryCurrentChart.data.labels = labels;
  batteryCurrentChart.data.datasets[0].data = history.battery_current;
  batteryCurrentChart.update('none');
  
  batteryPowerChart.data.labels = labels;
  batteryPowerChart.data.datasets[0].data = history.battery_power;
  batteryPowerChart.update('none');
  
  // Update Signal Charts
  rssiChart.data.labels = labels;
  rssiChart.data.datasets[0].data = history.rssi;
  rssiChart.update('none');
  
  snrChart.data.labels = labels;
  snrChart.data.datasets[0].data = history.snr;
  snrChart.update('none');
  
  // Update Capacity Chart
  capacityChart.data.labels = labels;
  capacityChart.data.datasets[0].data = history.battery_capacity;
  capacityChart.update('none');
}

async function updateData() {
  try {
    const response = await fetch('/data');
    const data = await response.json();
    
    // Update stat cards
    document.getElementById('solarVoltage').textContent = data.solar_voltage.toFixed(2);
    document.getElementById('solarCurrent').textContent = data.solar_current.toFixed(2);
    document.getElementById('solarPower').textContent = data.solar_power.toFixed(2);
    document.getElementById('batteryVoltage').textContent = data.battery_voltage.toFixed(2);
    document.getElementById('batteryCurrent').textContent = data.battery_current.toFixed(2);
    document.getElementById('batteryPower').textContent = data.battery_power.toFixed(2);
    document.getElementById('lightLevel').textContent = data.light_level.toFixed(2);
    document.getElementById('solarIrradiance').textContent = data.solar_irradiance.toFixed(2);
    document.getElementById('rssi').textContent = data.rssi;
    document.getElementById('snr').textContent = data.snr.toFixed(1);
    document.getElementById('lastUpdate').textContent = data.last_update_string;
    
    // Update battery capacity
    document.getElementById('batteryPercent').textContent = data.battery_capacity_percent.toFixed(1) + '%';
    document.getElementById('batteryFill').style.width = data.battery_capacity_percent.toFixed(1) + '%';
    document.getElementById('batteryCapacity').textContent = data.battery_capacity_mah.toFixed(0) + ' mAh';
    document.getElementById('batteryRemaining').textContent = data.battery_remaining_hours.toFixed(1) + ' hours remaining';
    
    // Update relay state
    const relayStateElement = document.getElementById('relayState');
    if (data.relay_state) {
      relayStateElement.textContent = 'ON';
      relayStateElement.className = 'status-badge status-on';
    } else {
      relayStateElement.textContent = 'OFF';
      relayStateElement.className = 'status-badge status-off';
    }
    
    // Update error indicator
    const errorIndicator = document.getElementById('errorIndicator');
    if (data.has_errors) {
      errorIndicator.innerHTML = '<span class="error-badge">⚠️ Errors Detected</span>';
    } else {
      errorIndicator.innerHTML = '';
    }

    // Get history and update charts
    const historyResponse = await fetch('/history');
    const history = await historyResponse.json();
    updateCharts(history);
    
  } catch (error) {
    console.error('Error updating data:', error);
  }
}

// Initialize charts and start updating data
document.addEventListener('DOMContentLoaded', function() {
  initCharts();
  updateData();
  setInterval(updateData, 5000); // Update every 5 seconds
});
</script>
</body></html>
)=====";
  
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"solar_voltage\":" + String(sensorData.solar_voltage, 2) + ",";
  json += "\"solar_current\":" + String(sensorData.solar_current, 2) + ",";
  json += "\"solar_power\":" + String(sensorData.solar_power, 2) + ",";
  json += "\"battery_voltage\":" + String(sensorData.battery_voltage, 2) + ",";
  json += "\"battery_current\":" + String(sensorData.battery_current, 2) + ",";
  json += "\"battery_power\":" + String(sensorData.battery_power, 2) + ",";
  json += "\"light_level\":" + String(sensorData.light_level, 2) + ",";
  json += "\"solar_irradiance\":" + String(sensorData.solar_irradiance, 2) + ",";
  json += "\"relay_state\":" + String(sensorData.relay_state ? "true" : "false") + ",";
  json += "\"battery_capacity_percent\":" + String(sensorData.battery_capacity_percent, 1) + ",";
  json += "\"battery_capacity_mah\":" + String(sensorData.battery_capacity_mah, 0) + ",";
  json += "\"battery_remaining_hours\":" + String(sensorData.battery_remaining_hours, 1) + ",";
  json += "\"rssi\":" + String(sensorData.rssi) + ",";
  json += "\"snr\":" + String(sensorData.snr, 1) + ",";
  json += "\"last_update\":" + String(sensorData.last_update) + ",";
  json += "\"last_update_string\":\"" + getLastUpdateString() + "\",";
  json += "\"has_errors\":" + String(hasAnyError() ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleHistory() {
  String json = "{";
  json += "\"solar_voltage\":[";
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    json += String(solarVoltageHistory[(dataIndex + i) % MAX_DATA_POINTS], 2);
    if (i < MAX_DATA_POINTS - 1) json += ",";
  }
  json += "],\"solar_current\":[";
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    json += String(solarCurrentHistory[(dataIndex + i) % MAX_DATA_POINTS], 2);
    if (i < MAX_DATA_POINTS - 1) json += ",";
  }
  json += "],\"solar_power\":[";
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    json += String(solarPowerHistory[(dataIndex + i) % MAX_DATA_POINTS], 2);
    if (i < MAX_DATA_POINTS - 1) json += ",";
  }
  json += "],\"battery_voltage\":[";
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    json += String(batteryVoltageHistory[(dataIndex + i) % MAX_DATA_POINTS], 2);
    if (i < MAX_DATA_POINTS - 1) json += ",";
  }
  json += "],\"battery_current\":[";
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    json += String(batteryCurrentHistory[(dataIndex + i) % MAX_DATA_POINTS], 2);
    if (i < MAX_DATA_POINTS - 1) json += ",";
  }
  json += "],\"battery_power\":[";
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    json += String(batteryPowerHistory[(dataIndex + i) % MAX_DATA_POINTS], 2);
    if (i < MAX_DATA_POINTS - 1) json += ",";
  }
  json += "],\"battery_capacity\":[";
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    json += String(batteryCapacityHistory[(dataIndex + i) % MAX_DATA_POINTS], 1);
    if (i < MAX_DATA_POINTS - 1) json += ",";
  }
  json += "],\"irradiance\":[";
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    json += String(irradianceHistory[(dataIndex + i) % MAX_DATA_POINTS], 2);
    if (i < MAX_DATA_POINTS - 1) json += ",";
  }
  json += "],\"rssi\":[";
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    json += String(rssiHistory[(dataIndex + i) % MAX_DATA_POINTS]);
    if (i < MAX_DATA_POINTS - 1) json += ",";
  }
  json += "],\"snr\":[";
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    json += String(snrHistory[(dataIndex + i) % MAX_DATA_POINTS], 1);
    if (i < MAX_DATA_POINTS - 1) json += ",";
  }
  json += "],\"timestamps\":[";
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    json += String(timestampHistory[(dataIndex + i) % MAX_DATA_POINTS]);
    if (i < MAX_DATA_POINTS - 1) json += ",";
  }
  json += "]}";
  
  server.send(200, "application/json", json);
}

void handleStatus() {
  String json = "{";
  json += "\"connected\":true,";
  json += "\"data_received\":" + String(sensorData.data_received ? "true" : "false") + ",";
  json += "\"last_update\":" + String(sensorData.last_update) + ",";
  json += "\"last_update_string\":\"" + getLastUpdateString() + "\",";
  json += "\"has_errors\":" + String(hasAnyError() ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}
