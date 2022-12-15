/*
   (c) 2022 Tim IIOT PMM2 "Timbangan Ayam" Institut Teknologi Bandung
   program ini dibuat sebagai tugas akhir dari mata kuliah Industrial IOT Prodi Teknik Fisika, Institut Teknologi
   Bandung. Proyek ini diciptakan oleh mahasiswa Pertukaran Mahasiswa Merdeka 2 (PMM 2) dari beragam universitas
   yang sedang mengemban kulah di Institut Teknologi bandung sebagai salah satu wujud program Merdeka Belajar Kam-
   pus Merdeka oleh Kemendikbud. Proyek ini berhasil tercipta berkat bimbingan dari dosen serta kakak dari Prodi 
   Teknik Fisika, Pak Eko Mursito, Pak Fhaqiza dan Kak Iqbal. Terima Kasih :)

   +Tim IIOT PMM2 "Timbangan Ayam" ITB:
   --Muhammad Akmal (Ketua) (Universitas Hasanuddin)
   --Agil Arham Iskandar (Universitas Hasanuddin)
   --Alda Latifa (Universitas Mulawarman)
   --Dhea Aulia (Universitas Bina Bangsa)
   --Harmiati Harbi (Universitas Hasanuddin)
   --Jacky Wu (Universitas Internasional Batam)
   --Andi Silvia Indriani (Universitas Hasanuddin)
*/
//Timbangan----------------------------------------------
//#include <HX711.h>
#include <HX711_ADC.h> 
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
//#include "soc/rtc.h"
#include <ezButton.h>
#include <Effortless_SPIFFS.h>
eSPIFFS fileSystem;

//global variabel
char layar[12];
static char lastLayar[12];
char strbeban[10];
//char strbebanBT[10];
char kg[4] = "Kg";  //tulisan kg
  
int count = 1;  //count beban kalibrasi
int disp = 1;   //step layar kalibrasi
int maindisp = 1; //step layar menimbang

bool spiffsSetCorrectly;
float callibration_factor = 1.0;
boolean autoHold = false;
int kecerahan = 3;

float beban;
int newDataReady = 0;
//float bebanBT = 0;
int nilaibeban;
unsigned long t = 0;

//untuk debugging----------------
int s1;
int s2;
int old_s1;
int old_s2;
int old_maindisp;
int old_disp;
int old_count;
//-------------------------------

unsigned int startMillis;
unsigned int currentMillis;

int menuState = 0;
int caldone = 0;
int tarestate = 0;
int savemodedotstate = 0;
int savemode = 0;

String str = "";

const int LedHold = 22;
const int LedBT   = 21;

//tombol
ezButton tmblhold(16);
ezButton tmbltare(17);

//buzzer
//int buzzerStart;
//int buzzerCurrent;
//int buzzerState;
const int pinBuzzer = 4;
//ezBuzzer buzzer(4);

//Load Cell & HX711
const int HX711_dout = 32; //mcu > HX711 dout pin
const int HX711_sck = 33; //mcu > HX711 sck pin
HX711_ADC LoadCell(HX711_dout, HX711_sck);

//Inisialisasi Dot Matrix
#include "3x7.h"
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 15
//Pin DIN=23
//Pin CLK=18
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

void initScale() { //Setup Timbangan
  pinMode(pinBuzzer, OUTPUT);
  pinMode(LedHold, OUTPUT);
  pinMode(LedBT, OUTPUT);

  //Debounce Tombol
  tmblhold.setDebounceTime(50);
  tmbltare.setDebounceTime(50);

  digitalWrite(LedHold, HIGH);
  digitalWrite(LedBT, HIGH);
  //membaca config
  
  if (loadConfig() == false) {
    Serial.println("File 'config.txt' gagal dibaca. menyimpan default");
    saveConfig(); 
  }
  digitalWrite(LedHold, LOW);
  digitalWrite(LedBT, LOW);

  //Mengatur display
  myDisplay.begin();
  myDisplay.setFont(myFont);
  myDisplay.setIntensity(kecerahan);
  myDisplay.displayClear();
  myDisplay.setTextAlignment(PA_CENTER);

  if (!spiffsSetCorrectly){
    myDisplay.print("E:SPIFFS");
    digitalWrite(pinBuzzer, HIGH);
    delay(200);
    digitalWrite(pinBuzzer, LOW);
    delay(200);
    digitalWrite(pinBuzzer, HIGH);
    delay(200);
    digitalWrite(pinBuzzer, LOW);
    for (;;){}
  }

  if (tmbltare.getStateRaw() == LOW) {
    menuState = 1;
    myDisplay.print("TUNGGU");
  }
  else {
    myDisplay.print("HALO");
  }
//  int _calSuccess = 0;
//  if (fileSystem.openFromFile("/kalibrasi.txt", callibration_factor)) {
//    Serial.print("'kalibrasi.txt' terdeteksi. Scale factor: ");
//    Serial.println(callibration_factor,2);
//    _calSuccess = 1;
//    }
    
  LoadCell.begin();
  unsigned long stabilizingtime = 1000;
  LoadCell.start(stabilizingtime);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    myDisplay.print("E: HX711");
    delay(10000);
    ESP.restart();
  }
  else{
    LoadCell.setCalFactor(callibration_factor);
  }
  
  if (menuState) menu();
}

