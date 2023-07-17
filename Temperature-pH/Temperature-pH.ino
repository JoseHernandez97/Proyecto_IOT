#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#define ONE_WIRE_BUS D2
#define SENSOR_INTERVAL 1000    // Tiempo de lectura de sensores (1 segundos)
#define SEND_INTERVAL 300000    // Tiempo de envío de datos (5 minutos)
#define RELAY_1_PIN D5            // Relé 1
#define RELAY_2_PIN D6            // Relé 2
#define RELAY_ON LOW              // Rele Encendido
#define RELAY_OFF HIGH            // Rele Apagado

const char* ssid = "Comunicate-Leon";
const char* password = "darkshadow2125.";

//const char* ssid = "SweetAntonella";
//const char* password = "ancamacho";

const char* serverURL = "http://monitoreos.purplelabsoft.com/insert/";
const char* relay1URL = "http://monitoreos.purplelabsoft.com/componente/Bomba";
const char* relay2URL = "http://monitoreos.purplelabsoft.com/componente/Aspersor";
const char* modeURL = "http://monitoreos.purplelabsoft.com/componente/Automatico";

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const int pH_pin = A0;
float pH_value;
float pH_calibration_offset = -5.5;

unsigned long lastSensorTime = 0;
unsigned long lastSendTime = 0;
int manualValue = 0; // Variable para almacenar el modo manual o automático

WiFiClient client; // Declarar instancia de WiFiClient fuera del bucle loop

void setup() {
  Serial.begin(9600);
  pinMode(pH_pin, INPUT);
  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);

  digitalWrite(RELAY_1_PIN, RELAY_OFF);
  digitalWrite(RELAY_2_PIN, RELAY_OFF);

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
  
  float tempC; // Variable para almacenar la temperatura

  // Obtener el modo de control (manual o automático) desde la endpoint
  HTTPClient httpMode;
  httpMode.begin(client, modeURL);
  int httpResponseCodeMode = httpMode.GET();
  if (httpResponseCodeMode > 0) {
    String responseMode = httpMode.getString();
    Serial.println("Modo");
    Serial.println(httpResponseCodeMode);

    // JSON para el modo
    DynamicJsonDocument jsonMode(256);
    deserializeJson(jsonMode, responseMode);

    // Obtener el valor de "modo" (manual o automático)
    manualValue = jsonMode[0]["estado"];
    Serial.println(manualValue);
  } else {
    Serial.print("Error en la solicitud. Código de error HTTP: ");
    Serial.println(httpResponseCodeMode);
  }
  httpMode.end();

  
  // Lectura Sensores (Cada 3S )
  if (currentTime - lastSensorTime >= SENSOR_INTERVAL) {
    sensors.requestTemperatures();
    tempC = sensors.getTempCByIndex(0);

    int pH_raw = analogRead(pH_pin);
    pH_value = map(pH_raw, 0, 1023, 0, 140) / 10.0;
    pH_value += pH_calibration_offset;
    lastSensorTime = currentTime;

    // Control de los reles en modo automático
    if (manualValue == 1) {
      // Rele 1 Temperatura
      if (tempC >= 32.0) {
        digitalWrite(RELAY_1_PIN, RELAY_ON);
      } else if (tempC <= 25.0) {
        digitalWrite(RELAY_1_PIN, RELAY_OFF);
      }


      // Rele 2 pH
      if (pH_value < 6.0 || pH_value == 6.0) {
        digitalWrite(RELAY_2_PIN, RELAY_ON);
      } else if (pH_value > 7.5 || pH_value == 7.5) {
        digitalWrite(RELAY_2_PIN, RELAY_OFF);
      } else if (pH_value > 9.0) {
        digitalWrite(RELAY_2_PIN, RELAY_ON);
      }
    }

    Serial.print("Temperatura: ");
    Serial.print(tempC);
    Serial.println(" °C");

    Serial.print("pH: ");
    Serial.println(pH_value);
  }

  // Envío de datos (Cada 5 minutos)
  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    String jsonData = "{";
    jsonData += "\"temperatura\": ";
    jsonData += String(tempC);
    jsonData += ", ";
    jsonData += "\"pH\": ";
    jsonData += String(pH_value);
    jsonData += "}";

    // Conexión al servidor para enviar los datos
    HTTPClient http;
    http.begin(client, serverURL);
    http.addHeader("Content-Type", "application/json");

    // Envío JSON a la API
    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Respuesta de la API: " + response);
    } else {
      Serial.print("Error en la solicitud. Código de error HTTP: ");
      Serial.println(httpResponseCode);
    }
    http.end();

    lastSendTime = currentTime;
  }

  // Control de los reles en modo manual
  if (manualValue == 0) {
    // Recibir JSON para el relé 1
    HTTPClient httpRelay1;
    httpRelay1.begin(client, relay1URL);
    int httpResponseCodeRelay1 = httpRelay1.GET();
    if (httpResponseCodeRelay1 > 0) {
      String responseRelay1 = httpRelay1.getString();
      Serial.println("Rele 1");
      Serial.println(httpResponseCodeRelay1);

      // JSON para el relé 1 (Arreglo de JSON)
      DynamicJsonDocument jsonRelay1(256);
      deserializeJson(jsonRelay1, responseRelay1);

      // Obtener el valor de "estado" del primer elemento del arreglo
      int relay1Value = jsonRelay1[0]["estado"];
      Serial.println(relay1Value);

      // Controlar el relé 1
      if (relay1Value == 1) {
        digitalWrite(RELAY_1_PIN, RELAY_ON);
      } else {
        digitalWrite(RELAY_1_PIN, RELAY_OFF);
      }
    } else {
      Serial.print("Error en la solicitud. Código de error HTTP: ");
      Serial.println(httpResponseCodeRelay1);
    }
    httpRelay1.end();

    // Recibir JSON para el relé 2
    HTTPClient httpRelay2;
    httpRelay2.begin(client, relay2URL);
    int httpResponseCodeRelay2 = httpRelay2.GET();
    if (httpResponseCodeRelay2 > 0) {
      String responseRelay2 = httpRelay2.getString();
      Serial.println("Rele 2");
      Serial.println(httpResponseCodeRelay2);

      // JSON para el relé 2 (Arreglo de JSON)
      DynamicJsonDocument jsonRelay2(256);
      deserializeJson(jsonRelay2, responseRelay2);

      // Obtener el valor de "estado" del primer elemento del arreglo
      int relay2Value = jsonRelay2[0]["estado"];
      Serial.println(relay2Value);

      // Controlar el relé 2
      if (relay2Value == 1) {
        digitalWrite(RELAY_2_PIN, RELAY_ON);
      } else {
        digitalWrite(RELAY_2_PIN, RELAY_OFF);
      }
    } else {
      Serial.print("Error en la solicitud. Código de error HTTP: ");
      Serial.println(httpResponseCodeRelay2);
    }
    httpRelay2.end();
  }

  delay(100);
}
