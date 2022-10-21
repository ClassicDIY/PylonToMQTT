
# Pylontech MQTT Publisher Python Implementation

The code in this repository will read data from your Jakiper Battery console interface and publish it to an MQTT broker. It is a read-only program with respect to the battery BMS, it does not write any data to BMS. It is intended to be used with InfluxDb and Grafana.  

The software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND, express or implied.


## **Get It**

1. Copy this repository (if you understand git, you can get it that way too)  
    `wget https://github.com/ClassicDIY/PylonToMQTT/archive/refs/heads/main.zip`
2. Extract the zip file:  
    `unzip main.zip`
3. Change directory  
     `cd PylonToMQTT-main/Code/Python/`

When it comes time to run the program, there are parameters that can be set or passed they are:  
**Parameters:**  
```  
--pylon_port </dev/ttyUSB0>     : The USB port on the raspberry pi (default is /dev/ttyUSB0).  
--rack_name <Main>              : The name used to identify the battery rack. 
--mqtt_host <127.0.0.1>         : The IP or URL of the MQTT Broker, defaults to 127.0.0.1 if unspecified.  
--mqtt_port <1883>              : The port to you to connect to the MQTT Broker, defaults to 1883 if unspecified.  
--mqtt_root <PylonToMQTT>       : The root for your MQTT topics, defaults to PylonToMQTT if unspecified.  
--mqtt_user <username>          : The username to access the MQTT Broker (default is no user).  
--mqtt_pass <password>          : The password to access the MQTT Broker (default is no password).
--publish_rate <5>              : The amount of seconds between updates when in wake mode (default is 5 seconds).

```  

## **Run It**

There are several ways to run this program:

1. **Standalone** - must have an MQTT server available and python 3 installed
2. **docker** - must have an MQTT server available and docker installed
3. **docker-compose** - must have docker and docker-compose installed, provides it's own MQTT broker 

### **1. Standalone**

Make sure that you have access to an MQTT broker; either install one on your server or use one of the internet based ones like [Dioty](http://www.dioty.co/). Once you have that setup, make sure that you have a username and password defined, you will need it to both publish data and to get the data once it is published.  

1. Install Python 3.9 or newer and pip. Consult the documentation for your particular computer.
2. Install these libraries: **paho-mqtt**, **pyserial**, **numpy**, **construct** using pip:  
    ```
    pip install pymodbus paho-mqtt
    ```   
3. Install or setup access to an MQTT server like [Dioty](http://www.dioty.co/).  Make sure that you have a username and password defined
4. Run the program from the command line where the pylon_to_mqtt.py is located with the proper parameters:  
    ```
    python3 pylon_to_mqtt.py --pylon_port /dev/ttyUSB0 --rack_name Main --mqtt_host <127.0.0.1> --mqtt_root <PylonToMQTT> --mqtt_user <username> --mqtt_pass <password> --publish_rate <5>
    ```

### **2. Using docker**

Using the "Dockerfile" in this directory will allow an image to be built that can run the program. The Dockerfile uses a base image that already includes python and instructions to install the 3 needed libraries so you can skip installing python and pip, but you must install docker.  

1. Install docker on your host - look this up on the web and follow the instructions for your computer.
2. Install or setup access to an MQTT server like [Dioty](http://www.dioty.co/) or mosquitto on your local network
3. Issue the following command to build the docker image in the docker virtual environment (only need to do this once):  
    ```
    docker build -t pylon_to_mqtt:latest .
    ```
    note: the period at the end is required
    
4. Run the docker image and pass the parameters (substituting the correct values for parameter values):  
    ```
    docker run --name pylon_to_mqtt --device=/dev/ttyUSB0:/dev/ttyUSB0  pylon_to_mqtt --pylon_port /dev/ttyUSB0 --mqtt_host <127.0.0.1> --mqtt_port <1883> --mqtt_root <PylonToMQTT> --mqtt_user <username> --mqtt_pass <password>
    ```
5. For example, if your Raspberry Pi is running the mosquitto MQTT broker (with no user/pw) and it's IP address is 192.168.86.23, the docker run command would look similar to this:  
    ```
    docker run --name pylon_to_mqtt --device=/dev/ttyUSB0:/dev/ttyUSB0  pylon_to_mqtt --pylon_port /dev/ttyUSB0 --mqtt_host 192.168.86.23
    ```  


### **3. Using docker-compose**

Use this method if you want to automatically install an MQTT broker (mosquitto) locally and run the program at the same time. This method takes advantage of docker-compose which will build a system that includes both an MQTT service and a service running the classic_mqtt.py script automatically. The definition for these services are in pylon_mqtt_compose.yml. If you are pushing your data to the internet, this may not be the preferred method for you.
Note: if you need to change anything in the yml file or the ".env" file, you need to tell docker-compose to rebuild the images with the command listed in step 4 below.  

1. Install docker and docker-compose on your host - look this up on the web and follow the instructions for your computer.
2. In the pylon_mqtt_compose.yml file, you can set environment variables needed to run the program. Edit the yml to set them to the needed values. They correspond to the parameters for the pylon_to_mqtt.py program. To edit this file on the Raspberry Pi, I like nano.
    ```
      - LOGLEVEL=DEBUG
      - PYLON_PORT=<The URL or IP address of your classic>
      - RACK_NAME=<The port of your classic, usually 502>
      - MQTT_HOST=mosquitto
      - MQTT_PORT=1883
      - MQTT_ROOT=PylonToMQTT
      - MQTT_USER=
      - MQTT_PASS=
    ```
3. Tell docker-compose to download, build and start up the both mosquitto and the script with the following command.
    ```
    docker-compose -f pylon_mqtt_compose.yml up
    ```
4. Only use this if you change anything in pylon_mqtt_compose.yml or Dockerfile after you have already run the command in step 3 above. This tells docker-compose to rebuild and save the images (then use the command in step 3 to run it again):
    ```
    docker-compose -f pylon_mqtt_compose.yml build
    ```