void scaleCallback(){ // Loop Timbangan
  tmblhold.loop();
  tmbltare.loop();

  if (tmblhold.isPressed()){
    buzzerON();
    maindisp++;
  }
      
  if (LoadCell.update()) {
    newDataReady = 1;
  }

  if (newDataReady){
    float i = LoadCell.getData();
    beban = round(20*i)/20;
    newDataReady = 0;
  }

  //untuk debugging------------------
//  s1 = tmblhold.getCount();
//  s2 = tmbltare.getCount();
//  if (old_s1 != s1 || old_s2 != s2 || old_maindisp != maindisp) {
//    Serial.print("tombol hold: ");
//    Serial.print(s1);
//    Serial.print("\t tombol tare: ");
//    Serial.print(s2);
//    Serial.print("\t maindisp: ");
//    Serial.println(maindisp);
//    
//  }
//  old_s1 = s1;
//  old_s2 = s2;
//  old_maindisp = maindisp;
  //---------------------------------

  switch (maindisp) {
    case 1 :{
      if (tmbltare.isPressed()){
        savemode = 0;
        startMillis = millis();        
        //tombol tare
        buzzerON();
        LoadCell.tareNoDelay();
      }
    
//    Serial.print("Beban: ");
//    Serial.println(beban);
      static unsigned int holdMillis;
      static float lastBeban = -1;
      static float lastBebanHold = -1;
      if(beban != lastBeban){
        dtostrf(beban,6,2,strbeban);
        strcpy (layar, strbeban);
        strcat (layar, kg);
        myDisplay.setTextAlignment(PA_RIGHT);
        myDisplay.print(layar);
        Serial.println(layar);
        lastBeban = beban;
      }
//    if (savemode){
//      myDisplay.setIntensity(0);
//    } else {
//      myDisplay.setIntensity(kecerahan);
//    }
//
//    if (beban >= 0.3) {
//      savemode = 0; 
//      startMillis = millis();
//    } 
//    else if (currentMillis - startMillis >= 10000 && !savemode){
//      savemode = 1;
//    }

      if (autoHold){ //cek jika autohold aktif
        if (millis() - holdMillis >= 1000){
          if (beban >= 1){
            if (beban - lastBebanHold >= -0.1 && beban - lastBebanHold <= 0.1) {
              maindisp++;
            }
            lastBebanHold = beban;
          }
          holdMillis = millis();
        }
      } 
    break;
  }
  case 2 : {      //mulai proses Hold
     Serial.print("Hold Beban: ");
     Serial.println(beban);
     digitalWrite(LedHold, HIGH);
     maindisp++;
     break;
  }

  case 3 : {
//    if (beban <= 0.5){
//      maindisp++;
//    }
    break;
  }

  case 4 : {     //Keluar mode hold
    Serial.println("Keluar");
      digitalWrite(LedHold, LOW);
      //digitalWrite(LED_BUILTIN, LOW);
      maindisp = 1;
      break;
    }
  } 
}

void menu() {
  Serial.println("Mode Pengaturan berjalan");
  for(;;) {
    static int pilihan = 1;
    char nilai[9];
    static int lastKecerahan = 0;
    tmbltare.loop();
    tmblhold.loop();
    if (tmbltare.isPressed()){
      buzzerON();
      switch (pilihan) {
        case 1:
          kecerahan++;
          if (kecerahan > 15) kecerahan = 0;
          break;
          
        case 2:
          autoHold = !autoHold;
          break;

        case 3:
          saveConfig();
          return;
          break;

        case 4:
          kalibrasi();
          return;
          break;
          
        default:
          break;
      }
    }

    if (tmblhold.isPressed()) {
      pilihan++;
      if (pilihan > 4) pilihan = 1;
      buzzerON();
    }
    switch (pilihan) {
      case 1:
        itoa(kecerahan, nilai, 10);
        strcpy (layar,"B:    "); 
        strcat (layar, nilai);
        break;
      case 2:
        if (autoHold) strcpy(nilai, "ON");
        else strcpy(nilai, "OFF");
        strcpy (layar,"AH:  ");
        strcat (layar, nilai);
        break;
      case 3:
        strcpy (layar,"SIMPAN");
        break;
      case 4:
        strcpy (layar,"KLIBRASI");
        break;
      default:
        strcpy (layar,"CEK LAGI");
        break;
    }

    if (strcmp(layar, lastLayar) != 0 || lastKecerahan != kecerahan){
      myDisplay.setIntensity(kecerahan);
      myDisplay.setTextAlignment(PA_LEFT);
      myDisplay.print(layar);
      lastKecerahan = kecerahan;
      strcpy(lastLayar, layar);
    }
  }
}

