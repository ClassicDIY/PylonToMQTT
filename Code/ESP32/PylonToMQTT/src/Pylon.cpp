#include <vector>
#include "Pylon.h"
#include "Log.h"

CommandInformation _commands[] = { CommandInformation::GetVersionInfo, CommandInformation::GetBarCode, CommandInformation::AnalogValueFixedPoint, CommandInformation::AlarmInfo };
std::string _tempKeys[] = { "CellTemp1~4", "CellTemp5~8", "CellTemp9~12", "CellTemp13~16", "MOS_T", "ENV_T"};

Pylon::Pylon()
{
}

bool Pylon::loop() {
	bool sequenceComplete = false;
    if (_numberOfPacks == 0){
		_root.clear();
	    send_cmd(0xFF, CommandInformation::GetPackCount);
    }
    else {
		send_cmd(_currentPack+1, _commands[_commandIndex]);
		sequenceComplete = Next();
    }
	return sequenceComplete;
}

bool Pylon::Next() {
	bool sequenceComplete = false;
	_commandIndex++;
	if (_commandIndex == sizeof(_commands)){
		_commandIndex = 0;
		sequenceComplete = true;
		if (_root.size() > 0) {
			String s;
			serializeJson(_root, s);
			_root.clear();
			char buf[64];
			sprintf(buf, "readings/pack-%d", _currentPack+1); 
			_pcb(buf, s.c_str(), false);
		}
		_currentPack++;
		_currentPack %= _numberOfPacks;
	}
	return sequenceComplete;
}

uint16_t Pylon::get_frame_checksum(char* frame){
	uint16_t sum = 0;
	uint16_t len = strlen(frame);
	for (int i = 0; i < len; i++) {
		sum += frame[i];
	}
	sum = ~sum;
	sum %= 0x10000;
	sum += 1;
	return sum;
}

int Pylon::get_info_length(const char* info) {
	size_t lenid = strlen(info);
	if (lenid == 0)
		return 0;
	int lenid_sum = (lenid & 0xf) + ((lenid >> 4) & 0xf) + ((lenid >> 8) & 0xf);
	int lenid_modulo = lenid_sum % 16;
	int lenid_invert_plus_one = 0b1111 - lenid_modulo + 1;
	return (lenid_invert_plus_one << 12) + lenid;
}

void Pylon::encode_cmd(char* frame, uint8_t address, uint8_t cid2, const char* info) {
	char sub_frame[64];
	uint8_t cid1 = 0x46;
	sprintf(sub_frame, "%02X%02X%02X%02X%04X", 0x25, address, cid1, cid2, get_info_length(info));
	strcat(sub_frame, info);
	sprintf(frame, "~%s%04X\r", sub_frame, get_frame_checksum(sub_frame));
	return;
}

void Pylon::send_cmd(uint8_t address, CommandInformation cmd) {
	_currentCommand = cmd;
	char raw_frame[64];
	memset(raw_frame, 0, 64);
    char bdevid[4];
    sprintf(bdevid, "%02X", address);
	encode_cmd(raw_frame, address, cmd, bdevid);
	logd("send_cmd: %s", raw_frame);
    _asyncSerial->Send(cmd, (byte*)raw_frame, strlen(raw_frame));
}


String Pylon::convert_ASCII(char* p){
   String ascii = "";
   String hex = p;
   for (size_t i = 0; i < strlen(p); i += 2){
      String part = hex.substring(i, 2);
      ascii += strtol(part.c_str(), nullptr, 16);
   }
   return ascii;
}

#define toShort(i, v) (v[i++]<<8) | v[i++]

unsigned char parse_hex(char c)
{
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    return 0;
}

std::vector<unsigned char> parse_string(const std::string & s)
{
    size_t size = s.size();
    if (size % 2 != 0) size++;
    std::vector<unsigned char> result(size / 2);
    for (std::size_t i = 0; i != size / 2; ++i)
        result[i] = 16 * parse_hex(s[2 * i]) + parse_hex(s[2 * i + 1]);
    return result;
}

