from flask import Flask, render_template, jsonify, request
import serial
import serial.tools.list_ports
import threading
import json
import time
import os

app = Flask(__name__)

# --------------------------------------------------
# Config
# --------------------------------------------------
# You can set SERIAL_PORT manually, for example:
# Windows USB: COM5
# Windows Bluetooth SPP: COM7
# Linux USB: /dev/ttyACM0 or /dev/ttyUSB0
# Linux Bluetooth SPP: /dev/rfcomm0
SERIAL_PORT = os.environ.get("SERIAL_PORT", "").strip()
BAUD_RATE = int(os.environ.get("BAUD_RATE", "115200"))
SERIAL_TIMEOUT = float(os.environ.get("SERIAL_TIMEOUT", "0.2"))
STALE_TIMEOUT = float(os.environ.get("STALE_TIMEOUT", "2.0"))
SCAN_INTERVAL = float(os.environ.get("SCAN_INTERVAL", "2.0"))

# Friendly name from your ESP32 sketch
BT_DEVICE_NAME_HINT = os.environ.get("BT_DEVICE_NAME_HINT", "ESP32-IMU-Flight")

# --------------------------------------------------
# Shared IMU state
# --------------------------------------------------
latest_data = {
    "pitch": 0.0,
    "roll": 0.0,
    "rawPitch": 0.0,
    "rawRoll": 0.0,
    "gx": 0.0,
    "gy": 0.0,
    "gz": 0.0,
    "connected": False,
    "stale": True,
    "timestamp": 0.0,
    "age_ms": None,
    "packets": 0,
    "last_line": "",
    "port": None,
    "baudRate": BAUD_RATE,
    "status": "starting",
}

data_lock = threading.Lock()

ser = None
ser_lock = threading.Lock()

# --------------------------------------------------
# Helpers
# --------------------------------------------------
def update_data(**kwargs):
    with data_lock:
        latest_data.update(kwargs)

def get_data_copy():
    with data_lock:
        return dict(latest_data)

