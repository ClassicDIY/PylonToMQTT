from paho.mqtt import client as mqttclient
from collections import OrderedDict
import json
import time
import threading
import logging
import os
import sys
from random import randint, seed
from enum import Enum

from support.pylon_jsonencoder import encodePylon_readings, encodePylon_info
from support.pylon_validate import handleArgs
from support.pylontech import Pylontech
from time import time_ns

# --------------------------------------------------------------------------- # 
# GLOBALS
# --------------------------------------------------------------------------- # 
MAX_PUBLISH_RATE            = 15        #in seconds
MIN_PUBLISH_RATE            = 3         #in seconds
DEFAULT_WAKE_RATE           = 5         #in seconds
MQTT_MAX_ERROR_COUNT        = 300       #Number of errors on the MQTT before the tool exits
MAIN_LOOP_SLEEP_SECS        = 5         #Seconds to sleep in the main loop
CONFIG_VERSION              = "V1.2.1" # major.minor.build (major or minor will invalidate the configuration)
HOME_ASSISTANT_PREFIX       = "homeassistant" # MQTT prefix used in autodiscovery

tempKeys = ["CellTemp1_4", "CellTemp5_8", "CellTemp9_12", "CellTemp13_16", "MOS_T", "ENV_T"]

# --------------------------------------------------------------------------- # 
# Default startup values. Can be over-ridden by command line options.
# --------------------------------------------------------------------------- # 
argumentValues = { \
    'pylonPort':os.getenv('PYLON_PORT', "/dev/ttyUSB0"), \
    'rackName':os.getenv('RACK_NAME', "Main"), \
    'mqttHost':os.getenv('MQTT_HOST', "mosquitto"), \
    'mqttPort':os.getenv('MQTT_PORT', "1883"), \
    'mqttRoot':os.getenv('MQTT_ROOT', "PylonToMQTT"), \
    'mqttUser':os.getenv('MQTT_USER', ""), \
    'mqttPassword':os.getenv('MQTT_PASS', ""), \
    'publishRate':int(os.getenv('PUBLISH_RATE', str(DEFAULT_WAKE_RATE)))
}

# --------------------------------------------------------------------------- # 
# Counters and status variables
# --------------------------------------------------------------------------- # 
infoPublished               = False
mqttConnected               = False
doStop                      = False
mqttErrorCount              = 0
currentPollRate             = DEFAULT_WAKE_RATE
mqttClient                  = None
number_of_packs             = 0 
current_pack_index                = 0
info_published              = None
discovery_published         = None
pack_versions               = None
pack_barcodes               = None

p = Pylontech()

# --------------------------------------------------------------------------- # 
# configure the logging
# --------------------------------------------------------------------------- # 
log = logging.getLogger("PylonToMQTT")
if not log.handlers:
    handler = logging.StreamHandler(sys.stdout)
    formatter = logging.Formatter('%(asctime)s:%(levelname)s:%(name)s:%(message)s')
    handler.setFormatter(formatter)
    log.addHandler(handler) 
    log.setLevel(os.environ.get("LOGLEVEL", "INFO"))

# --------------------------------------------------------------------------- # 
# MQTT On Connect function
# --------------------------------------------------------------------------- # 
def on_connect(client, userdata, flags, rc):
    global mqttConnected, mqttErrorCount, mqttClient
    if rc==0:
        log.info("MQTT connected OK Returned code={}".format(rc))
        #subscribe to the commands
        try:
            topic = "{}{}/cmnd/#".format(argumentValues['mqttRoot'], argumentValues['rackName'])
            client.subscribe(topic)
            log.info("Subscribed to {}".format(topic))
            
            #publish that we are Online
            will_topic = "{}{}/tele/LWT".format(argumentValues['mqttRoot'], argumentValues['rackName'])
            mqttClient.publish(will_topic, "Online",  qos=0, retain=False)
        except Exception as e:
            log.error("MQTT Subscribe failed")
            log.exception(e, exc_info=True)

        mqttConnected = True
        mqttErrorCount = 0
    else:
        mqttConnected = False
        log.error("MQTT Bad connection Returned code={}".format(rc))

# --------------------------------------------------------------------------- # 
# MQTT On Disconnect
# --------------------------------------------------------------------------- # 
def on_disconnect(client, userdata, rc):
    global mqttConnected, mqttClient
    mqttConnected = False
    #if disconnetion was unexpectred (not a result of a disconnect request) then log it.
    if rc!=mqttclient.MQTT_ERR_SUCCESS:
        log.info("on_disconnect: Disconnected. ReasonCode={}".format(rc))

