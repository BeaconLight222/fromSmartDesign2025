import ssl
import json
import paho.mqtt.client as mqtt

thing_name = "test-beacon-05-21"
client_id = thing_name
endpoint = "ado4y320oe6ti-ats.iot.us-east-1.amazonaws.com"
port = 8883

def on_connect(client, userdata, flags, rc):
    print(f"[Connected] Result code: {rc}")
    
    # Subscribe to topics
    client.subscribe(f"{thing_name}/command/sub")
    client.subscribe(f"{thing_name}/deviceConfig/sub")
    print("Subscribed to topics.")

    # Publish a test message
    client.publish(f"{thing_name}/command/sub", json.dumps({"status": "hello from beacon"}))
    print("Published to command/sub.")

def on_message(client, userdata, msg):
    print(f"[Message] Topic: {msg.topic} | Payload: {msg.payload.decode()}")

def on_disconnect(client, userdata, rc):
    print(f"[Disconnected] Result code: {rc}")

client = mqtt.Client(client_id=client_id)
client.on_connect = on_connect
client.on_disconnect = on_disconnect
client.on_message = on_message

client.tls_set(
    ca_certs="certs/AmazonRootCA1.pem",
    certfile="certs/test-beacon-05-21-certificate.pem.crt",
    keyfile="certs/test-beacon-05-21-private.pem.key",
    tls_version=ssl.PROTOCOL_TLS_CLIENT
)

client.connect(endpoint, port=port, keepalive=60)

try:
    client.loop_forever()
except KeyboardInterrupt:
    print("Exiting...")
    client.disconnect()
