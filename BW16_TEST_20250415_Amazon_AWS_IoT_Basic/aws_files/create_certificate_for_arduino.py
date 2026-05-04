import os
import urllib.request

#check if root-CA.crt exists in the current directory

if not os.path.exists("root-CA.crt"):
    print("Downloading root-CA.crt from Amazon Trust Repository...")
    try:
        url = "https://www.amazontrust.com/repository/AmazonRootCA1.pem"
        urllib.request.urlretrieve(url, "root-CA.crt")
        print("root-CA.crt downloaded successfully.")
    except Exception as e:
        print(f"Failed to download root-CA.crt: {e}")
    exit(1)

#prepare the format for the certificate
readFile = open("root-CA.crt", "r")
fileContent = readFile.read()
readFile.close()
outputString = ""
for line in fileContent.splitlines():
    line = line.strip()
    line = '\"' + line + '\\n\" \\'
    outputString += line + "\n"
outputString = outputString[:-3]
outputString = outputString + ';'

print("\n\n-----root ca----\n\n")
print(outputString)

readFile = open("amebaDevBoard.cert.pem", "r")
fileContent = readFile.read()
readFile.close()
outputString = ""
for line in fileContent.splitlines():
    line = line.strip()
    line = '\"' + line + '\\n\" \\'
    outputString += line + "\n"
outputString = outputString[:-3]
outputString = outputString + ';'

print("\n\n-----device cert----\n\n")
print(outputString)

readFile = open("amebaDevBoard.private.key", "r")
fileContent = readFile.read()
readFile.close()
outputString = ""
for line in fileContent.splitlines():
    line = line.strip()
    line = '\"' + line + '\\n\" \\'
    outputString += line + "\n"
outputString = outputString[:-3]
outputString = outputString + ';'

print("\n\n-----device private key----\n\n")
print(outputString)

