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
    bool ReadyToPublish() {
        return (!_discoveryPublished && InfoPublished() && _numberOfTemps > 0 && _numberOfCells > 0);
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