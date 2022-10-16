
#pragma once

#define CONFIG_VERSION "V1.1.3" // major.minor.build (major or minor will invalidate the configuration)
#define HOME_ASSISTANT_PREFIX "homeassistant" // MQTT prefix used in autodiscovery
#define STR_LEN 255                            // general string buffer size
#define CONFIG_LEN 32                         // configuration string buffer size
#define NUMBER_CONFIG_LEN 6
#define AP_TIMEOUT 30000
#define MAX_PUBLISH_RATE 30000
#define MIN_PUBLISH_RATE 1000
#define CheckBit(var,pos) ((var) & (1<<(pos))) ? true : false
#define toShort(i, v) (v[i++]<<8) | v[i++]

#define TempKeys std::string _tempKeys[] = { "CellTemp1_4", "CellTemp5_8", "CellTemp9_12", "CellTemp13_16", "MOS_T", "ENV_T"};