void kalibrasi(){   // Program kalibrasi
  Serial.println("Mode kalibrasi berjalan");
  startMillis = millis();
  disp = 3;
  for (;;){   
    static boolean newDataReady = 0; 
    currentMillis = millis();   
    tmblhold.loop();
    tmbltare.loop();

    //untuk debugging---------------------------
//    s1 = tmblhold.getState();
//    s2 = tmbltare.getState();
//    if (old_s1 != s1 || old_s2 != s2 || old_disp != disp || old_count != count) {
//      Serial.print("tombol hold: ");
//      Serial.print(s1);
//      Serial.print("\t tombol tare: ");
//      Serial.print(s2);
//      Serial.print("\t Display: ");
//      Serial.print(disp);
//      Serial.print("\t beban kalibrasi: ");
//      Serial.println(nilaibeban);
//    }
//    old_s1 = s1;
//    old_s2 = s2;
//    old_disp = disp;
//    old_count = count;
    //-------------------------------------------

    if (tmblhold.isPressed()){
      disp++;
    } 

    switch (disp){
      case 1 : {
        if (currentMillis - startMillis >= 20) {
          myDisplay.setTextAlignment(PA_LEFT);
          myDisplay.print("KLBRASI?");
          startMillis = millis();}  
        break;
      }

      case 2 :
        buzzerON();
        disp++;
        break;

      case 3 : {
        strcpy(layar, "BEBAN");
        if (tmbltare.isPressed()) {
          buzzerON();          
          if (count >= 5)
            count = 1;
          else
            count++;
        }
        switch (count) {
          case 1 :  
            nilaibeban = 5; 
            strcpy(strbeban, "  5");             
            break;
          case 2 :
            nilaibeban = 10;
            strcpy(strbeban, " 10");
            break;
          case 3 :
            nilaibeban = 20;
            strcpy(strbeban, " 20");
            break;
          case 4:
            nilaibeban = 50;
            strcpy(strbeban, " 50");
            break;
          case 5 :
            nilaibeban = 100;
            strcpy(strbeban, "100");
            break;
        }
        strcat (layar, strbeban);

        //if (currentMillis - startMillis >= 20) {
          myDisplay.setTextAlignment(PA_LEFT);
          myDisplay.print(layar);
          startMillis = millis();
        //}
        break;
      }

      case 4 : {
        buzzerON();
        disp++;
        break;
      }
      case 5 : {
        myDisplay.setTextAlignment(PA_CENTER);
        myDisplay.print("TUNGGU");
        static boolean _hasTare = false;
        LoadCell.update();
        if (_hasTare == false) {
          LoadCell.tareNoDelay();
          _hasTare = true;
        }
        if (LoadCell.getTareStatus() == true) {
          Serial.println("Tare complete");
          disp++;
        }
        break;       
      }

//      case 5 ... 50 : {
//        disp++;
//        break;
//      } 
  
      case 6 : {
        if (tmbltare.isPressed()) {
          buzzerON();
          LoadCell.tareNoDelay();
        }
        static int jmlDesimal = 2;
        if (callibration_factor >= -10 && callibration_factor <= 10)
          jmlDesimal = 0;
        if (LoadCell.update()) newDataReady = true;
        if (newDataReady) {
          beban = LoadCell.getData();
          Serial.println(beban); 
          dtostrf(beban,8,jmlDesimal,strbeban);
          strcpy(layar, strbeban);
          myDisplay.setTextAlignment(PA_RIGHT);
          myDisplay.print(layar);
          startMillis = millis();
          newDataReady = false;
        }
        break;        
      }

      case 7: {
        buzzerON();
        disp++;
        break;
      }
      
      case 8 : {
        LoadCell.refreshDataSet();
        callibration_factor = LoadCell.getNewCalibration(nilaibeban);
        Serial.print("New Callibration factor: ");
        Serial.println(callibration_factor);
        
        if (saveConfig()) {
          myDisplay.setTextAlignment(PA_CENTER);
          myDisplay.print("SELESAI");
        }
        else {
          myDisplay.setTextAlignment(PA_CENTER);
          myDisplay.print("GAGAL");
        }
        buzzerON();
        delay(2000);
        return;
        break;
      }
    }   
  }
}

