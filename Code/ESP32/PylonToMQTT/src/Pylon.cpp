#include "Pylon.h"
#include "Log.h"

Pylon::Pylon()
{
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
	loge("send_cmd: %s", raw_frame);
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

int Pylon::parseValue(char** pp, int l){
	char* buf = (char *)malloc(l+1);
	for (int i =0; i < l; i++) {
		buf[i] = **pp;
		*pp += 1;
	}
	buf[l] = 0;
	int number = (int)strtol(buf, NULL, 16);
	free(buf);
	return number;
}

int Pylon::ParseResponse(char *szResponse, size_t readNow, CommandInformation cmd)
{
	if(readNow > 0 && szResponse[0] != '\0')
	{
		logd("received: %d", readNow);
		logd("data: %s", szResponse);

		char* eptr = &szResponse[readNow-4];
		char** pp = &eptr;
		uint16_t CHKSUM = parseValue(pp, 4);
		uint16_t sum = 0;
		for (int i = 1; i < readNow-4; i++) {
			sum += szResponse[i];
		}
		if (((CHKSUM+sum) & 0xFFFF) != 0) { 
			uint16_t c = ~sum + 1;
			loge("Checksum failed: %04x, should be: %04X", sum, c); 
			return -1;
		} 
			else { 
				logi("Checksum passed"); 
		}
		char* ptr = &szResponse[1]; // skip SOI (~)
		pp = &ptr;
		uint16_t VER = parseValue(pp, 2);
		uint16_t ADR = parseValue(pp, 2);
		uint16_t CID1 = parseValue(pp, 2);
		uint16_t CID2 = parseValue(pp, 2);
		uint16_t LENGTH = parseValue(pp, 4);
		uint16_t LCHKSUM = LENGTH & 0xF000;
		uint16_t LENID = LENGTH & 0x0FFF;
		if (readNow < (LENID + 17)) {  
			loge("Data length error LENID: %d, Received: %d", LENID, (readNow-17));
			return -1;
		}
		if (LENID > 0) {
			String hex = ptr;
			String part = hex.substring(0,LENID);
			logd("INFO: %s", part.c_str());
		}
		logd("VER: %02X, ADR: %02X, CID1: %02X, CID2: %02X, LENID: %02X (%d), CHKSUM: %02X", VER, ADR, CID1, CID2, LENID, LENID, CHKSUM);
		if (CID2 != ResponseCode::Normal) {
			loge("CID2 error code: %02X", CID2);
			return -1;
		}
		StaticJsonDocument<2048> root;
		String s;
		switch (cmd) {
			case CommandInformation::AnalogValueFixedPoint:
			{
				uint16_t INFO = parseValue(pp,4);
				uint16_t packNumber = INFO & 0x00FF;
				logd("AnalogValueFixedPoint: INFO: %04X, Pack: %d", INFO, packNumber);
				root.clear();
				char* startOfModuleData = ptr;
				JsonArray modules = root.createNestedArray("Modules");
				logd("initial data size: %d", eptr - ptr);
				// while (ptr < eptr) {
					JsonObject module = modules.createNestedObject();
					char key[16];
					uint16_t CELLS = parseValue(pp, 2);
					logd("CELLS: %d", CELLS);
					for (int i = 0; i < CELLS; i++) {
						sprintf(key, "Cell%d", i+1);
						module[key] = parseValue(pp, 4)/1000.0;
					}
					uint16_t TEMPS = parseValue(pp,2);
					logd("TEMPS: %d", TEMPS);
					for (int i = 0; i < (TEMPS); i++) {

						sprintf(key, "Temp%d", i+1);
						modules[key] = parseValue(pp,4)/100.0;
					}
					module["BatCurrent"] = (parseValue(pp, 4)/1000.0);
					module["BatVoltage"] = (parseValue(pp, 4)/1000.0);
					int remain = parseValue(pp, 4);
					logd("remain: %d", remain);
					module["RemainingCapacity"] = (remain/100.0);	
					ptr += 2; // skip 02 userdef
					int total = parseValue(pp, 4);
					logd("total: %d", total);
					module["TotalCapacity"] = (total/100.0);
					module["CycleCount"] = parseValue(pp, 4);
					module["SOC"] = (remain * 100) / total;	
					module["LAST"] = (parseValue(pp, 6));
				// }
				serializeJson(root, s);
				_pcb("readings", s.c_str(), false);

			}
			break;
			case CommandInformation::GetVersionInfo:
				logi("GetVersionInfo");
			break;
			case CommandInformation::AlarmInfo:
				logi("GetAlarm");
			break;
			case CommandInformation::GetBarCode:
				logi("GetBarCode");
			break;
			case CommandInformation::GetPackCount:
			{
				uint8_t num = parseValue(pp, LENID);
				logi("GetPackCount: %d", num);
			}
			break;
		}

	}
	return 0;
}