#we want to get the result form example.txt
#"sig-sha256-ecdsa": "MEYCIQClLGxvGlVlljtlmrHc+LkKz2hyNKaE7jDHuQYhoYkBmAIhANdZpypAQ09n2VMKtNg89HlAmYOUonW4q6BVsWtxor+S"
#"sig-sha256-ecdsa": "MEQCIADVSxWrHnluei0bBn1EZD9+MQ5JlcHo/2Vp7ackiqEOAiB/yC/0QjPyE9OcD9B4j+jXCA3CdJHcBVv/1ySvppk+7Q=="

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization
from cryptography.x509 import load_pem_x509_certificate
from cryptography.hazmat.primitives.asymmetric.utils import Prehashed

import base64

# Load private key
with open("ecdsasigner.key", "rb") as key_file:
    private_key = serialization.load_pem_private_key(
        key_file.read(),
        password=None,
    )

#load data from example.txt as binary
with open("example.txt", "rb") as f:
    data = f.read()

#print data as c array for c code to load
print("const uint8_t example_data[] = {")
for i in range(0, len(data), 16):
    print("    " + ", ".join(f"0x{byte:02x}" for byte in data[i:i+16]) + ",")
print("};")

#sign the data
signature = private_key.sign(
    data,
    ec.ECDSA(hashes.SHA256())
)
# Encode the signature in base64
signature_b64 = base64.b64encode(signature).decode('utf-8')
# Print the base64 encoded signature
print(signature_b64)

awsSignature = "MEYCIQClLGxvGlVlljtlmrHc+LkKz2hyNKaE7jDHuQYhoYkBmAIhANdZpypAQ09n2VMKtNg89HlAmYOUonW4q6BVsWtxor+S"
#validate if the the awsSignature is valid

# Load public key from certificate
with open("ecdsasigner.crt", "rb") as cert_file:
    cert = load_pem_x509_certificate(cert_file.read())
    public_key = cert.public_key()

with open("ecdsasigner.crt", "r") as cert_file_text:
    cert_text = cert_file_text.read()
    #print the certificate in a multiline string for c
    print("const char *certificate = ")
    for line in cert_text.splitlines():
        print("    \"" + line.strip() + "\\n\"")
    print(";")
# Print the public key in a multiline string for c


signature = base64.b64decode(awsSignature)

# Verify the signature
try:
    public_key.verify(
        signature,
        data,
        ec.ECDSA(hashes.SHA256())
    )
    print("AWS Signature is valid.")
except Exception as e:
    print("AWS Signature is invalid.")
    print(e)

signature = base64.b64decode(signature_b64)

try:
    public_key.verify(
        signature,
        data,
        ec.ECDSA(hashes.SHA256())
    )
    print("Calculated Signature is valid.")
except Exception as e:
    print("Calculated Signature is invalid.")
    print(e)

#divide data into 2048 byte chunks
chunk_size = 2048
data_chunks = [data[i:i + chunk_size] for i in range(0, len(data), chunk_size)]

digest = hashes.Hash(hashes.SHA256())
for chunk in data_chunks:
    digest.update(chunk)

data_hash = digest.finalize()

#print data_hash as hex numbers
print("const uint8_t data_hash[] = {")
for i in range(0, len(data_hash), 16):
    print("    " + ", ".join(f"0x{byte:02x}" for byte in data_hash[i:i+16]) + ",")
print("};")

digestWhole = hashes.Hash(hashes.SHA256())
digestWhole.update(data)
data_hash_whole = digestWhole.finalize()
print("const uint8_t data_hash_whole[] = {")
for i in range(0, len(data_hash_whole), 16):
    print("    " + ", ".join(f"0x{byte:02x}" for byte in data_hash_whole[i:i+16]) + ",")
print("};")

signature = base64.b64decode(awsSignature)
try:
    public_key.verify(
        signature,
        data_hash,
        ec.ECDSA(Prehashed(hashes.SHA256()))
    )
    print("AWS Signature is valid for chunks.")
except Exception as e:
    print("AWS Signature is invalid for chunks.")
    print(e)