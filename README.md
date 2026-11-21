# Control de acceso facial a quirófano con ESP32-CAM

Prototipo académico de control de acceso a una sala restringida. Un ESP32-CAM fotografía a quien se acerca, un servidor Python reconoce el rostro, verifica que el médico esté autorizado y responde si se permite o se deniega el acceso. Cada intento queda registrado con fecha, hora, médico y resultado.

> Es un prototipo para una materia. No debe usarse para proteger un quirófano real.

## Cómo funciona

```
ESP32-CAM  --(foto JPEG por HTTP POST)-->  Servidor Flask
                                             1. detecta el rostro
                                             2. lo compara con los médicos registrados
                                             3. verifica el permiso
                                             4. guarda el intento en el historial
ESP32-CAM  <--------(respuesta JSON)-------  {"status": "ok", "user": "Dr. Perez", "authorized": true}
```

Ante cualquier duda (sin rostro, persona desconocida, médico sin permiso, error) la respuesta es siempre `authorized: false`.

## Estado del proyecto

| Etapa | Estado |
|---|---|
| ESP32-CAM captura y envía fotos por Wi-Fi | Listo |
| Servidor Python que recibe las fotos | Listo |
| Reconocimiento facial, base de datos e historial | Listo |
| Administración de médicos por línea de comandos | Listo |
| Firmware que actúa según la respuesta (LEDs, buzzer, relé) | Listo |
| Sensor PIR HC-SR501 para disparar la captura | Listo |
| Simulación del circuito en Wokwi | Listo |
| Funcionamiento en protoboard con fuente externa | Pendiente (ver "Estado actual") |
| Pantalla LCD | Pendiente |
| Panel web administrativo | Pendiente |

Cuando el sensor detecta movimiento, el ESP32 saca una foto y la envía. Según la respuesta prende el LED verde y activa el relé (acceso permitido), o prende el LED rojo y hace sonar el buzzer (persona desconocida o sin permiso). Si no hay nadie en la foto no hace nada.

## Estado actual (21/11/2026) y cómo seguir

**Probado y funcionando** con el ESP32-CAM en la base programadora: captura, envío por Wi-Fi, reconocimiento, historial y respuesta `ACCESO PERMITIDO` para una persona registrada. Los LEDs, el buzzer, el relé y el sensor funcionan por separado.

**Pendiente:**

- [ ] Montado en la protoboard, el ESP32 no arranca: los dos LEDs quedan prendidos fijos. La causa más probable es la alimentación (ver "Problemas frecuentes"). Probar con 9-12 V en el jack del módulo de fuente o con un cargador USB.
- [ ] Mejorar la señal Wi-Fi: con poca señal el ESP32 tarda en conectar o no ve la red. Acercarlo al router o usar el celular como hotspot de 2,4 GHz.
- [ ] Grabar el video de evidencia (persona registrada y persona desconocida).
- [ ] Antes de montarlo en la protoboard con el sensor, poner `MODO_PRUEBA_SIN_SENSOR = false` en el firmware y volver a subirlo.

**Para continuar en otra computadora** seguir "Puesta en marcha". Lo que no está en el repositorio y hay que rehacer:

- `secrets.h` con el Wi-Fi y la IP de esa computadora (paso 4).
- La base de datos con las personas registradas: registrarlas de nuevo (paso 7). También se puede copiar `backend/acceso.db` a mano, con el consentimiento de las personas registradas.

El informe de avance para la cátedra está en `docs/`.

## Estructura

```
control-acceso-quirofano/
├── esp32/
│   ├── control_acceso/
│   │   ├── control_acceso.ino Firmware del ESP32-CAM
│   │   └── secrets.example.h  Plantilla de configuración (Wi-Fi y servidor)
│   └── prueba_flash/          Prueba mínima de la placa: hace parpadear el flash
├── backend/
│   ├── app.py                 Servidor Flask: recibe la foto y decide el acceso
│   ├── reconocimiento.py      Detección y comparación de rostros (OpenCV)
│   ├── database.py            Base de datos SQLite
│   ├── gestionar.py           Comandos para administrar médicos e historial
│   └── requirements.txt       Dependencias de Python
├── simulacion-wokwi/          Simulación del circuito en Wokwi
├── docs/                      Informe de avance
└── README.md
```

