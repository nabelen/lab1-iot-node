# Nodo IoT de Confort Térmico - Laboratorio 1

Este repositorio contiene la implementación y documentación del Nodo IoT basado en **ESP32** para el monitoreo de confort térmico, evaluando los protocolos **MQTT** y **HTTP**.

## Estructura del Repositorio

* `documentos/`: Documento de Decisión de Arquitectura (`ADR-01.md`) e informe académico.
* `evidencia/`: Capturas de pantalla, archivos de texto con logs seriales y datos experimentales.
* `firmware/`: Código fuente C++ (.ino) para el microcontrolador ESP32.
* `servidor/`: Scripts en Python para la API REST (Flask) y el responder de confirmaciones MQTT (ACK).

## Resultados Principales (ADR-01)

| Métrica | HTTP | MQTT | Conclusión |
| :--- | :---: | :---: | :--- |
| **Latencia Mediana** | 920 ms | **192 ms** | MQTT cumple RNF-01 (< 200 ms) |
| **Tasa de Entrega** | ~92% | **100%** | Sin pérdida de paquetes |
| **Tiempo de Recuperación ($T_{rec}$)** | 80 s | **30 s** | MQTT reconecta 2.6x más rápido |
| **Consumo de Ancho de Banda (10s)** | 1.65 KB/min | **0.87 KB/min** | Ahorro del 44.8% en payload |

## Conclusiones Técnicas

1. **Superioridad de MQTT en Latencia (RNF-01):** La conexión TCP persistente de MQTT permitió una latencia mediana de **192 ms**, cumpliendo con holgura el límite de 200 ms exigido. En contraste, HTTP registró una mediana de **920 ms**, fallando el requisito debido a la sobrecarga reiterada del handshake TCP y cabeceras en cada transmisión.
2. **Confiabilidad y Tolerancia a Señal Débil (RSSI):** Al atenuarse la cobertura Wi-Fi a ~-60 dBm, MQTT conservó una tasa de entrega del **100%** sin pérdida de paquetes. HTTP, por su parte, sufrió reintentos y timeouts que redujeron el éxito en el ESP32 a un **90%** con picos de latencia de hasta 4.2 s.
3. **Resiliencia ante Caídas de Red:** Tras una interrupción forzada del punto de acceso, MQTT restableció la sesión y confirmó telemetría en **30.5 s**, siendo **2.69 veces más veloz** en recuperarse que la arquitectura HTTP (80.7 s).
4. **Optimización de Ancho de Banda:** Para el monitoreo de confort térmico, un intervalo de muestreo de **10 segundos** demostró ser el óptimo, reduciendo el tráfico de datos en más de un **90%** frente al reporte a 1 s (0.87 KB/min vs 9.05 KB/min).
