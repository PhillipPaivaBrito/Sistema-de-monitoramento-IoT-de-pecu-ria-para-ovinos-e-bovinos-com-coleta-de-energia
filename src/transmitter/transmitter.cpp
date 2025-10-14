#include <RadioLib.h>
#include <TinyGPSplus.h>
#include <SoftwareSerial.h>
#include <MPU6050_tockn.h>
#include "HT_SSD1306Wire.h"
#include "esp_task_wdt.h"

//fator de conversão de microsegundos para segundos
#define uS_TO_S_FACTOR 1000000
//tempo que o ESP32 ficará em modo sleep (em segundos)
#define TIME_TO_SLEEP 247 // = 5min
#define NIGHT_TIME_TO_SLEEP 900// = 15min
#define LOW_BATTERY_TIME_TO_SLEEP 1800// = 30min

#define samplesNums 3

#ifdef HELTEC_WIFI_LORA_32_V2
static const int RXPin = 13, TXPin = 12;
static const int SDApin = 4, SCLpin = 15;
SX1276 radio = new Module(18, 26, 14, 35);
#define batterypin 37;
#endif
#ifdef HELTEC_WIFI_LORA_32_V3
static const int RXPin = 6, TXPin = 5;
static const int SDApin = 35, SCLpin = 36;// 41 42
SX1262 radio = new Module(8, 14, 12, 13);
#define batterypin 37;
#endif
int VBatPin = batterypin;
static const uint32_t GPSBaud = 9600;

bool monitordebug = false;
// flag to indicate that a packet was sent or received
volatile bool operationDone = false;
// save transmission states between loops
int transmissionState = RADIOLIB_ERR_NONE;

static const double ORIGIN_LAT = -7.153354, ORIGIN_LON = -34.891542;

// The TinyGPSPlus object
TinyGPSPlus gps;
//byte sleepCmd[]  = {0xB5, 0x62, 0x02, 0x41, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x4D, 0x3B};
byte sleepCmd[]  = {0xB5, 0x62, 0x02, 0x41, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4B, 0xE0};
byte wakeupCmd[] = {0xB5, 0x62, 0x02, 0x41, 0x00, 0x00, 0x4D, 0x3B};

String myID = String(ESP.getEfuseMac());

int GMT = -3;
int morning = 5;
int night = 18;

RTC_DATA_ATTR bool firstboot = true; 
RTC_DATA_ATTR float gyrox, gyroy, gyroz;
// The serial connection to the GPS device
RTC_DATA_ATTR double last_lat = 0.0;
RTC_DATA_ATTR double last_lng = 0.0;
RTC_DATA_ATTR double last_alt = 0.0;

SoftwareSerial ss(RXPin, TXPin);

MPU6050 mpu6050(Wire);

//SX1276 radio = new Module(18, 26, 14, 35);
SSD1306Wire  factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst

static void printStr(const char *str, int len);
static void printDateTime(TinyGPSDate &d, TinyGPSTime &t);
static void smartDelay(unsigned long ms);
static void printFloat(float val, bool valid, int len, int prec);
static void printInt(unsigned long val, bool valid, int len);

void setupLora();
void setupDisplay();
#ifdef HELTEC_WIFI_LORA_32_V3
  uint16_t voltage;
  void getBatteryVoltage();
#endif
void NightTime();
String buildPackeger(String GPS);

static void IRAM_ATTR interuptISR();

void sendLoRaPacket(String str);
void sendStatusLCD(String str);

void ShowMonitorcabecalho(bool opcao);
void ShowMonitorGPS(bool opcao);
void ShowMonitorIMU(bool opcao);

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void);

void setup(){

  esp_task_wdt_init(30, true); // 30 segundos de timeout, reset automático
  esp_task_wdt_add(NULL);      // Adiciona a task atual (loop principal) ao watchdog

  attachInterrupt(GPIO_NUM_0, interuptISR ,GPIO_INTR_POSEDGE);
  pinMode(GPIO_NUM_0, INPUT_PULLUP);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0,LOW);

  Serial.begin(115200);
  
  setupLora();
