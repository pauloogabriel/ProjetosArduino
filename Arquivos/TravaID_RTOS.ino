#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 15 // D8
#define RST_PIN 2 // D4
MFRC522 mfrc522(SS_PIN, RST_PIN);

const int rele = 16;
const int LEDok = 5;
const int LEDnOK = 4;

String card_list[13] = {
  /* Coloque os códigos das TAGS aqui */
};
int total_card;
String card_num;

// Variáveis para controle de tempo assíncrono (Substitutos do delay)
unsigned long previousMillisLoop = 0;
const long intervalLoop = 200; // Intervalo equivalente ao antigo delay do loop

// Estados e temporizadores para animações dos LEDs (Tranca / Erro / Contador)
enum AnimationState { IDLE, COUNTER_BLINK, TRANCA_OK, TRANCA_NOK };
AnimationState currentState = IDLE;

unsigned long animPreviousMillis = 0;
int animStep = 0;
int animRepeats = 0;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println();

  pinMode(rele, OUTPUT);
  pinMode(LEDok, OUTPUT);
  pinMode(LEDnOK, OUTPUT);
  
  digitalWrite(rele, LOW);
  digitalWrite(LEDok, LOW);
  digitalWrite(LEDnOK, LOW);
}

void loop() {
  unsigned long currentMillis = millis();

  // Executa a verificação principal a cada 200ms sem travar o ESP8266
  if (currentMillis - previousMillisLoop >= intervalLoop) {
    previousMillisLoop = currentMillis;
    
    // Só processa o contador periódico se o sistema estiver ocioso
    if (currentState == IDLE) {
      static int contLED = 0;
      contLED++;
      if (contLED >= 250) { // ~50 segundos (250 * 200ms)
        contLED = 0;
        startCounterAnimation();
      }
    }

    // Leitura do RFID (só ocorre se não estiver executando uma animação crítica de LEDs)
    if (currentState == IDLE) {
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        card_num = getCardNumber();
        processCardRead();
      }
    }
  }

  // Máquina de estados não bloqueante para gerenciar LEDs e relé
  handleAnimations(currentMillis);
}

String getCardNumber() {
  String UID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    UID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    UID += String(mfrc522.uid.uidByte[i], HEX);
  }
  UID.toUpperCase();
  return UID;
}

void processCardRead() {
  boolean user_found = false;
  total_card = sizeof(card_list) / sizeof(card_list[0]);

  for (int i = 0; i < total_card; i++) {
    if (card_num.equals(card_list[i])) {
      user_found = true;
      break;
    }
  }

  if (user_found) {
    Serial.print("Card ID : ");
    Serial.println(card_num);
    Serial.println("Entrada liberada!");
    Serial.println("------------");
    startTrancaOkAnimation();
  } else {
    Serial.print("Card ID : ");
    Serial.println(card_num);
    Serial.println("Sem registro!");
    Serial.println("------------");
    startTrancaNokAnimation();
  }
}

// Funções para iniciar as animações
void startCounterAnimation() {
  currentState = COUNTER_BLINK;
  animStep = 0;
  animRepeats = 0;
  animPreviousMillis = millis();
}

void startTrancaOkAnimation() {
  currentState = TRANCA_OK;
  animStep = 0;
  animRepeats = 0;
  digitalWrite(rele, HIGH); // Aciona o relé imediatamente
  animPreviousMillis = millis();
}

void startTrancaNokAnimation() {
  currentState = TRANCA_NOK;
  animStep = 0;
  animRepeats = 0;
  animPreviousMillis = millis();
}

// Máquina de estados temporal (substitui os loops com delay dentro de tranca/contador)
void handleAnimations(unsigned long currentMillis) {
  if (currentState == IDLE) return;

  unsigned long elapsed = currentMillis - animPreviousMillis;

  switch (currentState) {
    case COUNTER_BLINK:
      // Alterna LEDok e LEDnOK por 6 ciclos
      if (animStep == 0 && elapsed >= 0) {
        digitalWrite(LEDok, HIGH);
        digitalWrite(LEDnOK, LOW);
        animStep = 1;
        animPreviousMillis = currentMillis;
      } else if (animStep == 1 && elapsed >= 100) {
        digitalWrite(LEDok, LOW);
        digitalWrite(LEDnOK, HIGH);
        animStep = 2;
        animPreviousMillis = currentMillis;
      } else if (animStep == 2 && elapsed >= 100) {
        digitalWrite(LEDnOK, LOW);
        animRepeats++;
        if (animRepeats >= 6) {
          currentState = IDLE;
        } else {
          animStep = 0;
        }
        animPreviousMillis = currentMillis;
      }
      break;

    case TRANCA_OK:
      // Sequência de liberação: Relé liga por 300ms, desliga, depois pisca LEDok 4 vezes
      if (animStep == 0 && elapsed >= 300) {
        digitalWrite(rele, LOW);
        digitalWrite(LEDok, LOW);
        animStep = 1;
        animPreviousMillis = currentMillis;
      } else if (animStep == 1 && elapsed >= 100) {
        digitalWrite(LEDok, HIGH);
        animStep = 2;
        animPreviousMillis = currentMillis;
      } else if (animStep == 2 && elapsed >= 300) {
        digitalWrite(LEDok, LOW);
        animStep = 3;
        animPreviousMillis = currentMillis;
      } else if (animStep == 3 && elapsed >= 100) {
        animRepeats++;
        if (animRepeats >= 4) {
          currentState = IDLE;
        } else {
          animStep = 1; // Repete o ciclo de piscar
        }
        animPreviousMillis = currentMillis;
      }
      break;

    case TRANCA_NOK:
      // Pisca LEDnOK 5 vezes (300ms ligado, 100ms desligado)
      if (animStep == 0 && elapsed >= 0) {
        digitalWrite(LEDnOK, HIGH);
        animStep = 1;
        animPreviousMillis = currentMillis;
      } else if (animStep == 1 && elapsed >= 300) {
        digitalWrite(LEDnOK, LOW);
        animStep = 2;
        animPreviousMillis = currentMillis;
      } else if (animStep == 2 && elapsed >= 100) {
        animRepeats++;
        if (animRepeats >= 5) {
          currentState = IDLE;
        } else {
          animStep = 0;
        }
        animPreviousMillis = currentMillis;
      }
      break;

    default:
      currentState = IDLE;
      break;
  }
}
