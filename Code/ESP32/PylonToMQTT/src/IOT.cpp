#include "IOT.h"
#include <sys/time.h>
#include <EEPROM.h>
#include "time.h"
#include "Log.h"
#include "HelperFunctions.h"
#include "IotWebConfOptionalGroup.h"
#include <IotWebConfTParameter.h>

namespace PylonToMQTT
{
	AsyncMqttClient _mqttClient;
	TimerHandle_t mqttReconnectTimer;
	DNSServer _dnsServer;
	WebServer *_pWebServer;
	HTTPUpdateServer _httpUpdater;
	IotWebConf _iotWebConf(TAG, &_dnsServer, _pWebServer, DEFAULT_AP_PASSWORD, CONFIG_VERSION);
	unsigned long _lastBootTimeStamp = millis();
	char _willTopic[STR_LEN];
	char _rootTopicPrefix[64];
	IotWebConfParameterGroup mqttGroup = IotWebConfParameterGroup("mqtt", "MQTT configuration");
	iotwebconf::TextTParameter<IOTWEBCONF_WORD_LEN> mqttServerParam = iotwebconf::Builder<iotwebconf::TextTParameter<IOTWEBCONF_WORD_LEN>>("mqttServer").label("MQTT server").defaultValue("").build();
	iotwebconf::IntTParameter<int16_t> mqttPortParam = iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("mqttPort").label("MQTT port").defaultValue(1883).build();
	iotwebconf::TextTParameter<IOTWEBCONF_WORD_LEN> mqttUserNameParam = iotwebconf::Builder<iotwebconf::TextTParameter<IOTWEBCONF_WORD_LEN>>("mqttUserName").label("MQTT user").defaultValue("").build();
	iotwebconf::PasswordTParameter<IOTWEBCONF_WORD_LEN> mqttUserPasswordParam = iotwebconf::Builder<iotwebconf::PasswordTParameter<IOTWEBCONF_WORD_LEN>>("mqttUserPassword").label("MQTT password").defaultValue("").build();
	iotwebconf::TextTParameter<IOTWEBCONF_WORD_LEN> mqttBankNameParam = iotwebconf::Builder<iotwebconf::TextTParameter<IOTWEBCONF_WORD_LEN>>("bankName").label("Battery Bank Name").defaultValue("Bank1").build();
	iotwebconf::IntTParameter<int16_t> publishRateParam = iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("publishRateStr").label("Publish Rate (S)").defaultValue(2).min(1).max(30).build();

	void onMqttConnect(bool sessionPresent)
	{
		logi("Connected to MQTT. Session present: %d", sessionPresent);
		char buf[64];
		sprintf(buf, "%s/cmnd/#", _rootTopicPrefix);
		_mqttClient.subscribe(buf, 0);
		_iot.IOTCB()->onMqttConnect(sessionPresent);
		_mqttClient.publish(_willTopic, 0, true, "Offline"); // toggle online in run loop
	}

	void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
	{
		logw("Disconnected from MQTT. Reason: %d", (int8_t)reason);
		if (WiFi.isConnected())
		{
			xTimerStart(mqttReconnectTimer, 0);
		}
	}

	void connectToMqtt()
	{
		if (WiFi.isConnected())
		{
			if (strlen(mqttServerParam.value()) > 0) // mqtt configured
			{
				logd("Connecting to MQTT...");
				int len = strlen(_iotWebConf.getThingName());
				strncpy(_rootTopicPrefix, _iotWebConf.getThingName(), len);
				if (_rootTopicPrefix[len - 1] != '/')
				{
					strcat(_rootTopicPrefix, "/");
				}
				strcat(_rootTopicPrefix, mqttBankNameParam.value());

				sprintf(_willTopic, "%s/tele/LWT", _rootTopicPrefix);
				_mqttClient.setWill(_willTopic, 0, true, "Offline");
				_mqttClient.connect();
				logd("rootTopicPrefix: %s", _rootTopicPrefix);
			}
		}
	}

	void WiFiEvent(WiFiEvent_t event)
	{
		logd("[WiFi-event] event: %d", event);
		String s;
		JsonDocument doc;
		switch (event)
		{
		case SYSTEM_EVENT_STA_GOT_IP:
			// logd("WiFi connected, IP address: %s", WiFi.localIP().toString().c_str());
			doc["IP"] = WiFi.localIP().toString().c_str();
			doc["ApPassword"] = DEFAULT_AP_PASSWORD;
			serializeJson(doc, s);
			s += '\n';
			Serial.printf(s.c_str()); // send json to flash tool
			configTime(0, 0, NTP_SERVER);
			printLocalTime();
			xTimerStart(mqttReconnectTimer, 0);
			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			logw("WiFi lost connection");
			xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
			break;
		default:
			break;
		}
	}

	void onMqttPublish(uint16_t packetId)
	{
		logd("Publish acknowledged.  packetId: %d", packetId);
	}

