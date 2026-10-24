# Append the MatrixOS USB serial log to a file; reopens the port if the board reboots.
import serial, sys, time
port, out, seconds = sys.argv[1], sys.argv[2], float(sys.argv[3])
end = time.time() + seconds
with open(out, 'ab', buffering=0) as f:
    while time.time() < end:
        try:
            with serial.Serial(port, 115200, timeout=0.5) as s:
                s.dtr = True
                f.write(b'\n[seriallog] opened %s\n' % port.encode())
                while time.time() < end:
                    f.write(s.read(4096))
        except (serial.SerialException, OSError):
            time.sleep(1)
