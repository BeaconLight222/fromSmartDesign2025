import ssl
import paho.mqtt.client as mqtt
import time
import json
import base64

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric.utils import Prehashed
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509 import load_pem_x509_certificate

##Maybe update CA certificate over time (2038 maybe not care)

class JobStates:
    WAIT_FOR_CONNECTION = 0
    CONNECTED = 1
    READY_TO_SEND_INIT_JOB_QUERY = 2
    WAIT_FOR_JOB_QUERY_RESPONSE = 3
    IDLE = 4
    REQUEST_JOB_DETAIL = 5
    WAIT_FOR_JOB_DETAIL = 6
    DO_THE_JOB = 7
    DO_THE_OTA_JOB = 8



timeoutLimit = []
timeoutLimit = [0] * (max(value for name, value in vars(JobStates).items() if not name.startswith('__')) + 1)

timeoutLimit[JobStates.WAIT_FOR_JOB_QUERY_RESPONSE] = 5000
timeoutLimit[JobStates.WAIT_FOR_JOB_DETAIL] = 5000
timeoutLimit[JobStates.DO_THE_OTA_JOB] = 5000


messageDataList = []
awsJobList = []
processingJobName = ""
processingJobContent = ""
processingOTAStreamName = ""
processingOTAFileSize = 0
processingOTASignature = ""
processingOTAReadedBytes = 0
processingOTARequestedBytes = 0



# MQTT configuration
thing_name = "amebaDevBoard"
mqtt_server = "a1e9mdr77wm319-ats.iot.us-east-1.amazonaws.com"
client_id = "amebaClient"
port = 8883

jobState = JobStates.WAIT_FOR_CONNECTION
jobStateChangeTimeMillis = time.time()*1000

# Topics
subscribe_shadow_topics = [
    f"shadow/update/accepted",
    f"shadow/update/rejected",
    f"shadow/update/delta",
    f"shadow/get/accepted",
    f"shadow/get/rejected",
]

otaDigest = hashes.Hash(hashes.SHA256())
def process_OTA_initHash():
    global otaDigest
    otaDigest = hashes.Hash(hashes.SHA256())


def process_OTA_datachunk(data_chunk, length, offset):
    global otaDigest
    # Process the data chunk here
    # For example, write it to a file or perform some operation
    print(f"Processing OTA data chunk of length {length} at offset {offset}")
    print(f"Data chunk: {data_chunk}")
    otaDigest.update(data_chunk)

def process_OTA_verify_signature(signatureBase64):
    global otaDigest
    # Verify the OTA signature here
    print("Verifying OTA signature...")

    data_hash = otaDigest.finalize()

    signature = base64.b64decode(signatureBase64)
    # Load public key from certificate
    with open("ecdsasigner.crt", "rb") as cert_file:
        cert = load_pem_x509_certificate(cert_file.read())
        public_key = cert.public_key()
    try:
        public_key.verify(
            signature,
            data_hash,
            ec.ECDSA(Prehashed(hashes.SHA256()))
        )
        return True
    except Exception as e:
        return False

# Callback functions
def on_connect(client, userdata, flags, rc):
    global jobState
    print(f"Connected with result code {rc}")
    for partial_topic in subscribe_shadow_topics:
        topic = f"$aws/things/{thing_name}/{partial_topic}"
        client.subscribe(topic)
        print(f"Subscribed to {topic}")
    jobState = JobStates.CONNECTED


def on_message(client, userdata, msg):
    print(f"Message received on topic {msg.topic}: {msg.payload.decode()}")
    #try to parse the message as JSON and print it
    # try:
    #     payload = json.loads(msg.payload.decode())
    #     print(f"Parsed JSON: {json.dumps(payload, indent=2)}")
    # except json.JSONDecodeError:
    #     print("Failed to parse JSON")
    #create a named tuple from the topic and payload
    topic = msg.topic
    payload = msg.payload.decode()
    messageData = {
        "topic": topic,
        "payload": payload
    }
    #append the message data to the list
    messageDataList.append(messageData)





# Create MQTT client
client = mqtt.Client(client_id=client_id)
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = lambda client, userdata, rc: print(f"Disconnected with result code {rc}")

