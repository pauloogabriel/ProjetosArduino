#include <SPI.h>
#include <MFRC522.h>

// Pinos recomendados para SPI e controle no ESP32-C3 Super Mini
// (Ajuste conforme a sua pinagem física exata)
#define SS_PIN    7   // GPIO 7 (CS / SS)
#define RST_PIN   2   // GPIO 2 (RST)
#define SCK_PIN   4   // GPIO 4 (SCK)
#define MISO_PIN  5   // GPIO 5 (MISO)
#define MOSI_PIN  6   // GPIO 6 (MOSI)

#define RELE_PIN  10  // GPIO 10
#define LED_OK    8   // GPIO 8
#define LED_NOK   9   // GPIO 9

MFRC522 mfrc522(SS_PIN, RST_PIN);

// Lista de cartões autorizados
String card_list[13] = {
  /* Coloque os códigos das TAGS aqui */
};

// Estrutura para enviar o resultado da leitura do RFID via Fila do FreeRTOS
struct CardMessage {
  bool authorized;
  char uid[17];
};

// Handles para as Tarefas e Fila do FreeRTOS
QueueHandle_t cardQueue;
TaskHandle_t TaskRFIDHandle;
TaskHandle_t TaskControlHandle;

// Protótipos das Tarefas
void vTaskRFID(void *pvParameters);
void vTaskControl(void *pvParameters);
String getCardNumber();

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inicializa o barramento SPI com os pinos definidos para o ESP32-C3
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  mfrc522.PCD_Init();

  pinMode(RELE_PIN, OUTPUT);
  pinMode(LED_OK, OUTPUT);
  pinMode(LED_NOK, OUTPUT);

  digitalWrite(RELE_PIN, LOW);
  digitalWrite(LED_OK, LOW);
  digitalWrite(LED_NOK, LOW);

  // Cria uma fila para até 5 mensagens de cartões pendentes
  cardQueue = xQueueCreate(5, sizeof(CardMessage));

  if (cardQueue == NULL) {
    Serial.println("Erro ao criar a fila do FreeRTOS!");
  }

  // Criação das Tarefas do FreeRTOS
  // Task 1: Lê o leitor RFID (prioridade média)
  xTaskCreate(
    vTaskRFID,          // Função da tarefa
    "Task_RFID",        // Nome descritivo
    4096,               // Tamanho da pilha (Stack em words)
    NULL,               // Parâmetro de entrada
    2,                  // Prioridade (maior que a de controle)
    &TaskRFIDHandle     // Handle da tarefa
  );

  // Task 2: Controla LEDs, relés, contadores e animações (prioridade baixa)
  xTaskCreate(
    vTaskControl,
    "Task_Control",
    4096,
    NULL,
    1,
    &TaskControlHandle
  );

  Serial.println("Sistema iniciado com FreeRTOS no ESP32-C3!");
}

void loop() {
  // No ESP32 com FreeRTOS, o loop() principal fica vazio ou pode ser deletado.
  // O FreeRTOS gerencia tudo através das Tasks criadas no setup().
  vTaskDelete(NULL); 
}

// -----------------------------------------------------------------
// TAREFA 1: Monitora o leitor RFID de forma contínua
// -----------------------------------------------------------------
void vTaskRFID(void *pvParameters) {
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(100); // Verifica a cada 100ms
  xLastWakeTime = xTaskGetTickCount();

  while (1) {
    // Verifica se há nova tag presente
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      String card_num = getCardNumber();
      
      bool authorized = false;
      int total_card = sizeof(card_list) / sizeof(card_list[0]);
      
      for (int i = 0; i < total_card; i++) {
        if (card_num.equals(card_list[i])) {
          authorized = true;
          break;
        }
      }

      // Preenche a estrutura para enviar via Fila
      CardMessage msg;
      msg.authorized = authorized;
      card_num.toCharArray(msg.uid, sizeof(msg.uid));

      // Envia a mensagem para a Fila (espera até 50ms se a fila estiver cheia)
      xQueueSend(cardQueue, &msg, pdMS_TO_TICKS(50));
      
      // Pequeno delay para evitar leituras duplas instantâneas da mesma tag
      vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Mantém a tarefa rodando de forma cadenciada sem sobrecarregar a CPU
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// -----------------------------------------------------------------
// TAREFA 2: Controla relés, animações de LED e o contador periódico
// -----------------------------------------------------------------
void vTaskControl(void *pvParameters) {
  CardMessage receivedMsg;
  unsigned long counterTicks = 0;

  while (1) {
    // 1. Verifica se chegou algum evento de cartão na Fila (aguarda 100ms por ciclo)
    if (xQueueReceive(cardQueue, &receivedMsg, pdMS_TO_TICKS(100)) == pdPASS) {
      if (receivedMsg.authorized) {
        Serial.print("Card ID : ");
        Serial.println(receivedMsg.uid);
        Serial.println("Entrada liberada!");
        Serial.println("------------");

        // Sequência de liberação (com vTaskDelay para não travar o sistema)
        digitalWrite(RELE_PIN, HIGH);
        digitalWrite(LED_OK, HIGH);
        vTaskDelay(pdMS_TO_TICKS(300));
        
        digitalWrite(RELE_PIN, LOW);
        digitalWrite(LED_OK, LOW);
        vTaskDelay(pdMS_TO_TICKS(100));

        for (int i = 1; i <= 4; i++) {
          digitalWrite(LED_OK, HIGH);
          vTaskDelay(pdMS_TO_TICKS(300));
          digitalWrite(LED_OK, LOW);
          vTaskDelay(pdMS_TO_TICKS(100));
        }
      } else {
        Serial.print("Card ID : ");
        Serial.print(receivedMsg.uid);
        Serial.println("Sem registro!");
        Serial.println("------------");

        // Sequência de acesso negado
        for (int i = 1; i <= 5; i++) {
          digitalWrite(LED_NOK, HIGH);
          vTaskDelay(pdMS_TO_TICKS(300));
          digitalWrite(LED_NOK, LOW);
          vTaskDelay(pdMS_TO_TICKS(100));
        }
      }
    } else {
      // 2. Se nenhum cartão foi lido, gerencia o contador periódico (~50 segundos)
      counterTicks++;
      if (counterTicks >= 500) { // 500 ciclos * 100ms = 50 segundos
        counterTicks = 0;
        
        // Pisca os LEDs alternadamente
        for (int i = 0; i < 6; i++) {
          digitalWrite(LED_OK, HIGH);
          digitalWrite(LED_NOK, LOW);
          vTaskDelay(pdMS_TO_TICKS(100));
          digitalWrite(LED_OK, LOW);
          digitalWrite(LED_NOK, HIGH);
          vTaskDelay(pdMS_TO_TICKS(100));
        }
        digitalWrite(LED_NOK, LOW);
      }
    }
  }
}

// Função auxiliar para extrair o UID do cartão
String getCardNumber() {
  String UID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    UID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    UID += String(mfrc522.uid.uidByte[i], HEX);
  }
  UID.toUpperCase();
  return UID;
}