## Requisitos

- ESP32-CAM AI Thinker (cámara OV2640 u OV3660) y su base programadora USB.
- Sensor de movimiento HC-SR501, LED verde, LED rojo, 2 resistencias de 220 Ω, buzzer activo y módulo relé.
- Protoboard y una fuente que entregue 5 V reales al riel (ver "Hardware y conexiones").
- Arduino IDE 2.x.
- Python 3.9 o superior.
- Una red Wi-Fi de 2,4 GHz. La computadora y el ESP32 tienen que estar en la misma red.

## Hardware y conexiones

| Componente | Pin del componente | Va a |
|---|---|---|
| Alimentación | Riel + / riel − | Pin **5V** / pin **GND** del ESP32 |
| Sensor HC-SR501 | OUT (VCC a riel +, GND a riel −) | **IO13** |
| LED verde | Pata larga, con resistencia de 220 Ω (pata corta a riel −) | **IO14** |
| LED rojo | Pata larga, con resistencia de 220 Ω (pata corta a riel −) | **IO15** |
| Buzzer | + (− a riel −) | **IO2** |
| Módulo relé | IN (VCC a riel +, GND a riel −) | **U0R** (GPIO3, en la otra fila de pines) |

- No usar IO12 (si queda en alto al arrancar, la placa no inicia), IO16 (PSRAM), IO4 (flash) ni IO0.
- Para programar hay que montar el ESP32 en la base, así que los componentes quedan desconectados. Con el relé en U0R, desconectarlo antes de poner la placa en la base.
- **Alimentación:** el módulo de fuente para protoboard regula la tensión del jack y necesita entre 6,5 V y 12 V para entregar 5 V. Con una fuente de 5 V en el jack no alcanza para el ESP32-CAM. Usar 9-12 V en el jack o 5 V por la entrada USB del módulo.

Al arrancar, el firmware hace una autoprueba: LED verde, LED rojo, pitido corto y clic del relé.

| Señal | Significado |
|---|---|
| LED verde + relé 3 s | Acceso permitido |
| LED rojo + buzzer 1 s | Persona desconocida o sin permiso |
| Dos parpadeos verdes | Durante un registro: rostro guardado |
| Tres parpadeos rojos rápidos | No se pudo conectar con el servidor |
| LED rojo fijo | Falla de la cámara |

### Modo prueba

En `control_acceso.ino`, `MODO_PRUEBA_SIN_SENSOR = true` hace que el ESP32 saque una foto cada 4 segundos sin esperar al sensor. Sirve para probar con la placa en la base programadora, donde el sensor no se puede conectar. En ese modo el relé no se usa, y el monitor serie muestra todos los mensajes. Para el montaje final en la protoboard, ponerlo en `false`.

## Puesta en marcha

### 1. Descargar el proyecto

```bash
git clone https://github.com/Federicoegas001/control-acceso-quirofano.git
cd control-acceso-quirofano
```

### 2. Levantar el servidor

macOS o Linux:

```bash
cd backend
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python app.py
```

Windows:

```bat
cd backend
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
python app.py
```

La primera vez descarga los modelos de reconocimiento (unos 38 MB). Queda listo cuando muestra `Running on http://127.0.0.1:5001`. Si el sistema pregunta por conexiones entrantes, hay que permitirlas.

### 3. Averiguar la IP de la computadora

- macOS: `ipconfig getifaddr en0`
- Windows: `ipconfig` (buscar "Dirección IPv4")
- Linux: `hostname -I`

### 4. Configurar el firmware

1. En `esp32/control_acceso/`, copiar `secrets.example.h` con el nombre `secrets.h`.
2. Abrir `secrets.h` y completar los tres datos:
   - `WIFI_SSID`: nombre de la red Wi-Fi.
   - `WIFI_PASSWORD`: contraseña de la red.
   - `SERVER_URL`: `http://IP_DEL_PASO_3:5001/api/acceso`

