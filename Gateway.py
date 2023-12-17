import paho.mqtt.client as mqtt
import random  
import time
from azure.iot.device import IoTHubDeviceClient, Message

# MQTT broker address (Raspberry Pi IP address)
broker = "10.0.0.151"

# Azure IoT Hub connection string
CONNECTION_STRING = "HostName=vital-sign-monitor.azure-devices.net;DeviceId=rpi-gateway;SharedAccessKey=u7nR4VlTYJEXSs42MwZdIScuPs+ogzyUtAIoTGKOYSs="
_client = IoTHubDeviceClient.create_from_connection_string(CONNECTION_STRING)

# Message template for Azure IoT Hub
MSG_TXT = '{{"is_accident": {is_accident}, "pulse": {pulse}, "spo2": {spo2}, "temp": {temp}}}'

# accident = "false" and add global var in on_message
pulse = -1       
spo2 = -1
temp = -1

# Method to send a formatted message to Azure IoT Hub
def iothub_client_message_run(is_accident_rpi, pulse_rpi, spo2_rpi, temp_rpi):
    msg_txt_formatted = MSG_TXT.format(is_accident=is_accident_rpi, pulse=pulse_rpi, spo2=spo2_rpi, temp=temp_rpi)
    msg = Message(msg_txt_formatted)
    print("Sending message: {}".format(msg))
    _client.send_message(msg)
    print("Message successfully sent")

# MQTT message handling method
def on_message(client, userdata, message):
    global pulse
    global spo2
    global temp
    
    msg = str(message.payload.decode("utf-8"))
    print("Received message = ", msg)

    if message.topic == "pulse":
        pulse = float(msg)
    elif message.topic == "spo2":
        spo2 = float(msg)
    elif message.topic == "temp":
        temp = float(msg)
    elif message.topic == "is_accident":
        # accident = msg, change msg below into accident, then set accident back to "false"
        iothub_client_message_run(msg, pulse, spo2, temp)       
        
if __name__ == '__main__':
    client = mqtt.Client("RPi-Client")
    client.on_message = on_message

    print("Connecting to broker", broker)
    client.connect(broker)

    print("Subscribing")
    client.subscribe("is_accident")
    client.subscribe("pulse")
    client.subscribe("spo2")
    client.subscribe("temp")
    last = time.time()

    try:
        while True:
            client.loop()
            
            # Periodically send data to Azure IoT Hub
            if time.time() - last > 1:
                iothub_client_message_run("false", pulse, spo2, temp)
                last = time.time()
    except KeyboardInterrupt:  
        print("IoTHubClient telemetry stopped")
