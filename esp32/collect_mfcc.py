"""
collect_mfcc.py
Triggers ESP32 to record + compute MFCC, saves directly to CSV.

Usage:
  python collect_mfcc.py --port COM3 --label noise --count 100
"""

import serial, time, os, argparse
import pandas as pd

BAUD_RATE  = 115200
DATA_DIR   = "mfcc_data"
NUM_COEFFS = 13
MAX_FRAMES = 47  # must match MFCC_MAX_FRAMES

def collect_one(ser):
    ser.reset_input_buffer()
    ser.write(b'm')

    # Wait for READY
    deadline = time.time() + 3.0
    while time.time() < deadline:
        line = ser.readline().decode("utf-8", errors="ignore").strip().strip('\x00')
        if line == "READY":
            break
    else:
        raise TimeoutError("ESP32 didn't respond")

    print("    Speak now...")

    # Wait for START
    deadline = time.time() + 8.0
    while time.time() < deadline:
        line = ser.readline().decode("utf-8", errors="ignore").strip().strip('\x00')
        if line == "START":
            break
        if line == "TIMEOUT":
            raise TimeoutError("VAD timeout — no speech detected")
    else:
        raise TimeoutError("No START marker")

    # Read MAX_FRAMES rows
    rows = []
    while True:
        line = ser.readline().decode("utf-8", errors="ignore").strip().strip('\x00')
        if not line:
            continue          # skip empty lines
        if line == "END":
            break
        try:
            vals = [float(x) for x in line.split(",")]
            if len(vals) == NUM_COEFFS:   # only accept complete rows
                rows.append(vals)
        except ValueError:
            print("corrupted row")
            continue

    if len(rows) == 0:
        raise ValueError("No rows received")
    
    # Pad with zero rows if slightly short — matches memset in mfcc_compute()
    while len(rows) < MAX_FRAMES:
        print("Pad with zero rows if slightly short")
        rows.append([0.0] * NUM_COEFFS)
    
    # Trim if somehow longer
    rows = rows[:MAX_FRAMES]
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port",  required=True)
    parser.add_argument("--label", required=True)
    parser.add_argument("--count", type=int, default=40)
    args = parser.parse_args()

    out_dir = os.path.join(DATA_DIR, args.label)
    os.makedirs(out_dir, exist_ok=True)

    existing = [f for f in os.listdir(out_dir) if f.endswith(".csv")]
    start_idx = len(existing) + 1

    print(f"\nCollecting '{args.label}' — {args.count} clips")
    print(f"Each clip: {MAX_FRAMES} frames × {NUM_COEFFS} coeffs\n")

    with serial.Serial(args.port, BAUD_RATE, timeout=5) as ser:
        time.sleep(2)
        ser.reset_input_buffer()

        for i in range(args.count):
            clip_num = start_idx + i
            fname = f"clip_{clip_num:03d}.csv"
            fpath = os.path.join(out_dir, fname)

            input(f"  [{i+1}/{args.count}] Press Enter then say '{args.label}'...")

            try:
                rows = collect_one(ser)
                df = pd.DataFrame(rows,
                    columns=[f"mfcc_{c}" for c in range(NUM_COEFFS)])
                df.to_csv(fpath, index=False)
                print(f"    Saved {fname}")
            except Exception as e:
                print(f"    ERROR: {e}")

    print(f"\nDone. Run: python train.py")


if __name__ == "__main__":
    main()