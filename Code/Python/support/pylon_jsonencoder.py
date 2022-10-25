#!/usr/bin/env python

# --------------------------------------------------------------------------- # 
# Handle creating the Json packaging of the Pylon data.
# 
# --------------------------------------------------------------------------- # 


import json
import logging

log = logging.getLogger("PylonToMQTT")

# --------------------------------------------------------------------------- # 
# Handle creating the Json for Readings
# --------------------------------------------------------------------------- # 

_tempNames = {0 : 'CellTemp1_4', 1 : 'CellTemp5_8', 2 : 'CellTemp9_12', 3 : 'CellTemp13_16', 4 : 'MOS_T', 5 : 'ENV_T' }

def checkBit(var, pos):
    return (((var) & (1<<(pos))) != 0)

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
        if ai: # got alarm info?
            cellData["State"] = ai.CellState[c]
        key = "Cell_{}"
        cells[key.format(c+1)] = cellData
    pylonData["Cells"] = cells
    numberOfTemperatures = decoded.NumberOfTemperatures
    temperatures = {}
    for t in range(numberOfTemperatures):
        if t < 6: #protect lookup
            temperatureData = {}
            temperatureData["Reading"] = decoded.GroupedCellsTemperatures[t]
            if ai: # got alarm info?
                temperatureData["State"] = ai.CellsTemperatureStates[t]
            temperatures[_tempNames[t]] = temperatureData
    pylonData["Temps"] = temperatures

    current = {}
    current["Reading"] = decoded.Current
    if ai: # got alarm info?
        current["State"] = ai.CurrentState
    pylonData["PackCurrent"] = current

    voltage = {}
    voltage["Reading"] = decoded.Voltage
    if ai: # got alarm info?
        voltage["State"] = ai.VoltageState
    pylonData["PackVoltage"] = voltage

    pylonData["RemainingCapacity"] = decoded.RemainingCapacity
    pylonData["FullCapacity"] = decoded.TotalCapacity
    pylonData["CycleCount"] = decoded.CycleNumber
    pylonData["SOC"] = decoded.StateOfCharge
    pylonData["Power"] = decoded.Power
    if ai: # got alarm info?
        pso = {}
        ProtectSts1 = ai.ProtectSts1
        ProtectSts2 = ai.ProtectSts2
        pso["Charger_OVP"] = checkBit(ProtectSts1, 7)
        pso["SCP"] = checkBit(ProtectSts1, 6)
        pso["DSG_OCP"] = checkBit(ProtectSts1, 5)
        pso["CHG_OCP"] = checkBit(ProtectSts1, 4)
        pso["Pack_UVP"] = checkBit(ProtectSts1, 3)
        pso["Pack_OVP"] = checkBit(ProtectSts1, 2)
        pso["Cell_UVP"] = checkBit(ProtectSts1, 1)
        pso["Cell_OVP"] = checkBit(ProtectSts1, 0)
        pso["ENV_UTP"] = checkBit(ProtectSts2, 6)
        pso["ENV_OTP"] = checkBit(ProtectSts2, 5)
        pso["MOS_OTP"] = checkBit(ProtectSts2, 4)
        pso["DSG_UTP"] = checkBit(ProtectSts2, 3)
        pso["CHG_UTP"] = checkBit(ProtectSts2, 2)
        pso["DSG_OTP"] = checkBit(ProtectSts2, 1)
        pso["CHG_OTP"] = checkBit(ProtectSts2, 0)
        pylonData["Protect_Status"] = pso

        sso = {}
        SystemSts = ai.SystemSts
        sso["Fully_Charged"] = checkBit(ProtectSts2, 7)
        sso["Heater"] = checkBit(SystemSts, 7)
        sso["AC_in"] = checkBit(SystemSts, 5)
        sso["Discharge_MOS"] = checkBit(SystemSts, 2)
        sso["Charge_MOS"] = checkBit(SystemSts, 1)
        sso["Charge_Limit"] = checkBit(SystemSts, 0)
        pylonData["System_Status"] = sso

        fso = {}
        FaultSts = ai.FaultSts
        fso["Heater_Fault"] = checkBit(FaultSts, 7)
        fso["CCB_Fault"] = checkBit(FaultSts, 6)
        fso["Sampling_Fault"] = checkBit(FaultSts, 5)
        fso["Cell_Fault"] = checkBit(FaultSts, 4)
        fso["NTC_Fault"] = checkBit(FaultSts, 2)
        fso["DSG_MOS_Fault"] = checkBit(FaultSts, 1)
        fso["CHG_MOS_Fault"] = checkBit(FaultSts, 0)
        pylonData["Fault_Status"] = fso

        aso = {}
        AlarmSts1 = ai.AlarmSts1
        AlarmSts2 = ai.AlarmSts2
        aso["DSG_OC"] = checkBit(AlarmSts1, 5)
        aso["CHG_OC"] = checkBit(AlarmSts1, 4)
        aso["Pack_UV"] = checkBit(AlarmSts1, 3)
        aso["Pack_OV"] = checkBit(AlarmSts1, 2)
        aso["Cell_UV"] = checkBit(AlarmSts1, 1)
        aso["Cell_OV"] = checkBit(AlarmSts1, 0)

        aso["SOC_Low"] = checkBit(AlarmSts2, 7)
        aso["MOS_OT"] = checkBit(AlarmSts2, 6)
        aso["ENV_UT"] = checkBit(AlarmSts2, 5)
        aso["ENV_OT"] = checkBit(AlarmSts2, 4)
        aso["DSG_UT"] = checkBit(AlarmSts2, 3)
        aso["CHG_UT"] = checkBit(AlarmSts2, 2)
        aso["DSG_OT"] = checkBit(AlarmSts2, 1)
        aso["CHG_OT"] = checkBit(AlarmSts2, 0)
        pylonData["Alarm_Status"] = aso
    return json.dumps(pylonData, sort_keys=False, separators=(',', ':'))

