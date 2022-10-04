#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <ArduinoJson.h>
#include "AsyncMqttClient.h"
#include "Log.h"
#include "Enumerations.h"

#include "Pylon.h"

// Include Update server
#ifdef ESP8266
# include <ESP8266HTTPUpdateServer.h>
#elif defined(ESP32)
# include <IotWebConfESP32HTTPUpdateServer.h>
#endif

#define MAX_PUBLISH_RATE 30000
#define MIN_PUBLISH_RATE 1000
#define WAKE_PUBLISH_RATE 10000
#define SNOOZE_PUBLISH_RATE 300000
#define WAKE_COUNT 60
#define CONFIG_VERSION "V1.0.1" // major.minor.build (major or minor will invalidate the configuration)
#define NUMBER_CONFIG_LEN 6
#define WATCHDOG_TIMER 600000 //time in ms to trigger the watchdog

AsyncMqttClient _mqttClient;
TimerHandle_t mqttReconnectTimer;
DNSServer _dnsServer;
WebServer _webServer(80);
#ifdef ESP8266
ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
HTTPUpdateServer httpUpdater;
#endif
IotWebConf _iotWebConf(TAG, &_dnsServer, &_webServer, TAG, CONFIG_VERSION);
hw_timer_t *_watchdogTimer = NULL;

char _bankName[IOTWEBCONF_WORD_LEN];
char _mqttServer[IOTWEBCONF_WORD_LEN];
char _mqttPort[NUMBER_CONFIG_LEN];
char _mqttUserName[IOTWEBCONF_WORD_LEN];
char _mqttUserPassword[IOTWEBCONF_WORD_LEN];
char _mqttRootTopic[64];
char _wakePublishRateStr[NUMBER_CONFIG_LEN];
char _willTopic[64];
char _rootTopicPrefix[64];

iotwebconf::ParameterGroup Battery_group = iotwebconf::ParameterGroup("BatteryGroup", "Battery");
iotwebconf::TextParameter bankNameParam = iotwebconf::TextParameter("Battery Bank Name", "Bank1", _bankName, IOTWEBCONF_WORD_LEN);
iotwebconf::ParameterGroup MQTT_group = iotwebconf::ParameterGroup("MQTT", "MQTT");
iotwebconf::TextParameter mqttServerParam = iotwebconf::TextParameter("MQTT server", "mqttServer", _mqttServer, IOTWEBCONF_WORD_LEN);
iotwebconf::NumberParameter mqttPortParam = iotwebconf::NumberParameter("MQTT port", "mqttSPort", _mqttPort, NUMBER_CONFIG_LEN, "text", NULL, "1883");
iotwebconf::TextParameter mqttUserNameParam = iotwebconf::TextParameter("MQTT user", "mqttUser", _mqttUserName, IOTWEBCONF_WORD_LEN);
iotwebconf::PasswordParameter mqttUserPasswordParam = iotwebconf::PasswordParameter("MQTT password", "mqttPass", _mqttUserPassword, IOTWEBCONF_WORD_LEN, "password");
iotwebconf::TextParameter mqttRootTopicParam = iotwebconf::TextParameter("MQTT Root Topic", "mqttRootTopic", _mqttRootTopic, IOTWEBCONF_WORD_LEN);
iotwebconf::NumberParameter wakePublishRateParam = iotwebconf::NumberParameter("Publish Rate (S)", "wakePublishRate", _wakePublishRateStr, NUMBER_CONFIG_LEN, "text", NULL, "2");

unsigned long _lastPublishTimeStamp = 0;
unsigned long _currentPublishRate = WAKE_PUBLISH_RATE; // rate currently being used
unsigned long _wakePublishRate = WAKE_PUBLISH_RATE; // wake publish rate set by config or mqtt command
boolean _stayAwake = true;
bool _clientsConfigured = false;
int _publishCount = 0;

Pylon _Pylon;
void complete(AsyncSerial& sender)
{
	_Pylon.ParseResponse((char*)sender.GetContent(), sender.GetContentLength(), (CommandInformation)sender.GetToken());

}

void overflow(AsyncSerial& sender)
{
	loge("  Response from AsyncSerial: overflow");
	_Pylon.Next(); // move on
}

