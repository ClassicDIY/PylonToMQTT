#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Log.h"
#include "Enumerations.h"
#include "IOTCallbackInterface.h"
#include "AsyncSerial.h"
#include "Pack.h"
#include "Defines.h"

namespace PylonToMQTT
{

class Pylon : public AsyncSerialCallbackInterface
{
    
public:
	Pylon();
    ~Pylon();
    void begin(IOTCallbackInterface* pcb) { _pcb = pcb; _asyncSerial->begin(this, BAUDRATE, SERIAL_8N1, RXPIN, TXPIN);};
    void Receive(int timeOut) { _asyncSerial->Receive(timeOut); };
    bool Transmit();
    int ParseResponse(char *szResponse, size_t readNow, CommandInformation cmd);


    //AsyncSerialCallbackInterface
    void complete() { 
        ParseResponse((char*)_asyncSerial->GetContent(), _asyncSerial->GetContentLength(), _asyncSerial->GetToken());
    };
	void overflow() {
        loge("AsyncSerial: overflow");
    };
	void timeout() {
        loge("AsyncSerial: timeout");
    };

 protected:
 	StaticJsonDocument<4096> _root;
    uint8_t _infoCommandIndex = 0;
    uint8_t _readingsCommandIndex = 0;
    uint8_t _numberOfPacks = 0;
    uint8_t _currentPack = 0;
    AsyncSerial* _asyncSerial;
    IOTCallbackInterface* _pcb;
    CommandInformation _currentCommand = CommandInformation::None;

    uint16_t get_frame_checksum(char* frame);
    int get_info_length(const char* info);
    void encode_cmd(char* frame, uint8_t address, uint8_t cid2, const char* info);
    String convert_ASCII(char* p);
    int parseValue(char** pp, int l);
    void send_cmd(uint8_t address, CommandInformation cmd);

private:
    std::vector<Pack> _Packs;
    std::vector<string> _TempKeys;
};
} // namespace PylonToMQTT