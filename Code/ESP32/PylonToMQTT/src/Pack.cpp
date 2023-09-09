#include "Log.h"
#include "Defines.h"
#include "Pack.h"

namespace PylonToMQTT
{

void Pack::PublishDiscovery()
{
    if (ReadyToPublish()) {
        logd("Publishing discovery for %s", Name().c_str());
        PublishDiscoverySub("sensor", "PackVoltage", "PackVoltage.Reading", "voltage", "V", "mdi:lightning-bolt");
        PublishDiscoverySub("sensor", "PackCurrent", "PackCurrent.Reading", "current", "A", "mdi:current-dc");
        PublishDiscoverySub("sensor", "SOC", "SOC", "battery", "%");
        PublishDiscoverySub("sensor", "RemainingCapacity", "RemainingCapacity", "current", "Ah", "mdi:ev-station");
        PublishTempsDiscovery();
        PublishCellsDiscovery();
        _discoveryPublished = true;
    }
}

void Pack::PublishTempsDiscovery() {
	char jsonElement[STR_LEN];
	for (int i = 0; i < _numberOfTemps; i++) {
        if (i < _pTempKeys->size()) {
            sprintf(jsonElement, "Temps.%s.Reading", _pTempKeys->at(i).c_str());
            PublishDiscoverySub("sensor",  _pTempKeys->at(i).c_str(), jsonElement, "temperature", "°C");
        }
	}
}

void Pack::PublishCellsDiscovery() {
	char entityName[STR_LEN];
    char jsonElement[STR_LEN];
	for (int i = 0; i < _numberOfCells; i++) {
        sprintf(entityName, "Cell_%d", i+1);
        sprintf(jsonElement, "Cells.Cell_%d.Reading", i+1);
        PublishDiscoverySub("sensor",  entityName, jsonElement, "voltage", "V", "mdi:lightning-bolt");
	}
}

// <discovery_prefix>/<component>/<object_id>/config -> homeassistant/sensor/1FC220_Pack2_SOC/config
// object_id -> ESP<uniqueId>_<pack>_<entity>
void Pack::PublishDiscoverySub(const char *component, const char *entity, const char *jsonElement, const char *device_class, const char *unit_of_meas, const char *icon)
{
	char buffer[STR_LEN];
	StaticJsonDocument<1024> doc; // MQTT discovery
	doc["device_class"] = device_class;
	doc["unit_of_measurement"] = unit_of_meas;
	doc["state_class"] = "measurement";

	doc["name"] = entity;
	if (strlen(icon) > 0) {
		doc["icon"] = icon;
	}

	sprintf(buffer, "%s/stat/readings/%s", _pcb->getRootTopicPrefix().c_str(), Name().c_str());
	doc["state_topic"] = buffer;

	sprintf(buffer, "ESP%X_%s_%s", _pcb->getUniqueId(), Name().c_str(), entity);
	doc["unique_id"] = buffer;
	String object_id = buffer;

	sprintf(buffer, "{{ value_json.%s }}", jsonElement);
	doc["value_template"] = buffer;

	sprintf(buffer, "%s/tele/LWT", _pcb->getRootTopicPrefix().c_str());
	doc["availability_topic"] = buffer;
    doc["pl_avail"] = "Online";
    doc["pl_not_avail"] = "Offline";
	JsonObject device = doc.createNestedObject("device");
	device["name"] = Name();
	device["via_device"] = _pcb->getThingName();
	device["hw_version"] = _barCode.substr(0, 15);
	device["sw_version"] = CONFIG_VERSION;
	device["manufacturer"] = "ClassicDIY";
	device["model"] = _versionInfo.substr(0, 19);
	sprintf(buffer, "%s_%s", Name().c_str(), _barCode.substr(0, 15).c_str());
	device["identifiers"] = buffer;
	sprintf(buffer, "%s/%s/%s/config", HOME_ASSISTANT_PREFIX, component, object_id.c_str());
	_pcb->PublishMessage(buffer, doc);
}
}