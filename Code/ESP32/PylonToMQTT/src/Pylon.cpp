#include <vector>
#include "Pylon.h"
#include "Log.h"

CommandInformation _commands[] = { CommandInformation::GetVersionInfo, CommandInformation::GetBarCode, CommandInformation::AnalogValueFixedPoint, CommandInformation::AlarmInfo, CommandInformation::None };
std::string _tempKeys[] = { "CellTemp1~4", "CellTemp5~8", "CellTemp9~12", "CellTemp13~16", "MOS_T", "ENV_T"};

namespace PylonToMQTT
{

Pylon::Pylon()
{
	_asyncSerial = new AsyncSerial();
}

Pylon::~Pylon()
{
	delete _asyncSerial;
}

bool Pylon::Transmit() {
	bool sequenceComplete = false;
    if (_numberOfPacks == 0){
		_root.clear();
	    send_cmd(0xFF, CommandInformation::GetPackCount);
    }
    else {
		if (_commands[_commandIndex] != CommandInformation::None){
			send_cmd(_currentPack+1, _commands[_commandIndex]);
		}
		else {
			if (_root.size() > 0) {
				String s;
				serializeJson(_root, s);
				_root.clear();
				char buf[64];
				sprintf(buf, "readings/pack-%d", _currentPack+1); 
				_pcb->Publish(buf, s.c_str(), false);
			}
		}
		sequenceComplete = Next();
    }
	return sequenceComplete;
}

bool Pylon::Next() {
	bool sequenceComplete = false;
	_commandIndex++;
	if (_commandIndex == sizeof(_commands)){
		_commandIndex = 0;
		_currentPack++;
		if (_currentPack == _numberOfPacks){
			_currentPack = 0;
			sequenceComplete = true;
		}
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
				uint16_t numberOfCells = v[index++];
				for (int i = 0; i < numberOfCells; i++) {
					sprintf(key, "Cell-%d", i+1);
					JsonObject cell = cells.createNestedObject(key);
					cell["Reading"] = (toShort(index, v))/1000.0;
					cell["State"] = 0xF0;
				}
				JsonObject temps = _root.createNestedObject("Temps");
				uint16_t numberOfTemps = v[index++];
				for (int i = 0; i < numberOfTemps; i++) {
					if ( i < _tempKeys->length()) {
						JsonObject entry = temps.createNestedObject(_tempKeys[i]);
						entry["Reading"] = (toShort(index, v))/100.0;
						entry["State"] = 0;  // default to ok
					}
				}
				JsonObject entry = _root.createNestedObject("PackCurrent");
				entry["Reading"] = ((int16_t)toShort(index, v))/100.0;
				entry["State"] = 0;  // default to ok
				entry = _root.createNestedObject("PackVoltage");
				entry["Reading"] = (toShort(index, v))/1000.0;
				entry["State"] = 0;  // default to ok
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
			case CommandInformation::AlarmInfo: {
                uint16_t INFO = toShort(index, v);
				uint16_t packNumber = INFO & 0x00FF;
				
				JsonObject cells = _root.getMember("Cells");
				JsonObject cell = cells.getMember("Cell-1");
				logi("GetAlarm: Pack: %d", packNumber);
				char key[16];
				uint16_t numberOfCells = v[index++];
				logd("number of cells: %d", numberOfCells );
				for (int i = 0; i < numberOfCells; i++) {
					sprintf(key, "Cell-%d", i+1);
					JsonObject cell = cells.getMember(key);
					cell["State"] = v[index++];  
				}
				JsonObject temps = _root.getMember("Temps");
				uint16_t numberOfTemps = v[index++];
				for (int i = 0; i < numberOfTemps; i++) {
					if ( i < _tempKeys->length()) {
						JsonObject entry = temps.getMember(_tempKeys[i]);
						entry["State"] = v[index++]; 
					}
				}
				index++; //skip 65
				JsonObject entry = _root.getMember("PackCurrent");
				entry["State"] = v[index++];
				entry = _root.getMember("PackVoltage");
				entry["State"] = v[index++];
				uint8_t ProtectSts1 = v[index++];
				uint8_t ProtectSts2 = v[index++];
				uint8_t SystemSts = v[index++];
				uint8_t FaultSts = v[index++];
				index++; //skip 81
				index++; //skip 83
				uint8_t AlarmSts1 = v[index++];
				uint8_t AlarmSts2 = v[index++];

				JsonObject pso = _root.createNestedObject("Protect Status");
				pso["Charger OVP"] = CheckBit(ProtectSts1, 7);
				pso["SCP"] = CheckBit(ProtectSts1, 6);
				pso["DSG OCP"] = CheckBit(ProtectSts1, 5);
				pso["CHG OCP"] = CheckBit(ProtectSts1, 4);
				pso["Pack UVP"] = CheckBit(ProtectSts1, 3);
				pso["Pack OVP"] = CheckBit(ProtectSts1, 2);
				pso["Cell UVP"] = CheckBit(ProtectSts1, 1);
				pso["Cell OVP"] = CheckBit(ProtectSts1, 0);
				pso["ENV UTP"] = CheckBit(ProtectSts2, 6);
				pso["ENV OTP"] = CheckBit(ProtectSts2, 5);
				pso["MOS OTP"] = CheckBit(ProtectSts2, 4);
				pso["DSG UTP"] = CheckBit(ProtectSts2, 3);
				pso["CHG UTP"] = CheckBit(ProtectSts2, 2);
				pso["DSG OTP"] = CheckBit(ProtectSts2, 1);
				pso["CHG OTP"] = CheckBit(ProtectSts2, 0);

				JsonObject sso = _root.createNestedObject("System Status");
				sso["Fully Charged"] = CheckBit(ProtectSts2, 7);
				sso["Heater"] = CheckBit(SystemSts, 7);
				sso["AC in"] = CheckBit(SystemSts, 5);
				sso["Discharge-MOS"] = CheckBit(SystemSts, 2);
				sso["Charge-MOS"] = CheckBit(SystemSts, 1);
				sso["Charge-Limit"] = CheckBit(SystemSts, 0);
				
				JsonObject fso = _root.createNestedObject("Fault Status");
				fso["Heater Fault"] = CheckBit(FaultSts, 7);
				fso["CCB Fault"] = CheckBit(FaultSts, 6);
				fso["Sampling Fault"] = CheckBit(FaultSts, 5);
				fso["Cell Fault"] = CheckBit(FaultSts, 4);
				fso["NTC Fault"] = CheckBit(FaultSts, 2);
				fso["DSG MOS Fault"] = CheckBit(FaultSts, 1);
				fso["CHG MOS Fault"] = CheckBit(FaultSts, 0);

				JsonObject aso = _root.createNestedObject("Alarm Status");
				aso["DSG OC"] = CheckBit(AlarmSts1, 5);
				aso["CHG OC"] = CheckBit(AlarmSts1, 4);
				aso["Pack UV"] = CheckBit(AlarmSts1, 3);
				aso["Pack OV"] = CheckBit(AlarmSts1, 2);
				aso["Cell UV"] = CheckBit(AlarmSts1, 1);
				aso["Cell OV"] = CheckBit(AlarmSts1, 0);

				aso["SOC Low"] = CheckBit(AlarmSts2, 7);
				aso["MOS OT"] = CheckBit(AlarmSts2, 6);
				aso["ENV UT"] = CheckBit(AlarmSts2, 5);
				aso["ENV OT"] = CheckBit(AlarmSts2, 4);
				aso["DSG UT"] = CheckBit(AlarmSts2, 3);
				aso["CHG UT"] = CheckBit(AlarmSts2, 2);
				aso["DSG OT"] = CheckBit(AlarmSts2, 1);
				aso["CHG OT"] = CheckBit(AlarmSts2, 0);
			}
			break;
			case CommandInformation::GetBarCode: {
				logi("GetBarCode");
				std::string bc;
				std::string s(v.begin(), v.end()-2);
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
} // namespace PylonToMQTT