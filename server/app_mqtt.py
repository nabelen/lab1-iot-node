"""
mqtt_ack_responder.py (Versión Optimizada)
Responde inmediatamente al ESP32 reduciendo la latencia interna
y asegurando compatibilidad con Paho MQTT v1 y v2.
"""

import sys
import socket
import json
import paho.mqtt.client as mqtt

BROKER = "192.168.0.103"
PORT = 1883
NODE_ID = "ESP32_01"

TELEMETRY_TOPIC = f"telecom/lab1/{NODE_ID}/telemetry"
ACK_TOPIC = f"telecom/lab1/{NODE_ID}/ack"


def on_connect(client, userdata, flags, rc, properties=None):
    # Compatible tanto con rc entero (v1) como con ReasonCode (v2)
    rc_code = getattr(rc, "value", rc)
    if rc_code == 0:
        print(f"[MQTT] Conectado exitosamente a {BROKER}")
        # Suscripción con QoS 1 para reducir pérdidas en el broker
        client.subscribe(TELEMETRY_TOPIC, qos=1)
        print(f"[MQTT] Suscrito a: {TELEMETRY_TOPIC} (QoS 1)")
        print(f"[MQTT] Publicando ACKs en: {ACK_TOPIC}")
    else:
        print(f"[MQTT] Error de conexion: rc={rc}")


def on_message(client, userdata, msg):
    try:
        # Decodificación y parseo rápido
        payload_str = msg.payload.decode("utf-8")
        data = json.loads(payload_str)
        seq = data.get("seq")
    except Exception as e:
        print(f"[MQTT] Error de decodificacion: {e}")
        return

    if seq is None:
        return

    # Construcción directa de la cadena para evitar la sobrecarga de json.dumps()
    ack_payload = f'{{"seq":{seq},"ack":true}}'
    
    # Publicación inmediata (QoS 0 para el ACK es suficiente para round-trip rápido)
    client.publish(ACK_TOPIC, ack_payload, qos=0)
    
    print(f"[MQTT] << seq={seq} recibido | >> ACK enviado", flush=True)


def main():
    # Detección de versión de paho-mqtt para evitar deprecations
    try:
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    except AttributeError:
        client = mqtt.Client()

    client.on_connect = on_connect
    client.on_message = on_message

    print(f"[MQTT] Conectando a {BROKER}:{PORT}...")
    client.connect(BROKER, PORT, keepalive=60)

    # Forzar el envío inmediato de paquetes TCP pequeños sin esperar buffers (TCP_NODELAY)
    try:
        sock = client.socket()
        if sock:
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    except Exception:
        pass

    print("[MQTT] Servidor ACK activo. Presiona Ctrl+C para detener.\n")
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n[MQTT] Desconectando...")
        client.disconnect()
        sys.exit(0)


if __name__ == "__main__":
    main()