void timeout(AsyncSerial& sender)
{
	loge("  Response from AsyncSerial: timeout");
	_Pylon.Next(); // move on
}
AsyncSerial _asyncSerial(complete, timeout, overflow);


void IRAM_ATTR resetModule()
{
	// ets_printf("watchdog timer expired - rebooting\n");
	esp_restart();
}

void init_watchdog()
{
	if (_watchdogTimer == NULL)
	{
		_watchdogTimer = timerBegin(0, 80, true);					   //timer 0, div 80
		timerAttachInterrupt(_watchdogTimer, &resetModule, true);	  //attach callback
		timerAlarmWrite(_watchdogTimer, WATCHDOG_TIMER * 1000, false); //set time in us
		timerAlarmEnable(_watchdogTimer);							   //enable interrupt
	}
}

void feed_watchdog()
{
	if (_watchdogTimer != NULL)
	{
		timerWrite(_watchdogTimer, 0); // feed the watchdog
	}
}


void publish(const char *subtopic, const char *value, boolean retained = false)
{
	if (_mqttClient.connected())
	{
		char buf[64];
		sprintf(buf, "%s/stat/%s", _rootTopicPrefix, subtopic);
		_mqttClient.publish(buf, 0, retained, value);
	}
}

void Wake()
{
	_currentPublishRate = _wakePublishRate;
	_lastPublishTimeStamp = 0;
	_publishCount = 0;
}


void onMqttConnect(bool sessionPresent)
{
	logd("Connected to MQTT. Session present: %d", sessionPresent);
	char buf[64];
	sprintf(buf, "%s/cmnd/#", _rootTopicPrefix);
	_mqttClient.subscribe(buf, 0);
	_mqttClient.publish(_willTopic, 0, false, "Online");
	Wake();
	logi("Subscribed to [%s], qos: 0", buf);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
	logi("Disconnected from MQTT. Reason: %d", (int8_t)reason);
	if (WiFi.isConnected())
	{
		xTimerStart(mqttReconnectTimer, 0);
	}
}

void connectToMqtt()
{
	if (WiFi.isConnected())
	{
		if (strlen(_mqttServer) > 0) // mqtt configured
		{
			logi("Connecting to MQTT...");
			int len = strlen(_mqttRootTopic);
			strncpy(_rootTopicPrefix, _mqttRootTopic, len);
			if (_rootTopicPrefix[len - 1] != '/')
			{
				strcat(_rootTopicPrefix, "/");
			}
			strcat(_rootTopicPrefix, _bankName);

			sprintf(_willTopic, "%s/tele/LWT", _rootTopicPrefix);
			_mqttClient.setWill(_willTopic, 0, true, "Offline");
			_mqttClient.connect();

			logd("_mqttRootTopic: %s", _mqttRootTopic);
			logd("rootTopicPrefix: %s", _rootTopicPrefix);
		}
	}
}

/**
 * Handle web requests to "/" path.
 */
void handleRoot()
{
	// -- Let IotWebConf test and handle captive portal requests.
	if (_iotWebConf.handleCaptivePortal())
	{
		// -- Captive portal request were already served.
		return;
	}
	String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
	s += "<title>PylonToMQTT</title></head><body>";
	s += _iotWebConf.getThingName();
	s += "<ul>";
	s += "<li>MQTT server: ";
	s += _mqttServer;
	s += "</ul>";
	s += "<ul>";
	s += "<li>MQTT port: ";
	s += _mqttPort;
	s += "</ul>";
	s += "<ul>";
	s += "<li>MQTT user: ";
	s += _mqttUserName;
	s += "</ul>";
	s += "<ul>";
	s += "<li>MQTT root topic: ";
	s += _mqttRootTopic;
	s += "</ul>";
	s += "<ul>";
	s += "<li>Publish Rate : ";
	s += _wakePublishRateStr;
	s += " (S)</ul>";
	s += "Go to <a href='config'>configure page</a> to change values.";
	s += "</body></html>\n";
	_webServer.send(200, "text/html", s);
}

void configSaved()
{
	logi("Configuration was updated.");
}