	void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
	{
		logd("MQTT Message arrived [%s]  qos: %d len: %d index: %d total: %d", topic, properties.qos, len, index, total);
		printHexString(payload, len);
		JsonDocument doc;
		DeserializationError err = deserializeJson(doc, payload);
		if (err) // not json!
		{
			logd("MQTT payload {%s} is not valid JSON!", payload);
		}
		else
		{
			if (doc.containsKey("status"))
			{
				doc.clear();
				doc["name"] = mqttBankNameParam.value();
				doc["sw_version"] = CONFIG_VERSION;
				doc["IP"] = WiFi.localIP().toString().c_str();
				doc["SSID"] = WiFi.SSID();
				doc["uptime"] = formatDuration(millis() - _lastBootTimeStamp);
				_iot.Publish("status", doc, true);
			}
		}
	}

	IOT::IOT(WebServer *pWebServer)
	{
		_pWebServer = pWebServer;
	}

	void getSettingsHTML()
	{
		if (_iotWebConf.handleCaptivePortal()) // -- Let IotWebConf test and handle captive portal requests.
		{
			logd("Captive portal"); // -- Captive portal request were already served.
			return;
		}
		logd("handleSettings");
		String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
		s += "<title>";
		s += _iotWebConf.getThingName();
		s += "</title></head><body>";
		s += "<h2>";
		s += _iotWebConf.getThingName();
		s += " Settings</h2><hr><p>";
		s += _iot.IOTCB()->getSettingsHTML();
		s += "</p>";
		s += "MQTT:";
		s += "<ul>";
		s += htmlConfigEntry<char *>(mqttServerParam.label, mqttServerParam.value());
		s += htmlConfigEntry<int16_t>(mqttPortParam.label, mqttPortParam.value());
		s += htmlConfigEntry<char *>(mqttUserNameParam.label, mqttUserNameParam.value());
		s += htmlConfigEntry<const char *>(mqttUserPasswordParam.label, strlen(mqttUserPasswordParam.value()) > 0 ? "********" : "");
		s += htmlConfigEntry<char *>(mqttBankNameParam.label, mqttBankNameParam.value());
		s += htmlConfigEntry<int16_t>(publishRateParam.label, publishRateParam.value());
		s += "</ul>";
		s += "<p>Go to <a href='config'>configure page</a> to change values.</p>";
		s += "<p>Log in with 'admin', AP password (default is 12345678)</p>";
		s += "</body></html>\n";
		_pWebServer->send(200, "text/html", s);
	}

	void configSaved()
	{
		logi("Configuration was updated.");
	}