# --------------------------------------------------------------------------- # 
# MQTT On Message
# --------------------------------------------------------------------------- # 
def on_message(client, userdata, message):
        #print("Received message '" + str(message.payload) + "' on topic '"
        #+ message.topic + "' with QoS " + str(message.qos))

        global currentPollRate, infoPublished, doStop, mqttConnected, mqttErrorCount, argumentValues

        mqttConnected = True #got a message so we must be up again...
        mqttErrorCount = 0

        msg = message.payload.decode(encoding='UTF-8').upper()
        log.info("Received MQTT message {}".format(msg))

        if msg == "{\"STOP\"}":
            doStop = True
        else: #JSON messages
            theMessage = json.loads(message.payload.decode(encoding='UTF-8'))
            log.debug(theMessage)
            if "publishRate" in theMessage:
                newRate_msecs = theMessage['publishRate']
                newRate = round(newRate_msecs/1000)
                if newRate < MIN_PUBLISH_RATE:
                    log.error("Received publishRate of {} which is below minimum of {}".format(newRate,MIN_PUBLISH_RATE))
                elif newRate > MAX_PUBLISH_RATE:
                    log.error("Received publishRate of {} which is above maximum of {}".format(newRate,MAX_PUBLISH_RATE))
                else:
                    argumentValues['publishRate'] = newRate
                    currentPollRate = newRate
                    log.info("publishRate message received, setting rate to {}".format(newRate))
            else:
                log.error("on_message: Received something else")
            
# --------------------------------------------------------------------------- # 
# MQTT Publish the data
# --------------------------------------------------------------------------- # 
def mqttPublish(data, subtopic, retain):
    global mqttConnected, mqttClient, mqttErrorCount

    topic = "{}{}/stat/{}".format(argumentValues['mqttRoot'], argumentValues['rackName'], subtopic)
    log.info("Publishing: {}".format(topic))
    
    try:
        mqttClient.publish(topic, data, qos=0, retain=retain)
        return True
    except Exception as e:
        log.error("MQTT Publish Error Topic:{}".format(topic))
        log.exception(e, exc_info=True)
        mqttConnected = False
        mqttErrorCount += 1
        return False

def PublishDiscoverySub(component, entity, jsonElement, device_class, unit_of_meas, icon=0):
    global current_pack_index, pack_barcodes, pack_versions

    current_pack_number = current_pack_index + 1 # pack number is origin 1
    doc = {}
    doc["device_class"] = device_class
    doc["unit_of_measurement"] = unit_of_meas
    doc["state_class"] = "measurement"
    doc["name"] = entity
    if (icon):
        doc["icon"] = icon
    doc["state_topic"] = "{}{}/stat/readings/Pack{}".format(argumentValues['mqttRoot'], argumentValues['rackName'], current_pack_number)
    object_id = "Rpi_Pack{}_{}".format(current_pack_number, entity)
    doc["unique_id"] = object_id
    doc["value_template"] = "{{{{ value_json.{} }}}}".format(jsonElement)
    doc["availability_topic"] = "{}{}/tele/LWT".format(argumentValues['mqttRoot'], argumentValues['rackName'])
    doc["pl_avail"] = "Online"
    doc["pl_not_avail"] = "Offline"
    device = {}
    device["name"] = "Pack{}".format(current_pack_number)
    device["via_device"] = argumentValues['mqttRoot'][:-1]
    device["hw_version"] = pack_barcodes[current_pack_index]
    device["sw_version"] = CONFIG_VERSION
    device["manufacturer"] = "ClassicDIY"
    device["model"] = pack_versions[current_pack_index]
    device["identifiers"] = "Pack{}_{}".format(current_pack_number, pack_barcodes[current_pack_index])
    doc["device"] = device
    mqttClient.publish("{}/{}/{}/config".format(HOME_ASSISTANT_PREFIX, component, object_id),json.dumps(doc, sort_keys=False, separators=(',', ':')), qos=0, retain=False)
   
def PublishTempsDiscovery(numberOfTemps):
    for x in range(numberOfTemps):
        tempKey = "Temp{}".format(x)
        if (x < len(tempKeys)):
            tempKey = tempKeys[x]
        PublishDiscoverySub("sensor",  tempKey, "Temps.{}.Reading".format(tempKey), "temperature", "°C")

def PublishCellsDiscovery(numberOfCells):
    for x in range(numberOfCells):
        PublishDiscoverySub("sensor",  "Cell_{}".format(x+1), "Cells.Cell_{}.Reading".format(x+1), "voltage", "V", "mdi:lightning-bolt")

def publishDiscovery(pylonData):
    global current_pack_index
    
    PublishDiscoverySub("sensor", "PackVoltage", "PackVoltage.Reading", "voltage", "V", "mdi:lightning-bolt")
    PublishDiscoverySub("sensor", "PackCurrent", "PackCurrent.Reading", "current", "A", "mdi:current-dc")
    PublishDiscoverySub("sensor", "SOC", "SOC", "battery", "%", icon=0)
    PublishDiscoverySub("sensor", "RemainingCapacity", "RemainingCapacity", "current", "Ah", "mdi:ev-station")
    PublishCellsDiscovery(pylonData.NumberOfCells)
    PublishTempsDiscovery(pylonData.NumberOfTemperatures)
    discovery_published[current_pack_index] = True