`secrets.h` no se sube al repositorio: cada integrante tiene el suyo.

### 5. Preparar Arduino IDE (solo la primera vez)

1. **File → Preferences → Additional boards manager URLs** y agregar:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. **Tools → Board → Boards Manager**, buscar `esp32` e instalar "esp32 by Espressif Systems".
3. En Mac con Apple Silicon, instalar Rosetta: `softwareupdate --install-rosetta --agree-to-license`

### 6. Subir el firmware

1. **File → Open** y elegir `esp32/control_acceso/control_acceso.ino`.
2. Conectar el ESP32-CAM por USB.
3. En el selector de arriba, elegir la placa **AI Thinker ESP32-CAM** y el puerto USB serial.
4. Clic en la flecha **Upload**.
5. Abrir **Tools → Serial Monitor** a **115200 baud** y presionar **RST** en la placa.

Si todo está bien, en modo prueba aparece:

```
Iniciando camara...
Camara OK
Buscando redes Wi-Fi...
  NombreDeTuRed            -55 dBm  canal 6
Conectando al WiFi "NombreDeTuRed"...
Conectado. IP del ESP32: 192.168.x.x
Servidor: http://192.168.x.x:5001/api/acceso
Movimiento detectado
Foto capturada: 640x480, 21382 bytes. Enviando...
Respuesta HTTP 200: {"authorized": false, "status": "denied", ...}
```

La lista de redes muestra la señal de cada una: cerca de -50 dBm es muy buena, -70 dBm aceptable y -80 dBm o menos es demasiado débil.

### 7. Registrar un médico

En otra terminal, dentro de `backend` y con el entorno virtual activado:

```bash
python gestionar.py registrar "Dr. Perez"
```

La persona mira a la cámara, de frente, a 40-60 cm y con buena luz. Las siguientes 3 fotos con un rostro se guardan como referencia; el avance se ve en la terminal del servidor (`rostro guardado, faltan 2`...). Al terminar, el sistema vuelve solo al modo normal.

### 8. Probar

Con el médico frente a la cámara, la terminal del servidor muestra `PERMITIDO: Dr. Perez (similitud 0.xx)`. Sin nadie, o con una persona no registrada, muestra `DENEGADO`.

## Comandos de administración

Todos se ejecutan dentro de `backend`, con el entorno virtual activado.

| Comando | Qué hace |
|---|---|
| `python gestionar.py registrar "Nombre"` | Da de alta un médico con la cámara. Repetirlo suma más rostros. |
| `python gestionar.py registrar "Nombre" --fotos 5` | Igual, usando 5 fotos. |
| `python gestionar.py registrar "Nombre" --archivo foto.jpg` | Da de alta desde una foto guardada. |
| `python gestionar.py cancelar` | Corta un registro con cámara que quedó a medias. |
| `python gestionar.py listar` | Muestra los médicos, su permiso y cuántos rostros tienen. |
| `python gestionar.py revocar ID` | Quita el permiso de acceso. |
| `python gestionar.py autorizar ID` | Devuelve el permiso de acceso. |
| `python gestionar.py eliminar ID` | Borra un médico y sus rostros. |
| `python gestionar.py historial` | Muestra los últimos 20 intentos de acceso. |

Mientras hay un registro en curso, el sistema no evalúa accesos: la próxima cara que vea se guarda a nombre de ese médico.

## Simulación en Wokwi

`simulacion-wokwi/` tiene el mismo circuito y la misma lógica en el simulador Wokwi. La cámara y el servidor se reemplazan por un interruptor que simula la respuesta. Instrucciones en `simulacion-wokwi/README.md`.

## API del servidor

| Método y ruta | Descripción |
|---|---|
| `GET /` | Estado del servidor. |
| `POST /api/acceso` | Recibe una foto JPEG en el cuerpo y devuelve la decisión. |
| `GET /api/medicos` | Lista de médicos registrados. |
| `GET /api/accesos?limite=50` | Historial de intentos. |

