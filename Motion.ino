#include <dummy.h>

#include <Wire.h> 
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// WiFi network variables
const char* ssid = "####";
const char* password = "####";

// MQTT broker and message variables
const char* mqtt_server = "10.0.0.151"; // Raspberry Pi IP address
int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);
char msg[50];

// MPU-6050 sensor variables
const uint8_t MPU_ADDR = 0x68; 
const size_t NUM_ADDR = 14;
const uint8_t scl = D1;
const uint8_t sda = D2;

int16_t temp;                       
int16_t x_accel, y_accel, z_accel, x_gyro, y_gyro, z_gyro;

float temp_scaled = 0;
float x_accel_scaled = 0, y_accel_scaled = 0, z_accel_scaled = 0, x_gyro_scaled = 0, y_gyro_scaled = 0, z_gyro_scaled = 0;
float total_accel_unit = 0;
float total_accel_scaled = 0;  
float ang_vel = 0;

// Accident detection variables
boolean is_accident = false; 
boolean flag1 = false, flag2 = false, flag3 = false;
byte count1 = 0, count2 = 0, count3 = 0;

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);  
  digitalWrite(BUILTIN_LED, LOW);
  Serial.begin(115200);

  // Set PWR_MGMT_1 register to wake up sensor
  Wire.begin(sda, scl);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  
  // Connect to WiFi and MQTT broker
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
    if (client.connect("ESP8266-ClientB")) {
      // Reconnect to the MQTT broker
      Serial.println("Connected");
      client.publish("to_rpi", "hello world");
      client.subscribe("from_rpi");
    } else {
      // Could not reconnect to the MQTT broker
      Serial.print("Failed, rc = ");
      Serial.print(client.state());
      Serial.println(", try again in 5 seconds");
      
      delay(5000);
    }
  }
}

void mpu_read() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);                                 // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);                      // the parameter indicates that the Arduino will send a restart. As a result, the connection is kept active.
  Wire.requestFrom(MPU_ADDR, NUM_ADDR, true);       // request a total of 14 registers

  x_accel = Wire.read() << 8 | Wire.read();  // reading registers 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  y_accel = Wire.read() << 8 | Wire.read();  // reading registers 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  z_accel = Wire.read() << 8 | Wire.read();  // reading registers 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  temp    = Wire.read() << 8 | Wire.read();  // reading registers 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  x_gyro  = Wire.read() << 8 | Wire.read();  // reading registers 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  y_gyro  = Wire.read() << 8 | Wire.read();  // reading registers 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  z_gyro  = Wire.read() << 8 | Wire.read();  // reading registers 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
}

void process_input() {
  x_accel_scaled = ((float) (x_accel + 386)) / 16384.00;
  y_accel_scaled = ((float) (y_accel + 312)) / 16384.00;
  z_accel_scaled = ((float) (z_accel - 869)) / 16384.00;

  temp_scaled = ((float) (temp / 340.00)) + 36.53;
  
  x_gyro_scaled = ((float) (x_gyro + 255)) / 131.07;
  y_gyro_scaled = ((float) (y_gyro + 1)) / 131.07;
  z_gyro_scaled = ((float) (z_gyro + 16)) / 131.07;

  /* To check individual acceleration components:
  Serial.print("X Acceleration: ");
  Serial.println(x_accel_scaled);
  Serial.print("y Acceleration: ");
  Serial.println(y_accel_scaled);
  Serial.print("z Acceleration: ");
  Serial.println(z_accel_scaled);
  */

  total_accel_scaled = (pow(pow(x_accel_scaled, 2) + pow(y_accel_scaled, 2) + pow(z_accel_scaled, 2), 0.5) * 10.0);
  Serial.print("Acceleration: ");
  Serial.println(total_accel_scaled);

  ang_vel = pow(pow(x_gyro_scaled, 2) + pow(y_gyro_scaled, 2) + pow(z_gyro_scaled, 2), 0.5);
  Serial.print("Angular velocity: ");
  Serial.println(ang_vel);
  Serial.println("-----------------");

  delay(100);
}

void detect_accident() {
  // Check total_accel_scaled breaks lower threshold
  if (total_accel_scaled <= 14 && flag2 == false) {
    flag1 = true;
    Serial.println("Low accel; flag (1)");
  }

  // Check total_accel_scaled exceeded upper threshold in the last 0.5s
  if (flag1 == true) {
    count1++;
    if (total_accel_scaled >= 25) {
      flag2 = true;      
      flag1 = false;
      count1 = 0;
      Serial.println("Significant Δaccel; flag (2)");
    }
  }

  // Check ang_vel changes dramatically (80-100 degC) in last 0.5s
  if (flag2 == true) {
    count2++;

    if (ang_vel >= 100 && ang_vel <= 250) {
      flag3 = true;
      flag2 = false;
      count2 = 0;
      Serial.println("Significant Δang_vel; flag (3)");
    }
  }

  // Check angular velocity changes little-to-none (0-10 deg) in last 5s and reset flag if change exceeds range
  if (flag3 == true) {
    count3++;
    if (count3 >= 50) {
      if ((ang_vel >= 0) && (ang_vel <= 12.5)) {
        is_accident = true;
        flag3 = false;
        count3 = 0;
      } else {
        flag3 = false;
        count3 = 0;
        Serial.println("User regained mobility; false alarm (3)");
      }
    }
  }

  // Detect accident and send message buffer to RPI
  if (is_accident) {  
    Serial.println("*ACCIDENT DETECTED*");
    snprintf(msg, sizeof(msg), "true");
    client.publish("is_accident", msg);
    is_accident = false;
  } 

  // Reset flag if ang_vel sustains high value for longer than 0.5s
  if (count2 > 5) {
    flag2 = false;
    count2 = 0;
    Serial.println("Non-momentary spike in angular velocity; false alarm (2)");
  }

  // Reset flag if total_accel_scaled sustains high value for longer than 0.5s
  if (count1 > 5) {
    flag1 = false;
    count1 = 0;
    Serial.println("Non-momentary spike in acceleration; false alarm (1)");
  }
}

void loop() {
  if (!client.connected())
    reconnect();

  client.loop();
  mpu_read();
  process_input();
  detect_accident();
}
