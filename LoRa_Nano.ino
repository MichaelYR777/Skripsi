#include <Wire.h>
#include <Adafruit_INA219.h>
#include <BH1750.h>
#include <SPI.h>
#include <LoRa.h>

// Sensor objects
Adafruit_INA219 ina219_solar(0x40);  // Solar panel INA219
Adafruit_INA219 ina219_battery(0x41); // Battery INA219
BH1750 lightMeter;

// LoRa settings
const int csPin = 10;          // LoRa radio chip select
const int resetPin = 9;        // LoRa radio reset
const int irqPin = 2;          // LoRa hardware interrupt pin

// ADD LORA TRANSMIT POWER SETTING
const int loraTxPower = 20;    // Transmit power in dBm (adjust as needed: 2, 5, 7, 10, 14, 17, 20)

// Relay control
const int relayPin = 5;
bool lightState = false;
float lightThreshold = 20.0; // Lux threshold for light control

// Conversion factor (approximate for sunlight)
const float LUX_TO_WM2 = 126.7; // Typical conversion factor for sunlight

// Calibration factors - YOU NEED TO ADJUST THESE VALUES
float solar_calibration_factor = 1.0;  // Start with 1.0 (no calibration)
float battery_calibration_factor = 1.0; // Start with 1.0 (no calibration)

// Calibration mode
bool calibration_mode = false; // Set to true when calibrating

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  // Initialize I2C bus
  Wire.begin();
  
  // Initialize sensors
  if (!ina219_solar.begin()) {
    Serial.println("Failed to find INA219 solar chip");
    while (1);
  }
  
  if (!ina219_battery.begin()) {
    Serial.println("Failed to find INA219 battery chip");
    while (1);
  }
  
  lightMeter.begin();
  
  // Initialize LoRa
  LoRa.setPins(csPin, resetPin, irqPin);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (1);
  }
  
  // SET LORA TRANSMIT POWER (ADD THIS LINE)
  LoRa.setTxPower(loraTxPower);
  
  // Initialize relay
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  // Print system information
  Serial.println("=== SOLAR LIGHT MONITORING TRANSMITTER ===");
  Serial.println("LoRa Frequency: 433 MHz");
  Serial.println("LoRa Tx Power: " + String(loraTxPower) + " dBm");
  Serial.println("=======================================");
  
  // Print calibration instructions
  if (calibration_mode) {
    Serial.println("=== INA219 CALIBRATION MODE ===");
    Serial.println("1. Connect a reference ammeter in series");
    Serial.println("2. Measure known current with reference meter");
    Serial.println("3. Calculate: new_factor = reference_current / measured_current");
    Serial.println("4. Update solar_calibration_factor and battery_calibration_factor");
    Serial.println("5. Set calibration_mode = false when done");
    Serial.println("=======================================");
  }
}

float getCalibratedCurrent(Adafruit_INA219 &sensor, float calibration_factor) {
  float raw_current = sensor.getCurrent_mA();
  return raw_current * calibration_factor;
}

float getCalibratedPower(Adafruit_INA219 &sensor, float calibration_factor) {
  float shunt_voltage = sensor.getShuntVoltage_mV();
  float bus_voltage = sensor.getBusVoltage_V();
  float voltage = bus_voltage + (shunt_voltage / 1000);
  float current = getCalibratedCurrent(sensor, calibration_factor);
  return voltage * current; // Power in mW
}

void loop() {
  // Read sensor data
  float solar_shunt = ina219_solar.getShuntVoltage_mV();
  float solar_voltage = ina219_solar.getBusVoltage_V() + (solar_shunt / 1000);
  float solar_current_raw = ina219_solar.getCurrent_mA();
  float solar_current_calibrated = getCalibratedCurrent(ina219_solar, solar_calibration_factor);
  float solar_power_calibrated = getCalibratedPower(ina219_solar, solar_calibration_factor);
  
  float battery_shunt = ina219_battery.getShuntVoltage_mV();
  float battery_voltage = ina219_battery.getBusVoltage_V() + (battery_shunt / 1000);
  float battery_current_raw = ina219_battery.getCurrent_mA();
  float battery_current_calibrated = getCalibratedCurrent(ina219_battery, battery_calibration_factor);
  float battery_power_calibrated = getCalibratedPower(ina219_battery, battery_calibration_factor);
  
  float lux = lightMeter.readLightLevel();
  float solar_irradiance = lux * LUX_TO_WM2;

  // Control light based on ambient light
  if (lux < lightThreshold && !lightState) {
    digitalWrite(relayPin, HIGH);  // Turn light ON
    lightState = true;              // Update state to ON
    Serial.println("Light turned ON");
  } else if (lux >= lightThreshold && lightState) {
    digitalWrite(relayPin, LOW);   // Turn light OFF
    lightState = false;             // Update state to OFF
    Serial.println("Light turned OFF");
  }
  
  // Prepare LoRa packet - ADD TX POWER AS LAST FIELD
  String data = String(solar_voltage) + "," + 
                String(solar_current_calibrated) + "," + 
                String(solar_power_calibrated) + "," +
                String(battery_voltage) + "," + 
                String(battery_current_calibrated) + "," + 
                String(battery_power_calibrated) + "," +
                String(lux) + "," + 
                String(solar_irradiance) + "," + 
                String(lightState) + "," +
                String(loraTxPower);  // ADD TX POWER AS LAST FIELD
  
  // Send data via LoRa
  LoRa.beginPacket();
  LoRa.print(data);
  LoRa.endPacket();
  
  // Print to serial for debugging
  if (calibration_mode) {
    Serial.println("=== CALIBRATION MODE ===");
    Serial.println("Solar - Raw I: " + String(solar_current_raw) + "mA, Calibrated I: " + String(solar_current_calibrated) + "mA");
    Serial.println("Battery - Raw I: " + String(battery_current_raw) + "mA, Calibrated I: " + String(battery_current_calibrated) + "mA");
    Serial.println("Update factors: solar_calibration_factor = " + String(solar_calibration_factor));
    Serial.println("Update factors: battery_calibration_factor = " + String(battery_calibration_factor));
    Serial.println("=========================");
  } else {
    Serial.println("=== TRANSMISSION ===");
    Serial.println("Tx Power: " + String(loraTxPower) + " dBm");
    Serial.println("Solar - V: " + String(solar_voltage, 2) + "V, I: " + String(solar_current_calibrated, 2) + "mA, P: " + String(solar_power_calibrated, 2) + "mW");
    Serial.println("Battery - V: " + String(battery_voltage, 2) + "V, I: " + String(battery_current_calibrated, 2) + "mA, P: " + String(battery_power_calibrated, 2) + "mW");
    Serial.println("Light: " + String(lux, 2) + " lx (" + String(solar_irradiance, 2) + " W/m²), Light State: " + String(lightState));
    Serial.println();
  }
  
  delay(5000); // Send data every 5 seconds
}
