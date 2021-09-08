#ifndef CodersboxESP8266SSL_MQTT_h
#define CodersboxESP8266SSL_MQTT_h

#include "Arduino.h"

#if defined(ESP8266)

#ifdef ESP8266
#include <core_version.h>
#endif

#ifdef ESP8266
extern "C"
{
#include "user_interface.h"
}
#include "ESP8266WiFi.h"
#include "ESP8266HTTPClient.h"
#include "WiFiClientSecure.h"
#include "WiFiClient.h"
#include "time.h"
#endif

#include "ArduinoJson.h"
#include "PubSubClient.h"

#define API_URL "http://54.169.179.39/api/v1"

#ifndef CODERSBOX_SSL_RX_BUFFER_SIZE
#define CODERSBOX_SSL_RX_BUFFER_SIZE 2048
#endif

#ifndef CODERSBOX_SSL_TX_BUFFER_SIZE
#define CODERSBOX_SSL_TX_BUFFER_SIZE 512
#endif

static const char CODERSBOX_ROOT_CA[] PROGMEM =
#include "letsencrypt_pem.h"

    static X509List CodersboxCertificate(CODERSBOX_ROOT_CA);


typedef void (*data_send)(void);

class CodersboxESP8266SSL_MQTT
{

public:
  WiFiClientSecure wifiClientSSL;
  PubSubClient _mqttClient = PubSubClient(wifiClientSSL);
  CodersboxESP8266SSL_MQTT() : _dataJSON(1024)
  {
    _lastTime = 0;
    Serial.begin(9600);
    Serial.println(F("Welcome to Codersbox!"));
  }

  CodersboxESP8266SSL_MQTT(String boxToken, String authToken) : _dataJSON(1024)
  {
    _lastTime = 0;
    _boxToken = boxToken;
    _authToken = authToken;
    Serial.begin(9600);
    Serial.println(F("Welcome to Codersbox!"));
    setMqttTopic();
  }

  void begin(const char *ssid, const char *pass)
  {
    connectToWifi(ssid, pass);
    wifiClientSSL.setSSLVersion(BR_TLS12, BR_TLS12);
    setMqttCredentials();
    setTime(7);
    setBuffer();
    setX509List(&CodersboxCertificate);
    verifyTLS();
    _mqttClient.setKeepAlive(60);
    _mqttClient.setSocketTimeout(30);

    if (_isBrokerConnected)
    {
      mqttConnect();
    }
  }

  void setMqttTopic()
  {
    _pubTopic = "/codersbox/nodemcu/pub/" + _boxToken;
    _subTopic = "/codersbox/nodemcu/sub/" + _boxToken;
  }

  template <typename T>
  void setField(const char *field, T dataValue)
  {
    _doc[field] = dataValue;
  }

  String getDataField()
  {
    String _data;
    serializeJson(_doc, _data);
    return _data;
  }

  void setPayload(byte *payload)
  {
    _payload = (char *)payload;
    DeserializationError error = deserializeJson(_dataJSON, _payload);
    if (error)
    {
      Serial.print("[JSON] : Deserialize error on = ");
      Serial.print(error.f_str());
      Serial.print(",");
      Serial.println(_payload);
      return;
    }

    _dataObject = _dataJSON.as<JsonObject>();
  }

  template <typename T>
  T getDataByField(const char *fieldname)
  {
    T result = _dataObject[fieldname];
    return result;
  }

  void setInterval(unsigned long interval, data_send func)
  {
    _interval = interval;
    _func = func;
  }

  void sendData()
  {
    _mqttClient.publish(_pubTopic.c_str(), getDataField().c_str());
  }
  void setCallback(std::function<void(char *, uint8_t *, unsigned int)> callback)
  {
    _mqttClient.setCallback(callback);
  }

  void setBuffer()
  {
    wifiClientSSL.setBufferSizes(CODERSBOX_SSL_RX_BUFFER_SIZE, CODERSBOX_SSL_TX_BUFFER_SIZE);
  }

  void setFingerPrint(const char *fp)
  {
    wifiClientSSL.setFingerprint(fp);
  }

  void setX509List(X509List *certs)
  {
    wifiClientSSL.setTrustAnchors(certs);
  }

