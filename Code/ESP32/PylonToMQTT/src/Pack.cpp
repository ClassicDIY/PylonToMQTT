#include "Log.h"
#include "Defines.h"
#include "Pack.h"

namespace PylonToMQTT
{

	void Pack::PublishDiscovery()
	{
		if (ReadyToPublish())
		{
			logd("Publishing discovery for %s", Name().c_str());
			char buffer[STR_LEN];
			JsonDocument doc;
			JsonObject device = doc["device"].to<JsonObject>();
			device["name"] = _name.c_str();
			device["sw_version"] = CONFIG_VERSION;
			device["manufacturer"] = "ClassicDIY";
			sprintf(buffer, "ESP32-Bit (%X)", _psi->getUniqueId());
			device["model"] = buffer;

			JsonObject origin = doc["origin"].to<JsonObject>();
			origin["name"] = _psi->getBankName().c_str();

			JsonArray identifiers = device["identifiers"].to<JsonArray>();
			identifiers.add(_barCode.c_str());

			JsonObject components = doc["components"].to<JsonObject>();
			JsonObject PackVoltage = components["PackVoltage"].to<JsonObject>();
			PackVoltage["platform"] = "sensor";
			PackVoltage["name"] = "PackVoltage";
			PackVoltage["device_class"] = "voltage";
			PackVoltage["unit_of_measurement"] = "V";
			PackVoltage["value_template"] = "{{ value_json.PackVoltage.Reading }}";
			sprintf(buffer, "%s_PackVoltage", _name.c_str());
			PackVoltage["unique_id"] = buffer;
			PackVoltage["icon"] = "mdi:lightning-bolt";

			JsonObject PackCurrent = components["PackCurrent"].to<JsonObject>();
			PackCurrent["platform"] = "sensor";
			PackCurrent["name"] = "PackCurrent";
			PackCurrent["device_class"] = "current";
			PackCurrent["unit_of_measurement"] = "A";
			PackCurrent["value_template"] = "{{ value_json.PackCurrent.Reading }}";
			sprintf(buffer, "%s_PackCurrent", _name.c_str());
			PackCurrent["unique_id"] = buffer;
			PackCurrent["icon"] = "mdi:current-dc";

			JsonObject SOC = components["SOC"].to<JsonObject>();
			SOC["platform"] = "sensor";
			SOC["name"] = "SOC";
			SOC["device_class"] = "battery";
			SOC["unit_of_measurement"] = "%";
			SOC["value_template"] = "{{ value_json.SOC }}";
			sprintf(buffer, "%s_SOC", _name.c_str());
			SOC["unique_id"] = buffer;

			JsonObject RemainingCapacity = components["RemainingCapacity"].to<JsonObject>();
			RemainingCapacity["platform"] = "sensor";
			RemainingCapacity["name"] = "RemainingCapacity";
			RemainingCapacity["unit_of_measurement"] = "Ah";
			RemainingCapacity["value_template"] = "{{ value_json.RemainingCapacity }}";
			sprintf(buffer, "%s_RemainingCapacity", _name.c_str());
			RemainingCapacity["unique_id"] = buffer;
			RemainingCapacity["icon"] = "mdi:ev-station";

			char jsonElement[STR_LEN];
			for (int i = 0; i < _numberOfTemps; i++)
			{
				if (i < _pTempKeys->size())
				{
					sprintf(jsonElement, "{{ value_json.Temps.%s.Reading }}", _pTempKeys->at(i).c_str());
					JsonObject TMP = components[_pTempKeys->at(i).c_str()].to<JsonObject>();
					TMP["platform"] = "sensor";
					TMP["name"] = _pTempKeys->at(i).c_str();
					TMP["device_class"] = "temperature";
					TMP["unit_of_measurement"] = "°C";
					TMP["value_template"] = jsonElement;
					sprintf(buffer, "%s_%s", _name.c_str(), _pTempKeys->at(i).c_str());
					TMP["unique_id"] = buffer;
				}
			}
		
			char entityName[STR_LEN];
			for (int i = 0; i < _numberOfCells; i++)
			{
				sprintf(entityName, "Cell_%d", i + 1);
				sprintf(jsonElement, "{{ value_json.Cells.Cell_%d.Reading }}", i + 1);
				JsonObject CELL = components[entityName].to<JsonObject>();
				CELL["platform"] = "sensor";
				CELL["name"] = entityName;
				CELL["device_class"] = "voltage";
				CELL["unit_of_measurement"] = "V";
				CELL["value_template"] = jsonElement;
				sprintf(buffer, "%s_%s", _name.c_str(), entityName);
				CELL["unique_id"] = buffer;
				CELL["icon"] = "mdi:lightning-bolt";
			}

			sprintf(buffer, "%s/stat/readings/%s", _psi->getRootTopicPrefix().c_str(), _name.c_str());
			doc["state_topic"] = buffer;
			sprintf(buffer, "%s/tele/LWT", _psi->getRootTopicPrefix().c_str());
			doc["availability_topic"] = buffer;
			doc["pl_avail"] = "Online";
			doc["pl_not_avail"] = "Offline";

			_psi->PublishHADiscovery(_name.c_str(), doc);
			_discoveryPublished = true;
		}
	}

}