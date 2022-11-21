#include <SoftwareSerial.h>
#include <WiFi.h>          //biblioteca WiFi da ESP32
#include <PubSubClient.h>  //biblioteca para comunicação MQTT

SoftwareSerial NFCserial(16, 17);  //RX, TX

#define LED_Verde 15
#define LED_Vermelho 2
#define buzzer 4

uint8_t received_buf_pos = 0;
uint8_t response_byte;
uint8_t data_len;
boolean received_complete = false;
String tag_id = "";
char Byte_In_Hex[3];


uint8_t echo_command[1] = { 0x55 };

uint8_t initialise_cmd_iso14443_1[6] = { 0x09, 0x04, 0x68, 0x01, 0x07, 0x10 };
uint8_t initialise_cmd_iso14443_2[6] = { 0x09, 0x04, 0x68, 0x01, 0x07, 0x00 };
uint8_t initialise_cmd_iso14443_3[6] = { 0x02, 0x04, 0x02, 0x00, 0x02, 0x80 };
uint8_t initialise_cmd_iso14443_4[6] = { 0x09, 0x04, 0x3A, 0x00, 0x58, 0x04 };
uint8_t initialise_cmd_iso14443_5[6] = { 0x09, 0x04, 0x68, 0x01, 0x01, 0xD3 };

uint8_t detect_cmd_iso14443_1[4] = { 0x04, 0x02, 0x26, 0x07 };
uint8_t detect_cmd_iso14443_2[5] = { 0x04, 0x03, 0x93, 0x20, 0x08 };

struct tag {
  String Card;
  int Access;
};
tag Tags[3] = {
  { "c975f64e", 0 },  //Nubank
  { "51fc3a64", 1 },
};  //Cartão RFID

int Tag_Status;

const char* ssid = "LG House";               //nome da rede Wi-Fi
const char* password = "LG19931995";                      //senha da rede Wi-Fi
const char* mqtt_server = "broker.hivemq.com";  //BROKER MQTT


WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];


void setup_wifi() {  //função para a conexão WiFi

  delay(10);

  Serial.println();
  Serial.print("Conectando à rede: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {  //função de reconexão
  // Loop até se reconectar
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Cria um ID randomico
    String clientId = "ENGEASIER_MQTT";
    clientId += String(random(0xffff), HEX);
    // Tenta se reconectar ao broker
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("Tentando novamente em 5 segundos...");
      // Espera 5 segundos até tentar novamente
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  NFCserial.begin(57600);

  pinMode(LED_Verde, OUTPUT);
  pinMode(LED_Vermelho, OUTPUT);
  pinMode(buzzer, OUTPUT);

  setup_wifi();                         //chama a função de conexão Wi-Fi
  client.setServer(mqtt_server, 1883);  //conecta ao servideo e porta MQTT


  Serial.println("Echo command: ");
  NFCserial.write(echo_command, 1);
  delay(1000);
  show_serial_data();
  delay(1000);
  Serial.println("Initialise commands: ");
  NFCserial.write(initialise_cmd_iso14443_1, 6);
  delay(1000);
  show_serial_data();
  NFCserial.write(initialise_cmd_iso14443_2, 6);
  delay(1000);
  show_serial_data();
  NFCserial.write(initialise_cmd_iso14443_3, 6);
  delay(1000);
  show_serial_data();
  NFCserial.write(initialise_cmd_iso14443_4, 6);
  delay(1000);
  show_serial_data();
  NFCserial.write(initialise_cmd_iso14443_5, 6);
  delay(1000);
  show_serial_data();
  tone(buzzer, 262, 500);
}

void show_serial_data() {
  while (NFCserial.available() != 0)
    Serial.print(NFCserial.read(), HEX);
  Serial.println("");
}

void Read_Tag() {
  uint8_t received_char;
  while (NFCserial.available() != 0) {
    received_char = char(NFCserial.read());

    if (received_buf_pos == 0) response_byte = received_char;
    else if (received_buf_pos == 1) data_len = received_char;
    else if (received_buf_pos >= 2 and received_buf_pos < 6) {
      sprintf(Byte_In_Hex, "%x", received_char);
      tag_id += Byte_In_Hex;  //adding to a string
    }
    received_buf_pos++;
    if (received_buf_pos >= data_len) {
      received_complete = true;
    }
  }
}

void loop() {
  received_buf_pos = 0;
  received_complete = false;
  tag_id = "";
  response_byte = 0;

  //Serial.println("Searching new card...");
  NFCserial.write(detect_cmd_iso14443_1, 4);
  delay(800);
  //Serial.println("Response:");
  show_serial_data();

  NFCserial.write(detect_cmd_iso14443_2, 5);
  delay(300);

  if (NFCserial.available()) {
    Read_Tag();
  }

  if (response_byte == 0x80) {

    Tag_Status = 2;
    for (int i = 0; i < sizeof(Tags) / sizeof(tag); i++) {
      if (Tags[i].Card == tag_id) Tag_Status = Tags[i].Access;
    }
    if (Tag_Status == 1) {
      Serial.println("Acesso Liberado");
      digitalWrite(LED_Verde, HIGH);
      tone(buzzer, 392, 400);
      snprintf(msg, MSG_BUFFER_SIZE, "Acesso Liberado");  //concatena texto a ser enviado
      client.publish("engeasier/rfid", msg);              //publica texto no tópico
    }
    if (Tag_Status == 0) {
      Serial.println("Acesso Negado");
      digitalWrite(LED_Vermelho, HIGH);
      tone(buzzer, 262, 400);
      snprintf(msg, MSG_BUFFER_SIZE, "Acesso Negado");  //concatena texto a ser enviado
      client.publish("engeasier/rfid", msg);            //publica texto no tópico
    }

    if (Tag_Status == 2) {
      Serial.println("Nao reconhecido");
      tone(buzzer, 440, 600);
      delay(500);
      tone(buzzer, 440, 600);
      snprintf(msg, MSG_BUFFER_SIZE, "Nao Reconhecido");  //concatena texto a ser enviado
      client.publish("engeasier/rfid", msg);              //publica texto no tópico
    }
    delay(2000);
    digitalWrite(LED_Verde, LOW);
    digitalWrite(LED_Vermelho, LOW);
  }

   if (!client.connected()) { //se não está conectado, tenta reconectar
    reconnect();
  }
  client.loop(); 
}