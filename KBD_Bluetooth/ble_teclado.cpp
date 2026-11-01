#include "ble_teclado.h"
#include "placa.h"
#include "log.h"

#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

static constexpr uint8_t ID_TECLADO = 1;
static constexpr uint8_t ID_MIDIA   = 2;

/* Mapa de relatorios HID (o mesmo formato de um teclado USB comum):
   - ID 1: teclado boot (modificadores, reservado, 6 teclas) + LEDs
   - ID 2: teclas de midia (um uso de 16 bits da pagina Consumer) */
static const uint8_t MAPA_HID[] = {
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x06,        // Usage (Keyboard)
  0xA1, 0x01,        // Collection (Application)
  0x85, ID_TECLADO,  //   Report ID (1)
  0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
  0x19, 0xE0,        //   Usage Minimum (Left Control)
  0x29, 0xE7,        //   Usage Maximum (Right GUI)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x75, 0x01,        //   Report Size (1)
  0x95, 0x08,        //   Report Count (8)
  0x81, 0x02,        //   Input (Data, Var, Abs)      modificadores
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x08,        //   Report Size (8)
  0x81, 0x01,        //   Input (Const)               reservado
  0x95, 0x05,        //   Report Count (5)
  0x75, 0x01,        //   Report Size (1)
  0x05, 0x08,        //   Usage Page (LEDs)
  0x19, 0x01,        //   Usage Minimum (Num Lock)
  0x29, 0x05,        //   Usage Maximum (Kana)
  0x91, 0x02,        //   Output (Data, Var, Abs)     LEDs
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x03,        //   Report Size (3)
  0x91, 0x01,        //   Output (Const)              completa o byte
  0x95, 0x06,        //   Report Count (6)
  0x75, 0x08,        //   Report Size (8)
  0x15, 0x00,        //   Logical Minimum (0)
  0x26, 0xFF, 0x00,  //   Logical Maximum (255)
  0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
  0x19, 0x00,        //   Usage Minimum (0)
  0x2A, 0xFF, 0x00,  //   Usage Maximum (255)
  0x81, 0x00,        //   Input (Data, Array, Abs)    6 teclas
  0xC0,              // End Collection

  0x05, 0x0C,        // Usage Page (Consumer)
  0x09, 0x01,        // Usage (Consumer Control)
  0xA1, 0x01,        // Collection (Application)
  0x85, ID_MIDIA,    //   Report ID (2)
  0x15, 0x00,        //   Logical Minimum (0)
  0x26, 0xFF, 0x03,  //   Logical Maximum (0x3FF)
  0x19, 0x00,        //   Usage Minimum (0)
  0x2A, 0xFF, 0x03,  //   Usage Maximum (0x3FF)
  0x75, 0x10,        //   Report Size (16)
  0x95, 0x01,        //   Report Count (1)
  0x81, 0x00,        //   Input (Data, Array, Abs)
  0xC0,              // End Collection
};

static NimBLEServer*         s_srv     = nullptr;
static NimBLEHIDDevice*      s_hid     = nullptr;
static NimBLECharacteristic* s_teclado = nullptr;
static NimBLECharacteristic* s_midia   = nullptr;
static NimBLECharacteristic* s_leds_ch = nullptr;

static volatile EstadoBle s_estado = BLE_ANUNCIANDO;
static volatile uint8_t   s_leds = 0;
static volatile bool      s_leds_novos = false;

/* =====================================================================
   CALLBACKS (rodam na tarefa do NimBLE)
   ===================================================================== */
class CallbacksServidor : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* srv, NimBLEConnInfo& info) override {
    s_estado = BLE_CONECTADO;
    kbd_log("[BLE] conectado: %s\n", info.getAddress().toString().c_str());
  }
  void onDisconnect(NimBLEServer* srv, NimBLEConnInfo& info, int motivo) override {
    s_estado = BLE_ANUNCIANDO;
    kbd_log("[BLE] desconectado (motivo %d): anunciando de novo\n", motivo);
  }
  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    if (info.isEncrypted()) {
      s_estado = BLE_PRONTO;
      kbd_log("[BLE] pareado%s\n", info.isBonded() ? " (vinculo salvo)" : "");
    } else {
      kbd_log("[BLE] falha no pareamento\n");
    }
  }
};

class CallbacksLeds : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
    NimBLEAttValue v = c->getValue();
    if (v.length() < 1) return;
    s_leds = v.data()[0];
    s_leds_novos = true;
  }
};

/* =====================================================================
   API
   ===================================================================== */
void bleteclado_iniciar() {
  NimBLEDevice::init(BLE_NOME_DISPOSITIVO);
  NimBLEDevice::setPower(3);   // dBm: suficiente para a mesa, poupa bateria
  // Pareamento "Just Works" com vinculo salvo (o PC lembra do teclado)
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  s_srv = NimBLEDevice::createServer();
  s_srv->setCallbacks(new CallbacksServidor());
  s_srv->advertiseOnDisconnect(true);

  s_hid = new NimBLEHIDDevice(s_srv);
  s_hid->setManufacturer(BLE_FABRICANTE);
  s_hid->setPnp(0x02, 0x303A, 0x4B42, 0x0010);   // origem USB, VID Espressif
  s_hid->setHidInfo(0x00, 0x01);                  // sem pais, remote wake
  s_hid->setReportMap((uint8_t*)MAPA_HID, sizeof(MAPA_HID));

  s_teclado = s_hid->getInputReport(ID_TECLADO);
  s_midia   = s_hid->getInputReport(ID_MIDIA);
  s_leds_ch = s_hid->getOutputReport(ID_TECLADO);
  s_leds_ch->setCallbacks(new CallbacksLeds());
  s_hid->setBatteryLevel(100);

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setAppearance(0x03C1);                      // HID: teclado
  adv->addServiceUUID(s_hid->getHidService()->getUUID());
  adv->setName(BLE_NOME_DISPOSITIVO);
  adv->enableScanResponse(true);
  adv->start();

  kbd_log("[BLE] anunciando como \"%s\"\n", BLE_NOME_DISPOSITIVO);
}

EstadoBle bleteclado_estado() { return s_estado; }

bool bleteclado_enviar_teclado(const uint8_t rel[8]) {
  if (s_estado != BLE_PRONTO || !s_teclado) return false;
  s_teclado->setValue(rel, 8);
  return s_teclado->notify();
}

bool bleteclado_enviar_midia(uint16_t uso) {
  if (s_estado != BLE_PRONTO || !s_midia) return false;
  const uint8_t v[2] = { (uint8_t)(uso & 0xFF), (uint8_t)(uso >> 8) };
  s_midia->setValue(v, 2);
  return s_midia->notify();
}

bool bleteclado_leds_mudaram(uint8_t* leds) {
  if (!s_leds_novos) return false;
  s_leds_novos = false;
  *leds = s_leds;
  return true;
}

void bleteclado_apagar_pareamentos() {
  NimBLEDevice::deleteAllBonds();
  if (s_srv) for (uint16_t h : s_srv->getPeerDevices()) s_srv->disconnect(h);
  kbd_log("[BLE] pareamentos apagados\n");
}

void bleteclado_definir_bateria(uint8_t porcento) {
  if (s_hid) s_hid->setBatteryLevel(porcento, true);
}
