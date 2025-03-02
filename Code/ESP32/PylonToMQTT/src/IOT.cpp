
#include <sys/time.h>
#include <EEPROM.h>
#include "time.h"
#include "Log.h"
#include "HelperFunctions.h"
#include "IOT.h"
#include "IotWebConfOptionalGroup.h"
#include <IotWebConfTParameter.h>

namespace PylonToMQTT
{

	AsyncMqttClient _mqttClient;
	TimerHandle_t mqttReconnectTimer;
	DNSServer _dnsServer;
	HTTPUpdateServer _httpUpdater;
	WebServer webServer(IOTCONFIG_PORT);
	IotWebConf _iotWebConf(TAG, &_dnsServer, &webServer, DEFAULT_AP_PASSWORD, CONFIG_VERSION);
	unsigned long _lastBootTimeStamp = millis();
	char _willTopic[STR_LEN];
	char _rootTopicPrefix[64];
	IotWebConfParameterGroup mqttGroup = IotWebConfParameterGroup("mqtt", "MQTT configuration");
	iotwebconf::TextTParameter<IOTWEBCONF_WORD_LEN> mqttServerParam = iotwebconf::Builder<iotwebconf::TextTParameter<IOTWEBCONF_WORD_LEN>>("mqttServer").label("MQTT server").defaultValue("").build();
	iotwebconf::IntTParameter<int16_t> mqttPortParam = iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("mqttPort").label("MQTT port").defaultValue(1883).build();
	iotwebconf::TextTParameter<IOTWEBCONF_WORD_LEN> mqttUserNameParam = iotwebconf::Builder<iotwebconf::TextTParameter<IOTWEBCONF_WORD_LEN>>("mqttUserName").label("MQTT user").defaultValue("").build();
	iotwebconf::PasswordTParameter<IOTWEBCONF_WORD_LEN> mqttUserPasswordParam = iotwebconf::Builder<iotwebconf::PasswordTParameter<IOTWEBCONF_WORD_LEN>>("mqttUserPassword").label("MQTT password").defaultValue("").build();
	iotwebconf::TextTParameter<IOTWEBCONF_WORD_LEN> mqttSubtopicParam = iotwebconf::Builder<iotwebconf::TextTParameter<IOTWEBCONF_WORD_LEN>>("bankName").label("Battery Bank Name").defaultValue("Bank1").build();
	iotwebconf::IntTParameter<int16_t> publishRateParam = iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("publishRateStr").label("Publish Rate (S)").defaultValue(2).min(1).max(30).build();

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
		mqttGroup.addItem(&mqttSubtopicParam);
		mqttGroup.addItem(&publishRateParam);

		_iotWebConf.addSystemParameter(&mqttGroup);
		if (_iotCB->parameterGroup() != NULL)
		{
			_iotWebConf.addParameterGroup(_iotCB->parameterGroup());
		}
		_iotWebConf.getApTimeoutParameter()->visible = true;

