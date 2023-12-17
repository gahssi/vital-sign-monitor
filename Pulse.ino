#include <Wire.h>
#include "MAX30105.h"
#include <movingAvg.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Initialize sensor object
MAX30105 particleSensor;

// WiFi network variables
const char* ssid = "####";
const char* password = "####";

// MQTT broker variables
const char* mqtt_server = "10.0.0.151"; // Raspberry Pi IP address
int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);
char msg[50];

const uint32_t THRESHOLD_CONTACT = 7000;       // Threshold for detecting if a finger is placed
const int32_t THRESHOLD_START = 300;      // Threshold for detecting pulse initiation
const int32_t MIN_INIT = 9999999;   // Initial value for minimum
const int32_t MAX_INIT = 0;         // Initial value for maximum

// Range for displaying heart rate and SpO2 (adjustable based on application)
const uint32_t DISPLAY_MIN_HR   = 30;
const uint32_t DISPLAY_MAX_HR   = 180;
const uint32_t DISPLAY_MIN_SPO2 = 70;
const uint32_t DISPLAY_MAX_SPO2 = 100;

long last_time = millis();  // Storing the last pulse detection time
int32_t prev_ir_v = 0;      // Storing the last ir_v value
int32_t prev_diff = 0;      // Storing the last difference
long pulse_interval = -1;   // Pulse interval time

// Storing the maximum and minimum raw data values during one pulse
int32_t min_ir_v  = MIN_INIT, max_ir_v  = MAX_INIT;
int32_t min_red_v = MIN_INIT, max_red_v = MAX_INIT;

// Moving average values (IR_DC, RED_DC) for 30 (adjustable) samples
movingAvg avgIr_v(25);
movingAvg avgRed_v(25);

// Moving average values for heart rate (last 3 beats) and SpO2 (last 5 beats)
movingAvg avgHR(3);
movingAvg avgSPO2(3);

void setup() {
  Serial.begin(115200);
  Wire.begin(0, 2);

  while(!particleSensor.begin(Wire, I2C_SPEED_FAST))
  {
    Serial.println("MAX30102 was not found. Please check wiring/power. ");
  }
  Serial.println("OK!");

  byte ledBrightness = 0x1F;
  byte sampleAverage = 8;
  byte ledMode = 2; // enable Red + IR LEDs
  int sampleRate = 400;
  int pulseWidth = 411;
  int adcRange = 4096;

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor.enableDIETEMPRDY();

  // Read once as an initial value (using ir_v for pulse measurement)
  prev_ir_v =  particleSensor.getRed();
  
  // Initialize the pulse detection time
  last_time = millis();
  
  // Initialize moving average libraries
  avgIr_v.begin();
  avgRed_v.begin();
  avgHR.begin();
  avgSPO2.begin();

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String message = "";
  for (int i = 0; i < length; i++) {
    Serial.print((char) payload[i]);
    message += (char) payload[i];
  }
  Serial.println();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266-ClientA")) {
      Serial.println("Connected");
      client.subscribe("from_rpi");
    } else {
      Serial.print("Failed, rc = ");
      Serial.print(client.state());
      Serial.println(", try again in 5 seconds");

      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected())
    reconnect();

  client.loop();
  
  // The sensor readings are reversed. Check based on your sensor's orientation
  uint32_t red_v = particleSensor.getIR();
  uint32_t ir_v = particleSensor.getRed();

  float temp = particleSensor.readTemperature();

  // Check if a finger is placed
  if(red_v < THRESHOLD_CONTACT || ir_v < THRESHOLD_CONTACT) {
    Serial.println("Finger not detected");
    return;
  }

  // Calculate moving average values (IR_DC, RED_DC)
  double ir_v_dc = avgIr_v.reading(ir_v);
  double red_v_dc = avgRed_v.reading(red_v);

  // Update maximum and minimum values for IR and RED to calculate their AC components
  if(ir_v<min_ir_v) 
    min_ir_v = ir_v; 
  if(ir_v>max_ir_v) 
    max_ir_v = ir_v;
  if(red_v<min_red_v) 
    min_red_v = red_v; 
  if(red_v>max_red_v) 
    max_red_v = red_v;

  // Calculate the pulse difference from the previous value
  int32_t diff =  prev_ir_v - ir_v;

  // If the change is above THRESHOLD_START and the difference is positive, 
  // consider it as the initiation of a pulse
  if(prev_diff < THRESHOLD_START && diff > THRESHOLD_START){
    // Calculate the time of one pulse
    pulse_interval = millis() - last_time;
    last_time = millis();

    // Calculate heart rate based on the time of one pulse. Multiply by 1000 for fractional values
    double pulse = (double) avgHR.reading(60000*1000/pulse_interval) / 1000.0;

    // Calculate the AC components of IR and RED 
    int32_t ir_v_ac = max_ir_v-min_ir_v;
    int32_t red_v_ac = max_red_v-min_red_v;

    // Use the formula R = (AC_RED / DC_RED) / (AC_IR / DC_IR)
    double red_div = ((double) red_v_ac) / red_v_dc;
    double ir_div = ((double) ir_v_ac) / ir_v_dc;
    double R = red_div / ir_div;

    // Use the formula SPO2 = -45.060*R^2 + 30.354*R + 94.845 (taken from MAX30105.h::spo2_algorithm.cpp in github repo)
    // Multiply and divide by 1000 for fractional values
    double spo2 = (double) avgSPO2.reading((-45.060*R*R + 30.354*R + 94.845)*1000.0) / 1000.0;

    // Initialize maximum and minimum values
    min_ir_v = MIN_INIT;
    max_ir_v = MAX_INIT;
    min_red_v = MIN_INIT;
    max_red_v = MAX_INIT;

    // Display heart rate, SpO2, and temp 
    if(pulse <= DISPLAY_MAX_HR && pulse >= DISPLAY_MIN_HR && spo2 <= DISPLAY_MAX_SPO2 && spo2 >= DISPLAY_MIN_SPO2){    
      Serial.print("Pulse=");
      Serial.print(pulse);
      Serial.print("BPM, SpO2=");
      Serial.print(spo2);
      Serial.print("%, Temp=");
      Serial.print(temp);
      Serial.println("°C");
    }

    snprintf(msg, sizeof(msg), "%.1f", pulse);
    client.publish("pulse", msg);
    snprintf(msg, sizeof(msg), "%.1f", spo2);
    client.publish("spo2", msg);
    snprintf(msg, sizeof(msg), "%.1f", temp);
    client.publish("temp", msg);
  }

  // Store values for pulse detection
  prev_ir_v = ir_v;
  prev_diff = diff;

  delay(100);
}
