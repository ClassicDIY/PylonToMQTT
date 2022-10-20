import socket
import logging
import sys, getopt
import os
import re


log = logging.getLogger("PylonToMQTT")

def validateStrParameter(param, name, defaultValue):
    if isinstance(param, str): 
        return param
    else:
        log.error("Invalid parameter, {} passed for {}".format(param, name))
        return defaultValue


def validateHostnameParameter(param, name, defaultValue):
    try:
        socket.gethostbyname(param)
        # It works -- use it.  Prevents conflicts with 'invalid' configurations
        # that still work due to OS quirks
        return param
    except Exception as e:
        log.warning("Name resolution failed for {!r} passed for {}".format(param, name))
        log.exception(e, exc_info=False)
    try:
        assert len(param) < 253
        # Permit name to end with a single dot.
        hostname = param[:-1] if param.endswith('.') else param
        # check each hostname segment.
        # '_': permissible in domain names, but not hostnames --
        #      however, many OSes permit them, so we permit them.
        allowed = re.compile("^(?!-)[A-Z\d_-]{1,63}(?<!-)$", re.IGNORECASE)
        assert all(allowed.match(s) for s in hostname.split("."))
        # Host is down, but name is valid.  Use it.
        return param
    except AssertionError:
        log.error("Invalid parameter: {!r} passed for {}, using default instead"
                  .format(param, name))
        return defaultValue


def validateIntParameter(param, name, defaultValue):
    try: 
        temp = int(param) 
    except Exception as e:
        log.error("Invalid parameter, {} passed for {}".format(param, name))
        log.exception(e, exc_info=False)
        return defaultValue
    return temp


# --------------------------------------------------------------------------- # 
# Handle the command line arguments
# --------------------------------------------------------------------------- # 
def handleArgs(argv,argVals):
    
    from PylonToMQTT import MAX_PUBLISH_RATE, MIN_PUBLISH_RATE
    
    try:
      opts, args = getopt.getopt(argv,"h",
                    ["pylon_port=",
                     "rack_name=",
                     "mqtt=",
                     "mqtt_port=",
                     "mqtt_root=",
                     "mqtt_user=",
                     "mqtt_pass=",
                     "publish_rate="])
    except getopt.GetoptError:
        print("Error parsing command line parameters, please use: py --pylon_port <{}> --rack_name <{}> --mqtt_host <{}> --mqtt_port <{}> --mqtt_root <{}> --mqtt_user <username> --mqtt_pass <password> --publish_rate <{}>".format( \
                   argVals['pylonPort'], argVals['rack_name'], argVals['mqttHost'], argVals['mqttPort'], argVals['mqttRoot'], argVals['publishRate']))
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print ("Parameter help: py --pylon_port <{}> --rackName <{}> --mqtt_host <{}> --mqtt_port <{}> --mqtt_root <{}> --mqtt_user <username> --mqtt_pass <password> --publish_rate <{}>".format( \
                    argVals['pylonPort'], argVals['rackName'], argVals['mqttHost'], argVals['mqttPort'], argVals['mqttRoot'], argVals['publishRate']))
            sys.exit()
        elif opt in ('--pylon_port'):
            argVals['pylonPort'] = validateStrParameter(arg,"pylon_port", argVals['pylonPort'])
        elif opt in ('--rackName'):
            argVals['rackName'] = validateStrParameter(arg,"rack_name", argVals['rackName'])
        elif opt in ("--mqtt"):
            argVals['mqttHost'] = validateHostnameParameter(arg,"mqtt_host",argVals['mqttHost'])
        elif opt in ("--mqtt_port"):
            argVals['mqttPort'] = validateIntParameter(arg,"mqtt_port", argVals['mqttPort'])
        elif opt in ("--mqtt_root"):
            argVals['mqttRoot'] = validateStrParameter(arg,"mqtt_root", argVals['mqttRoot'])
        elif opt in ("--mqtt_user"):
            argVals['mqttUser'] = validateStrParameter(arg,"mqtt_user", argVals['mqttUser'])
        elif opt in ("--mqtt_pass"):
            argVals['mqttPassword'] = validateStrParameter(arg,"mqtt_pass", argVals['mqttPassword'])
        elif opt in ("--publish_rate"):
            argVals['publishRate'] = int(validateIntParameter(arg,"publish_rate", argVals['publishRate']))

    if ((argVals['publishRate'])<MIN_PUBLISH_RATE):
        print("--publishRate must be greater than or equal to {} seconds".format(MIN_PUBLISH_RATE))
        sys.exit()
    elif ((argVals['publishRate'])>MAX_PUBLISH_RATE):
        print("--publishRate must be less than or equal to {} seconds".format(MAX_PUBLISH_RATE))
        sys.exit()

    argVals['rackName'] = argVals['rackName'].strip()
    argVals['pylonPort'] = argVals['pylonPort'].strip()

    argVals['mqttHost'] = argVals['mqttHost'].strip()
    argVals['mqttUser'] = argVals['mqttUser'].strip()

    log.info("pylonPort = {}".format(argVals['pylonPort']))
    log.info("rackName = {}".format(argVals['rackName']))
    log.info("mqttHost = {}".format(argVals['mqttHost']))
    log.info("mqttPort = {}".format(argVals['mqttPort']))
    log.info("mqttRoot = {}".format(argVals['mqttRoot']))
    log.info("mqttUser = {}".format(argVals['mqttUser']))
    log.info("mqttPassword = **********")
    #log.info("mqttPassword = {}".format(argVals['mqttPassword']))
    log.info("publishRate = {}".format(argVals['publishRate']))

    #Make sure the last character in the root is a "/"
    if (not argVals['mqttRoot'].endswith("/")):
        argVals['mqttRoot'] += "/"