#ifdef HELTEC_WIFI_LORA_32_V3
  getBatteryVoltage();

  if (voltage < 3400) { // 3.4V em mV
    esp_sleep_enable_timer_wakeup(LOW_BATTERY_TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
#endif
  //GPIO_NUM_21; // Vext v2
  //GPIO_NUM_36; // Vext v3
  
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext,LOW);
  delay(50);

  ss.begin(GPSBaud);
  ss.write(wakeupCmd, sizeof(wakeupCmd));

  Wire.begin(SDApin,SCLpin,500000);

  if(firstboot){
    //monitordebug = true;
    mpu6050.begin();
    mpu6050.calcGyroOffsets(monitordebug);
    //digitalWrite(Vext,HIGH);
    gyrox = mpu6050.getGyroXoffset();
    gyroy = mpu6050.getGyroYoffset();
    gyroz = mpu6050.getGyroZoffset();
    firstboot = false;
    if(monitordebug)
      Serial.println("\nGyro offsets: " + String(gyrox) + " " + String(gyroy) + " " + String(gyroz));
  }

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B);
  Wire.write(0x40);  // Bit 6 (SLEEP) = 1
  Wire.endTransmission(true);

  esp_task_wdt_reset();

  sendStatusLCD("ID: " + String(myID) + " sending data ...");

  if(monitordebug){
    sendStatusLCD("waiting for GPS...");
    Serial.println("waiting for GPS...");
  }

  ShowMonitorcabecalho(monitordebug);

  for(int i = 0;(!gps.satellites.isValid() || !gps.location.isValid()) && i < 50; i++){ // timeout de 50 segundos

    smartDelay(1000);
    ShowMonitorGPS(monitordebug);
    esp_task_wdt_reset();
    if (millis() > 5000 && gps.charsProcessed() < 10 && monitordebug)
      Serial.println(F("No GPS data received: check wiring"));
  }

  String GPS;
  if (gps.location.isValid()) {
      GPS = "\nGPS: " +
          String(gps.location.lat(),6) + " " +
          String(gps.location.lng(),6) + " " +
          String(gps.altitude.meters()) + " " +
          String(gps.course.deg()) + " " +
          String(gps.speed.mps());
  } else if (last_lat != 0.0 && last_lng != 0.0) {
      GPS = "\nGPS: " +
          String(last_lat,6) + " " +
          String(last_lng,6) + " " +
          String(last_alt) + " 0 0"; // Sem course/speed
  } else {
      GPS = "\nGPS: 0 0 0 0 0";
  }

  ss.write(sleepCmd, sizeof(sleepCmd));
  mpu6050.begin();
  mpu6050.setGyroOffsets(gyrox, gyroy, gyroz);
  delay(500);
  esp_task_wdt_reset();
  for (int i = 0; i <4  ; i++) {
    sendLoRaPacket(buildPackeger(GPS));
    delay(500);
  }
  esp_task_wdt_reset();
  radio.sleep();
  if (gps.location.isValid()) {
    last_lat = gps.location.lat();
    last_lng = gps.location.lng();
    last_alt = gps.altitude.meters();
  }
  //NightTime();
  if(monitordebug)
    Serial.println("goin to sleep...");

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B);
  Wire.write(0x40);  // Bit 6 (SLEEP) = 1
  Wire.endTransmission(true);
  
  Serial.flush();
  
  digitalWrite(Vext,HIGH); // Vext é a alimentação do gps e imu

  esp_task_wdt_delete(NULL); // Desativa o watchdog da task principal
  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void loop(){
}

static void smartDelay(unsigned long ms){
  // This custom version of delay() ensures that the gps object is being "fed".
  unsigned long start = millis();
  do {
    while (ss.available())
      gps.encode(ss.read());
  } while (millis() - start < ms);
}