void buzzerON() {
  digitalWrite(pinBuzzer, HIGH);
  delay(100);
  digitalWrite(pinBuzzer, LOW);
  // buzzerStart = millis();
  // buzzerState = 1;
}

boolean saveConfig(){
  char buf[30];
  char strCal[15];
  char strKecerahan[3];
  char strAutoHold[3];
  dtostrf(callibration_factor,0,2,strCal);
  itoa (kecerahan, strKecerahan, 10);
  itoa (autoHold, strAutoHold, 10);
  strcpy(buf, strCal);
  strcat(buf, ",");
  strcat(buf, strKecerahan);
  strcat(buf, ",");
  strcat(buf, strAutoHold);
  Serial.println("Data yang disimpan adalah:");
  Serial.println(buf);
  if (fileSystem.saveToFile("/config.txt", buf)) return true;
  else return false;
}

boolean loadConfig(){
  char *token;
  int i = 0;
  char extract[3][10];
  char *buf;
  spiffsSetCorrectly = fileSystem.checkFlashConfig();
  if (spiffsSetCorrectly){
    if (fileSystem.openFromFile("/config.txt", buf)){
      token= strtok(buf, ",");
      while (token != NULL){
        strcpy(extract[i], token);
        //Serial.println(extract[i]);
        i++;
        token= strtok(NULL, ",");
      }
      callibration_factor = strtof(extract[0],NULL);
      kecerahan = atoi(extract[1]);
      autoHold = atoi(extract[2]);
      Serial.print("File 'config.txt' sukses dibaca. Calibration factor: ");
      Serial.print(callibration_factor);
      Serial.print(", kecerahan: ");
      Serial.print(kecerahan);
      Serial.print(", Auto Hold: ");
      Serial.println(autoHold);
      return true;
    }
    else {
      return false;
    }
  }
  else {
    return false; 
  }
}

//BLE-----------------------------------------------------------------------
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
//#define CHARACTERISTIC_UUID_RX "beb5483d-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      Serial.println ("=====START RECEIVE=====");
      Serial.println ("Received Value");

      for (int i = 0; i < rxValue.length(); i++) {
        Serial.print(rxValue[i]);
      }

      Serial.println();
      //pastikan Value yang diterima dari aplikasi sesuai dengan yang dibawah!!!
      if(rxValue.find("1") != -1) {  //Terima Value 1 = hold
        maindisp++;
      }

      else if (rxValue.find("2") != -1) { //Terima Value 2 = tare
        if (maindisp == 1) {
          digitalWrite(pinBuzzer, HIGH);
          delay(100);
          digitalWrite(pinBuzzer, LOW);
          LoadCell.tareNoDelay();
        }
      }

      Serial.println();
      Serial.println("=====END RECEIVE=====");
    }
  }
};

void initBT(){    //setup Bluetooth
  Serial.println("Memulai Bluetooth");
  // Create the BLE Device
  BLEDevice::init("Timbangan Ayam IIOT");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());
  
  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
  
}

void btCallback(){  //loop Bluetooth
  // notify changed value
    if (deviceConnected) {
      str = "";
      str += strbeban;
      
      pCharacteristic->setValue((char*)str.c_str());
      pCharacteristic->notify();
    }

    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        //delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("Bluetooth tidak tersambung. Menunggu koneksi");
        oldDeviceConnected = deviceConnected;
        digitalWrite(LedBT, LOW);
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        digitalWrite(LedBT, HIGH);
        Serial.println("Bluetooth tersambung");
        oldDeviceConnected = deviceConnected;
    }  
}

//Thread---------------------------------------------------------------------
#include <Thread.h>
#include <ThreadController.h>

// ThreadController that will controll all threads
ThreadController controll = ThreadController();

Thread* btThread = new Thread();
Thread* scaleThread = new Thread();
 
void setup() {
//  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
  Serial.begin(115200);
  Serial.println("Timbangan IOT");

  initScale();
  initBT();
  
  btThread->onRun(btCallback);
  btThread->setInterval(200);

  scaleThread->onRun(scaleCallback);
  //scaleThread->setInterval(4000);

  controll.add(btThread);
  controll.add(scaleThread);

  if (menuState == 0) {
    digitalWrite(pinBuzzer, HIGH);
    delay(100);
    digitalWrite(pinBuzzer, LOW);
  }
  startMillis = millis();
}

void loop() { 
    controll.run();
}
