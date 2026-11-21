#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"

// Credenciales y direccion del servidor: se definen en secrets.h, que no se
// sube al repositorio. Copiar secrets.example.h como secrets.h y completarlo.
#include "secrets.h"

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *serverUrl = SERVER_URL;

// Tiempo minimo entre dos fotos mientras el sensor sigue detectando a alguien
const unsigned long INTERVALO_MS = 4000;

// Modo prueba: saca una foto cada INTERVALO_MS sin esperar al sensor.
// Sirve para probar en la base programadora, donde el sensor no se puede conectar.
// En la protoboard con el sensor conectado, poner en false.
const bool MODO_PRUEBA_SIN_SENSOR = true;

// ====== Actuadores ======
// Pines libres del ESP32-CAM (no usar IO12, IO16 ni IO4)
// El rele va en U0R (GPIO3, el RX del puerto serie): en la protoboard no se usa
// para programar. Hay que desconectarlo para subir firmware desde la base.
#define PIN_RELE 3
#define PIN_SENSOR 13  // salida OUT del sensor de movimiento HC-SR501
#define PIN_LED_VERDE 14
#define PIN_LED_ROJO 15
#define PIN_BUZZER 2

// La mayoria de los modulos rele se activan con LOW. Si el tuyo hace "clic"
// al reves (activo cuando deberia estar apagado), cambiar a false.
const bool RELE_ACTIVO_EN_BAJO = true;

const unsigned long TIEMPO_PUERTA_MS = 3000;   // cuanto queda abierta la "cerradura"
const unsigned long TIEMPO_ALARMA_MS = 1000;   // cuanto suena el buzzer si se deniega

// Pines de la camara en la placa AI Thinker ESP32-CAM
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

void rele(bool activo) {
  // En modo prueba (placa en la base) el rele no esta conectado: no se toca su pin,
  // asi el monitor serie (que usa el mismo pin) sigue funcionando.
  if (MODO_PRUEBA_SIN_SENSOR) return;
  bool nivel = RELE_ACTIVO_EN_BAJO ? !activo : activo;
  digitalWrite(PIN_RELE, nivel ? HIGH : LOW);
}

void apagarTodo() {
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_ROJO, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  rele(false);
}

void iniciarActuadores() {
  pinMode(PIN_SENSOR, INPUT);
  rele(false);  // se fija el nivel antes de configurar el pin para que no haga clic al arrancar
  if (!MODO_PRUEBA_SIN_SENSOR) pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  apagarTodo();
}

// Autoprueba al arrancar: prende cada componente de a uno para verificar el cableado
void autoprueba() {
  Serial.println("Autoprueba: LED verde");
  digitalWrite(PIN_LED_VERDE, HIGH); delay(600); digitalWrite(PIN_LED_VERDE, LOW);
  Serial.println("Autoprueba: LED rojo");
  digitalWrite(PIN_LED_ROJO, HIGH); delay(600); digitalWrite(PIN_LED_ROJO, LOW);
  Serial.println("Autoprueba: buzzer");
  digitalWrite(PIN_BUZZER, HIGH); delay(200); digitalWrite(PIN_BUZZER, LOW);
  Serial.println("Autoprueba: rele");
  rele(true); delay(600); rele(false);
}

void accesoPermitido() {
  Serial.println(">> ACCESO PERMITIDO");
  digitalWrite(PIN_LED_VERDE, HIGH);
  rele(true);
  delay(TIEMPO_PUERTA_MS);
  apagarTodo();
}

void accesoDenegado() {
  Serial.println(">> ACCESO DENEGADO");
  digitalWrite(PIN_LED_ROJO, HIGH);
  digitalWrite(PIN_BUZZER, HIGH);
  delay(TIEMPO_ALARMA_MS);
  apagarTodo();
}

// Parpadeo corto: el servidor guardo un rostro durante un registro
void senalRegistro() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(PIN_LED_VERDE, HIGH); delay(150);
    digitalWrite(PIN_LED_VERDE, LOW); delay(150);
  }
}

// Tres parpadeos rojos rapidos: no se pudo hablar con el servidor
void senalErrorServidor() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_LED_ROJO, HIGH); delay(100);
    digitalWrite(PIN_LED_ROJO, LOW); delay(100);
  }
}

