#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#define SENSOR_INTERVAL 1000 //300000    // Tiempo de lectura sensores
#define RELAY_1_PIN D5            // Relé 1
#define RELAY_ON LOW              // Rele Encendido
#define RELAY_OFF HIGH            // Rele Apagado

const char* ssid = "Comunicate-Leon";
const char* password = "darkshadow2125.";

//const char* ssid = "SweetAntonella";
//const char* password = "ancamacho";

const char* serverURL = "http://monitoreos.purplelabsoft.com/insert/tumamaeshombre";
const char* relay1URL = "http://monitoreos.purplelabsoft.com/componente/Aireador";
const char* modeURL = "http://monitoreos.purplelabsoft.com/componente/Automatico";

unsigned long lastSensorTime = 0;
unsigned long lastSendTime = 0;
int manualValue = 0; // Variable para almacenar el modo manual o automático

#define DO_PIN A0

#define VREF 5000    //VREF (mv)
#define ADC_RES 1024 //ADC Resolution

//Single-point calibration Mode=0
//Two-point calibration Mode=1
#define TWO_POINT_CALIBRATION 0

#define READ_TEMP (25) //Current water temperature ℃, Or temperature sensor function

//Single point calibration needs to be filled CAL1_V and CAL1_T
#define CAL1_V (1600) //mv
#define CAL1_T (25)   //℃
//Two-point calibration needs to be filled CAL2_V and CAL2_T
//CAL1 High temperature point, CAL2 Low temperature point
#define CAL2_V (1300) //mv
#define CAL2_T (15)   //℃

const uint16_t DO_Table[41] = {
    14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530,
    11260, 11010, 10770, 10530, 10300, 10080, 9860, 9660, 9460, 9270,
    9080, 8900, 8730, 8570, 8410, 8250, 8110, 7960, 7820, 7690,
    7560, 7430, 7300, 7180, 7070, 6950, 6840, 6730, 6630, 6530, 6410};

uint8_t Temperaturet;
uint16_t ADC_Raw;
uint16_t ADC_Voltage;
uint16_t DO;
uint16_t D_OXYGEN;

int16_t readDO(uint32_t voltage_mv, uint8_t temperature_c)
{
#if TWO_POINT_CALIBRATION == 0
  uint16_t V_saturation = (uint32_t)CAL1_V + (uint32_t)35 * temperature_c - (uint32_t)CAL1_T * 35;
  return (voltage_mv * DO_Table[temperature_c] / V_saturation);
#else
  uint16_t V_saturation = (int16_t)((int8_t)temperature_c - CAL2_T) * ((uint16_t)CAL1_V - CAL2_V) / ((uint8_t)CAL1_T - CAL2_T) + CAL2_V;
  return (voltage_mv * DO_Table[temperature_c] / V_saturation);
#endif
}

WiFiClient client; // Declarar instancia de WiFiClient fuera del bucle loop

void setup() {
  Serial.begin(9600);
  pinMode(RELAY_1_PIN, OUTPUT);

  digitalWrite(RELAY_1_PIN, RELAY_OFF);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi");
}

void loop() {
  unsigned long currentTime = millis();

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
    Temperaturet = (uint8_t)READ_TEMP;
    ADC_Raw = analogRead(DO_PIN);
    ADC_Voltage = uint32_t(VREF) * ADC_Raw / ADC_RES;
    D_OXYGEN = readDO(ADC_Voltage, Temperaturet) / 1000;
    Serial.println("Oxygen mg/L:\t" + String(D_OXYGEN) + "\t");
    lastSensorTime = currentTime;

    // Control de los reles en modo automático
    if (manualValue == 1) {
      // Rele 1 Oxigeno
      if (D_OXYGEN < 5.0){
        Serial.println(D_OXYGEN);
        digitalWrite(RELAY_1_PIN, LOW); // Enciende el relé
      }
      else if (D_OXYGEN > 9.0){
        Serial.println(D_OXYGEN);
        digitalWrite(RELAY_1_PIN, HIGH); // Apaga el relé
      }
    }

    String jsonData = "{";
    jsonData += "\"oxigeno\": ";
    jsonData += String(D_OXYGEN);
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
    }
  delay(100);
}