# Configure TLS/SSL using buffers
client.tls_set(ca_certs="AmazonRootCA1.pem", certfile="amebaDevBoard.cert.pem", keyfile="amebaDevBoard.private.key", tls_version=ssl.PROTOCOL_TLS_CLIENT, ciphers=None)

# Connect to MQTT server
client.connect(mqtt_server, port, keepalive=1200)
# do not keep the Arduino on at the same time, otherwise the connection will be closed frequently

appStartTime = time.time()
jobStateChangeTimeMillis = appStartTime*1000

# Main loop
try:
    while True:
        client.loop(timeout=0.1)  # Process MQTT messages
        # Add your custom logic here
        #print("Main loop running...")
        #mqtt loop will handle messages in the background in the above loop, the same thread
        time.sleep(0.1)  # Sleep for a while to avoid busy waiting  
        passedMillis = (time.time()-appStartTime)*1000 - jobStateChangeTimeMillis
        prevJobState = jobState  
        if jobState == JobStates.WAIT_FOR_CONNECTION:
            pass
        elif jobState == JobStates.CONNECTED:
            #also subscribe to new job notification
            client.subscribe(f"$aws/things/{thing_name}/jobs/notify")
            #subscibe to the job topic
            client.subscribe(f"$aws/things/{thing_name}/jobs/get/accepted")
            jobState = JobStates.READY_TO_SEND_INIT_JOB_QUERY
        elif jobState == JobStates.READY_TO_SEND_INIT_JOB_QUERY:
            #send the job query
            client.publish(f"$aws/things/{thing_name}/jobs/get", "{}")
            jobState = JobStates.WAIT_FOR_JOB_QUERY_RESPONSE
        elif jobState == JobStates.WAIT_FOR_JOB_QUERY_RESPONSE:
            #iterate through the message data list and check if there is a job message
            messageDataListLength = len(messageDataList)
            for i in range(messageDataListLength):
                messageData = messageDataList[i]
                if messageData["topic"] == f"$aws/things/{thing_name}/jobs/get/accepted":
                    try:
                        jsonData = json.loads(messageData["payload"])
                        inProgressJobs = jsonData.get("inProgressJobs", [])
                        if inProgressJobs:
                            for job in inProgressJobs:
                                jobId = job.get("jobId")
                                if jobId:
                                    print(f"inProgress Job ID: {jobId}")
                                    #not care inProgress or queued
                                    awsJobList.append(jobId)    
                        queuedJobs = jsonData.get("queuedJobs", [])
                        if queuedJobs:
                            for job in queuedJobs:
                                jobId = job.get("jobId")
                                if jobId:
                                    #not care inProgress or queued
                                    print(f"queued Job ID: {jobId}")
                                    awsJobList.append(jobId)
                    except json.JSONDecodeError:
                        pass
                    messageDataList.pop(i)
                    client.unsubscribe(f"$aws/things/{thing_name}/jobs/get/accepted")
                    jobState = JobStates.IDLE
                    break
            #check if the timeout has passed
            if passedMillis > timeoutLimit[JobStates.WAIT_FOR_JOB_QUERY_RESPONSE]:
                print("Timeout waiting for job query response")
                jobState = JobStates.READY_TO_SEND_INIT_JOB_QUERY
        elif jobState == JobStates.IDLE:
            #check if there is a job in the awsJobList
            if len(awsJobList) > 0:
                #get the first job in the list
                print("we have jobs: ", awsJobList)
                processingJobName = awsJobList[0]
                awsJobList.pop(0)
                print(f"Go to process job: {processingJobName}")
                print("left jobs: ", awsJobList)
                jobState = JobStates.REQUEST_JOB_DETAIL
            pass
        elif jobState == JobStates.REQUEST_JOB_DETAIL:
            client.subscribe(f"$aws/things/{thing_name}/jobs/{processingJobName}/get/accepted")
            client.publish(f"$aws/things/{thing_name}/jobs/{processingJobName}/get", "{}")
            jobState = JobStates.WAIT_FOR_JOB_DETAIL
        elif jobState == JobStates.WAIT_FOR_JOB_DETAIL:
            #iterate through the message data list and check if there is a job message
            messageDataListLength = len(messageDataList)
            for i in range(messageDataListLength):
                messageData = messageDataList[i]
                if messageData["topic"] == f"$aws/things/{thing_name}/jobs/{processingJobName}/get/accepted":
                    try:
                        jsonData = json.loads(messageData["payload"])
                        execution = jsonData.get("execution", {})
                        if execution:
                            jobDocument = execution.get("jobDocument", {})
                            print(f"Job document: {json.dumps(jobDocument, indent=2)}")
                            #store job document in processingJobContent as a string
                            processingJobContent = json.dumps(jobDocument)
                            #need to check if it is a OTA, but for now just print it

                            # sample reboot job document
                            # {
                            #     "version": "1.0",
                            #     "steps": [
                            #         {
                            #         "action": {
                            #             "name": "Reboot",
                            #             "type": "runHandler",
                            #             "input": {
                            #             "handler": "reboot.sh",
                            #             "path": ""
                            #             },
                            #             "runAsUser": ""
                            #         }
                            #         }
                            #     ]
                            # }

                            # sample OTA job document
                            # {
                            #     "afr_ota": {
                            #         "protocols": [
                            #         "MQTT"
                            #         ],
                            #         "streamname": "AFR_OTA-8c147463-5e5c-4da6-b80a-96c1cf6f4825",
                            #         "files": [
                            #         {
                            #             "filepath": "/",
                            #             "filesize": 2574,
                            #             "fileid": 0,
                            #             "certfile": "/test.pem",
                            #             "sig-sha256-ecdsa": "MEUCIQDeMuJNNznBaZjgV2ldHZKbW6Zn6/YFfKvMe4gJmWwTQQIgfK5paVfXwDQit8kLucTLcj002xPDVH0q9Bo8Am8Z3/Q="
                            #         }
                            #         ]
                            #     }
                            # }

                    except json.JSONDecodeError:
                        pass
                    messageDataList.pop(i)
                    client.publish(f"$aws/things/{thing_name}/jobs/{processingJobName}/update", "{\"status\": \"IN_PROGRESS\"}")
                    client.unsubscribe(f"$aws/things/{thing_name}/jobs/{processingJobName}/get/accepted")
                    jobState = JobStates.DO_THE_JOB
                    break
            #check if the timeout has passed
            if passedMillis > timeoutLimit[JobStates.WAIT_FOR_JOB_DETAIL]:
                print("Timeout waiting for job detail response")
                jobState = JobStates.IDLE
        elif jobState == JobStates.DO_THE_JOB:
            print(f"Doing the job: {processingJobName}")
            print(f"Job content: {processingJobContent}")
            #pretend to do the job

            #regular job sample:
            # {"version": "1.0", "steps": [{"action": {"name": "Reboot", "type": "runHandler", "input": {"handler": "reboot.sh", "path": ""}, "runAsUser": ""}}]}
            #OTA job sample:
            # {"afr_ota": {"protocols": ["MQTT"], "streamname": "AFR_OTA-6f19676b-e00a-495e-95ae-3800f36a2050", "files": [{"filepath": "/", "filesize": 2574, "fileid": 0, "certfile": "/test.pem", "sig-sha256-ecdsa": "MEUCIQDeMuJNNznBaZjgV2ldHZKbW6Zn6/YFfKvMe4gJmWwTQQIgfK5paVfXwDQit8kLucTLcj002xPDVH0q9Bo8Am8Z3/Q="}]}}

            #parse the job content
            parseJobContent = json.loads(processingJobContent)
            #check if the job content has "afr_ota" key
            if "afr_ota" in parseJobContent:
                content = parseJobContent["afr_ota"]
                if "protocols" in content:
                    #check if the MQTT is in the protocols
                    if "MQTT" in content["protocols"]:
                        print("MQTT is in the protocols")
                        if "streamname" in content:
                            processingOTAStreamName = content["streamname"]
                            print(f"Processing OTA stream name: {processingOTAStreamName}")
                        if "files" in content:
                            processingOTAFileSize = content["files"][0]["filesize"]
                            processingOTASignature = content["files"][0]["sig-sha256-ecdsa"]
                            print(f"Processing OTA file size: {processingOTAFileSize}")
                            print(f"Processing OTA signature: {processingOTASignature}")
                        #check if the file size > 0 and the streamename, signature is not empty
                        if processingOTAFileSize > 0 and len(processingOTAStreamName) > 0 and len(processingOTASignature) > 0:
                            #OTA job
                            print("OTA job")
                            processingOTAReadedBytes = 0
                            processingOTARequestedBytes = 0
                            jobState = JobStates.DO_THE_OTA_JOB
            
            if jobState != JobStates.DO_THE_OTA_JOB:
                #publish the job update
                client.publish(f"$aws/things/{thing_name}/jobs/{processingJobName}/update", "{\"status\": \"SUCCEEDED\"}")
                client.unsubscribe(f"$aws/things/{thing_name}/jobs/{processingJobName}/get/accepted")
                processingJobName = ""
                jobState = JobStates.IDLE
            
        elif jobState == JobStates.DO_THE_OTA_JOB:
            endOta = 0
            OtaSuccess = 0
            requestBytes = 2048
            if processingOTAReadedBytes == 0 and processingOTARequestedBytes == 0:
                print("OTA job start to request")
                process_OTA_initHash()
                jsonDataTopic = f"$aws/things/{thing_name}/streams/{processingOTAStreamName}/data/json"
                client.subscribe(jsonDataTopic)
                print(f"OTA job subscribe topic: {jsonDataTopic}")
                jsonDataRejectedTopic = f"$aws/things/{thing_name}/streams/{processingOTAStreamName}/rejected/json"
                client.subscribe(jsonDataRejectedTopic)
                print(f"OTA job subscribe topic: {jsonDataRejectedTopic}")

            if (processingOTAReadedBytes < processingOTARequestedBytes):
                jsonDataTopic = f"$aws/things/{thing_name}/streams/{processingOTAStreamName}/data/json"
                jsonDataRejectedTopic = f"$aws/things/{thing_name}/streams/{processingOTAStreamName}/rejected/json"
                #iterate through messageDataList backwards to avoid index error
                messageDataListLength = len(messageDataList)
                for i in range(messageDataListLength-1, -1, -1):
                    messageData = messageDataList[i]
                    if messageData["topic"] == jsonDataTopic:
                        try:
                            jsonData = json.loads(messageData["payload"])
                            dataLen = 0
                            blockIndex = -1
                            if "l" in jsonData:
                                dataLen = jsonData["l"]
                            dataPayload = 0
                            if "p" in jsonData:
                                dataPayload = jsonData["p"]
                            if "i" in jsonData:
                                blockIndex = jsonData["i"]
                            #print(f"OTA job data length: {dataLen}")
                            #print(f"OTA job data payload: {dataPayload}")
                            #convert the base64 dataPayload to string for debug
                            decodedPayload = base64.b64decode(dataPayload)
                            process_OTA_datachunk(decodedPayload, dataLen, blockIndex*requestBytes)

                            if dataLen > 0:
                                processingOTAReadedBytes += dataLen
                                if processingOTAReadedBytes != processingOTARequestedBytes:
                                    print(f"OTA job readed bytes: {processingOTAReadedBytes}")
                                    print(f"OTA job requested bytes: {processingOTARequestedBytes}")
                                    #OTA job error
                                    print("OTA job error!!!!!!!")
                                    endOta = 1
                                else:
                                    print(f"OTA job readed bytes: {processingOTAReadedBytes}")
                                    print(f"OTA job requested bytes: {processingOTARequestedBytes}")
                                    #process the data
                                    #print(f"OTA job data: {json.dumps(jsonData, indent=2)}")
                            #process the data
                        except json.JSONDecodeError:
                            pass
                    elif messageData["topic"] == jsonDataRejectedTopic:
                        print(f"OTA job rejected: {messageData['payload']}")
                        endOta = 1
                    messageDataList.pop(i)

                pass
            elif (processingOTAReadedBytes == processingOTARequestedBytes):
                if processingOTAReadedBytes == processingOTAFileSize:
                    print("OTA job finished")
                    if (process_OTA_verify_signature(processingOTASignature)):
                        print("OTA job signature is valid.")
                        endOta = 1 
                        OtaSuccess = 1
                    else:
                        print("OTA job signature is invalid.")
                        endOta = 1
                        OtaSuccess = 0
 
                else:
                    jsonDataRequestTopic = f"$aws/things/{thing_name}/streams/{processingOTAStreamName}/get/json"
                    #create the GetStream request https://docs.aws.amazon.com/iot/latest/developerguide/mqtt-based-file-delivery-in-devices.html
                    idOfStream = 0
                    payloadLength = requestBytes
                    offset = processingOTAReadedBytes//requestBytes 
                    numberOfBlocks = 1
                    getStreamRequest = f"{{\"f\": {idOfStream}, \"l\": {payloadLength}, \"o\": {offset}, \"n\": {numberOfBlocks}}}"
                    print(f"OTA job request: {getStreamRequest}")
                    client.publish(jsonDataRequestTopic, getStreamRequest)
                    actualExpectedBytes = requestBytes
                    leftoverBytes = processingOTAFileSize - processingOTAReadedBytes
                    if leftoverBytes < requestBytes:
                        actualExpectedBytes = leftoverBytes
                    processingOTARequestedBytes = processingOTAReadedBytes + actualExpectedBytes
                    jobStateChangeTimeMillis = (time.time()-appStartTime)*1000
                pass


            else:
                print("OTA job error!!!!!!!")
                endOta = 1

            if passedMillis > timeoutLimit[JobStates.DO_THE_OTA_JOB]:
                print("Timeout waiting for job OTA response")
                if endOta==0:
                    endOta = 1

            if endOta != 0:
                jsonDataTopic = f"$aws/things/{thing_name}/streams/{processingOTAStreamName}/data/json"
                client.unsubscribe(jsonDataTopic)
                print(f"OTA job unsubscribe topic: {jsonDataTopic}")
                jsonDataRejectedTopic = f"$aws/things/{thing_name}/streams/{processingOTAStreamName}/rejected/json"
                client.unsubscribe(jsonDataRejectedTopic)
                print(f"OTA job unsubscribe topic: {jsonDataRejectedTopic}")
                jobUpdateTopic = f"$aws/things/{thing_name}/jobs/{processingJobName}/update"
                client.unsubscribe(jobUpdateTopic)
                print(f"OTA job unsubscribe topic: {jobUpdateTopic}")
                if OtaSuccess != 0:
                    client.publish(jobUpdateTopic, "{\"status\": \"SUCCEEDED\"}")
                    print("update OTA job status to SUCCEEDED")
                else:
                    client.publish(jobUpdateTopic, "{\"status\": \"FAILED\"}")
                    print("update OTA job status to FAILED")

                processingJobName = ""
                processingOTAStreamName = ""
                processingOTAFileSize = 0
                processingOTASignature = ""
                processingOTAReadedBytes = 0
                processingOTARequestedBytes = 0

                jobState = JobStates.IDLE



        if jobState != prevJobState:
            jobStateChangeTimeMillis = (time.time()-appStartTime)*1000
            print(f"Job state changed to {jobState} at {int(jobStateChangeTimeMillis)} ms")

        #general process

        #check if there is unexpected data in the messageDataList

        #iterate backwards to avoid index error
        messageDataListLength = len(messageDataList)
        for i in range(messageDataListLength-1, -1, -1):
            messageData = messageDataList[i]
            if messageData["topic"] == f"$aws/things/{thing_name}/jobs/notify":
                try:
                    jsonData = json.loads(messageData["payload"])
                    #print(f"Job notification: {json.dumps(jsonData, indent=2)}")
                    jobs = jsonData.get("jobs", [])
                    #print(f"Jobs: {json.dumps(jobs, indent=2)}")
                    if jobs:
                        queuedJobs = jobs.get("QUEUED", [])
                        #print(f"Queued Jobs: {json.dumps(queuedJobs, indent=2)}")
                        if queuedJobs:
                            for job in queuedJobs:
                                #print(f"Queued Job: {json.dumps(job, indent=2)}")
                                jobId = job.get("jobId")
                                if jobId:
                                    print(f"Queued Job ID: {jobId}")
                                    #not care inProgress or queued
                                    awsJobList.append(jobId)
                except json.JSONDecodeError:
                    pass
                messageDataList.pop(i)



        #all messages in the messageDataList are processed
        messageDataList.clear()

        #check if there is a job in the awsJobList



except KeyboardInterrupt:
    print("Exiting...")
    client.disconnect()