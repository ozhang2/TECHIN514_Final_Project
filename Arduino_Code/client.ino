#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// BLE UUID (Same as the Server side)
static BLEUUID serviceUUID("c36bda34-c052-4045-9635-7f42a81b0deb");
static BLEUUID charUUID("97a128ba-e071-4602-8d34-03c67a925c72");

// OLED Screen Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// LED Pin
#define LED_PIN 9
#define TEMP_THRESHOLD_LED 27.0     // Temperature threshold for LED activation
#define TEMP_THRESHOLD_MOTOR 30.0   // Temperature threshold for motor activation

// Stepper Motor Pins
#define COIL_A1 D2
#define COIL_A2 D3
#define COIL_B1 D0
#define COIL_B2 D1

// Stepper Motor Step Sequence
const int stepSequence[4][4] = {
    {1, 0, 1, 0}, 
    {0, 1, 1, 0}, 
    {0, 1, 0, 1}, 
    {1, 0, 0, 1}  
};

// Stepper Motor State
bool motorAtMax = false; // Flag for motor maximum position

// BLE Connection State
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

// ==================== Stepper Motor Control Function ====================
void stepMotor(int stepDelay, bool reverse) {
    for (int i = 0; i < 200; i++) { 
        for (int step = 0; step < 4; step++) {
            int s = reverse ? (3 - step) : step;
            digitalWrite(COIL_A1, stepSequence[s][0]);
            digitalWrite(COIL_A2, stepSequence[s][1]);
            digitalWrite(COIL_B1, stepSequence[s][2]);
            digitalWrite(COIL_B2, stepSequence[s][3]);
            delay(stepDelay); 
        }
    }
}

// ==================== Notify Callback Function ====================
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) 
{
    // Convert received data to string
    String temperatureData = "";
    for (size_t i = 0; i < length; i++) {
        temperatureData += (char)pData[i];
    }

    Serial.print("Received Temperature: ");
    Serial.println(temperatureData);

    // Convert string to float for temperature checking
    float temperatureValue = temperatureData.toFloat();

    // LED Control Logic
    if (temperatureValue > TEMP_THRESHOLD_LED) {
        digitalWrite(LED_PIN, HIGH); 
    } else {
        digitalWrite(LED_PIN, LOW);  
    }

    // Stepper Motor Control Logic
    if (temperatureValue > TEMP_THRESHOLD_MOTOR && !motorAtMax) {
        stepMotor(5, false);  // Move to maximum position
        motorAtMax = true;
    } 
    else if (temperatureValue <= TEMP_THRESHOLD_MOTOR && motorAtMax) {
        stepMotor(5, true);   // Return to zero position
        motorAtMax = false;
    }

    // Display temperature on OLED
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Temp:");
    display.setTextSize(2);
    display.println(temperatureData);
    display.display();
}

// ==================== BLE Connection Callback ====================
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        Serial.println("Client connected to Server.");
    }

    void onDisconnect(BLEClient* pclient) {
        connected = false;
        Serial.println("Client disconnected from Server.");
    }
};

// ==================== BLE Connection Function ====================
bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    BLEClient* pClient = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to Server
    pClient->connect(myDevice);
    Serial.println(" - Connected to server");
    pClient->setMTU(517);

    // Get Service
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.print("Failed to find Service UUID: ");
        Serial.println(serviceUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Found our service");

    // Get Characteristic
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.print("Failed to find Characteristic UUID: ");
        Serial.println(charUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Found our characteristic");

    // Register Notify Callback
    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }

    connected = true;
    return true;
}

// ==================== Scan Callback ====================
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.print("BLE Advertised Device found: ");
        Serial.println(advertisedDevice.toString().c_str());

        if (advertisedDevice.haveServiceUUID() && 
            advertisedDevice.isAdvertisingService(serviceUUID)) 
        {
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;
        }
    }
};

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE Client...");

    // Initialize OLED
    Wire.begin();
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
        Serial.println("SSD1306 allocation failed");
        for (;;);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("BLE Client Start");
    display.display();

    // Initialize LED Pin
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Initialize Stepper Motor Pins
    pinMode(COIL_A1, OUTPUT);
    pinMode(COIL_A2, OUTPUT);
    pinMode(COIL_B1, OUTPUT);
    pinMode(COIL_B2, OUTPUT);

    // Initialize BLE
    BLEDevice::init("");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);
}

// ==================== LOOP ====================
void loop() {
    if (doConnect) {
        if (connectToServer()) {
            Serial.println("We are now connected to the BLE Server.");
        } else {
            Serial.println("Failed to connect to Server.");
        }
        doConnect = false;
    }

    if (connected) {
        delay(1000);
    } else if (doScan) {
        BLEDevice::getScan()->start(0);
    }
}
