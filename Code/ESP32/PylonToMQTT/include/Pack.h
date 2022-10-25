#pragma once
#include "Arduino.h"
#include "ArduinoJson.h"
#include <vector>
#include "IOTCallbackInterface.h"

using namespace std;

namespace PylonToMQTT
{
class Pack {
  public:
	Pack(const std::string& name, std::vector<string>* tempKeys, IOTCallbackInterface* pcb) { _name = name; _pTempKeys = tempKeys; _pcb = pcb; };

    std::string Name() {
      return _name;
    }
    void setBarcode(const std::string& bc) {
      _barCode = bc;
    }

    void setVersionInfo(const std::string& ver) {
      _versionInfo = ver;
    }

    void setNumberOfCells(int val) {
      _numberOfCells = val;
    }

    void setNumberOfTemps(int val) {
      _numberOfTemps = val;
    }
 
    void PublishDiscovery();

    bool InfoPublished() {
        return _infoPublised;
    }
    void SetInfoPublished() {
      _infoPublised = true;
    }

protected:
    // <discovery_prefix>/<component>/<object_id>/config -> homeassistant/sensor/1FC220_Pack2_SOC/config
    // object_id -> ESP<uniqueId>_<pack>_<entity>
    void PublishDiscoverySub(const char *component, const char *entityName, const char *jsonElement, const char *device_class, const char *unit_of_meas, const char *icon = "");
    void PublishTempsDiscovery();
    void PublishCellsDiscovery();
    bool ReadyToPublish() {
        return (!_discoveryPublished && InfoPublished());
    }


private:
    std::string _name;
    std::string _barCode;
    std::string _versionInfo;
    boolean _infoPublised = false;
    boolean _discoveryPublished = false;
    IOTCallbackInterface* _pcb;
    std::vector<string>* _pTempKeys;
    int _numberOfCells = 0;
    int _numberOfTemps = 0;
};
}