		// setup callbacks for IotWebConf
		_iotWebConf.setConfigSavedCallback([this]() { 
			logi("Configuration was updated."); 
		});
		_iotWebConf.setFormValidator([this](iotwebconf::WebRequestWrapper *webRequestWrapper) {
			if (IOTCB()->validate(webRequestWrapper) == false)
				return false;
			return true;
		});
		_iotWebConf.setupUpdateServer(
			[](const char *updatePath)
			{ _httpUpdater.setup(&webServer, updatePath); },
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
		mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(5000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(+[] (TimerHandle_t) {
			if (WiFi.isConnected())
			{
				if (strlen(mqttServerParam.value()) > 0) // mqtt configured
				{
					logd("Connecting to MQTT...");
					_mqttClient.connect();
				}
			}
		}));
		WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
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
				this->IOTCB()->onWiFiConnect();
				break;
			case SYSTEM_EVENT_STA_DISCONNECTED:
				logw("WiFi lost connection");
				xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
				break;
			default:
				break;
			}
		});
		boolean validConfig = _iotWebConf.init();
		if (!validConfig)
		{
			logw("!invalid configuration!");
			_iotWebConf.resetWifiAuthInfo();
			_iotWebConf.getRootParameterGroup()->applyDefaultValue();
		}
		else
		{
			logi("wait in AP mode for %d seconds", _iotWebConf.getApTimeoutMs() / 1000);
			if (mqttServerParam.value()[0] != '\0') // skip if factory reset
			{
				logd("Valid configuration!");
				_clientsConfigured = true;
				// setup MQTT
				_mqttClient.onConnect( [this](bool sessionPresent)	{ 
					logi("Connected to MQTT. Session present: %d", sessionPresent);
					char buf[64];
					sprintf(buf, "%s/cmnd/#", _rootTopicPrefix);
					_mqttClient.subscribe(buf, 0);
					IOTCB()->onMqttConnect(sessionPresent);
					_mqttClient.publish(_willTopic, 0, true, "Offline"); // toggle online in run loop
				});
				_mqttClient.onDisconnect([this](AsyncMqttClientDisconnectReason reason)	{ 
					logw("Disconnected from MQTT. Reason: %d", (int8_t)reason);
					if (WiFi.isConnected())
					{
						xTimerStart(mqttReconnectTimer, 5000);
					}
				});
				_mqttClient.onMessage([this](char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)	{ 
					logd("MQTT Message arrived [%s]  qos: %d len: %d index: %d total: %d", topic, properties.qos, len, index, total);
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
							doc["name"] = mqttSubtopicParam.value();
							doc["sw_version"] = CONFIG_VERSION;
							doc["IP"] = WiFi.localIP().toString().c_str();
							doc["SSID"] = WiFi.SSID();
							doc["uptime"] = formatDuration(millis() - _lastBootTimeStamp);
							Publish("status", doc, true);
						}
						else
						{
							IOTCB()->onMqttMessage(topic, doc);
						}
					}
				});
				_mqttClient.onPublish([this](uint16_t packetId)	{ logd("Publish acknowledged.  packetId: %d", packetId); });
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
				int len = strlen(_iotWebConf.getThingName());
				strncpy(_rootTopicPrefix, _iotWebConf.getThingName(), len);
				if (_rootTopicPrefix[len - 1] != '/')
				{
					strcat(_rootTopicPrefix, "/");
				}
				strcat(_rootTopicPrefix, mqttSubtopicParam.value());
				logd("rootTopicPrefix: %s", _rootTopicPrefix);
				sprintf(_willTopic, "%s/tele/LWT", _rootTopicPrefix);
				logd("_willTopic: %s", _willTopic);
				_mqttClient.setWill(_willTopic, 0, true, "Offline");
				
			}
		}
		// generate unique id from mac address NIC segment
		uint8_t chipid[6];
		esp_efuse_mac_get_default(chipid);
		_uniqueId = chipid[3] << 16;
		_uniqueId += chipid[4] << 8;
		_uniqueId += chipid[5];
		// Set up required URL handlers on the web server.
		webServer.on("/settings", [this]() { 
			if (_iotWebConf.handleCaptivePortal()) // -- Let IotWebConf test and handle captive portal requests.
			{
				logd("Captive portal"); // -- Captive portal request were already served.
				return;
			}
			logd("handleSettings");
			std::stringstream ss;
			ss << "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><title>";
			ss << _iotWebConf.getThingName();
			ss << "</title></head><body><h2>";
			ss << _iotWebConf.getThingName();
			ss << " Settings</h2>";
			ss << "<div style='font-size: .6em;'>Firmware config version ";
			ss << CONFIG_VERSION;
			ss << "</div><hr><p>";
			ss << IOTCB()->getSettingsHTML().c_str();
			ss << "</p> MQTT: <ul>";
			ss << htmlConfigEntry<char *>(mqttServerParam.label, mqttServerParam.value()).c_str();
			ss << htmlConfigEntry<int16_t>(mqttPortParam.label, mqttPortParam.value()).c_str();
			ss << htmlConfigEntry<char *>(mqttUserNameParam.label, mqttUserNameParam.value()).c_str();
			ss << htmlConfigEntry<const char *>(mqttUserPasswordParam.label, strlen(mqttUserPasswordParam.value()) > 0 ? "********" : "").c_str();
			ss << htmlConfigEntry<char *>(mqttSubtopicParam.label, mqttSubtopicParam.value()).c_str();
			ss << "</ul> <div style='padding-top:25px;'> <p><a href='/' onclick='javascript:event.target.port=";
			ss << ASYNC_WEBSERVER_PORT;
			ss << "'>Return to home page.</a></p>";
			ss << "<p><a href='/config' >Configuration</a><div style='font-size: .6em;'> *Log in with 'admin', AP password (default is 12345678)</div></p>";
			ss << "<p><a href='/log'  onclick='javascript:event.target.port=";
			ss << ASYNC_WEBSERVER_PORT;
			ss << "' target='_blank'>Web Log</a></p>";
			ss << "<p><a href='firmware'>Firmware update</a></p>";
			ss << "<p><a href='reboot'>Reboot ESP32</a></p>";
			ss << "</div></body></html>\n";
			std::string html = ss.str();
			webServer.send(200, "text/html", html.c_str());
		});
		webServer.on("/config", []()	{ _iotWebConf.handleConfig(); });
		webServer.on("/reboot", [this]()	{ 
			logd("resetModule");
			String page = reboot_html;
			webServer.send(200, "text/html", page.c_str());
			delay(3000);
			esp_restart(); 
		 });
		webServer.onNotFound([]() { _iotWebConf.handleNotFound(); });
		webServer.on("/", []()	{ 
			if (_iotWebConf.getState() == iotwebconf::NetworkState::NotConfigured)
			{
				_iotWebConf.handleConfig();
				return;
			}
			String page = redirect_html;
			String url = "http://";
			url += WiFi.localIP().toString();
			url += ":";
			url += ASYNC_WEBSERVER_PORT;
			page.replace("{h}", url.c_str());
			page.replace("{hp}", String(WSOCKET_HOME_PORT));
			webServer.send(200, "text/html", page.c_str());
		 });
		 if (_watchdogTimer == NULL)
		 {
			 _watchdogTimer = timerBegin(0, 80, true);					   // timer 0, div 80
			 timerAttachInterrupt(_watchdogTimer, []() { esp_restart(); }, true);	   // attach callback
			 timerAlarmWrite(_watchdogTimer, WATCHDOG_TIMER * 1000, false); // set time in us
			 timerAlarmEnable(_watchdogTimer);							   // enable interrupt
		 }
	}

	boolean IOT::Run()
	{
		bool rVal = false;
		if (_watchdogTimer != NULL)
		{
			timerWrite(_watchdogTimer, 0); // feed the watchdog
		}
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

	std::string IOT::getSubtopicName()
	{
		std::string s(mqttSubtopicParam.value());
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