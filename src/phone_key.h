#pragma once
#include <Arduino.h>

// Llave por telefono (BLE): ENCENDER solo con un telefono vinculado cerca o con el PIN.
// Vincular: el telefono pide el codigo de 6 digitos que muestra la pantalla.
namespace phone {

enum Enroll : uint8_t { EN_OFF, EN_WAITING, EN_DONE, EN_FAILED };

void begin();               // despues de car::begin (WiFi ya iniciado)
void update(uint32_t now);  // vencimiento de la vinculacion
void setScanning(bool on);  // el escaneo BLE solo corre mientras se necesita (menu abierto con el auto apagado)
bool scanning();
void probe(uint32_t now, uint32_t durMs);  // sonda de cercania: anuncia el teclado vinculado y espera la reconexion del telefono
bool probing();
uint32_t probeHit();  // ms que tardo en reconectarse (0 = no se conecto)
void scanStats(char* out, size_t n);  // contadores de diagnostico del escaneo

bool near(uint32_t now);    // telefono vinculado visto hace poco con senal suficiente
int lastRssi();             // RSSI del ultimo anuncio reconocido (0 si nunca)
uint32_t lastSeenAgeMs(uint32_t now);
int count();                // telefonos vinculados

void startEnroll(uint32_t now);
void stopEnroll();
Enroll enrollState();
uint32_t passkey();         // codigo que hay que tipear en el iPhone
uint32_t enrollLeftMs(uint32_t now);

int selfTest();             // autotest; devuelve fallas

}  // namespace phone
