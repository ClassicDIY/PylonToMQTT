#!/usr/bin/env python

# --------------------------------------------------------------------------- # 
# Handle creating the Json packaging of the Pylon data.
# 
# --------------------------------------------------------------------------- # 


import json
import logging
import datetime

log = logging.getLogger("PylonToMQTT")

# --------------------------------------------------------------------------- # 
# Handle creating the Json for Readings
# --------------------------------------------------------------------------- # 
def encodePylon_info(vi, bc):

    pylonData = {}
    pylonData["Version"] = vi.Version
    pylonData["BarCode"] = bc.Barcode
    return json.dumps(pylonData, sort_keys=False, separators=(',', ':'))

def encodePylon_readings(decoded, ai):

    pylonData = {}
    cells = {}
    numberOfCells = decoded.NumberOfCells
    for c in range(numberOfCells):
        cellData = {}
        cellData["Reading"] = decoded.CellVoltages[c]
        cellData["State"] = ai.CellState[c]
        key = "Cell_{}"
        cells[key.format(c+1)] = cellData
    pylonData["Cells"] = cells
    numberOfTemperatures = decoded.NumberOfTemperatures
    temperatures = {}
    for t in range(numberOfTemperatures):
        temperatureData = {}
        temperatureData["Reading"] = decoded.GroupedCellsTemperatures[t]
        temperatureData["State"] = ai.CellsTemperatureStates[t]
        key = "CellTemp_{}"
        temperatures[key.format(t+1)] = temperatureData
    pylonData["Temps"] = temperatures

    current = {}
    current["Reading"] = decoded.Current
    current["State"] = ai.CurrentState
    pylonData["PackCurrent"] = current

    voltage = {}
    voltage["Reading"] = decoded.Voltage
    voltage["State"] = ai.VoltageState
    pylonData["PackVoltage"] = voltage

    pylonData["RemainingCapacity"] = decoded.RemainingCapacity
    pylonData["FullCapacity"] = decoded.TotalCapacity
    pylonData["CycleCount"] = decoded.CycleNumber
    pylonData["SOC"] = decoded.StateOfCharge
    pylonData["Power"] = decoded.Power

    return json.dumps(pylonData, sort_keys=False, separators=(',', ':'))

