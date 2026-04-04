#include <Arduino.h>
#include <WiFi.h>
#include <esp_http_server.h>
#include "esp_vfs.h"
#include "esp_log.h"
#include "esp_check.h"
#include "cJSON.h"
#include <fcntl.h>

// put function declarations here:
// sound speed in cm/µS
#define SOUND_SPEED 0.034
#define SOUND_SPEED_WATER 0.148
// distance between ultrasonic sensor and water bin in cm
#define AIR_GAP 30
// max distance to ground from ultrasonic sensor in cm
#define MAX_DISTANCE 200

static const char *TAG = "esp-rest";

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

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

static esp_err_t drain_info_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "application/json");
  cJSON *root = cJSON_CreateObject();
  const char *drain_info = cJSON_Print(root);
  httpd_resp_sendstr(req, drain_info);
  free((void *)drain_info);
  cJSON_Delete(root);
  return ESP_OK;
}

static esp_err_t measurementUpdateHandler(httpd_req_t *req) {
  int total_len = req->content_len;
  int cur_len = 0;
  char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
  int received = 0;
  if (total_len >= SCRATCH_BUFSIZE) {
    /* Respond with 500 Internal Server Error */
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
    return ESP_FAIL;
  }
  while (cur_len < total_len) {
    received = httpd_req_recv(req, buf + cur_len, total_len);
    if (received <= 0) {
      /* Respond with 500 Internal Server Error */
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
      return ESP_FAIL;
    }
    cur_len += received;
  }
  buf[total_len] = '\0';

  cJSON *root = cJSON_Parse(buf);
  int sensorId = cJSON_GetObjectItem(root, "sensorId")->valueint;
  double measurement = cJSON_GetObjectItem(root, "measurement")->valuedouble;
  cJSON_Delete(root);
  httpd_resp_sendstr(req, "Measurement updated successfully");
  return ESP_OK;
}

esp_err_t start_rest_server(const char *base_path) {

  esp_err_t ret = ESP_OK;
  ESP_RETURN_ON_FALSE(base_path && strlen(base_path) < ESP_VFS_PATH_MAX, ESP_ERR_INVALID_ARG, TAG, "Invalid base path");
  rest_server_context_t *rest_context = (rest_server_context_t*) calloc(1, sizeof(rest_server_context_t));
  ESP_RETURN_ON_FALSE(rest_context, ESP_ERR_NO_MEM, TAG, "No memory for rest context");
  strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;

  ESP_LOGI(TAG, "Starting HTTP Server");
  // ESP_GOTO_ON_ERROR(httpd_start(&server, &config), err, TAG, "Failed to start http server");

  httpd_uri_t drain_info_get_uri = {
    .uri = "/api/v1/drain",
    .method = HTTP_GET,
    .handler = drain_info_handler,
    .user_ctx = rest_context
  };
  httpd_register_uri_handler(server, &drain_info_get_uri);

  httpd_uri_t measurement_update_uri = {
    .uri = "/api/v1/measurementUpdate",
    .method = HTTP_POST,
    .handler = measurementUpdateHandler,
    .user_ctx = rest_context
  };
  httpd_register_uri_handler(server, &measurement_update_uri)
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