static void printFloat(float val, bool valid, int len, int prec){
  if (!valid){
    while (len-- > 1)
      Serial.print('*');
    Serial.print(' ');
  }
  else{
    Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . and -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      Serial.print(' ');
  }
  smartDelay(0);
}

static void printInt(unsigned long val, bool valid, int len){
  char sz[32] = "*****************";
  if (valid)
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  Serial.print(sz);
  smartDelay(0);
}

static void printDateTime(TinyGPSDate &d, TinyGPSTime &t){
  if (!d.isValid()){
    Serial.print(F("********** "));
  }
  else{
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
    Serial.print(sz);
  }
  
  if (!t.isValid()){
    Serial.print(F("******** "));
  }
  else{
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d ", t.hour(), t.minute(), t.second());
    Serial.print(sz);
  }

  printInt(d.age(), d.isValid(), 5);
  smartDelay(0);
}

static void printStr(const char *str, int len){
  int slen = strlen(str);
  for (int i=0; i<len; ++i)
    Serial.print(i<slen ? str[i] : ' ');
  smartDelay(0);
}

void setupDisplay() {
  //Inicializa o display
  factory_display.init();
  //Limpa o display
  factory_display.clear();
  //Modifica direcionamento do texto
  factory_display.flipScreenVertically();  
  //Alinha o texto à esquerda
  factory_display.setTextAlignment(TEXT_ALIGN_LEFT);
  //Altera a fonte
  factory_display.setFont(ArialMT_Plain_10);

  //factory_display.sleep();
}

void sendLoRaPacket(String str) {
  
  // send another one
  if(monitordebug)
    Serial.print(F("Sending another packet ... "));
  transmissionState = radio.startTransmit(str);
  while(!operationDone) {
    if(monitordebug)
      Serial.print(F("."));
    // wait for the previous operation to finish
    delay(10);
  }
  operationDone = false;
    // reset flag
    // the previous operation was transmission, listen for response
    // print the result
  if (transmissionState == RADIOLIB_ERR_NONE) {
    // packet was successfully sent
    if(monitordebug)
      Serial.println("transmission finished!");

  } else {
    Serial.print("failed, code ");
    Serial.println(transmissionState);

  }

}

void sendStatusLCD(String display) {
  if(monitordebug){
    digitalWrite(Vext,LOW);
    delay(50);
    setupDisplay();

    factory_display.displayOn();

    factory_display.drawString(0, 0, String("ID: " + myID));
    factory_display.drawString(0, 10, display);
    factory_display.drawString(0, 20,"GMT time: " + 
      String(gps.time.hour()) + "h - here: " + 
      String((gps.time.hour() + GMT) < 0 ?
        gps.time.hour() + GMT + 24:
        gps.time.hour() + GMT) + "h" );
    #ifdef HELTEC_WIFI_LORA_32_V3
      factory_display.drawString(0, 30, "battery: " + String(voltage)+" mV");
    #endif
    //factory_display.drawString(0, 40, "boot count: " + String(boot_count));
    factory_display.display();
  /*}else{
    factory_display.display->displayOff();//*/ 
  }
  //factory_display.stop();
}

void ShowMonitorcabecalho(bool opcao){
  if(opcao){
  
    Serial.println(("\n meu ID:"+myID));

    //Serial.print(F("TinyGPSPlus library v. ")); Serial.println(TinyGPSPlus::libraryVersion());
    
    Serial.println(F("Sats HDOP  Latitude   Longitude   Fix  Date       Time     Date Alt    Course Speed Card  Distance Course Card  Chars Sentences Checksum"));
    Serial.println(F("           (deg)      (deg)       Age             (GMT)    Age  (m)    --- from GPS ----  ---- to HOME    ----  RX    RX        Fail"));
    Serial.println(F("----------------------------------------------------------------------------------------------------------------------------------------"));
  }
}

