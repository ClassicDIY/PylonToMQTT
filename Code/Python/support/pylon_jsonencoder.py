#!/usr/bin/env python

# --------------------------------------------------------------------------- # 
# Handle creating the Json packaging of the Pylon data.
# 
# --------------------------------------------------------------------------- # 


import json
import logging
import datetime

log = logging.getLogger('pylon_mqtt')


# --------------------------------------------------------------------------- # 
# Handle creating the Json for Readings
# --------------------------------------------------------------------------- # 
def encodePylonData_readings(decoded):

    pylonData = {}

    # "BatTemperature":-1.99,
    pylonData["BatTemperature"] = decoded["BatTemperature"]
     # "SOC":99,
    pylonData["SOC"] = decoded["SOC"]

    # "PCBTemperature":12.71
    pylonData["PCBTemperature"] = decoded["PCBTemperature"]

    return json.dumps(pylonData, sort_keys=False, separators=(',', ':'))

