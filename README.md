# Nodo IoT de Confort Térmico - Laboratorio 1

Este repositorio contiene la implementación y documentación del Nodo IoT basado en **ESP32** para el monitoreo de confort térmico, evaluando los protocolos **MQTT** y **HTTP**.

## 📁 Estructura del Repositorio

* `documentos/`: Documento de Decisión de Arquitectura (`ADR-01.md`) e informe académico.
* `evidencia/`: Capturas de pantalla, archivos de texto con logs seriales y datos experimentales.
* `firmware/`: Código fuente C++ (.ino) para el microcontrolador ESP32.
* `servidor/`: Scripts en Python para la API REST (Flask) y el responder de confirmaciones MQTT (ACK).

## 📊 Resultados Principales (ADR-01)

| Métrica | HTTP | MQTT | Conclusión |
| :--- | :---: | :---: | :--- |
| **Latencia Mediana** | 920 ms | **192 ms** | MQTT cumple RNF-01 (< 200 ms) |
| **Tasa de Entrega** | ~92% | **100%** | Sin pérdida de paquetes |
| **Tiempo de Recuperación ($T_{rec}$)** | 80 s | **30 s** | MQTT reconecta 2.6x más rápido |
| **Consumo de Ancho de Banda (10s)** | 1.65 KB/min | **0.87 KB/min** | Ahorro del 44.8% en payload |

## 👤 Autor
* **Nabelén** (`nabelen`)
