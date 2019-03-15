#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Servo.h>
#include <SPI.h>
#include <WiFi101.h>
#include "wifi_ssid.h"

#define DHTPIN     6
#define DHTTYPE    DHT22     // DHT 22 (AM2302)
#define VBATPIN    A7

DHT_Unified dht(DHTPIN, DHTTYPE);

Servo myservo;

// Control and feedback pins
int servoPin = 10;
int feedbackPin = A1;

// Calibration values
int minDegrees;
int maxDegrees;
int minFeedback;
int maxFeedback;
int tolerance = 2; // max feedback measurement error

// Please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;            // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;
bool ready_to_grasp = false;


// Initialize the WiFi client library
WiFiSSLClient client;

// server address:
char server[] = "us-central1-fir-authdemo2-13925.cloudfunctions.net";
// IPAddress server(64,131,82,241);

unsigned long lastConnectionTime = 0;            // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 10L * 1000L; // delay between updates, in milliseconds

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  WiFi.setPins(8, 7, 4, 2);
  WiFi.lowPowerMode();
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  //  while (!Serial) {
  //    ; // wait for serial port to connect. Needed for native USB port only
  //  }

  //  // check for the presence of the shield:
  //  if (WiFi.status() == WL_NO_SHIELD) {
  //    Serial.println("WiFi shield not present");
  //    // don't continue:
  //    while (true);
  //  }
  dht.begin();

  connectWiFi();

  // servo setup
  myservo.attach(servoPin);
  calibrate(myservo, feedbackPin, 0, 180);  // calibrate for the 20-160 degree range
  // you're connected now, so print out the status:
  //  printWiFiStatus();
  digitalWrite(LED_BUILTIN, LOW);
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
void loop() {
  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:

  bool ready_to_print = false;
  String buffer_string = "";

  //  while (client.available()) {
  //    char c = client.read();
  //    buffer_string += c;
  //    ready_to_print = true;
  //  }

  while (client.available()) {
    buffer_string = client.readString();
    Serial.println(buffer_string);
    ready_to_print = true;
  }

  if (ready_to_print && ready_to_grasp) {
    String temp = buffer_string.substring(buffer_string.indexOf("close") + 7);
    int target_temp = temp.toInt();
    int servo_pos = target_temp * 2;
    myservo.write(servo_pos);
    Serial.print("Servo Pos: ");
    Serial.println(servo_pos);
    ready_to_print = false;
    digitalWrite(LED_BUILTIN, LOW);
    ready_to_grasp = false;
  }

  long rssi = WiFi.RSSI();

  if (WiFi.status() != WL_CONNECTED) {
    status = WL_IDLE_STATUS;
    connectWiFi();
  }



  // if ten seconds have passed since your last connection,
  // then connect again and send data:
  if (millis() - lastConnectionTime > postingInterval) {
    Serial.print("RSSI: ");
    Serial.print(rssi);
    Serial.println(" dBm");
    readBat();
    httpRequest();
  }

}

// this method makes a HTTP connection to the server:
void httpRequest() {
  //Measure temp and humidity
  sensors_event_t event;
  float bat, t, h;
  // Get temperature event and print its value.
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
//    Serial.print(F("Temperature: "));
//    Serial.print(event.temperature);
//    Serial.println(F(" C"));
    t = event.temperature * 1.4 + 32;
  }


  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
//    Serial.print(F("Humidity: "));
//    Serial.print(event.relative_humidity);
//    Serial.println(F("%"));
    h = event.relative_humidity;
  }

  bat = readBatPercent();
  String stringT = String(int(t));
  String stringH = String(int(h));
  String stringBat = String(int(bat));
  Serial.println("temp: " + stringT);
  Serial.println("hum: " + stringH);
  Serial.println("bat: " + stringBat);


  // close any connection before send a new request.
  // This will free the socket on the WiFi shield





  ////////////////////SET TEMP//////////////////////////////////////////
  digitalWrite(LED_BUILTIN, HIGH);
  client.stop();

  // if there's a successful connection:
  if (client.connect(server, 443)) {
    client.println("GET /setTemp?roomId=Kitchen&current_temp=" + stringT + " HTTP/1.1");
    client.println("Host: us-central1-fir-authdemo2-13925.cloudfunctions.net");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();

    // note the time that the connection was made:
    lastConnectionTime = millis();
  }
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
  }

  ////////////////////SET HUMIDITY////////////////////////////////////////
  digitalWrite(LED_BUILTIN, HIGH);
  client.stop();
  if (client.connect(server, 443)) {
    client.println("GET /setHumidity?roomId=Kitchen&current_humidity=" + stringH + " HTTP/1.1");
    client.println("Host: us-central1-fir-authdemo2-13925.cloudfunctions.net");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();

    // note the time that the connection was made:
    lastConnectionTime = millis();
  }
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
  }



  ////////////////////GET HUMIDITY////////////////////////////////////////
  digitalWrite(LED_BUILTIN, HIGH);
  client.stop();
  if (client.connect(server, 443)) {
    client.println("GET /getHumidity?roomId=Kitchen HTTP/1.1");
    client.println("Host: us-central1-fir-authdemo2-13925.cloudfunctions.net");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();

    // note the time that the connection was made:
    lastConnectionTime = millis();
  }
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
  }

  ////////////////////SET V_BAT////////////////////////////////////////
  digitalWrite(LED_BUILTIN, HIGH);
  client.stop();
  if (client.connect(server, 443)) {
    client.println("GET /setBatteryPercent?roomId=Kitchen&battery_percent=" + stringBat + " HTTP/1.1");
    client.println("Host: us-central1-fir-authdemo2-13925.cloudfunctions.net");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();

    // note the time that the connection was made:
    lastConnectionTime = millis();
  }
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
  }


  ////////////////////GET TEMP////////////////////////////////////////
  digitalWrite(LED_BUILTIN, HIGH);
  client.stop();
  if (client.connect(server, 443)) {
    client.println("GET /getTemp?roomId=Kitchen HTTP/1.1");
    client.println("Host: us-central1-fir-authdemo2-13925.cloudfunctions.net");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();

    // note the time that the connection was made:
    lastConnectionTime = millis();
    ready_to_grasp = true;
  }
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
  }
}

void connectWiFi() {
  // attempt to connect to WiFi network:
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait for good signal strength
    //    long rssi = 0;

    //    while (rssi == 0) {
    //      rssi = WiFi.RSSI();
    //      Serial.print("RSSI: ");
    //      Serial.print(rssi);
    //      Serial.println(" dBm");
    //      delay(1000);
    //    }

    delay(5000);
  }
}


void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void calibrate(Servo servo, int analogPin, int minPos, int maxPos)
{
  // Move to the minimum position and record the feedback value
  servo.write(minPos);
  minDegrees = minPos;
  delay(2000); // make sure it has time to get there and settle
  minFeedback = analogRead(analogPin);

  // Move to the maximum position and record the feedback value
  servo.write(maxPos);
  maxDegrees = maxPos;
  delay(2000); // make sure it has time to get there and settle
  maxFeedback = analogRead(analogPin);
}

float readBat() {
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat /= 1024; // convert to voltage
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage

  Serial.print("VBat: " ); Serial.println(measuredvbat);
  return measuredvbat;
}

float readBatPercent() {
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat /= 1024; // convert to voltage
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage


  measuredvbat = (measuredvbat - 2.2) * 100.0 / 2.1;
  if (measuredvbat > 100.0)
    measuredvbat = 100.0;
  else if (measuredvbat < 0.0)
    measuredvbat = 0.0;
  return measuredvbat;
}
