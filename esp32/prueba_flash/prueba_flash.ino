// Prueba del flash integrado del ESP32-CAM (GPIO4)
#define PIN_FLASH 4

void setup() {
  Serial.begin(115200);
  pinMode(PIN_FLASH, OUTPUT);
  Serial.println("Prueba del flash");
}

void loop() {
  Serial.println("Flash ON");
  digitalWrite(PIN_FLASH, HIGH);
  delay(300);            // encendido corto: el flash es muy potente y calienta

  Serial.println("Flash OFF");
  digitalWrite(PIN_FLASH, LOW);
  delay(1500);
}
