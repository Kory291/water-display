#include <Arduino.h>
#include <WiFi.h>

// put function declarations here:
const char* ssid = "foo";
const char* password = "foo";


unsigned long current_time = millis();
unsigned long previous_time = 0;
const long timeout_time = 2000;

WiFiServer server(80);

String header;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  delay(1000);

  WiFi.mode(WIFI_STA); //Optional
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");

  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(100);
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());

  server.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  WiFiClient client = server.available();

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

// put function definitions here: