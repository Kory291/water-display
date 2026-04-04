#include <Arduino.h>
#include <WiFi.h>
#include <esp_http_server.h>

// put function declarations here:
// sound speed in cm/µS
#define SOUND_SPEED 0.034
#define SOUND_SPEED_WATER 0.148
// distance between ultrasonic sensor and water bin in cm
#define AIR_GAP 30
// max distance to ground from ultrasonic sensor in cm
#define MAX_DISTANCE 200

const char* ssid = "foo";
const char* password = "foo";
const int trigPin = 22;
const int echoPin = 23;

unsigned long current_time = millis();
unsigned long previous_time = 0;
const long timeout_time = 2000;

long duration;
float distanceCm;
float fillPercentage;

WiFiServer server(80);

String header;

void setup_pins() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void setup_wifi() {
  WiFi.mode(WIFI_STA); //Optional

  int i=0;
  Serial.println("\nConnecting");
  
  while(i < 10 && WiFi.status() != WL_CONNECTED) {
    delay(10);
    Serial.println(".");
    i++;
  }
  if(WiFi.status() != WL_CONNECTED ) {
    WiFi.begin(ssid, password);
  }

  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(100);
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  delay(1000);

  setup_wifi();
  server.begin();
  setup_pins();
}

void serve(WiFiClient client) {
  if (client) {
    current_time = millis();
    previous_time = current_time;
    Serial.println("New client");
    String current_line = "";
    while (client.connected() && current_time - previous_time < timeout_time) {
      current_time = millis();
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        header += c;
        if (c == '\n') {
          if (current_line.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");

            client.println("<body><h1>ESP32 Web Server</h1>");
            client.println("</body></html>");
            client.println();
            break;
          } else {
            current_line = "";
          }
        } else if (c != '\r') {
        current_line += c;
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

float get_fill_measurement() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);

  fillPercentage = (1 / 2 * duration * (SOUND_SPEED + SOUND_SPEED_WATER) - AIR_GAP) / MAX_DISTANCE;

  return fillPercentage;
}

void loop() {
  // put your main code here, to run repeatedly:
  WiFiClient client = server.available();
  // serve(client);
  fillPercentage = get_fill_measurement();
  Serial.printf("Distance in cm: %f\n", fillPercentage);
  delay(1000);
}

// put function definitions here: