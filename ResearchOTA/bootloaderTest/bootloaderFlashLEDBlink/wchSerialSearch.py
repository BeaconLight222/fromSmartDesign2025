import serial.tools.list_ports

def find_serial_port_by_pid(pid):
    pid_hex = pid.lower()
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if pid_hex in port.hwid.lower():
            return port.device
    return None

# Replace '1a86' with the PID you are looking for
pid = "1a86"
serial_port = find_serial_port_by_pid(pid)

if serial_port:
    print(f"{serial_port}")