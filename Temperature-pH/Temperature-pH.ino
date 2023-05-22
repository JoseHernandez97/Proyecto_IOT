#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>

#define ONE_WIRE_BUS 2
#define SENSOR_INTERVAL 1000 //300000    // Tiempo de lectura sensores (5 minutos)
#define SEND_INTERVAL 10000 //900000      // Tiempo envío de datos al servidor (15 minutos)
#define RELAY_1_PIN 12            // Relé 1
#define RELAY_2_PIN 13            // Relé 2
#define RELAY_ON LOW              // Rele Apagado
#define RELAY_OFF HIGH            // Rele Encendido

const char* ssid = "Comunicate-Leon";
const char* password = "darkshadow2125.";

const char* serverURL = "http://123.102.45.34:8080/sensors";

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const int pH_pin = A0;
float pH_value;

unsigned long lastSensorTime = 0;
unsigned long lastSendTime = 0;

bool relay1State = false;
bool relay2State = false;

void setup() {
  Serial.begin(9600);
  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  
  WiFi.begin(ssid, password);
  sensors.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi");
}

void loop() {
  unsigned long currentTime = millis();

  // Lectura Sensores (Cada 5 Min)
  if (currentTime - lastSensorTime >= SENSOR_INTERVAL) {
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);

    int pH = analogRead(pH_pin);
    pH_value = map(pH, 0, 1023, 0, 14);
    lastSensorTime = currentTime;

    // Rele 1 Temperatura
    if (tempC >= 30.0) {
      digitalWrite(RELAY_1_PIN, RELAY_ON);
      
    } else {
      digitalWrite(RELAY_1_PIN, RELAY_OFF);
      
    }

    // Rele 2 pH
    if (pH_value <= 6.0) {
      digitalWrite(RELAY_2_PIN, RELAY_ON);
      relay2State = true;
    } else {
      digitalWrite(RELAY_2_PIN, RELAY_OFF);
      relay2State = false;
    }

    Serial.print("Temperatura: ");
    Serial.print(tempC);
    Serial.println(" °C");

    Serial.print("pH: ");
    Serial.println(pH_value);
  }

  // Enviar datos al servidor cada 15 minutos
  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = currentTime;

    // Crear el objeto JSON con los datos
    String jsonData = "{";
    jsonData += "\"temperatura\": ";
    jsonData += String(sensors.getTempCByIndex(0));
    jsonData += ", ";
    jsonData += "\"pH\": ";
    jsonData += String(pH_value);
    jsonData += "}";

    // Conexion Servidor
    WiFiClient client;
    HTTPClient http;
    http.begin(client, serverURL);
    http.addHeader("Content-Type", "application/json");

    // Envio JSON a API
    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Respuesta de la API: " + response);
    } else {
      Serial.print("Error en la solicitud. Código de error HTTP: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }

  delay(1000); 
}