  void connectToWifi(const char *ssid, const char *pass)
  {
    Serial.printf("[WiFi] : Connecting to %s \n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    while (!isWifiConnected())
    {
      delay(500);
      Serial.print(".");
    }
    Serial.printf("[WiFi] : Connected to %s \n", ssid);
    Serial.print("[WiFi] : IP ");
    Serial.println(WiFi.localIP());
  }

  void verifyTLS()
  {
    Serial.printf("[TLS]  : Verifying TLS Connection \n");
    Serial.printf("[TLS]  : Connecting to %s with Port = 8883 \n", _mqttHost);
    Serial.println(_mqttHost);
    if (wifiClientSSL.connect(_mqttHost, 8883))
    {
      Serial.printf("[TLS]  : Connected to broker \n");
      _isBrokerConnected = true;
      wifiClientSSL.stop();
      return;
    }

    char puff[100];
    wifiClientSSL.getLastSSLError(puff, sizeof(puff));
    _isBrokerConnected = false;
    Serial.printf("[TLS]  : Failed connect to broker: %s \n", puff);
    return;
  }

  void mqttConnect()
  {
    this->_mqttClient.setServer(_mqttHost, 8883);
    String clientId = "ESP8266Client-" + _boxToken + "-Codersbox-";
    clientId += String(random(0xffff), HEX);
    Serial.printf("[MQTT] : Connecting to MQTT with ID = %s \n", clientId.c_str());
    if (this->_mqttClient.connect(clientId.c_str(), _mqttUser, _mqttPass))
    {
      Serial.printf("[MQTT] : Connected rc = %d \n", this->_mqttClient.state());
      this->_mqttClient.publish(_pubTopic.c_str(), "Connected from Codersbox");
      this->_mqttClient.subscribe(_subTopic.c_str(), 1);
    }
    else
    {
      Serial.printf("[MQTT] : Failed connect, rc = %d \n", this->_mqttClient.state());
    }
  }

  void run()
  {
    if (!this->_mqttClient.connected())
    {
      Serial.println(F("[MQTT] : Disconnected from Broker"));
      Serial.printf("[MQTT] : rc = %d \n", this->_mqttClient.state());

      setMqttCredentials();
      verifyTLS();
      mqttConnect();
    }

    this->_mqttClient.loop();

    _now = millis();
    if (_now - _lastTime > _interval)
    {
      _lastTime = _now;
      _func();
    }
  }

  bool isWifiConnected()
  {
    return WiFi.status() == WL_CONNECTED;
  }

  void setTime(int timezone)
  {
    // Synchronize time useing SNTP. This is necessary to verify that
    // the TLS certificates offered by the server are currently valid.
    Serial.print("[TIME] : Setting time using SNTP \n");
    configTime(timezone * 3600, 0, "de.pool.ntp.org");
    time_t now = time(nullptr);
    while (now < 1000)
    {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
    }
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.printf("\n[TIME] : Current time: %s \n", asctime(&timeinfo));
  }

  void setMqttCredentials()
  {
    Serial.printf("\n[CDB] : Get credential from Codersbox \n");

    String creds = _sendHTTPGetRequest("/telemetry/" + _boxToken);
    DynamicJsonDocument credsDoc(512);
    DeserializationError error = deserializeJson(credsDoc, creds);

    if (error)
    {
      Serial.print(F("[CDB] : deserializeJson() failed: "));
      Serial.println(error.f_str());
      Serial.print(creds);

      return;
    }

    _mqttHost = "1d0a4037d8f645b9ab69895b4d041632.s1.eu.hivemq.cloud"; //credsDoc["host"];
    _mqttUser = "codersboxmqtt"; //credsDoc["user"];         
    _mqttPass = "c0dersb0xMQTT"; //credsDoc["pass"];
       
    // Serial.printf("\n[CDB] : Get credential from Codersbox success with Host = %s, User = %s, Pass = %s\n", _mqttHost, _mqttUser, _mqttPass);
  }

private:
  char *_payload;
  bool _isBrokerConnected;
  WiFiClient _cdbClient;
  String _boxToken;
  String _pubTopic;
  String _subTopic;
  String _authToken;

  unsigned long _now;
  unsigned long _lastTime;
  unsigned long _interval;

  const char *_mqttHost;
  const char *_mqttUser;
  const char *_mqttPass;
  data_send _func;
  JsonObject _dataObject;
  StaticJsonDocument<1024> _doc;
  DynamicJsonDocument _dataJSON;
  HTTPClient http;
  String _sendHTTPGetRequest(String params)
  {
    String api = API_URL + params;
    Serial.println(api);
    http.begin(_cdbClient, api);

    String payload;
    http.addHeader("Authorization", "Bearer " + _authToken);
    http.addHeader("X-Requested-With", "XMLHttpRequest");

    int response = http.GET();

    if (response == HTTP_CODE_OK)
    {
      payload = http.getString();
    }
    else
    {
      Serial.println(response);
    }

    http.end();
    _cdbClient.stop();
    return payload;
  }
};

#endif

#endif