int Pylon::ParseResponse(char *szResponse, size_t readNow, CommandInformation cmd)
{
	if(readNow > 0 && szResponse[0] != '\0')
	{
		logd("received: %d", readNow);
		logd("data: %s", szResponse);
        std::string chksum; 
        chksum.assign(&szResponse[readNow-4]);
        std::vector<unsigned char> cs = parse_string(chksum);
        int i = 0;
        uint16_t CHKSUM = toShort(i, cs);
        uint16_t sum = 0;
		for (int i = 1; i < readNow-4; i++) {
			sum += szResponse[i];
		}
		if (((CHKSUM+sum) & 0xFFFF) != 0) { 
			uint16_t c = ~sum + 1;
			loge("Checksum failed: %04x, should be: %04X", sum, c); 
			return -1;
		} 
        std::string frame;
        frame.assign(&szResponse[1]); // skip SOI (~)
        std::vector<unsigned char> v = parse_string(frame);
        int index = 0;
		uint16_t VER = v[index++];
		uint16_t ADR = v[index++];
		uint16_t CID1 = v[index++];
		uint16_t CID2 = v[index++];
		uint16_t LENGTH = toShort(index, v);
		uint16_t LCHKSUM = LENGTH & 0xF000;
		uint16_t LENID = LENGTH & 0x0FFF;
		if (readNow < (LENID + 17)) {  
			loge("Data length error LENGTH: %04X LENID: %04X, Received: %d", LENGTH, LENID, (readNow-17));
			return -1;
		}
		logd("VER: %02X, ADR: %02X, CID1: %02X, CID2: %02X, LENID: %02X (%d), CHKSUM: %02X", VER, ADR, CID1, CID2, LENID, LENID, CHKSUM);
		if (CID2 != ResponseCode::Normal) {
			loge("CID2 error code: %02X", CID2);
			return -1;
		}
		switch (cmd) {
			case CommandInformation::AnalogValueFixedPoint:
			{
				uint16_t INFO = toShort(index, v);
				uint16_t packNumber = INFO & 0x00FF;
				logi("AnalogValueFixedPoint: INFO: %04X, Pack: %d", INFO, packNumber);
				JsonObject cells = _root.createNestedObject("Cells");
				char key[16];
				uint16_t CELLS = v[index++];
				for (int i = 0; i < CELLS; i++) {
					sprintf(key, "Cell-%d", i+1);
					JsonObject cell = cells.createNestedObject(key);
					cell["Reading"] = (toShort(index, v))/1000.0;
					cell["State"] = 0;  // default to ok
				}
				JsonObject temps = _root.createNestedObject("Temps");
				uint16_t TEMPS = v[index++];
				for (int i = 0; i < TEMPS; i++) {
					if ( i < _tempKeys->length()) {
						JsonObject entry = temps.createNestedObject(_tempKeys[i]);
						entry["Reading"] = (toShort(index, v))/100.0;
						entry["State"] = 0;  // default to ok
					}
				}
				_root["PackCurrent"] = (toShort(index, v))/1000.0;
				_root["PackVoltage"] = (toShort(index, v))/1000.0;
				int remain = toShort(index, v);
				_root["RemainingCapacity"] = (remain/100.0);
				index++; //skip user def code	
				int total = toShort(index, v);
				_root["FullCapacity"] = (total/100.0);
				_root["CycleCount"] = ((v[index++]<<8) | v[index++]);
				_root["SOC"] = (remain * 100) / total;	
				// module["LAST"] = ((v[index++]<<8) | (v[index++]<<8) | v[index++]); 
			}
			break;
			case CommandInformation::GetVersionInfo: {
				logi("GetVersionInfo");
				std::string ver;
				std::string s(v.begin(), v.end());
				ver = s.substr(index);
				_root["Version"] = ver;
			}
			break;
			case CommandInformation::AlarmInfo:
				logi("GetAlarm");
                // uint16_t INFO = toShort(index, v);
				// uint16_t packNumber = INFO & 0x00FF;
			break;
			case CommandInformation::GetBarCode: {
				logi("GetBarCode");
				std::string bc;
				std::string s(v.begin(), v.end());
				bc = s.substr(index);
				_root["BarCode"] = bc;
			}
			break;
			case CommandInformation::GetPackCount:
			{
                _numberOfPacks = v[index];
                _numberOfPacks > 8 ? 1 : _numberOfPacks; // max 8, default to 1
				_root.clear();
				logi("GetPackCount: %d", _numberOfPacks);
			}
			break;
		}
	}
	return 0;
}
