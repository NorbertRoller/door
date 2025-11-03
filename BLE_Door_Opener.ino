/**
 * NimBLE_Secure_Server Demo:
 *
 * This example demonstrates the secure passkey protected conenction and communication between an esp32 server and an
 * esp32 client. Please note that esp32 stores auth info in nvs memory. After a successful connection it is possible
 * that a passkey change will be ineffective. To avoid this clear the memory of the esp32's between security testings.
 * esptool.py is capable of this, example: esptool.py --port /dev/ttyUSB0 erase_flash.
 *
 *  Created: on Jan 08 2021
 *      Author: mblasee
 
// PIN4 bei Boot > xV dann kein WLAN oder BLE wenn Strom über VIN. Nur USB geht dann !!!
// !!! Zum Programmieren IOO gegen Maße brücken bis Programmierung fertig. Dann RESET

https://randomnerdtutorials.com/esp32-web-bluetooth/

setSecurityPasskey(704520);    (31+12+60)*(11+7+77)*(8+5+59)
cmd2openDoor(0x031,0x12,0x60,0x11,0x07,0x77,0x08,0x05,0x59)
  */

#include <Arduino.h>
#include <NimBLEDevice.h>

// Relay für +/- Tausch Tür Stellmotor
#define K1 16
#define K2 17
#define LED 23

bool deviceConnected = false;

char BLE_buffer[55]="";
char cmd2openDoor[]={0x031,0x12,0x60,0x11,0x07,0x77,0x08,0x05,0x59};           //"311260110777080559"
int cmd_ok=0;

static NimBLEServer* pServer;

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        Serial.printf("Client address: %s\n", connInfo.getAddress().toString().c_str());

        /**
         *  We can use the connection handle here to ask for different connection parameters.
         *  Args: connection handle, min connection interval, max connection interval
         *  latency, supervision timeout.
         *  Units; Min/Max Intervals: 1.25 millisecond increments.
         *  Latency: number of intervals allowed to skip.
         *  Timeout: 10 millisecond increments.
         */
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
        digitalWrite(LED, HIGH);                     // LED on to show connection with client
        deviceConnected = true;
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.printf("Client disconnected - start advertising\n");
        NimBLEDevice::startAdvertising();
        digitalWrite(LED, LOW);                     // LED off == no client connected
        deviceConnected = false;
    }

    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
        Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, connInfo.getConnHandle());
    }

    /********************* Security handled here *********************/
    uint32_t onPassKeyDisplay() override {
        Serial.printf("Server Passkey Display\n");
        /**
         * This should return a random 6 digit number for security
         *  or make your own static passkey as done here.
         */
        return 123456;
    }

    void onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pass_key) override {
        Serial.printf("The passkey YES/NO number: %" PRIu32 "\n", pass_key);
        /** Inject false if passkeys don't match. */
        NimBLEDevice::injectConfirmPasskey(connInfo, true);
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        /** Check that encryption was successful, if not we disconnect the client */
        if (!connInfo.isEncrypted()) {
            NimBLEDevice::getServer()->disconnect(connInfo.getConnHandle());
            Serial.printf("Encrypt connection failed - disconnecting client\n");
            return;
        }

        Serial.printf("Secured connection to: %s\n", connInfo.getAddress().toString().c_str());
    }
} serverCallbacks;

