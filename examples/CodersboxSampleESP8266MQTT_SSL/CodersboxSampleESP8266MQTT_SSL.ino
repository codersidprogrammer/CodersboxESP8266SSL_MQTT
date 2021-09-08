#include <CodersboxESP8266SSL_MQTT.h>
#include <DHT.h>

#define DHTTYPE DHT11
#define DHTPIN D1

const char *wifiSSID = "<CHANGE-WITH-YOUR-SSID>";
const char *password = "<CHANGE-WITH-YOUR-PASS>";
String boxToken = "<CHANGE-WITH-YOUR-BOXTOKEN>";
String authToken = "<CHANGE-WITH-YOUR-AUTHTOKEN>";

CodersboxESP8266SSL_MQTT Codersbox(boxToken, authToken);
DHT dht(DHTPIN, DHTTYPE);
bool ledStatus = false;
int pwmValue = 0;

/**
 * Callback run for every incoming message from MQTT
 * 
 * 
 * */
void callback(char *topic, byte *payload, unsigned int length)
{
  Codersbox.setPayload(payload);
  float temp = Codersbox.getDataByField<float>("temperature");
  float humid = Codersbox.getDataByField<float>("humiditas");
  ledStatus = Codersbox.getDataByField<boolean>("relay");
   if (ledStatus)
   {
     digitalWrite(LED_BUILTIN, LOW);
   }
   else
   {
     digitalWrite(LED_BUILTIN, HIGH);
   }
  Serial.printf("[DEBUG] : Temperature = %f, Relay(LED) = %d, humidity = %f \n", temp, ledStatus, humid);
}

/**
 * Send Data Through MQTT
 * */
void sendData()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Codersbox.setField("temperature", t);
  Codersbox.setField("humiditas", h);
  Codersbox.setField("relay", ledStatus);
  Codersbox.sendData();
}

/**
 *  ------------------------------------------
 *    MAIN PROGRAM
 *  ------------------------------------------
 *         
 * */
void setup()
{
  dht.begin();
  Codersbox.begin(wifiSSID, password);
  Codersbox.setInterval(2000, sendData);
  Codersbox.setCallback(callback);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
  Codersbox.run();
}