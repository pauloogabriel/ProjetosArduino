#include <SPI.h>
#include <TFT_eSPI.h>             // Biblioteca apenas para desenhar na tela
#include <XPT2046_Touchscreen.h>  // Biblioteca dedicada para o Touchscreen

// Definições dos pinos do LED RGB traseiro da CYD (Lógica Invertida: LOW liga, HIGH desliga)
#define LED_RED   4          
#define LED_GREEN 16         
#define LED_BLUE  17         

// PINO DO BACKLIGHT DA TELA (Iluminação de fundo da CYD)
#define SCREEN_BL_PIN 21

// NOVO PINO PARA O LED EXTERNO (Mudado do 21 para o 22 para não apagar a tela)
#define LED_EXTERNO_PIN 22

// PINOS EXCLUSIVOS DO TOQUE NA PLACA CYD
#define TOUCH_MISO_PIN  39  // DOUT
#define TOUCH_MOSI_PIN  32  // DIN
#define TOUCH_CLK_PIN   25  // DCLK
#define TOUCH_CS_PIN    33  // CS

// Inicializa um barramento SPI exclusivo para o Touch
SPIClass touchSPI = SPIClass(VSPI);

// Inicialização dos controladores
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS_PIN); 

// Variável de controle de estado global (ambos os LEDs sincronizados)
bool ledEstado = false;

// Coordenadas e tamanho do botão único centralizado
int btnX, btnY, btnW, btnH;

// Função para desenhar o botão central conforme o estado global
void desenharBotao(bool estado) {
  if (estado) {
    tft.fillRoundRect(btnX, btnY, btnW, btnH, 10, TFT_GREEN);
    tft.setTextColor(TFT_BLACK);
    tft.drawCentreString("LIGADO", btnX + (btnW / 2), btnY + (btnH / 2) - 10, 4);
  } else {
    tft.fillRoundRect(btnX, btnY, btnW, btnH, 10, TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("DESLIGADO", btnX + (btnW / 2), btnY + (btnH / 2) - 10, 4);
  }
}

// Função auxiliar para atualizar o estado físico de ambos os LEDs
void atualizarHardwareLEDs(bool estado) {
  if (estado) {
    // 1. LED Interno: Acende o VERDE (LOW) e desliga Vermelho/Azul (HIGH)
    digitalWrite(LED_GREEN, LOW); 
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    
    // 2. LED Externo (Agora no IO22): HIGH liga
    digitalWrite(LED_EXTERNO_PIN, HIGH);
  } else {
    // 1. LED Interno: Acende o VERMELHO (LOW) e desliga Verde/Azul (HIGH)
    digitalWrite(LED_GREEN, HIGH); 
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_BLUE, HIGH);
    
    // 2. LED Externo (Agora no IO22): LOW desliga
    digitalWrite(LED_EXTERNO_PIN, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("--- INICIANDO CYD (FIX BACKLIGHT) ---");

  // --- CORREÇÃO DA TELA ---
  // Configura o pino 21 (Backlight) como saída e força ele a ficar LIGADO (HIGH) para sempre
  pinMode(SCREEN_BL_PIN, OUTPUT);
  digitalWrite(SCREEN_BL_PIN, HIGH);

  // Configura os pinos dos LEDs como saída
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_EXTERNO_PIN, OUTPUT);
  
  // Aplica o estado inicial aos LEDs físicos (Sem afetar a tela)
  atualizarHardwareLEDs(ledEstado);

  // Inicializa o Display
  tft.init();
  tft.setRotation(3); 
  tft.fillScreen(TFT_BLACK);

  // Inicializa o barramento SPI alternativo do Touchscreen
  touchSPI.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
  ts.begin(touchSPI);
  ts.setRotation(3); 

  // Dimensões da tela
  int largura = tft.width();
  int altura = tft.height();

  // Configura tamanho e centralização do botão
  btnW = 200;
  btnH = 60;
  btnX = (largura - btnW) / 2;
  btnY = (altura - btnH) / 2;

  // Título do painel
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("Controle do LED", largura / 2, 30, 4);

  // Desenha o botão inicial (Azul / Desligado)
  desenharBotao(ledEstado);
  
  Serial.println("Setup finalizado. Tela travada em modo ACESO.");
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    if (p.x > 100 && p.x < 3900 && p.y > 100 && p.y < 3900 && p.z > 300) {
      
      int x_touch = map(p.x, 150, 3850, 0, 320);
      int y_touch = map(p.y, 150, 3850, 0, 240);

      x_touch = 320 - x_touch;
      y_touch = 240 - y_touch;

      if (x_touch > btnX && x_touch < (btnX + btnW) && y_touch > btnY && y_touch < (btnY + btnH)) {
        ledEstado = !ledEstado;
        
        // Atualiza os LEDs (o pino 21 não é mais tocado aqui, logo a tela não pisca)
        atualizarHardwareLEDs(ledEstado);
        
        desenharBotao(ledEstado);
        
        Serial.printf("LEDs alterados para o estado: %s\n", ledEstado ? "LIGADO" : "DESLIGADO");

        while (ts.touched()) {
          delay(10); 
        }
        delay(150); 
      }
    }
  }
}