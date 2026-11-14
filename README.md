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
| Firmware que actúa según la respuesta (relé, LED, buzzer, LCD) | Pendiente |
| Sensor PIR para disparar la captura | Pendiente |
| Panel web administrativo | Pendiente |

Por ahora el ESP32 envía una foto cada 10 segundos y muestra la respuesta por el monitor serie.

## Estructura

```
control-acceso-quirofano/
├── esp32/control_acceso/
│   ├── control_acceso.ino     Firmware del ESP32-CAM
│   └── secrets.example.h      Plantilla de configuración (Wi-Fi y servidor)
├── backend/
│   ├── app.py                 Servidor Flask: recibe la foto y decide el acceso
│   ├── reconocimiento.py      Detección y comparación de rostros (OpenCV)
│   ├── database.py            Base de datos SQLite
│   ├── gestionar.py           Comandos para administrar médicos e historial
│   └── requirements.txt       Dependencias de Python
└── README.md
```

## Requisitos

- ESP32-CAM AI Thinker con cámara OV2640 y programador ESP32-CAM-MB.
- Arduino IDE 2.x.
- Python 3.9 o superior.
- Una red Wi-Fi de 2,4 GHz. La computadora y el ESP32 tienen que estar en la misma red.

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

Si todo está bien, cada 10 segundos aparece:

```
Foto capturada: 640x480, 21382 bytes. Enviando...
Respuesta HTTP 200: {"authorized": false, "status": "denied", ...}
```

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

## Seguridad y datos personales

- `secrets.h`, la base de datos (`acceso.db`) y las fotos (`registros/`) están excluidos del repositorio por `.gitignore`. No forzar su subida.
- Los rostros son datos biométricos: registrar solo a personas que dieron su consentimiento.
- La comunicación es HTTP sin cifrar y el servidor no autentica al dispositivo; alcanza para una demostración en una red propia, no para un uso real.
- El sistema no detecta fotos impresas ni pantallas.
- El relé solo debe usarse para simular una cerradura de baja tensión. No conmutar tensión de red sobre la protoboard.
