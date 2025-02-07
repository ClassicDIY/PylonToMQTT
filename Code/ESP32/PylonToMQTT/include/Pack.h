#pragma once
#include "Arduino.h"
#include "ArduinoJson.h"
#include <vector>
#include "IOTCallbackInterface.h"
#include "IOTServiceInterface.h"

using namespace std;

namespace PylonToMQTT
{
class Pack {
  public:
	Pack(const std::string& name, std::vector<string>* tempKeys, IOTServiceInterface* pcb) { _name = name; _pTempKeys = tempKeys; _psi = pcb; };

    std::string Name() {
      return _name;
    }
    std::string getBarcode() {
      if (_barCode.length() == 0) {
        return _name;
      }
      return _barCode;
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

    bool ReadyToPublish() {
        return (!_discoveryPublished && InfoPublished());
    }


private:
    std::string _name;
    std::string _barCode;
    std::string _versionInfo;
    boolean _infoPublised = false;
    boolean _discoveryPublished = false;
    IOTServiceInterface* _psi;
    std::vector<string>* _pTempKeys;
    int _numberOfCells = 0;
    int _numberOfTemps = 0;
};
}