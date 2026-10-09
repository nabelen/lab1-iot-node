# ADR-01: Selección del Protocolo de Transporte para el Nodo IoT de Confort Térmico

## Estado
Aprobado

## Contexto
El nodo terminal IoT basado en ESP32 debe transmitir datos de confort térmico (temperatura, humedad, estado del actuador, RSSI) hacia la infraestructura central. Se requiere seleccionar un protocolo de transporte que garantice el cumplimiento de latencia (RNF-01: mediana < 200 ms), alta confiabilidad en el enlace y una rápida recuperación ante caídas de red.

## Alternativas Evaluadas
1. **HTTP / REST (POST en JSON):** Comunicación SÍNCRONA sobre HTTP/1.1.
2. **MQTT (Pub/Sub con Broker Mosquitto + ACK):** Comunicación ASÍNCRONA mediante sockets TCP persistentes.

## Matriz de Evidencia Experimental

| Métrica / Escenario | HTTP | MQTT | Requisito / Impacto |
| :--- | :---: | :---: | :--- |
| **Latencia Mediana (Cerca, -40 dBm)** | 920.0 ms | **192.0 ms** | RNF-01 (< 200 ms): **Solo MQTT cumple** |
| **Latencia Mediana (Lejos, -60 dBm)** | 444.0 ms | **166.0 ms** | RNF-01 (< 200 ms): **Solo MQTT cumple** |
| **Tasa de Entrega ($P_{entrega}$)** | 90.0% - 93.8% | **100.0 %** | Transmisión sin pérdida de paquetes |
| **Desviación Estándar (Jitter)** | > 1000 ms | **82.3 - 107.0 ms** | Determinismo y estabilidad de red |
| **Tiempo de Recuperación ($T_{rec}$)** | 80.0 s | **30.0 s** | **MQTT es 2.6x más rápido en reconectar** |
| **Consumo Ancho de Banda (10s)** | 1.65 KB/min | **0.87 KB/min** | **44.8% menos datos consumidos con MQTT** |

## Decisión
Se decide adoptar **MQTT sobre el Broker Mosquitto** como el protocolo de transporte principal para el nodo IoT.

## Justificación
1. **Cumplimiento de RNF-01:** MQTT fue el único protocolo que mantuvo una latencia mediana por debajo de los 200 ms en todas las condiciones (192 ms y 166 ms).
2. **Tasa de Entrega Perfecta:** Se alcanzó un 100% de entregas confirmadas vía ACK a nivel de aplicación.
3. **Resiliencia:** Ante caídas de red Wi-Fi, MQTT reestableció la comunicación en tan solo **30 segundos**, mientras que HTTP tardó **80 segundos**.
4. **Eficiencia:** El intervalo de envío a 10s genera solo **0.87 KB/min**, reduciendo en un 90.38% el tráfico de red respecto al envío cada 1s.

## Consecuencias
* **Positivas:** Menor consumo de red, baja latencia y alta tolerancia a fallos de radiofrecuencia.
* **Negativas:** Dependencia de la disponibilidad continua del broker Mosquitto en la infraestructura central.