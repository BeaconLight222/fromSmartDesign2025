import os

fileToRead = ["km0_boot_all.bin","km4_boot_all.bin"]

# Check if the directory exists
if not os.path.exists("modifiedBootloader"):
    os.makedirs("modifiedBootloader")

for file in fileToRead:
    readFile = os.path.join("originalArduino", file)
    writeFile = os.path.join("modifiedBootloader", file)
    if os.path.exists(readFile):
        with open(readFile, "rb") as f:
            data = f.read()
        print(f"Read {len(data)} bytes from {readFile}")
        # search for binary sequence 00 60 00 08 00 60 10 08, then replace it with 00 60 00 08 00 60 20 08
        search_sequence = b'\x00\x60\x00\x08\x00\x60\x10\x08'
        replace_sequence = b'\x00\x60\x00\x08\x00\x60\x20\x08'

        # Check if the search sequence is in the data, if so, print the address
        if search_sequence in data:
            address = data.index(search_sequence)
            print(f"Found search sequence at address: {address}")
        else:
            print("Search sequence not found in the file.")
            continue

        # Replace the sequence in the data
        modified_data = data.replace(search_sequence, replace_sequence)

        # Write the modified data to the new file
        with open(writeFile, "wb") as f:
            f.write(modified_data)
        print(f"Modified data written to {writeFile}")

