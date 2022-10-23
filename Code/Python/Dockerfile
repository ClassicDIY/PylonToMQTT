FROM python

RUN pip install --no-cache-dir pyserial paho-mqtt numpy construct

ADD pylon_to_mqtt.py /
ADD support/*.py support/

ENTRYPOINT ["python3", "pylon_to_mqtt.py"]