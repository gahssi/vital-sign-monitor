import random
from random import choices
import time
from azure.iot.device import IoTHubDeviceClient, Message

# Azure IoT Hub connection string
CONNECTION_STRING = "HostName=vital-sign-monitor.azure-devices.net;DeviceId=rpi-gateway;SharedAccessKey=u7nR4VlTYJEXSs42MwZdIScuPs+ogzyUtAIoTGKOYSs="

# Message templates for Azure IoT Hub
MSG_TXT_ONE = '{{"is_accident": {is_accident}, "pulse": {pulse}, "spo2": {spo2}, "temp": {temp}}}'
MSG_TXT_TWO = '{{"pulse": {pulse}, "spo2": {spo2}}}, "temp": {temp}}}'

# Initialize Azure IoT Hub client
def iothub_client_init():
    client = IoTHubDeviceClient.create_from_connection_string(CONNECTION_STRING)
    return client

# Method to run the telemetry simulation
def iothub_client_telemetry_sample_run():
    try:
        client = iothub_client_init()
        print("IoT gateway sending periodic messages...")

        while True:
            pulse_sim = random.randint(60, 150) # Simulate heart rate data
            pulse_dead = -1  # Placeholder for disconnected heart rate sensor
            population = [pulse_sim, pulse_dead]
            weights = [0.9, 0.1]
            pulse = choices(population, weights)

            spo2_sim = random.randint(70, 100) # Simulate heart rate data
            spo2_dead = -1  # Placeholder for disconnected heart rate sensor
            population = [spo2_sim, spo2_dead]
            weights = [0.9, 0.1]
            spo2 = choices(population, weights)

            temp_sim = random.randint(31, 35) # Simulate heart rate data
            temp_dead = -1  # Placeholder for disconnected heart rate sensor
            population = [temp_sim, temp_dead]
            weights = [0.87, 0.13]
            temp = choices(population, weights)

            population = [0, 1] # Simulate accident status data
            weights = [0.85, 0.15]
            flag = choices(population, weights)

            # Format the message based on the simulation
            if flag[0]:
                msg_txt_formatted = MSG_TXT_ONE.format(is_accident = 0, pulse = pulse[0], spo2 = spo2[0], temp = temp[0])
            else:
                msg_txt_formatted = MSG_TXT_TWO.format(is_accident = flag[0], pulse = pulse[0], spo2 = spo2[0], temp = temp[0])

            message = Message(msg_txt_formatted)

            print("Sending message: {}".format(message))
            client.send_message(message)
            print("Message successfully sent")

            sleep_time = random.randint(10, 100) # Introduce a random delay before the next simulation
            time.sleep(20)
    except KeyboardInterrupt:
        print("IoTHubClient telemetry stopped")

if __name__ == '__main__':
    print("IoT Hub telemetry simulation")
    print("Press Q or Ctrl-C to exit")
    iothub_client_telemetry_sample_run()
