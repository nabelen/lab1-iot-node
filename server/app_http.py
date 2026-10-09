import csv
import os
from datetime import datetime

from flask import Flask, request, jsonify

app = Flask(__name__)

LOG_FILE = "http_log.csv"


def log_to_csv(data):
    """Guarda cada mensaje recibido (para calcular P_entrega real)."""
    new = not os.path.exists(LOG_FILE)
    with open(LOG_FILE, "a", newline="") as f:
        w = csv.writer(f)
        if new:
            w.writerow(["pc_time", "node_id", "seq", "rssi"])
        w.writerow([
            datetime.now().isoformat(),
            data.get("node_id"),
            data.get("seq"),
            data.get("rssi"),
        ])


@app.route("/telemetry", methods=["POST"])
def receive_telemetry():
    data = request.get_json(silent=True)

    if data is None:
        return jsonify({"status": "error", "message": "JSON no válido"}), 400

    print("\n========== TELEMETRÍA RECIBIDA ==========")
    print(f"Fecha/hora:    {datetime.now()}")
    print(f"Nodo:          {data.get('node_id')}")
    print(f"Secuencia:     {data.get('seq')}")
    print(f"Timestamp:     {data.get('timestamp')}")
    print(f"Temperatura:   {data.get('temperature')} °C")
    print(f"Humedad:       {data.get('humidity')} %")
    print(f"Ventilador:    {data.get('fan')}")
    print(f"RSSI:          {data.get('rssi')} dBm")
    print(f"Uptime:        {data.get('uptime_ms')} ms")
    print(f"Alerta:        {data.get('alert_type')}")
    print("==========================================")

    log_to_csv(data)

    return jsonify({
        "status": "ok",
        "message": "Telemetría recibida correctamente"
    }), 200


@app.route("/", methods=["GET"])
def home():
    return "API ESP32 funcionando correctamente"


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