	boolean formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper)
	{
		if (_iot.IOTCB()->validate(webRequestWrapper) == false)
			return false;
		int mqttServerParamLength = _pWebServer->arg(mqttServerParam.getId()).length();
		if (mqttServerParamLength == 0)
		{
			mqttServerParam.errorMessage = "MQTT server is required";
			return false;
		}
		int rate = _pWebServer->arg(publishRateParam.getId()).toInt() * 1000;
		if (rate < MIN_PUBLISH_RATE || rate > MAX_PUBLISH_RATE)
		{
			publishRateParam.errorMessage = "Invalid publish rate.";
			return false;
		}
		return true;
	}

	void IOT::Init(IOTCallbackInterface *iotCB)
	{
		_iotCB = iotCB;
		pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);
		_iotWebConf.setStatusPin(WIFI_STATUS_PIN);

		// setup EEPROM parameters
		mqttGroup.addItem(&mqttServerParam);
		mqttGroup.addItem(&mqttPortParam);
		mqttGroup.addItem(&mqttUserNameParam);
		mqttGroup.addItem(&mqttUserPasswordParam);
		mqttGroup.addItem(&mqttBankNameParam);
		mqttGroup.addItem(&publishRateParam);

		_iotWebConf.addSystemParameter(&mqttGroup);
		if (_iotCB->parameterGroup() != NULL)
		{
			_iotWebConf.addParameterGroup(_iotCB->parameterGroup());
		}
		_iotWebConf.getApTimeoutParameter()->visible = true;

		// setup callbacks for IotWebConf
		_iotWebConf.setConfigSavedCallback(&configSaved);
		_iotWebConf.setFormValidator(&formValidator);
		_iotWebConf.setupUpdateServer(
			[](const char *updatePath)
			{ _httpUpdater.setup(_pWebServer, updatePath); },
			[](const char *userName, char *password)
			{ _httpUpdater.updateCredentials(userName, password); });

		if (digitalRead(FACTORY_RESET_PIN) == LOW)
		{
			EEPROM.begin(IOTWEBCONF_CONFIG_START + IOTWEBCONF_CONFIG_VERSION_LENGTH);
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
			mqttServerParam.applyDefaultValue();
			mqttPortParam.applyDefaultValue();
			mqttUserNameParam.applyDefaultValue();
			mqttUserPasswordParam.applyDefaultValue();
			mqttBankNameParam.applyDefaultValue();
			_iotWebConf.resetWifiAuthInfo();
		}
		else
		{
			logi("wait in AP mode for %d seconds", _iotWebConf.getApTimeoutMs() / 1000);
			if (mqttServerParam.value()[0] != '\0') // skip if factory reset
			{
				logd("Valid configuration!");
				_clientsConfigured = true;
				// setup MQTT
				_mqttClient.onConnect(onMqttConnect);
				_mqttClient.onDisconnect(onMqttDisconnect);
				_mqttClient.onMessage(onMqttMessage);
				_mqttClient.onPublish(onMqttPublish);
				IPAddress ip;
				if (ip.fromString(mqttServerParam.value()))
				{
					_mqttClient.setServer(ip, mqttPortParam.value());
				}
				else
				{
					_mqttClient.setServer(mqttServerParam.value(), mqttPortParam.value());
				}
				_mqttClient.setCredentials(mqttUserNameParam.value(), mqttUserPasswordParam.value());
			}
		}
		// generate unique id from mac address NIC segment
		uint8_t chipid[6];
		esp_efuse_mac_get_default(chipid);
		_uniqueId = chipid[3] << 16;
		_uniqueId += chipid[4] << 8;
		_uniqueId += chipid[5];
		// Set up required URL handlers on the web server.
		_pWebServer->on("/", getSettingsHTML);
		_pWebServer->on("/config", []()
						{ _iotWebConf.handleConfig(); });
		_pWebServer->onNotFound([]()
								{ _iotWebConf.handleNotFound(); });
	}

	boolean IOT::Run()
	{
		bool rVal = false;
		_iotWebConf.doLoop();
		if (_clientsConfigured && WiFi.isConnected())
		{
			rVal = _mqttClient.connected();
		}
		else
		{
			if (Serial.peek() == '{')
			{
				String s = Serial.readStringUntil('}');
				s += "}";
				JsonDocument doc;
				DeserializationError err = deserializeJson(doc, s);
				if (err)
				{
					loge("deserializeJson() failed: %s", err.c_str());
				}
				else
				{
					if (doc["ssid"].is<const char *>() && doc["password"].is<const char *>())
					{
						iotwebconf::Parameter *p = _iotWebConf.getWifiSsidParameter();
						strcpy(p->valueBuffer, doc["ssid"]);
						logd("Setting ssid: %s", p->valueBuffer);
						p = _iotWebConf.getWifiPasswordParameter();
						strcpy(p->valueBuffer, doc["password"]);
						logd("Setting password: %s", p->valueBuffer);
						p = _iotWebConf.getApPasswordParameter();
						strcpy(p->valueBuffer, DEFAULT_AP_PASSWORD); // reset to default AP password
						_iotWebConf.saveConfig();
						esp_restart();
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
		return rVal;
	}

	unsigned long IOT::PublishRate()
	{
		return publishRateParam.value() * 1000;
	}

	boolean IOT::Publish(const char *subtopic, JsonDocument &payload, boolean retained)
	{
		String s;
		serializeJson(payload, s);
		return Publish(subtopic, s.c_str(), retained);
	}

	boolean IOT::Publish(const char *subtopic, const char *value, boolean retained)
	{
		boolean rVal = false;
		if (_mqttClient.connected())
		{
			char buf[64];
			sprintf(buf, "%s/stat/%s", _rootTopicPrefix, subtopic);
			logd("Publishing to %s: %s", buf, value);
			rVal = _mqttClient.publish(buf, 0, retained, value) > 0;
			if (!rVal)
			{
				loge("**** Failed to publish MQTT message");
			}
		}
		return rVal;
	}

	boolean IOT::Publish(const char *topic, float value, boolean retained)
	{
		char buf[256];
		snprintf_P(buf, sizeof(buf), "%.1f", value);
		return Publish(topic, buf, retained);
	}

	boolean IOT::PublishMessage(const char *topic, JsonDocument &payload, boolean retained)
	{
		boolean rVal = false;
		if (_mqttClient.connected())
		{
			String s;
			serializeJson(payload, s);
			rVal = _mqttClient.publish(topic, 0, retained, s.c_str(), s.length()) > 0;
			if (!rVal)
			{
				loge("**** Configuration payload exceeds MAX MQTT Packet Size, %d [%s] topic: %s", s.length(), s.c_str(), topic);
			}
		}
		return rVal;
	}

	boolean IOT::PublishHADiscovery(const char *bank, JsonDocument &payload)
	{
		boolean rVal = false;
		if (_mqttClient.connected())
		{
			char topic[64];
			sprintf(topic, "%s/device/%s/config", HOME_ASSISTANT_PREFIX, bank);
			rVal = PublishMessage(topic, payload, true);
		}
		return rVal;
	}

	std::string IOT::getRootTopicPrefix()
	{
		std::string s(_rootTopicPrefix);
		return s;
	};

	std::string IOT::getBankName()
	{
		std::string s(mqttBankNameParam.value());
		return s;
	};

	std::string IOT::getThingName()
	{
		std::string s(_iotWebConf.getThingName());
		return s;
	}

	void IOT::Online()
	{
		if (!_publishedOnline)
		{
			_publishedOnline = _mqttClient.publish(_willTopic, 0, true, "Online");
		}
	}

} // namespace PylonToMQTT