def to_float(value, default=0.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default

def list_candidate_ports():
    """Return likely serial ports, preferring the user-configured one first."""
    candidates = []

    if SERIAL_PORT:
        candidates.append(SERIAL_PORT)

    try:
        ports = list(serial.tools.list_ports.comports())
    except Exception:
        ports = []

    scored = []
    for p in ports:
        score = 0
        text = f"{p.device} {p.description} {p.manufacturer} {p.hwid}".lower()

        # Prefer obvious USB serial devices
        if "ttyacm" in p.device.lower() or "usb" in p.device.lower():
            score += 40

        # Prefer Bluetooth serial/SPP ports
        if "bluetooth" in text or "standard serial over bluetooth" in text:
            score += 50

        # Prefer ESP / CP210 / CH340 / USB-UART adapters
        if "esp32" in text or "cp210" in text or "ch340" in text or "uart" in text:
            score += 20

        # Prefer device name hint from your sketch
        if BT_DEVICE_NAME_HINT.lower() in text:
            score += 60

        scored.append((score, p.device))

    scored.sort(reverse=True)

    for _, dev in scored:
        if dev not in candidates:
            candidates.append(dev)

    # Common Linux fallbacks
    fallbacks = ["/dev/ttyUSB0"]
    for dev in fallbacks:
        if dev not in candidates:
            candidates.append(dev)

    return candidates

def open_serial_port():
    """Try to open the configured or detected serial port."""
    candidates = list_candidate_ports()

    for port in candidates:
        try:
            print(f"[SERIAL] Trying {port} @ {BAUD_RATE}")
            s = serial.Serial(port, BAUD_RATE, timeout=SERIAL_TIMEOUT)
            time.sleep(2.0)  # allow ESP32 reset / BT settle

            try:
                s.reset_input_buffer()
                s.reset_output_buffer()
            except Exception:
                pass

            print(f"[SERIAL] Opened {port}")
            update_data(
                connected=False,
                stale=True,
                port=port,
                baudRate=BAUD_RATE,
                status=f"opened {port}"
            )
            return s

        except Exception as e:
            print(f"[SERIAL] Failed {port}: {e}")

    return None

def close_serial():
    global ser
    with ser_lock:
        try:
            if ser and ser.is_open:
                ser.close()
        except Exception:
            pass
        ser = None

def send_command_char(cmd_char: str):
    """Send single-char command to ESP32: c or r."""
    global ser
    with ser_lock:
        if ser is None or not ser.is_open:
            return False, "serial not connected"

        try:
            ser.write(cmd_char.encode("utf-8"))
            ser.flush()
            return True, f"sent {cmd_char}"
        except Exception as e:
            return False, str(e)

# --------------------------------------------------
# Serial reader thread
# --------------------------------------------------
def serial_reader():
    global ser

    while True:
        try:
            if ser is None or not ser.is_open:
                ser = open_serial_port()

                if ser is None:
                    update_data(
                        connected=False,
                        stale=True,
                        status="no serial port found"
                    )
                    time.sleep(SCAN_INTERVAL)
                    continue

            raw = ser.readline()

            if not raw:
                current = get_data_copy()
                age = time.time() - current["timestamp"] if current["timestamp"] > 0 else 9999

                if age > STALE_TIMEOUT:
                    update_data(
                        connected=False,
                        stale=True,
                        age_ms=int(age * 1000),
                        status="no recent data"
                    )
                continue

            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            update_data(last_line=line)

            # Ignore boot/debug lines like:
            # "Starting ESP32..."
            # "[BT] client=connected"
            # "Calibration starts..."
            if not line.startswith("{"):
                continue

            try:
                data = json.loads(line)
            except json.JSONDecodeError:
                print("[SERIAL] Bad JSON:", line)
                continue

            now = time.time()
            current = get_data_copy()
            packet_count = int(current.get("packets", 0)) + 1

            update_data(
                pitch=to_float(data.get("pitch", 0.0)),
                roll=to_float(data.get("roll", 0.0)),
                rawPitch=to_float(data.get("rawPitch", data.get("pitch", 0.0))),
                rawRoll=to_float(data.get("rawRoll", data.get("roll", 0.0))),
                gx=to_float(data.get("gx", 0.0)),
                gy=to_float(data.get("gy", 0.0)),
                gz=to_float(data.get("gz", 0.0)),
                connected=True,
                stale=False,
                timestamp=now,
                age_ms=0,
                packets=packet_count,
                status="streaming"
            )

        except serial.SerialException as e:
            print("[SERIAL] Serial error:", e)
            update_data(
                connected=False,
                stale=True,
                status=f"serial error: {e}"
            )
            close_serial()
            time.sleep(SCAN_INTERVAL)

        except Exception as e:
            print("[SERIAL] Unexpected error:", e)
            update_data(
                connected=False,
                stale=True,
                status=f"unexpected error: {e}"
            )
            close_serial()
            time.sleep(SCAN_INTERVAL)

# --------------------------------------------------
# Routes
# --------------------------------------------------
@app.route("/")
def index():
    return render_template("index.html")

@app.route("/imu")
def imu():
    data = get_data_copy()

    if data["timestamp"] > 0:
        age = time.time() - data["timestamp"]
        data["age_ms"] = int(age * 1000)

        if age > STALE_TIMEOUT:
            data["connected"] = False
            data["stale"] = True
    else:
        data["connected"] = False
        data["stale"] = True
        data["age_ms"] = None

    return jsonify(data)

@app.route("/health")
def health():
    data = get_data_copy()
    return jsonify({
        "ok": data["connected"] and not data["stale"],
        "connected": data["connected"],
        "stale": data["stale"],
        "packets": data["packets"],
        "age_ms": data["age_ms"],
        "port": data["port"],
        "baudRate": data["baudRate"],
        "status": data["status"],
        "last_line": data["last_line"],
    })

@app.route("/ports")
def ports():
    items = []
    try:
        for p in serial.tools.list_ports.comports():
            items.append({
                "device": p.device,
                "description": p.description,
                "manufacturer": p.manufacturer,
                "hwid": p.hwid,
            })
    except Exception as e:
        return jsonify({"ok": False, "error": str(e), "ports": []}), 500

    return jsonify({
        "ok": True,
        "configured_port": SERIAL_PORT or None,
        "ports": items
    })

@app.route("/command/<cmd>", methods=["POST", "GET"])
def command(cmd):
    cmd = cmd.lower().strip()

    if cmd not in ("c", "r"):
        return jsonify({"ok": False, "error": "invalid command, use c or r"}), 400

    ok, msg = send_command_char(cmd)
    return jsonify({
        "ok": ok,
        "command": cmd,
        "message": msg
    }), (200 if ok else 503)

# Optional convenience routes
@app.route("/recenter", methods=["POST", "GET"])
def recenter():
    ok, msg = send_command_char("c")
    return jsonify({"ok": ok, "message": msg}), (200 if ok else 503)

@app.route("/recalibrate", methods=["POST", "GET"])
def recalibrate():
    ok, msg = send_command_char("r")
    return jsonify({"ok": ok, "message": msg}), (200 if ok else 503)

# --------------------------------------------------
# Main
# --------------------------------------------------
if __name__ == "__main__":
    t = threading.Thread(target=serial_reader, daemon=True)
    t.start()
    app.run(host="0.0.0.0", port=5000, debug=False, threaded=True)
