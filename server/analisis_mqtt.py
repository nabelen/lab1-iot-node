"""
Analisis Experimento (MQTT con Mosquitto).
Uso: python analisis_mqtt.py
Requiere: pip install pandas matplotlib
"""
import pandas as pd
import matplotlib.pyplot as plt

SERIAL_FILE = "serial_mqtt.txt"
UMBRAL_MS = 200          # Umbral de RNF-01 (200 ms)

# ---- 1. Extraer lineas DATA del log serial ----
raw = open(SERIAL_FILE, "rb").read()
if raw[:2] in (b"\xff\xfe", b"\xfe\xff"):
    text = raw.decode("utf-16", errors="ignore")
else:
    text = raw.decode("utf-8-sig", errors="ignore")
text = text.replace("\x00", "")

lines = text.splitlines()
rows = []
for line in lines:
    i = line.find("DATA,")
    if i == -1:
        continue
    p = line[i:].strip().split(",")
    if len(p) == 7:
        rows.append(p)

df = pd.DataFrame(rows, columns=["tipo", "proto", "seq", "rssi",
                                 "lat_ms", "exito", "bytes"])
for c in ["seq", "rssi", "lat_ms", "exito", "bytes"]:
    df[c] = pd.to_numeric(df[c], errors="coerce")

df = df.dropna(subset=["seq", "rssi", "lat_ms", "exito", "bytes"])
df = df.astype({"seq": int, "rssi": int, "lat_ms": int,
                "exito": int, "bytes": int})

if df.empty:
    raise SystemExit("No se encontraron lineas DATA validas en " + SERIAL_FILE)

df = df.drop_duplicates(subset="seq").sort_values("seq").reset_index(drop=True)
df.to_csv("datos_mqtt.csv", index=False)

# ---- 2. Metricas ----
n = len(df)
exitos = int(df["exito"].sum())
lat = df["lat_ms"]

print("=" * 50)
print(f"Muestras (enviados):           {n}")
print(f"Exitos con ACK:                {exitos}")
print(f"P_entrega (MQTT):              {exitos / n * 100:.1f} %")
print("-" * 50)
print(f"Latencia mediana:              {lat.median():.1f} ms")
print(f"Latencia media:                {lat.mean():.1f} ms")
print(f"Latencia desv. estandar:       {lat.std():.1f} ms")
print(f"Latencia min / max:            {lat.min()} / {lat.max()} ms")
print(f"Latencia p95:                  {lat.quantile(0.95):.1f} ms")
print(f"RSSI medio:                    {df['rssi'].mean():.1f} dBm")
print(f"Bytes payload (medio):         {df['bytes'].mean():.1f} B")
cumple = "CUMPLE" if lat.median() < UMBRAL_MS else "NO CUMPLE"
print(f"RNF-01 (mediana < {UMBRAL_MS} ms):    {cumple}")
print("=" * 50)

# ---- 3. Generar Grafica ----
fig, ax = plt.subplots(1, 2, figsize=(11, 4))

ax[0].plot(df["seq"] + 1, lat, marker="o", color="green")
ax[0].axhline(lat.median(), color="r", linestyle="--",
              label=f"Mediana = {lat.median():.0f} ms")
ax[0].set(xlabel="N.º de prueba", ylabel="Latencia [ms]",
          title="Latencia MQTT por envío")
ax[0].legend()

ax[1].boxplot(lat, tick_labels=["MQTT"])
ax[1].set(ylabel="Latencia [ms]", title="Boxplot Latencia MQTT")

plt.tight_layout()
plt.savefig("latencia_mqtt.png", dpi=150)
print("Archivos generados: datos_mqtt.csv, latencia_mqtt.png")