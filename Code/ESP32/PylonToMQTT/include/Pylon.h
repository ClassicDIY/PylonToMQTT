#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "log.h"
#include "Enumerations.h"
#include "AsyncSerial.h"



class Pylon
{
    
    typedef void(*PublishCallback)(const char *subtopic, const char *value, boolean retained);
 public:

	Pylon();
    void begin(AsyncSerial* ser, PublishCallback pcb) { _asyncSerial = ser; _pcb = pcb; };
    void loop();
    int ParseResponse(char *szResponse, size_t readNow, CommandInformation cmd);
    void Next();


 protected:
 	StaticJsonDocument<2048> _root;
    uint8_t _commandIndex = 0;
    uint8_t _numberOfPacks = 0;
    uint8_t _currentPack = 0;
    AsyncSerial* _asyncSerial;
    PublishCallback _pcb;
    CommandInformation _currentCommand = CommandInformation::None;
    uint16_t get_frame_checksum(char* frame);
    int get_info_length(const char* info);
    void encode_cmd(char* frame, uint8_t address, uint8_t cid2, const char* info);
    
    String convert_ASCII(char* p);
    int parseValue(char** pp, int l);
    void send_cmd(uint8_t address, CommandInformation cmd);
};