void ShowMonitorGPS(bool opcao){
  if(opcao){
    printInt(gps.satellites.value(), gps.satellites.isValid(), 5);  //Sats
    printFloat(gps.hdop.hdop(), gps.hdop.isValid(), 6, 1);          //HDOP
    printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);  //Latitude (deg)
    printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);  //Longitude(deg)
    printInt(gps.location.age(), gps.location.isValid(), 5);        //Fix Age
    printDateTime(gps.date, gps.time);                              //Time (GMT)
    printFloat(gps.altitude.meters(), gps.altitude.isValid(), 7, 2);//Alt (m)
    printFloat(gps.course.deg(), gps.course.isValid(), 7, 2);       //Course 
    printFloat(gps.speed.mps(), gps.speed.isValid(), 6, 2);         //Speed
    printStr(gps.course.isValid() ? TinyGPSPlus::cardinal(gps.course.deg()) :"*** ", 6);// Card
    
    unsigned long distanceKmToORIGIN =(unsigned long)TinyGPSPlus::distanceBetween(
      gps.location.lat(),
      gps.location.lng(),
      ORIGIN_LAT, 
      ORIGIN_LON) / 1000;
    printInt(distanceKmToORIGIN, gps.location.isValid(), 9);//Distance 

  double courseToORIGIN = TinyGPSPlus::courseTo(
      gps.location.lat(),
      gps.location.lng(),
      ORIGIN_LAT, 
      ORIGIN_LON);

  printFloat(courseToORIGIN, gps.location.isValid(), 7, 2);//Course 

  const char *cardinalToORIGIN = TinyGPSPlus::cardinal(courseToORIGIN);

  printStr(gps.location.isValid() ? cardinalToORIGIN : "*** ", 6);//Card

  printInt(gps.charsProcessed(), true, 6);    //Chars 
  printInt(gps.sentencesWithFix(), true, 10); //Sentences 
  printInt(gps.failedChecksum(), true, 9);    //Checksum
  Serial.println();// end of serial prints
  }
}

void ShowMonitorIMU(bool opcao){
  if(opcao){
    Serial.println("=======================================================");
    Serial.print("temp : ");Serial.println(mpu6050.getTemp());
    Serial.print("accX : ");Serial.print(mpu6050.getAccX());
    Serial.print("\taccY : ");Serial.print(mpu6050.getAccY());
    Serial.print("\taccZ : ");Serial.println(mpu6050.getAccZ());
    
    Serial.print("gyroX : ");Serial.print(mpu6050.getGyroX());
    Serial.print("\tgyroY : ");Serial.print(mpu6050.getGyroY());
    Serial.print("\tgyroZ : ");Serial.println(mpu6050.getGyroZ());
    
    Serial.print("accAngleX : ");Serial.print(mpu6050.getAccAngleX());
    Serial.print("\taccAngleY : ");Serial.println(mpu6050.getAccAngleY());
    
    Serial.print("gyroAngleX : ");Serial.print(mpu6050.getGyroAngleX());
    Serial.print("\tgyroAngleY : ");Serial.print(mpu6050.getGyroAngleY());
    Serial.print("\tgyroAngleZ : ");Serial.println(mpu6050.getGyroAngleZ());
      
    Serial.print("angleX : ");Serial.print(mpu6050.getAngleX());
    Serial.print("\tangleY : ");Serial.print(mpu6050.getAngleY());
    Serial.print("\tangleZ : ");Serial.println(mpu6050.getAngleZ());
    Serial.println("=======================================================\n");
  }
}

void NightTime(){
  if((gps.time.hour() + GMT) >= night || (gps.time.hour() + GMT) < morning){
    digitalWrite(Vext,HIGH);
    esp_sleep_enable_timer_wakeup(NIGHT_TIME_TO_SLEEP * uS_TO_S_FACTOR);
    sendStatusLCD(   "triggering night sleep mode");
    Serial.println("\ntriggering night sleep mode");
    delay(2000);
  }
}