# --------------------------------------------------------------------------- # 
# Periodic will be called when needed.
# If so, it will read from serial and publish to MQTT
# --------------------------------------------------------------------------- # 
def periodic(polling_stop):    

    global infoPublished, currentPollRate, number_of_packs, current_pack_index, info_published, discovery_published, pack_barcodes, pack_versions

    if not polling_stop.is_set():
        try:
            if mqttConnected:
                data = {}
                if number_of_packs == 0:
                    number_of_packs = p.get_pack_count().PackCount
                    log.info("Pack count: {}".format(number_of_packs))
                    current_pack_index = 0
                    info_published = [False] * number_of_packs
                    discovery_published = [False] * number_of_packs
                    pack_barcodes = [""] * number_of_packs
                    pack_versions = [""] * number_of_packs
                    
                else :
                    current_pack_number = current_pack_index + 1 # pack number is origin 1
                    if not info_published[current_pack_index]:
                        vi = p.get_version_info(current_pack_number)
                        pack_versions[current_pack_index] = vi.Version
                        log.info("version_info: {}".format(vi.Version))
                        if vi:
                            bc = p.get_barcode(current_pack_number)
                            log.info("barcode: {}".format(bc.Barcode))
                            if bc:
                                mqttPublish(encodePylon_info(vi, bc),"info/Pack{}".format(current_pack_number), True)
                                info_published[current_pack_index] = True
                                pack_barcodes[current_pack_index] = bc.Barcode
                    pylonData = p.get_values_single(current_pack_number)
                    log.debug("get_values_single: {}".format(pylonData))
                    ai = p.get_alarm_info(current_pack_number)
                    log.debug("get_alarm_info: {}".format(ai))
                    if pylonData: # got data
                        mqttPublish(encodePylon_readings(pylonData, ai),"readings/Pack{}".format(current_pack_number), False)
                        if discovery_published[current_pack_index] == False:
                            publishDiscovery(pylonData)
                            
                    else:
                        log.error("PYLON data not good, skipping publish")
                    current_pack_index += 1
                    current_pack_index %= number_of_packs

        except Exception as e:
            log.error("Failed to process response!")
            log.exception(e, exc_info=True)
            if number_of_packs > 0:
                current_pack_index += 1 # move on to next pack
                current_pack_index %= number_of_packs

        timeUntilNextInterval = currentPollRate
        # set myself to be called again in correct number of seconds
        threading.Timer(timeUntilNextInterval, periodic, [polling_stop]).start()

# --------------------------------------------------------------------------- # 
# Main
# --------------------------------------------------------------------------- # 
def run(argv):

    global doStop, mqttClient, currentPollRate, log

    log.info("pylon_mqtt starting up...")

    handleArgs(argv, argumentValues)
    currentPollRate = argumentValues['publishRate']

    #random seed from the OS
    seed(int.from_bytes( os.urandom(4), byteorder="big"))

    mqttErrorCount = 0

    #setup the MQTT Client for publishing and subscribing
    clientId = argumentValues['mqttUser'] + "_mqttclient_" + str(randint(100, 999))
    log.info("Connecting with clientId=" + clientId)
    mqttClient = mqttclient.Client(clientId) 
    mqttClient.username_pw_set(argumentValues['mqttUser'], password=argumentValues['mqttPassword'])
    mqttClient.on_connect = on_connect    
    mqttClient.on_disconnect = on_disconnect  
    mqttClient.on_message = on_message

    #Set Last Will 
    will_topic = "{}{}/tele/LWT".format(argumentValues['mqttRoot'], argumentValues['rackName'])
    mqttClient.will_set(will_topic, payload="Offline", qos=0, retain=False)

    try:
        log.info("Connecting to MQTT {}:{}".format(argumentValues['mqttHost'], argumentValues['mqttPort']))
        mqttClient.connect(host=argumentValues['mqttHost'],port=int(argumentValues['mqttPort'])) 
    except Exception as e:
        log.error("Unable to connect to MQTT, exiting...")
        sys.exit(2)
    
    mqttClient.loop_start()

    #define the stop for the function
    periodic_stop = threading.Event()
    # start calling periodic now and every 
    periodic(periodic_stop)

    log.debug("Starting main loop...")
    while not doStop:
        try:            
            time.sleep(MAIN_LOOP_SLEEP_SECS)
            if not mqttConnected:
                if (mqttErrorCount > MQTT_MAX_ERROR_COUNT):
                    log.error("MQTT Error count exceeded, disconnected, exiting...")
                    doStop = True

        except KeyboardInterrupt:
            log.error("Got Keyboard Interuption, exiting...")
            doStop = True
        except Exception as e:
            log.error("Caught other exception...")
            log.exception(e, exc_info=True)
    
    log.info("Exited the main loop, stopping other loops")
    log.info("Stopping periodic async...")
    periodic_stop.set()

    log.info("Stopping MQTT loop...")
    mqttClient.loop_stop()

    log.info("Exiting pylon_mqtt")

if __name__ == '__main__':
    number_of_packs  = 0
    run(sys.argv[1:])