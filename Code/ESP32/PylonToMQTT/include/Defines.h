
#pragma once

#define WATCHDOG_TIMER 600000 //time in ms to trigger the watchdog
#define COMMAND_PUBLISH_RATE 500 // delay between sequence of pylon commands sent to battery
#define SERIAL_RECEIVE_TIMEOUT 3000 // time in ms to wait for serial data from battery

#define STR_LEN 255 // general string buffer size
#define CONFIG_LEN 32 // configuration string buffer size
#define NUMBER_CONFIG_LEN 6
#define DEFAULT_AP_PASSWORD "12345678"

#define MAX_PUBLISH_RATE 30000
#define MIN_PUBLISH_RATE 1000
#define CheckBit(var,pos) ((var) & (1<<(pos))) ? true : false
#define toShort(i, v) (v[i++]<<8) | v[i++]

#define TempKeys std::string _tempKeys[] = { "CellTemp1_4", "CellTemp5_8", "CellTemp9_12", "CellTemp13_16", "MOS_T", "ENV_T"};