boolean formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{
	boolean valid = true;
	int mqttServerParamLength = _webServer.arg(mqttServerParam.getId()).length();
	if (mqttServerParamLength == 0)
	{
		mqttServerParam.errorMessage = "MQTT server is required";
		valid = false;
	}
	int rate = _webServer.arg(wakePublishRateParam.getId()).toInt() * 1000;
	if (rate < MIN_PUBLISH_RATE || rate > MAX_PUBLISH_RATE)
	{
		wakePublishRateParam.errorMessage = "Invalid publish rate.";
		valid = false;
	}
	return valid;
}

void WiFiEvent(WiFiEvent_t event)
{
	logd("[WiFi-event] event: %d", event);
	String s;
	StaticJsonDocument<128> doc;
	switch (event)
	{
	case SYSTEM_EVENT_STA_GOT_IP:
		doc["IP"] = WiFi.localIP().toString().c_str();
		doc["ApPassword"] = TAG;
		serializeJson(doc, s);
		s += '\n';
		Serial.printf(s.c_str()); // send json to flash tool
		xTimerStart(mqttReconnectTimer, 0); // connect to MQTT once we have wifi
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		logi("WiFi lost connection");
		xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
		break;
	default:
		break;
	}
}

void onMqttPublish(uint16_t packetId)
{
	logi("Publish acknowledged.  packetId: %d", packetId);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
	logd("MQTT Message arrived [%s]  qos: %d len: %d index: %d total: %d", topic, properties.qos, len, index, total);
	printHexString(payload, len);

	StaticJsonDocument<64> doc;
	DeserializationError err = deserializeJson(doc, payload);
	if (err) // not json!
	{
		logd("MQTT payload {%s} is not valid JSON!", payload);
	}
	else 
	{
		boolean messageProcessed = false;
		if (doc.containsKey("stayAwake") && doc["stayAwake"].is<boolean>())
		{
			_stayAwake = doc["stayAwake"];
			messageProcessed = true;
			logd("Stay awake: %s", _stayAwake ? "yes" :"no");
		}
		if (doc.containsKey("wakePublishRate") && doc["wakePublishRate"].is<int>())
		{
			int publishRate = doc["wakePublishRate"];
			messageProcessed = true;
			if (publishRate >= MIN_PUBLISH_RATE && publishRate <= MAX_PUBLISH_RATE)
			{
				_wakePublishRate = doc["wakePublishRate"];
				logd("Wake publish rate: %d", _wakePublishRate);
			}
			else
			{
				logd("wakePublishRate is out of rage!");
			}
		}
		if (!messageProcessed)
		{
			logd("MQTT Json payload {%s} not recognized!", payload);
		}
	}
	Wake(); // wake on any MQTT command received
}