String buildPackeger(String GPS){
  
  String GYRO = "\nIMU:\n";
  // IMU
  for (int i = 0; i < samplesNums; i++){
    mpu6050.update();
    GYRO += String(mpu6050.getTemp()) + " " +
    
            String(mpu6050.getAccX()) + " " +
            String(mpu6050.getAccY()) + " " +
            String(mpu6050.getAccZ()) + " " +

            String(mpu6050.getGyroX()) + " " +
            String(mpu6050.getGyroY()) + " " +
            String(mpu6050.getGyroZ()) + " " +
            
            String(mpu6050.getAccAngleX()) + " " +
            String(mpu6050.getAccAngleY()) + "\n" ;
            
    delay(500);
    
    }
  
            /*
            String(mpu6050.getTemp()) +
            String(mpu6050.getAccX()) +
            String(mpu6050.getAccY()) +
            String(mpu6050.getAccZ()) +

            String(mpu6050.getGyroX()) +
            String(mpu6050.getGyroY()) +
            String(mpu6050.getGyroZ()) +

            String(mpu6050.getAccAngleX()) +
            String(mpu6050.getAccAngleY()) +

            String(mpu6050.getGyroAngleX()) +
            String(mpu6050.getGyroAngleY()) +
            String(mpu6050.getGyroAngleZ()) +

            String(mpu6050.getAngleX()) +
            String(mpu6050.getAngleY()) +
            String(mpu6050.getAngleZ());
            //*/
            
  ShowMonitorIMU(monitordebug);
  return "ID: " + String(myID) + GPS + " " + GYRO;
}
#ifdef HELTEC_WIFI_LORA_32_V3
void getBatteryVoltage() {
  pinMode(VBatPin,INPUT);
  //digitalWrite(Vext,LOW);
  analogSetClockDiv(1);                 // Set the divider for the ADC clock, default is 1, range is 1 - 255
  analogSetAttenuation(ADC_11db);       // Sets the input attenuation for ALL ADC inputs, default is ADC_11db, range is ADC_0db, ADC_2_5db, ADC_6db, ADC_11db
  //  analogSetPinAttenuation(36,ADC_11db); // Sets the input attenuation, default is ADC_11db, range is ADC_0db, ADC_2_5db, ADC_6db, ADC_11db
  analogSetPinAttenuation(VBatPin, ADC_11db);
  analogReadResolution(12); // Configura o ADC para 12 bits (0-4095)
  //adcAttachPin(36);
  adcAttachPin(VBatPin);

  //float voltageBattery = ((analogRead(VBatPin)* 3.3) / 4095.0)* 2.0; // Multiplica por 2 (divisor 100k+100k
  voltage = ((analogRead(VBatPin)* 3.7) / 4095.0) * 1000; // Converte para mV

}
#endif
static void IRAM_ATTR interuptISR(){
  monitordebug = !monitordebug;
  return;
}

void setFlag(void) {
  // we sent or received a packet, set the flag
  operationDone = true;
}

void setupLora(){
  if(monitordebug)
  // initialize SX1276 with default settings
  Serial.print(F("Initializing LoRa... "));

  int state = radio.begin(915, //frequency in MHz
                          125, // bandwidth in kHz
                          7,   // spreading factor (SF) 7-12
                          5,   // coding rate (CR) 5-8
                          0x12,// sync word (default is 0x12 for LoRa)
                          10,  // preamble length in symbols (default is 10)
                          8,   // header length in bytes (default is 0 for variable length)
                          1.6f);// output power in dBm (default is 14 dBm)

  if (state == RADIOLIB_ERR_NONE) {
    if(monitordebug)
      Serial.println(F("success!"));
  } else {
    Serial.println(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // set the function that will be called
  // when new packet is received
  radio.setCRC(true);
  radio.setDio0Action(setFlag, RISING);

  radio.sleep();
}
