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

from support.pylon_jsonencoder import encodePylonData_readings
from support.pylon_validate import handleArgs
from support.pylontech import Pylontech
from time import time_ns

# --------------------------------------------------------------------------- # 
# GLOBALS
# --------------------------------------------------------------------------- # 
MAX_PUBLISH_RATE               = 15        #in seconds
MIN_PUBLISH_RATE               = 3         #in seconds
DEFAULT_WAKE_RATE           = 5         #in seconds
MQTT_MAX_ERROR_COUNT        = 300       #Number of errors on the MQTT before the tool exits
MAIN_LOOP_SLEEP_SECS        = 5         #Seconds to sleep in the main loop

# --------------------------------------------------------------------------- # 
# Default startup values. Can be over-ridden by command line options.
# --------------------------------------------------------------------------- # 
argumentValues = { \
    'pylonPort':os.getenv('PYLON_PORT', "/dev/ttyUSB0"), \
    'rackName':os.getenv('RACK_NAME', "Test"), \
    'mqttHost':os.getenv('MQTT_HOST', "192.168.86.23"), \
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

# --------------------------------------------------------------------------- # 
# configure the logging
# --------------------------------------------------------------------------- # 
log = logging.getLogger(__name__)
if not log.handlers:
    handler = logging.StreamHandler(sys.stdout)
    formatter = logging.Formatter('%(asctime)s:%(levelname)s:%(name)s:%(message)s')
    handler.setFormatter(formatter)
    log.addHandler(handler) 
    log.setLevel(os.environ.get("LOGLEVEL", "DEBUG"))

# --------------------------------------------------------------------------- # 
# MQTT On Connect function
# --------------------------------------------------------------------------- # 
def on_connect(client, userdata, flags, rc):
    global mqttConnected, mqttErrorCount, mqttClient
    if rc==0:
        log.debug("MQTT connected OK Returned code={}".format(rc))
        #subscribe to the commands
        try:
            topic = "{}{}/cmnd/#".format(argumentValues['mqttRoot'], argumentValues['rackName'])
            client.subscribe(topic)
            log.debug("Subscribed to {}".format(topic))
            
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
        log.debug("on_disconnect: Disconnected. ReasonCode={}".format(rc))

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
        log.debug("Received MQTT message {}".format(msg))

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
                    log.debug("publishRate message received, setting rate to {}".format(newRate))
            else:
                log.error("on_message: Received something else")
            
# --------------------------------------------------------------------------- # 
# MQTT Publish the data
# --------------------------------------------------------------------------- # 
def mqttPublish(client, data, subtopic):
    global mqttConnected, mqttErrorCount

    topic = "{}{}/stat/{}".format(argumentValues['mqttRoot'], argumentValues['rackName'], subtopic)
    log.debug("Publishing: {}".format(topic))
    
    try:
        client.publish(topic,data)
        return True
    except Exception as e:
        log.error("MQTT Publish Error Topic:{}".format(topic))
        log.exception(e, exc_info=True)
        mqttConnected = False
        return False

# --------------------------------------------------------------------------- # 
# Periodic will be called when needed.
# If so, it will read from serial and publish to MQTT
# --------------------------------------------------------------------------- # 
def periodic(modbus_stop):    

    global mqttClient, infoPublished, mqttErrorCount, currentPollRate

    if not modbus_stop.is_set():
        #Get the current time as a float of seconds.
        beforeTime = time_ns() /  1000000000.0

        try:
            if mqttConnected:
                data = {}
                # todo
                #Get the Modbus Data and store it.
                # data = getModbusData(modeAwake, argumentValues['pylonHost'], argumentValues['pylonPort'])
                if data: # got data
                    if mqttPublish(mqttClient, encodePylonData_readings(data),"readings") == False:
                        # if (not infoPublished): #Check if the Info has been published yet
                        #     if mqttPublish(mqttClient, encodePylonData_readings(data),"info"):
                        #         infoPublished = True                        
                        #     else:
                        #         mqttErrorCount += 1
                    # else:
                        mqttErrorCount += 1

                else:
                    log.error("PYLON data not good, skipping publish")

        except Exception as e:
            log.error("Caught Error in periodic")
            log.exception(e, exc_info=True)

        #Account for the time that has been spent on this cycle to do the actual work
        timeUntilNextInterval = currentPollRate - (time_ns()/1000000000.0 - beforeTime)

        # If doing the work took too long, skip as many polling forward so that we get a time in the future.
        while (timeUntilNextInterval < 0):
            log.debug("Adjusting next interval to account for cycle taking too long: {}".format(timeUntilNextInterval))
            timeUntilNextInterval = timeUntilNextInterval + currentPollRate 
            log.debug("Adjusted interval: {}".format(timeUntilNextInterval))

        #log.debug("Next Interval: {}".format(timeUntilNextInterval))
        # set myself to be called again in correct number of seconds
        threading.Timer(timeUntilNextInterval, periodic, [modbus_stop]).start()

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
    p = Pylontech()
    log.debug(p.get_values_single(2))
    run(sys.argv[1:])