#!/usr/bin/env python3

import sys
import os

current_script_path = os.path.abspath(__file__)
credential_path = os.path.dirname(current_script_path)
if not os.path.exists(credential_path):
    print(f"Path {credential_path} does not exist.")
    sys.exit(1)

#list all files in the eu-west-certs_tad_20250716 folder, filter files end with crt.pem or key.pem
cert_files = []
for root, dirs, files in os.walk(credential_path):
    for filename in files:
        if filename.endswith(".crt.pem") or filename.endswith(".key.pem") or filename.endswith(".cert.pem") or filename.endswith(".private.pem.key"):
            cert_files.append(os.path.join(root, filename))

nameList = []
for cert_file in cert_files:
    #get the name of the file without the path and extension
    name = os.path.splitext(os.path.basename(cert_file))[0].split('.')[0]
    nameList.append(name)
#remove duplicates
nameList = list(set(nameList))
#sort the list
nameList.sort()
for name in nameList:
    crtPath = os.path.join(credential_path, f"{name}.crt.pem")
    if not os.path.exists(crtPath):
        crtPath = os.path.join(credential_path, f"{name}.cert.pem")
    if not os.path.exists(crtPath):
        print(f"Certificate file {crtPath} does not exist.")
        continue

    keyPath = os.path.join(credential_path, f"{name}.key.pem")
    if not os.path.exists(keyPath):
        keyPath = os.path.join(credential_path, f"{name}.private.key")
    if not os.path.exists(keyPath):
        print(f"Private key file {keyPath} does not exist.")
        continue
    print(name)

    offset_client_id = 0
    offset_thing_name = 64
    offset_cert = 128
    offset_private_key = 128 + 1500

    client_id_str = name 
    thing_name_str = name
    with open(crtPath, "r") as f:
        cert_str = f.read()
    with open(keyPath, "r") as f:
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
    with open(os.path.join(credential_path, name + ".bin"), "wb") as f:
        f.write(overall_str.encode('utf-8'))