void setup()
{
	Serial.begin(115200);
	while (!Serial) {}
	// Set up serial port used to communicate with battery
	_asyncSerial.begin(BAUDRATE, SERIAL_8N1, RXPIN, TXPIN);
	_Pylon.begin(&_asyncSerial, publish);
	pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);
	_iotWebConf.setStatusPin(WIFI_STATUS_PIN);
	_iotWebConf.setConfigPin(WIFI_AP_PIN);
	// setup EEPROM parameters
	Battery_group.addItem(&bankNameParam);
   	MQTT_group.addItem(&mqttServerParam);
	MQTT_group.addItem(&mqttPortParam);
   	MQTT_group.addItem(&mqttUserNameParam);
	MQTT_group.addItem(&mqttUserPasswordParam);
	MQTT_group.addItem(&mqttRootTopicParam);
	MQTT_group.addItem(&wakePublishRateParam);
	_iotWebConf.addParameterGroup(&Battery_group);
	_iotWebConf.addParameterGroup(&MQTT_group);

	// setup callbacks for IotWebConf
	_iotWebConf.setConfigSavedCallback(&configSaved);
	_iotWebConf.setFormValidator(&formValidator);
	_iotWebConf.setupUpdateServer(
      [](const char* updatePath) { httpUpdater.setup(&_webServer, updatePath); },
      [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); });
	if (digitalRead(FACTORY_RESET_PIN) == LOW)
	{
		EEPROM.begin(IOTWEBCONF_CONFIG_START + IOTWEBCONF_CONFIG_VERSION_LENGTH );
		for (byte t = 0; t < IOTWEBCONF_CONFIG_VERSION_LENGTH; t++)
		{
			EEPROM.write(IOTWEBCONF_CONFIG_START + t, 0);
		}
		EEPROM.commit();
		EEPROM.end();
		_iotWebConf.resetWifiAuthInfo();
		logw("Factory Reset!");
	}
	mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(5000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
	WiFi.onEvent(WiFiEvent);
	boolean validConfig = _iotWebConf.init();
	if (!validConfig)
	{
		logw("!invalid configuration!");
		_mqttServer[0] = '\0';
		_mqttPort[0] = '\0';
		_mqttUserName[0] = '\0';
		_mqttUserPassword[0] = '\0';
		_mqttRootTopic[0] = '\0';
		_wakePublishRateStr[0] = '\0';
		_iotWebConf.resetWifiAuthInfo();
	}
	else
	{
		_wakePublishRate = atoi(_wakePublishRateStr) * 1000;
		_iotWebConf.skipApStartup(); // Set WIFI_AP_PIN to gnd to force AP mode
		if (_mqttServer[0] != '\0') // skip if factory reset
		{
			logi("Valid configuration!");
			_clientsConfigured = true;
			_mqttClient.onConnect(onMqttConnect);
			_mqttClient.onDisconnect(onMqttDisconnect);
			_mqttClient.onMessage(onMqttMessage);
			_mqttClient.onPublish(onMqttPublish);

			IPAddress ip;
			int port = atoi(_mqttPort);
			if (ip.fromString(_mqttServer))
			{
				_mqttClient.setServer(ip, port);
			}
			else
			{
				_mqttClient.setServer(_mqttServer, port);
			}
			_mqttClient.setCredentials(_mqttUserName, _mqttUserPassword);
		}
	}

	// IotWebConfParameter* p = _iotWebConf.getApPasswordParameter();
	// logi("AP Password: %s", p->valueBuffer);
	// Set up required URL handlers on the web server.
	_webServer.on("/", handleRoot);
	_webServer.on("/config", [] { _iotWebConf.handleConfig(); });
	_webServer.onNotFound([]() { _iotWebConf.handleNotFound(); });
	_lastPublishTimeStamp = millis() + WAKE_PUBLISH_RATE;
	init_watchdog();

	logd("Done setup");
}

void loop()
{
	_iotWebConf.doLoop();
	if (_clientsConfigured && WiFi.isConnected())
	{
		if (_mqttClient.connected())
		{
			_asyncSerial.Receive(WAKE_PUBLISH_RATE); 
			if (_lastPublishTimeStamp < millis())
			{
				_lastPublishTimeStamp = millis() + _currentPublishRate;
				feed_watchdog();

				_Pylon.loop();
			}
			if (!_stayAwake && _publishCount >= WAKE_COUNT)
			{
				_publishCount = 0;
				_currentPublishRate = SNOOZE_PUBLISH_RATE;
				logd("Snoozing!");
			}
		}
	}
	else
	{
		feed_watchdog(); // don't reset when not configured
		if (Serial.peek() == '{')
		{
			String s = Serial.readStringUntil('}');
			s += "}";
			StaticJsonDocument<128> doc;
			DeserializationError err = deserializeJson(doc, s);
			if (err)
			{
				loge("deserializeJson() failed: %s", err.c_str());
			}
			else
			{
				if (doc.containsKey("ssid") && doc.containsKey("password"))
				{
					iotwebconf::Parameter *p = _iotWebConf.getWifiSsidParameter();
					strcpy(p->valueBuffer, doc["ssid"]);
					logd("Setting ssid: %s", p->valueBuffer);
					p = _iotWebConf.getWifiPasswordParameter();
					strcpy(p->valueBuffer, doc["password"]);
					logd("Setting password: %s", p->valueBuffer);
					p = _iotWebConf.getApPasswordParameter();
					strcpy(p->valueBuffer, TAG); // reset to default AP password
					_iotWebConf.saveConfig();
					resetModule();
				}
				else
				{
					logw("Received invalid json: %s", s.c_str());
				}
			}
		}
		else
		{
			Serial.read(); // discard data
		}
	}
}