Respuesta de `/api/acceso`:

```json
{"status": "ok", "user": "Dr. Perez", "authorized": true}
```

```json
{"status": "denied", "user": null, "authorized": false, "motivo": "desconocido"}
```

Motivos de rechazo: `sin_rostro`, `rostro_muy_lejos`, `desconocido`, `sin_permiso`, `imagen_invalida`.

## Decisiones técnicas

- **Reconocimiento con OpenCV (modelos YuNet y SFace).** Se instala con un solo `pip`, sin compilar, y funciona en Apple Silicon.
- **Se guarda un vector numérico de cada rostro, no la foto del médico.**
- **Umbral de similitud 0,363**, el recomendado por los autores de SFace. Se ajusta en `reconocimiento.py` (`UMBRAL_SIMILITUD`): más alto es más estricto.
- **Rechazo por defecto.** Solo se permite el acceso con un rostro reconocido y un médico autorizado.
- **Tolerancia a la orientación.** Si la cámara está girada o cabeza abajo, el servidor prueba las otras orientaciones.
- **Las fotos recibidas se limitan a las 200 más recientes.**

## Problemas frecuentes

| Síntoma | Causa y solución |
|---|---|
| `ctags: bad CPU type in executable` al compilar | Falta Rosetta en Mac con Apple Silicon (ver paso 5). |
| `secrets.h: No such file or directory` | Falta crear `secrets.h` (ver paso 4). |
| `Camera probe failed with error 0x106` | El cable plano de la cámara está flojo o desconectado. |
| Puntos infinitos en `Conectando al WiFi` | SSID o contraseña incorrectos, o la red es de 5 GHz. |
| `Error de conexion: connection refused` | `SERVER_URL` tiene mal la IP, el servidor no está corriendo o el firewall lo bloquea. |
| El puerto 5000 está ocupado | En macOS lo usa AirPlay; por eso el servidor usa el 5001. |
| Siempre responde `sin_rostro` | Poca luz, cara muy cerca o muy lejos, o fuera de cuadro. Revisar las fotos en `backend/registros/`. |
| Similitud baja al reconocer | Registrar más rostros del mismo médico con poses y luz variadas. |
| `La red "X" NO aparece` o `WiFi sin conectar (estado 6)` | Señal muy débil o red de 5 GHz. Acercar el ESP32 al router o usar un hotspot de 2,4 GHz. |
| `send payload failed` | Se cortó el Wi-Fi durante el envío: señal débil. |
| En la protoboard los dos LEDs quedan prendidos fijos | El ESP32 no arranca. Casi siempre es alimentación insuficiente (ver "Hardware y conexiones"). Probar primero solo ESP32 + LEDs y sumar después buzzer y relé. |
| El monitor serie se queda en `Autoprueba: rele` | Con `MODO_PRUEBA_SIN_SENSOR = false`, el relé usa U0R, que es el mismo pin del monitor serie. Es normal; seguir el funcionamiento en la terminal del servidor. |
| El Upload no cambia nada en la placa | Hay otra ventana de Arduino IDE usando el mismo puerto. Dejar abierta solo la de `control_acceso` y mirar la pestaña Output. |
| El servidor no decide accesos y no anota nada en el historial | Quedó un registro a medias. Correr `python gestionar.py cancelar`. |
| La placa no responde o la cámara no inicia después de muchos uploads | Desenchufar el USB unos segundos y volver a enchufar. Revisar que el cable plano de la cámara esté bien trabado. |

## Seguridad y datos personales

- `secrets.h`, la base de datos (`acceso.db`) y las fotos (`registros/`) están excluidos del repositorio por `.gitignore`. No forzar su subida.
- Los rostros son datos biométricos: registrar solo a personas que dieron su consentimiento.
- La comunicación es HTTP sin cifrar y el servidor no autentica al dispositivo; alcanza para una demostración en una red propia, no para un uso real.
- El sistema no detecta fotos impresas ni pantallas.
- El relé solo debe usarse para simular una cerradura de baja tensión. No conmutar tensión de red sobre la protoboard.
