# Simulacion en Wokwi

Simula el circuito del control de acceso: sensor de movimiento, LEDs, buzzer y rele, con los mismos pines que el ESP32-CAM.

Wokwi no simula la camara, y para hablar con el servidor Python hace falta el gateway pago. Por eso la respuesta del servidor se simula con un interruptor.

## Como usarlo

1. Entrar a https://wokwi.com y crear un proyecto nuevo de **ESP32**.
2. Reemplazar el contenido de `sketch.ino` y `diagram.json` por los de esta carpeta.
3. Darle Play.
4. Elegir la persona con el interruptor: derecha = autorizada, izquierda = desconocida.
5. Hacer clic sobre el sensor PIR para simular movimiento.

## Diferencias con el hardware real

- Placa ESP32 DevKit en lugar de ESP32-CAM (Wokwi no tiene camara).
- El rele de Wokwi se activa con HIGH; el modulo real suele activarse con LOW.
- El buzzer usa `tone()` para que suene en el simulador.
- El interruptor en GPIO27 solo existe en la simulacion.