/** Handler class for characteristic actions */
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override 
        {
        //pCharacteristic->setValue(lin_data); // Wert der mit Read abgefragt wird
        Serial.printf("%s : onRead(), value: %s\n",pCharacteristic->getUUID().toString().c_str(),pCharacteristic->getValue().c_str());
        }

 void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override 
{
    size_t len = pCharacteristic->getValue().length(); // Correct length of raw bytes
    const uint8_t* value = (const uint8_t*)pCharacteristic->getValue().data();

    Serial.printf("%s \nonWrite(), length: %zu\n", pCharacteristic->getUUID().toString().c_str(), len);

    if(len != 0) {
        for(size_t i = 0; i < len; i++) {
            BLE_buffer[i] = value[i];  // Copy bytes
            Serial.printf("BLE_buffer[%zu]: 0x%02X\n", i, BLE_buffer[i]);
        }

        Serial.println("---------------");

        if(len == sizeof(cmd2openDoor)) { // Compare lengths
            bool match = true;
            for(size_t i = 0; i < len; i++) {
                Serial.printf("cmd2openDoor[%zu]: 0x%02X - BLE_buffer[%zu]: 0x%02X\n", i, cmd2openDoor[i], i, BLE_buffer[i]);
                if(cmd2openDoor[i] != BLE_buffer[i]) {
                    match = false;
                }
            }
            if(match) {
                Serial.println("Open CMD is OK!");
                cmd_ok = 1;
            } else {
                Serial.println("Open CMD is NOT OK.");
            }
        } else {
            Serial.println("Received code has wrong length.");
        }
    }
}
   

    /**
     *  The value returned in code is the NimBLE host return code.
     */
    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override {
        Serial.printf("Notification/Indication return code: %d, %s\n", code, NimBLEUtils::returnCodeToString(code));
    }

    /** Peer subscribed to notifications/indications */
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
        std::string str  = "Client ID: ";
        str             += connInfo.getConnHandle();
        str             += " Address: ";
        str             += connInfo.getAddress().toString();
        if (subValue == 0) {
            str += " Unsubscribed to ";
        } else if (subValue == 1) {
            str += " Subscribed to notifications for ";
        } else if (subValue == 2) {
            str += " Subscribed to indications for ";
        } else if (subValue == 3) {
            str += " Subscribed to notifications and indications for ";
        }
        str += std::string(pCharacteristic->getUUID());

        Serial.printf("%s\n", str.c_str());
        
    }
} chrCallbacks;

void setup() 
    {
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);                     // LED off if not client connected
    delay(1000);
    Serial.begin(115200);
    Serial.println("Starting NimBLE Server");
    NimBLEDevice::init("BLE2FORD");
    NimBLEDevice::setPower(3); /** +3db */

    NimBLEDevice::setSecurityAuth(true, true, false); /** bonding, MITM, don't need BLE secure connections as we are using passkey pairing */
    NimBLEDevice::setSecurityPasskey(704520);           // 6 Ziffern
                                                        // (8+5+59)*(11+07+77)*(31+12+60)=704520

    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // Display only passkey 

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    NimBLEService* pService = pServer->createService("A1DF2491-D1AC-43DB-A181-FBFE50A90B16");

    NimBLECharacteristic* pNonSecureCharacteristic = pService->createCharacteristic("A1DF2492-D1AC-43DB-A181-FBFE50A90B16", NIMBLE_PROPERTY::READ);

    NimBLECharacteristic* pRelaySecureCharacteristic =
    pService->createCharacteristic("A1DF2493-D1AC-43DB-A181-FBFE50A90B16",NIMBLE_PROPERTY::WRITE);
                                     //  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);
    //NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN | 
    pRelaySecureCharacteristic->setCallbacks(&chrCallbacks);

    pService->start();
    
    // Display MAC Address
    std::string serverAddress = NimBLEDevice::getAddress();
    Serial.printf("BLE2FORD-Server MAC-Address: %s\n\n",serverAddress.c_str());

    pNonSecureCharacteristic->setValue("Secret Key is 123456");         //:-))

    //pRelaySecureCharacteristic->setValue("Hello Secure BLE");      // Write only Characteristic
    
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID("A1DF2491-D1AC-43DB-A181-FBFE50A90B16");
    pAdvertising->enableScanResponse(true);
    pAdvertising->start();
    Serial.println("Waiting a client connection to notify...");
        
    // Relay für +/- Tausch Tür Stellmotor
    pinMode(K1, OUTPUT);
    digitalWrite(K1, LOW);                     // Relay  K1 off
    pinMode(K2, OUTPUT);
    digitalWrite(K2, LOW);                     // Relay  K2 off
    }


void loop() 
{
if(cmd_ok==1)           // Bluetooth PW war OK. Jetzt mit CAN sprechen
    {
    Serial.println("Open CMD is OK.");
    // Door Open is next
    digitalWrite(K1, HIGH);                     // Relay  K1 on
    digitalWrite(K2, HIGH);                     // Relay  K2 on
    delay(500);                         // Nur 500ms aktiv
    digitalWrite(K1, LOW);                     // Relay  K1 off
    digitalWrite(K2, LOW);                     // Relay  K2 off

    cmd_ok=0;           // Cmd only 1 time executeable
    }

}
