#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"

// Credenciales y direccion del servidor: se definen en secrets.h, que no se
// sube al repositorio. Copiar secrets.example.h como secrets.h y completarlo.
#include "secrets.h"

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *serverUrl = SERVER_URL;

const unsigned long INTERVALO_MS = 10000;

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

void conectarWifi() {
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("Conectando al WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado. IP del ESP32: ");
  Serial.println(WiFi.localIP());
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

  if (codigo > 0) {
    Serial.printf("Respuesta HTTP %d: ", codigo);
    Serial.println(http.getString());
  } else {
    Serial.printf("Error de conexion: %s\n", http.errorToString(codigo).c_str());
  }

  http.end();
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.printf("PSRAM detectada: %s\n", psramFound() ? "si" : "no");

  if (!iniciarCamara()) {
    Serial.println("Sistema detenido: revisar la camara.");
    while (true) delay(1000);
  }
  conectarWifi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWifi();
  }
  capturarYEnviar();
  delay(INTERVALO_MS);
}