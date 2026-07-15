#include <SPI.h>
#include <TFT_eSPI.h>             // Biblioteca apenas para desenhar na tela
#include <XPT2046_Touchscreen.h>  // Biblioteca dedicada para o Touchscreen

// Definições dos pinos do LED RGB traseiro da CYD (Lógica Invertida: LOW liga, HIGH desliga)
#define LED_RED   4          
#define LED_GREEN 16         
#define LED_BLUE  17         

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

// Variáveis de controle de estado do LED (Começa desligado fisicamente no sistema, mas agora inicia acendendo o vermelho)
bool ledEstado = false;

// Coordenadas e tamanho do botão centralizado
int btnX, btnY, btnW, btnH;

void desenharBotao(bool estado) {
  if (estado) {
    // Botão Verde na tela quando LIGADO
    tft.fillRoundRect(btnX, btnY, btnW, btnH, 10, TFT_GREEN);
    tft.setTextColor(TFT_BLACK);
    tft.drawCentreString("LIGADO", btnX + (btnW / 2), btnY + (btnH / 2) - 10, 4);
  } else {
    // Botão Azul na tela quando DESLIGADO
    tft.fillRoundRect(btnX, btnY, btnW, btnH, 10, TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("DESLIGADO", btnX + (btnW / 2), btnY + (btnH / 2) - 10, 4);
  }
}

// Função auxiliar para definir com precisão a cor física do LED RGB
void definirCorLed(bool estado) {
  if (estado) {
    // LIGADO: Acende apenas o VERDE (LOW) e desliga o Vermelho e Azul (HIGH)
    digitalWrite(LED_GREEN, LOW); 
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_BLUE, HIGH);
  } else {
    // DESLIGADO: Acende apenas o VERMELHO (LOW) e desliga o Verde e Azul (HIGH)
    digitalWrite(LED_GREEN, HIGH); 
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_BLUE, HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("--- INICIANDO CYD ---");

  // Configura os pinos do LED RGB como saída
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  
  // Define o estado inicial do LED físico (Vermelho aceso no início porque ledEstado é false)
  definirCorLed(ledEstado);

  // Inicializa o Display
  tft.init();
  tft.setRotation(3); // Mantém em paisagem
  tft.fillScreen(TFT_BLACK);

  // Inicializa o barramento SPI alternativo do Touchscreen
  touchSPI.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
  ts.begin(touchSPI);
  ts.setRotation(3); 

  // Dimensões da tela
  int largura = tft.width();
  int altura = tft.height();

  // Configura tamanho do botão central
  btnW = 200;
  btnH = 60;
  btnX = (largura - btnW) / 2;
  btnY = (altura - btnH) / 2;

  // Título do painel
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("Controle do LED", largura / 2, 30, 4);

  // Desenha o botão inicial (Azul / Desligado)
  desenharBotao(ledEstado);
  
  Serial.println("Setup finalizado com sucesso!");
}

void loop() {
  // 1. Verifica se há toque físico ativo
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    // Filtro de coordenadas e ruído de pressão
    if (p.x > 100 && p.x < 3900 && p.y > 100 && p.y < 3900 && p.z > 300) {
      
      // Traduz as coordenadas analógicas (raw) para pixels da tela (320x240)
      int x_touch = map(p.x, 150, 3850, 0, 320);
      int y_touch = map(p.y, 150, 3850, 0, 240);

      // Corrige a rotação e espelhamento (Modo 3)
      x_touch = 320 - x_touch;
      y_touch = 240 - y_touch;

      // 2. Verifica se o toque ocorreu em qualquer parte do botão (fundo ou texto)
      if (x_touch > btnX && x_touch < (btnX + btnW) && y_touch > btnY && y_touch < (btnY + btnH)) {
        
        ledEstado = !ledEstado;
        
        // Altera a cor do LED RGB traseiro fisicamente de acordo com o novo estado
        definirCorLed(ledEstado);
        
        // Atualiza o botão desenhado na tela
        desenharBotao(ledEstado);
        
        Serial.printf("Acao registrada! LED fisico alterado para %s\n", ledEstado ? "VERDE" : "VERMELHO");

        // --- TRAVA DE DEBOUNCE ---
        // Aguarda você retirar o dedo totalmente de qualquer parte do painel antes de permitir outro clique
        while (ts.touched()) {
          delay(10); 
        }
        delay(150); 
      }
    }
  }
}