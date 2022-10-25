FROM python:3.7.6-slim-stretch

RUN pip install --no-cache-dir pyserial paho-mqtt construct

ADD pylon_to_mqtt.py /
ADD support/*.py support/

ENTRYPOINT ["python3", "pylon_to_mqtt.py"]