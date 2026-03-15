from flask import Flask, render_template, jsonify
import serial
import threading
import json
import time
import os

app = Flask(__name__)

# --------------------------------------------------
# Config
# --------------------------------------------------
SERIAL_PORT = os.environ.get("SERIAL_PORT", "/dev/ttyACM0")
BAUD_RATE = int(os.environ.get("BAUD_RATE", "115200"))
SERIAL_TIMEOUT = float(os.environ.get("SERIAL_TIMEOUT", "0.2"))
STALE_TIMEOUT = float(os.environ.get("STALE_TIMEOUT", "2.0"))

# --------------------------------------------------
# Shared IMU state
# --------------------------------------------------
latest_data = {
    # final processed values from Arduino
    "pitch": 0.0,
    "roll": 0.0,

    # raw / pre-centered angles from Arduino (very useful for debugging)
    "rawPitch": 0.0,
    "rawRoll": 0.0,

    # gyro
    "gx": 0.0,
    "gy": 0.0,
    "gz": 0.0,

    # connection / health
    "connected": False,
    "stale": True,
    "timestamp": 0.0,
    "age_ms": None,

    # optional debug info
    "packets": 0,
    "last_line": "",
    "port": SERIAL_PORT,
    "baudRate": BAUD_RATE,
}

data_lock = threading.Lock()
ser = None


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


# --------------------------------------------------
# Serial reader thread
# --------------------------------------------------
def serial_reader():
    global ser

    while True:
        try:
            # Open serial port if needed
            if ser is None or not ser.is_open:
                print(f"[SERIAL] Opening {SERIAL_PORT} @ {BAUD_RATE}")
                ser = serial.Serial(
                    SERIAL_PORT,
                    BAUD_RATE,
                    timeout=SERIAL_TIMEOUT
                )

                # Give Arduino time to reboot after serial connection
                time.sleep(2.0)

                # Clear any boot logs / partial lines
                try:
                    ser.reset_input_buffer()
                    ser.reset_output_buffer()
                except Exception:
                    pass

                print("[SERIAL] Port opened")

            raw = ser.readline()

            if not raw:
                # No line this cycle: mark stale only if too old
                current = get_data_copy()
                age = time.time() - current["timestamp"] if current["timestamp"] > 0 else 9999

                if age > STALE_TIMEOUT:
                    update_data(
                        connected=False,
                        stale=True,
                        age_ms=int(age * 1000)
                    )
                continue

            line = raw.decode("utf-8", errors="ignore").strip()

            if not line:
                continue

            # Save the latest raw line for debugging
            update_data(last_line=line)

            # Ignore non-JSON boot / debug lines from Arduino
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
                # final processed control values
                pitch=to_float(data.get("pitch", 0.0)),
                roll=to_float(data.get("roll", 0.0)),

                # raw values from Arduino if present
                rawPitch=to_float(data.get("rawPitch", data.get("pitch", 0.0))),
                rawRoll=to_float(data.get("rawRoll", data.get("roll", 0.0))),

                # gyro values
                gx=to_float(data.get("gx", 0.0)),
                gy=to_float(data.get("gy", 0.0)),
                gz=to_float(data.get("gz", 0.0)),

                # connection state
                connected=True,
                stale=False,
                timestamp=now,
                age_ms=0,

                # debug
                packets=packet_count,
                port=SERIAL_PORT,
                baudRate=BAUD_RATE
            )

        except serial.SerialException as e:
            print("[SERIAL] Serial error:", e)
            update_data(connected=False, stale=True)

            try:
                if ser and ser.is_open:
                    ser.close()
            except Exception:
                pass

            ser = None
            time.sleep(2.0)

        except Exception as e:
            print("[SERIAL] Unexpected error:", e)
            update_data(connected=False, stale=True)

            try:
                if ser and ser.is_open:
                    ser.close()
            except Exception:
                pass

            ser = None
            time.sleep(2.0)


# --------------------------------------------------
# Routes
# --------------------------------------------------
@app.route("/")
def index():
    return render_template("index.html")


@app.route("/imu")
def imu():
    data = get_data_copy()

    # Compute staleness at response time too
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


# Optional debug route
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
        "last_line": data["last_line"],
    })


# --------------------------------------------------
# Main
# --------------------------------------------------
if __name__ == "__main__":
    t = threading.Thread(target=serial_reader, daemon=True)
    t.start()

    # Keep debug=False with serial threads
    app.run(host="0.0.0.0", port=5000, debug=False, threaded=True)