// Decide que hacer segun la respuesta JSON del servidor
void actuar(String cuerpo) {
  cuerpo.replace(" ", "");
  cuerpo.replace("\n", "");

  if (cuerpo.indexOf("\"authorized\":true") >= 0) {
    accesoPermitido();
  } else if (cuerpo.indexOf("\"motivo\":\"desconocido\"") >= 0 ||
             cuerpo.indexOf("\"motivo\":\"sin_permiso\"") >= 0) {
    // Solo se alarma si hay una persona frente a la camara que no esta autorizada.
    // Si no hay nadie (sin_rostro) no se hace nada.
    accesoDenegado();
  } else if (cuerpo.indexOf("\"motivo\":\"rostro_guardado\"") >= 0) {
    senalRegistro();
  }
}

bool iniciarCamara() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_UXGA;  // se reserva buffer para la resolucion maxima
  config.jpeg_quality = 10;            // 0-63, mas bajo = mejor calidad
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (!psramFound()) {
    // Sin PSRAM hay poca memoria: se baja la resolucion
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error al iniciar la camara: 0x%x\n", err);
    return false;
  }

  // Resolucion real de trabajo: 640x480, suficiente para reconocer rostros
  sensor_t *sensor = esp_camera_sensor_get();
  sensor->set_framesize(sensor, FRAMESIZE_VGA);
  delay(1000);

  return true;
}

// Diagnostico: lista las redes que ve el ESP32 y su intensidad de senal (RSSI).
// Cerca de -50 dBm es muy buena, -70 dBm aceptable, -80 dBm o menos es muy debil.
void escanearRedes() {
  Serial.println("Buscando redes Wi-Fi...");
  int n = WiFi.scanNetworks();
  bool encontrada = false;
  for (int i = 0; i < n; i++) {
    Serial.printf("  %-24s %4d dBm  canal %d\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
    if (WiFi.SSID(i) == ssid) encontrada = true;
  }
  if (!encontrada) Serial.printf("La red \"%s\" NO aparece (fuera de alcance o es de 5 GHz)\n", ssid);
  WiFi.scanDelete();
}

void conectarWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  escanearRedes();
  Serial.printf("Conectando al WiFi \"%s\"...\n", ssid);
  WiFi.begin(ssid, password);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    // Cada 15 s informa el estado y reintenta desde cero
    if (millis() - inicio > 15000) {
      Serial.printf("WiFi sin conectar (estado %d). Reintentando...\n", WiFi.status());
      WiFi.disconnect(true);
      delay(1000);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      inicio = millis();
    }
  }
  Serial.print("Conectado. IP del ESP32: ");
  Serial.println(WiFi.localIP());
  Serial.print("Servidor: ");
  Serial.println(serverUrl);
}

void capturarYEnviar() {
  // Se descarta una foto vieja que pudo quedar en el buffer
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Error: no se pudo capturar la foto");
    return;
  }
  Serial.printf("Foto capturada: %ux%u, %u bytes. Enviando...\n", fb->width, fb->height, fb->len);

  HTTPClient http;
  http.begin(serverUrl);
  http.setTimeout(10000);
  http.addHeader("Content-Type", "image/jpeg");
  int codigo = http.POST(fb->buf, fb->len);

  // La foto ya se envio: se libera el buffer antes de actuar
  esp_camera_fb_return(fb);

  if (codigo > 0) {
    String cuerpo = http.getString();
    Serial.printf("Respuesta HTTP %d: ", codigo);
    Serial.println(cuerpo);
    http.end();
    if (codigo == 200) actuar(cuerpo);
  } else {
    Serial.printf("Error de conexion: %s\n", http.errorToString(codigo).c_str());
    http.end();
    senalErrorServidor();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.printf("PSRAM detectada: %s\n", psramFound() ? "si" : "no");

  iniciarActuadores();
  autoprueba();

  Serial.println("Iniciando camara...");
  if (!iniciarCamara()) {
    Serial.println("Sistema detenido: revisar la camara.");
    // LED rojo fijo: falla de camara
    digitalWrite(PIN_LED_ROJO, HIGH);
    while (true) delay(1000);
  }
  Serial.println("Camara OK");
  conectarWifi();
}

unsigned long ultimaFoto = 0;

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWifi();
  }

  // Solo se saca la foto cuando el sensor detecta movimiento
  bool hayAlguien = MODO_PRUEBA_SIN_SENSOR || digitalRead(PIN_SENSOR) == HIGH;
  if (hayAlguien && (ultimaFoto == 0 || millis() - ultimaFoto >= INTERVALO_MS)) {
    Serial.println("Movimiento detectado");
    capturarYEnviar();
    ultimaFoto = millis();
  }
  delay(100);
}