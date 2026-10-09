"""
Analisis Experimento A (HTTP).

Uso:
  1. Guarda el monitor serial completo en serial_http.txt
  2. Deja http_log.csv (generado por app.py) en la misma carpeta
  3. python analisis_http.py

Requiere: pip install pandas matplotlib
"""
import pandas as pd
import matplotlib.pyplot as plt

SERIAL_FILE = "serial_http.txt"
SERVER_LOG = "http_log.csv"
UMBRAL_MS = 200          # umbral de RNF-01 (ajustalo a tu criterio)

# ---- 1. Extraer lineas DATA del log serial ----
raw = open(SERIAL_FILE, "rb").read()
if raw[:2] in (b"\xff\xfe", b"\xfe\xff"):
    text = raw.decode("utf-16", errors="ignore")
else:
    text = raw.decode("utf-8-sig", errors="ignore")
text = text.replace("\x00", "")

lines = text.splitlines()
print(f"Lineas en {SERIAL_FILE}: {len(lines)}")
print("Primeras lineas del archivo:")
for l in lines[:5]:
    print("   ", repr(l))

rows = []
for line in lines:
    i = line.find("DATA,")          # tolera prefijos como "12:30:45 -> "
    if i == -1:
        continue
    p = line[i:].strip().split(",")
    if len(p) == 7:
        rows.append(p)
print(f"Lineas DATA candidatas: {len(rows)}")

df = pd.DataFrame(rows, columns=["tipo", "proto", "seq", "rssi",
                                 "lat_ms", "exito", "bytes"])
for c in ["seq", "rssi", "lat_ms", "exito", "bytes"]:
    df[c] = pd.to_numeric(df[c], errors="coerce")

# descartar encabezados u otras lineas no numericas
df = df.dropna(subset=["seq", "rssi", "lat_ms", "exito", "bytes"])
df = df.astype({"seq": int, "rssi": int, "lat_ms": int,
                "exito": int, "bytes": int})

if df.empty:
    raise SystemExit("No se encontraron lineas DATA validas en "
                     + SERIAL_FILE)

df = df.drop_duplicates(subset="seq").sort_values("seq").reset_index(drop=True)
df.to_csv("datos_http.csv", index=False)

# ---- 2. Datos del servidor ----
srv = pd.read_csv(SERVER_LOG)
recibidos = srv["seq"].nunique()

# ---- 3. Metricas ----
n = len(df)
exitos = int(df["exito"].sum())
lat = df["lat_ms"]

print("=" * 50)
print(f"Muestras (enviados):           {n}")
print(f"Exitos segun ESP32:            {exitos}")
print(f"Recibidos segun servidor:      {recibidos}")
print(f"P_entrega (ESP32):             {exitos / n * 100:.1f} %")
print(f"P_entrega (servidor):          {recibidos / n * 100:.1f} %")
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

if n < 30:
    print("AVISO: hay menos de 30 muestras; la guia pide minimo 30.")

# ---- 4. Tabla de pruebas (formato de la guia) ----
tabla = df[["seq", "proto", "rssi", "lat_ms", "exito", "bytes"]].copy()
tabla.columns = ["Prueba", "Protocolo", "RSSI [dBm]", "Latencia [ms]",
                 "Exito", "Bytes"]
tabla["Prueba"] = tabla["Prueba"] + 1
tabla.to_csv("tabla_pruebas_http.csv", index=False)

# ---- 5. Graficas ----
fig, ax = plt.subplots(1, 3, figsize=(15, 4))

ax[0].plot(df["seq"] + 1, lat, marker="o")
ax[0].axhline(lat.median(), color="r", linestyle="--",
              label=f"Mediana = {lat.median():.0f} ms")
ax[0].set(xlabel="N.º de prueba", ylabel="Latencia [ms]",
          title="Latencia HTTP por envío")
ax[0].legend()

ax[1].hist(lat, bins=10, edgecolor="black")
ax[1].set(xlabel="Latencia [ms]", ylabel="Frecuencia",
          title="Histograma de latencia")

ax[2].boxplot(lat, tick_labels=["HTTP"])
ax[2].set(ylabel="Latencia [ms]", title="Boxplot")

plt.tight_layout()
plt.savefig("latencia_http.png", dpi=150)
print("Archivos generados: datos_http.csv, tabla_pruebas_http.csv, "
      "latencia_http.png")
