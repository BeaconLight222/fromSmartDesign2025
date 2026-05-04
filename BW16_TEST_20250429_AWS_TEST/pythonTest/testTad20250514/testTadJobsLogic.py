import ssl
import paho.mqtt.client as mqtt
import time
import json

##Maybe update CA certificate over time (2038 maybe not care)


# MQTT configuration
thing_name = "bcn-a000187"
mqtt_server = "ado4y320oe6ti-ats.iot.us-east-1.amazonaws.com"
client_id = "bcn-a000187"
port = 8883

# Topics
subscribe_topics = [
    f"/command/sub",
    f"/deviceConfig/sub",
]

# Callback functions
def on_connect(client, userdata, flags, rc):
    global jobState
    print(f"Connected with result code {rc}")
    for partial_topic in subscribe_topics:
        topic = f"$aws/things/{thing_name}{partial_topic}"
        client.subscribe(topic)
        print(f"Subscribed to {topic}")

def on_message(client, userdata, msg):
    print(f"Message received on topic {msg.topic}: {msg.payload.decode()}")
    #try to parse the message as JSON and print it
    try:
        payload = json.loads(msg.payload.decode())
        print(f"Parsed JSON: {json.dumps(payload, indent=2)}")
    except json.JSONDecodeError:
        print("Failed to parse JSON")

# Create MQTT client
client = mqtt.Client(client_id=client_id)
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = lambda client, userdata, rc: print(f"Disconnected with result code {rc}")

# Configure TLS/SSL using buffers
client.tls_set(ca_certs="beacon-cert/AmazonRootCA1.pem", certfile="beacon-cert/bcn-a000187-certificate.pem.crt", keyfile="beacon-cert/bcn-a000187-private.pem.key", tls_version=ssl.PROTOCOL_TLS_CLIENT, ciphers=None)

# Connect to MQTT server
client.connect(mqtt_server, port, keepalive=1200)
# do not keep the Arduino on at the same time, otherwise the connection will be closed frequently


# Main loop
try:
    while True:
        client.loop(timeout=0.1)  # Process MQTT messages
        # Add your custom logic here
        #print("Main loop running...")
        #mqtt loop will handle messages in the background in the above loop, the same thread
        time.sleep(0.1)  # Sleep for a while to avoid busy waiting  

except KeyboardInterrupt:
    print("Exiting...")
    client.disconnect()