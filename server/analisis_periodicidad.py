import os

files = {
    '1s': ('serial_mqtt_1s.txt', 60),
    '5s': ('serial_mqtt_5s.txt', 12),
    '10s': ('serial_mqtt_10s.txt', 6)
}

print("=== RESULTADOS EXPERIMENTO 4 (PERIODICIDAD MQTT) ===")
print(f"{'Intervalo':<10} | {'Msg/min':<8} | {'Bytes Payload (Prom)':<20} | {'Consumo (Bytes/min)':<20} | {'Consumo (KB/min)':<18}")
print("-" * 85)

for label, (filename, msg_min) in files.items():
    if not os.path.exists(filename):
        print(f"[-] Archivo no encontrado: {filename}")
        continue
    
    payload_bytes = []
    with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            if line.startswith("DATA,"):
                parts = line.strip().split(',')
                # La última columna corresponde a bytes_payload
                try:
                    payload_bytes.append(float(parts[-1]))
                except ValueError:
                    pass
    
    if payload_bytes:
        prom_bytes = sum(payload_bytes) / len(payload_bytes)
        bytes_min = msg_min * prom_bytes
        kb_min = bytes_min / 1024.0
        print(f"{label:<10} | {msg_min:<8} | {prom_bytes:<20.1f} | {bytes_min:<20.1f} | {kb_min:<18.2f}")
    else:
        print(f"[-] No se encontraron lineas 'DATA,' validas en {filename}")