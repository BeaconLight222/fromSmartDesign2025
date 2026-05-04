#!/usr/bin/env python3



import sys
import os

#the command must have at least 1 argument
if len(sys.argv) < 2:
    print("Usage: python generateBoardSettings.py <nameOfSubfolder>")
    sys.exit(1)

this_script_path = os.path.abspath(__file__)

target_folder_path = os.path.join(os.path.dirname(this_script_path), sys.argv[1])

if not os.path.exists(target_folder_path):
    print(f"Target folder {target_folder_path} does not exist.")
    sys.exit(1)

# look for a file that contains "cert" in filename
cert_file = None
for file in os.listdir(target_folder_path):
    if "cert" in file :
        cert_file = file
        break
private_key_file = None
for file in os.listdir(target_folder_path):
    if "private" in file :
        private_key_file = file
        break
client_id_file = None
if os.path.exists(os.path.join(target_folder_path, "client_id.txt")):
    client_id_file = "client_id.txt"
thing_name_file = None
if os.path.exists(os.path.join(target_folder_path, "thing_name.txt")):
    thing_name_file = "thing_name.txt"

print(f"cert_file: {cert_file}")
print(f"private_key_file: {private_key_file}")
print(f"client_id_file: {client_id_file}")
print(f"thing_name_file: {thing_name_file}")

#check if any of the files are None
if cert_file is None or private_key_file is None or client_id_file is None or thing_name_file is None:
    print("One or more files are missing. Please make sure all files are present.")
    sys.exit(1)

print("Generating board settings file.........................")
offset_client_id = 0
offset_thing_name = 64
offset_cert = 128
offset_private_key = 128 + 1500

with open(os.path.join(target_folder_path, client_id_file), "r") as f:
    client_id_str = f.read().strip()
with open(os.path.join(target_folder_path, thing_name_file), "r") as f:
    thing_name_str = f.read().strip()
with open(os.path.join(target_folder_path, cert_file), "r") as f:
    cert_str = f.read()
with open(os.path.join(target_folder_path, private_key_file), "r") as f:
    private_key_str = f.read()

valid_client_id_length = offset_thing_name - offset_client_id
if len(client_id_str) > valid_client_id_length:
    print(f"Client ID is too long. Maximum length is {valid_client_id_length} characters.")
    sys.exit(1)
valid_thing_name_length = offset_cert - offset_thing_name
if len(thing_name_str) > valid_thing_name_length:
    print(f"Thing Name is too long. Maximum length is {valid_thing_name_length} characters.")
    sys.exit(1)
valid_cert_length = offset_private_key - offset_cert
if len(cert_str) > valid_cert_length:
    print(f"Certificate is too long. Maximum length is {valid_cert_length} characters.")
    sys.exit(1)
valid_private_key_length = 4096-offset_private_key
if len(private_key_str) > valid_private_key_length:
    print(f"Private Key is too long. Maximum length is {valid_private_key_length} characters.")
    sys.exit(1)

spacing_between_client_id_and_thing_name = offset_thing_name - offset_client_id - len(client_id_str)
spacing_between_thing_name_and_cert = offset_cert - offset_thing_name - len(thing_name_str)
spacing_between_cert_and_private_key = offset_private_key - offset_cert - len(cert_str)
spacing_between_private_key_and_end = 4096 - offset_private_key - len(private_key_str)

#concatenate the strings with the correct spacing
client_id_str_padded = client_id_str + "\x00" * spacing_between_client_id_and_thing_name
thing_name_str_padded = thing_name_str + "\x00" * spacing_between_thing_name_and_cert
cert_str_padded = cert_str + "\x00" * spacing_between_cert_and_private_key
private_key_str_padded = private_key_str + "\x00" * spacing_between_private_key_and_end

overall_str = client_id_str_padded + thing_name_str_padded + cert_str_padded + private_key_str_padded
#write the string to a file
with open(os.path.join(target_folder_path, "deviceSetting.bin"), "wb") as f:
    f.write(overall_str.encode('utf-8'))
print("Board settings file generated successfully.")

print()
print()
print()

print("// Suppose this binary is going to be flashed to 2nd half of the user flash")
print("// That is 1 page. on 0x08201000")
print(f"#define CLIENT_ID_FLASH_ADDR {(0x08201000+offset_client_id):#x}")
print(f"#define THING_NAME_FLASH_ADDR {(0x08201000+offset_thing_name):#x}")
print(f"#define CERT_FLASH_ADDR {(0x08201000+offset_cert):#x}")
print(f"#define PRIVATE_KEY_FLASH_ADDR {(0x08201000+offset_private_key):#x}")

print()
print()
print()