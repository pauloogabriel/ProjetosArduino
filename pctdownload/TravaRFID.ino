#include <SPI.h>
#include <MFRC522.h>
#define SS_PIN 15 //D8
#define RST_PIN 2 //D4
MFRC522 mfrc522(SS_PIN, RST_PIN);


//Código feito utilizando a placa ESP8266


int rele = 16;
int LEDok = 5;
int LEDnOK = 4;
int contLED = 0;

String card_list[13] = {/*Colocar códigos das TAGS aqui*/};
int total_card;
String card_num;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println();

  pinMode(rele, OUTPUT);
  pinMode(LEDok, OUTPUT);
  pinMode(LEDnOK, OUTPUT);
}


void loop() {

  contLED++;
  delay(200);
  contador();

  if ( ! mfrc522.PICC_IsNewCardPresent() || ! mfrc522.PICC_ReadCardSerial()){
    return;
  }
  card_num = getCardNumber();
  showData();

}

String getCardNumber(){
  String UID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    UID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    UID += String(mfrc522.uid.uidByte[i], HEX);
  }
  UID.toUpperCase();
  return UID;


}

void contador(){
  if(contLED == 250){
    for(int i = 0; i <= 5; i++){
      digitalWrite(LEDok, HIGH);
      delay(100);
      digitalWrite(LEDok, LOW);
      digitalWrite(LEDnOK, HIGH);
      delay(100);
      digitalWrite(LEDnOK, LOW);
    }
    contLED = 0;
  }
}

void showData(){
  boolean user_found = false;
  total_card = sizeof(card_list)/sizeof(card_list[0]);
  
  for(int i=0;i<total_card;i++){
    String check_num = card_list[i];
    if(card_num.equals(check_num)){
      user_found = true;

      Serial.print("Card ID : ");
      Serial.println(card_num);
      Serial.println("Entrada liberada!");
      Serial.println("------------");

      tranca();
    }  
  }
  if(user_found == false){
    Serial.print("Card ID : ");
    Serial.println(card_num);
    Serial.println("Sem registro!");
    Serial.println("------------");

    trancaNok();
  }
  delay(500);
}


void tranca(){
  digitalWrite(rele, HIGH);
  digitalWrite(LEDok, HIGH);
  delay(300);
  digitalWrite(rele, LOW);
  digitalWrite(LEDok, LOW);
  delay(100);
  
  for(int i = 1; i <=4; i++){
    digitalWrite(LEDok, HIGH);
    delay(300);
    digitalWrite(LEDok, LOW);
    delay(100);
  }
}

void trancaNok(){
  for(int i = 1; i <=5; i++){
    digitalWrite(LEDnOK, HIGH);
    delay(300);
    digitalWrite(LEDnOK, LOW);
    delay(100);
  }
}