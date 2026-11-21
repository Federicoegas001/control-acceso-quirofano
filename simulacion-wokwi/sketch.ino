// Simulacion en Wokwi del control de acceso (ESP32 DevKit en lugar del ESP32-CAM).
// Wokwi no simula la camara ni llega al servidor Python sin el gateway pago, asi
// que la respuesta del servidor se simula con el interruptor:
//   interruptor a la derecha -> persona autorizada
//   interruptor a la izquierda -> persona desconocida
// Para disparar el sensor de movimiento: hacer clic sobre el sensor PIR durante la simulacion.
// La logica de actuadores (actuar, accesoPermitido, accesoDenegado) es la misma del firmware real.

#define PIN_RELE 3          // U0R en el ESP32-CAM
#define PIN_SENSOR 13       // OUT del HC-SR501
#define PIN_LED_VERDE 14
#define PIN_LED_ROJO 15
#define PIN_BUZZER 2
#define PIN_SIMULACION 27   // solo simulacion: reemplaza la respuesta del servidor

// El modulo rele de Wokwi se activa con HIGH (el real suele activarse con LOW)
const bool RELE_ACTIVO_EN_BAJO = false;

const unsigned long INTERVALO_MS = 4000;
const unsigned long TIEMPO_PUERTA_MS = 3000;
const unsigned long TIEMPO_ALARMA_MS = 1000;

void rele(bool activo) {
  bool nivel = RELE_ACTIVO_EN_BAJO ? !activo : activo;
  digitalWrite(PIN_RELE, nivel ? HIGH : LOW);
}

void buzzer(bool on) {
  // En Wokwi el buzzer necesita una frecuencia para sonar
  if (on) tone(PIN_BUZZER, 1000); else noTone(PIN_BUZZER);
}

void apagarTodo() {
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_ROJO, LOW);
  buzzer(false);
  rele(false);
}

void autoprueba() {
  Serial.println("Autoprueba: LED verde");
  digitalWrite(PIN_LED_VERDE, HIGH); delay(600); digitalWrite(PIN_LED_VERDE, LOW);
  Serial.println("Autoprueba: LED rojo");
  digitalWrite(PIN_LED_ROJO, HIGH); delay(600); digitalWrite(PIN_LED_ROJO, LOW);
  Serial.println("Autoprueba: buzzer");
  buzzer(true); delay(200); buzzer(false);
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
  buzzer(true);
  delay(TIEMPO_ALARMA_MS);
  apagarTodo();
}

// Igual que en el firmware real: decide segun el JSON del servidor
void actuar(String cuerpo) {
  cuerpo.replace(" ", "");
  cuerpo.replace("\n", "");
  if (cuerpo.indexOf("\"authorized\":true") >= 0) {
    accesoPermitido();
  } else if (cuerpo.indexOf("\"motivo\":\"desconocido\"") >= 0 ||
             cuerpo.indexOf("\"motivo\":\"sin_permiso\"") >= 0) {
    accesoDenegado();
  }
}

// Reemplaza a capturarYEnviar(): devuelve lo que contestaria el servidor
String simularRespuestaServidor() {
  if (digitalRead(PIN_SIMULACION) == HIGH) {
    return "{\"authorized\": true, \"status\": \"ok\", \"user\": \"Medico registrado\"}";
  }
  return "{\"authorized\": false, \"motivo\": \"desconocido\", \"status\": \"denied\", \"user\": null}";
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SENSOR, INPUT);
  pinMode(PIN_SIMULACION, INPUT);
  rele(false);
  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  apagarTodo();
  autoprueba();
  Serial.println("Listo. Hacer clic en el sensor PIR para simular una persona.");
}

unsigned long ultimaFoto = 0;

void loop() {
  bool hayAlguien = digitalRead(PIN_SENSOR) == HIGH;
  if (hayAlguien && (ultimaFoto == 0 || millis() - ultimaFoto >= INTERVALO_MS)) {
    Serial.println("Movimiento detectado: foto enviada (simulada)");
    String cuerpo = simularRespuestaServidor();
    Serial.println("Respuesta: " + cuerpo);
    actuar(cuerpo);
    ultimaFoto = millis();
  }
  delay(100);
}
