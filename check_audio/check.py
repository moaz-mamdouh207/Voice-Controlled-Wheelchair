import serial

PORT = "COM3"   # change this
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=1)

input("Press ENTER to start recording...")
ser.write(b'\n')   # start

input("Press ENTER to stop recording...")
ser.write(b'\n')   # stop

print("Receiving audio...")

with open("16newconnection.wav", "wb") as f:
    started = False

    while True:
        data = ser.read(512)

        if not data:
            continue

        if not started:
            if b"START\n" in data:
                started = True
                data = data.split(b"START\n")[1]
            else:
                continue

        if b"\nEND\n" in data:
            data = data.split(b"\nEND\n")[0]
            f.write(data)
            break

        f.write(data)

print("Saved as audio.wav")