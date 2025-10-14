#include <RadioLib.h>
#include <HT_SSD1306Wire.h>

// SX1262 has the following connections:
// NSS pin:   10
// DIO1 pin:  2
// NRST pin:  3
// BUSY pin:  9
SX1262 radio = new Module(8, 14, 12, 13);
SSD1306Wire  factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst

#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

WiFiClient espClient;
PubSubClient client(espClient);

volatile bool dispOn = true;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando ao WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  // Loop até conectar
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT...");
    if (client.connect("LoRaReceiver", mqtt_user, mqtt_pass)) {
      Serial.println("conectado!");
    } else {
      Serial.print("falhou, rc=");
      Serial.println(client.state());
      Serial.println(WiFi.localIP());
      Serial.println(WiFi.status());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

static void IRAM_ATTR interuptISR(){
  dispOn = !dispOn; // Alterna o estado do display
  Serial.print("Display is now ");
  Serial.println(dispOn);
}

// flag to indicate that a packet was received
volatile bool receivedFlag = false;

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  // we got a packet, set the flag
  receivedFlag = true;
}

void setup() {
  
  Serial.begin(115200);

  attachInterrupt(GPIO_NUM_0, interuptISR ,GPIO_INTR_POSEDGE);
  pinMode(GPIO_NUM_0, INPUT_PULLUP);

  pinMode(Vext,OUTPUT);
  digitalWrite(Vext,LOW);

  // initialize SX1262 with default settings
  Serial.print(F("[SX1262] Initializing ... "));
  int state = radio.begin(915, 125, 7, 5, 0x12, 10, 8, 1.6f);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }
  radio.setCRC(true); // enable CRC checking

  // set the function that will be called
  // when new packet is received
  radio.setPacketReceivedAction(setFlag);

  // start listening for LoRa packets
  factory_display.init();
  factory_display.fillRect(0, 0, factory_display.getWidth(), factory_display.getHeight());
  factory_display.setBrightness(255);
  factory_display.setTextAlignment(TEXT_ALIGN_LEFT);
  factory_display.setFont(ArialMT_Plain_10);
  //factory_display.displayOn();
  factory_display.clear();
  factory_display.drawString(0, 0, "LoRa Receiver");
  factory_display.display();

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  factory_display.drawString(0, 10, "Listening ...");
  factory_display.display();

  Serial.print(F("[SX1262] Starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }
}

void loop() {

  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  // check if the flag is set
  if(receivedFlag) {
    // reset flag
    receivedFlag = false;

    // you can read received data as an Arduino String
    String str;
    int state = radio.readData(str);

    if (state == RADIOLIB_ERR_NONE) {
      String id, gps, imu;
      //pegar tamanho do pacote
      int packetSize = radio.getPacketLength();
      // Extrai ID
      int id_start = str.indexOf("ID:");
      if (id_start != -1) {
        int id_end = str.indexOf('\n', id_start);
        id = str.substring(id_start + 3, id_end);
        id.trim();
      }
  
      // Extrai GPS
      int gps_start = str.indexOf("GPS:");
      if (gps_start != -1) {
        int gps_end = str.indexOf('\n', gps_start);
        gps = str.substring(gps_start + 4, gps_end);
        gps.trim();
      }
  
      // Extrai IMU (tudo após "IMU:")
      int imu_start = str.indexOf("IMU:");
      if (imu_start != -1) {
        imu = str.substring(imu_start + 4);
        imu.trim();
        imu.replace("\r", "");     // Remove carriage return, se houver
        imu.replace("\n", "/n");   // Escapa as quebras de linha para JSON
      }

      // Monta mensagem completa (exemplo em JSON)
      String payload = "{";
      payload += "\"id\":\"" + id + "\",";
      payload += "\"gps\":\"" + gps + "\",";
      payload += "\"imu\":\"" + imu + "\"";
      payload += "}";
  
      // Monta o tópico dinâmico
      String mqtt_topic_id = id;
  
      // Envia para o MQTT no tópico do ID
      if (client.connected()) {
        client.publish(mqtt_topic_id.c_str(), payload.c_str(), true); // retain = true
      } else {
        Serial.println("MQTT não conectado!");
      }
      if(dispOn){
        // print the packetgelength
        Serial.print(F("\n[SX1262] Packet length:\t"));
        Serial.println(packetSize);
        // print RSSI (Received Signal Strength Indicator)
        Serial.print(F("[SX1262] RSSI:\t"));
        Serial.print(radio.getRSSI());
        Serial.println(F(" dBm"));

        // print SNR (Signal-to-Noise Ratio)
        Serial.print(F("[SX1262] SNR:\t"));
        Serial.print(radio.getSNR());
        Serial.println(F(" dB"));

        // print frequency error
        Serial.print(F("[SX1262] Frequency error: "));
        Serial.print(radio.getFrequencyError());
        Serial.println(F(" Hz"));

        Serial.print("Enviado ao MQTT no tópico: ");
        Serial.println(mqtt_topic_id);
        Serial.print("Payload: ");
        Serial.println(payload);
      
        factory_display.displayOn();
        factory_display.clear();
        factory_display.drawString(0, 0, "LoRa Receiver");
        factory_display.drawString(0, 10, String(id+" received!"));
        factory_display.display();
      }

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      // packet was received, but is malformed
      Serial.println(F("CRC error!"));

    } else {
      // some other error occurred
      Serial.print(F("failed, code "));
      Serial.println(state);
    }
  }
  
  if(dispOn){
    factory_display.displayOn();
    factory_display.clear();
    factory_display.drawString(0, 0, "LoRa Receiver");
  }else{
    factory_display.clear();
    factory_display.displayOff();
  }
  delay(10);
}