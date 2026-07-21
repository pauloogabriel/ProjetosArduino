#include <SPI.h>
#include <TFT_eSPI.h>             // Biblioteca apenas para desenhar na tela
#include <XPT2046_Touchscreen.h>  // Biblioteca dedicada para o Touchscreen

// Definições dos pinos do LED RGB traseiro da CYD (Lógica Invertida: LOW liga, HIGH desliga)
#define LED_RED   4          
#define LED_GREEN 16         
#define LED_BLUE  17         

// Pino do Backlight da tela (Permite controle de brilho via PWM)
#define SCREEN_BL_PIN 21

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

// Variáveis de controle de estado do LED (Começa desligado/vermelho)
bool ledEstado = false;

// --- VARIÁVEIS DO TEMPORIZADOR DE TELA ---
unsigned long ultimoToqueTempo = 0; // Guarda quando o último toque ocorreu
const unsigned long TEMPO_ATIVO = 30000; // 30 segundos totalmente aceso
const unsigned long TEMPO_FADE   = 5000;  // 5 segundos diminuindo o brilho (Total 35s)

// Coordenadas e tamanho do botão centralizado (Adaptados para o modo Retrato)
int btnX, btnY, btnW, btnH;

// Desenha o botão na tela
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

// Controla as cores do LED RGB traseiro
void definirCorLed(bool estado) {
  if (estado) {
    digitalWrite(LED_GREEN, LOW); 
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_BLUE, HIGH);
  } else {
    digitalWrite(LED_GREEN, HIGH); 
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_BLUE, HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("--- CYD ROTACAO 0 (RETRATO) ---");

  // Configura o pino de backlight para aceitar controle de brilho (PWM)
  pinMode(SCREEN_BL_PIN, OUTPUT);
  analogWrite(SCREEN_BL_PIN, 255); // Inicializa com brilho máximo

  // Configura os pinos do LED RGB como saída
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  
  // Define o estado inicial do LED físico (Vermelho ativo no início)
  definirCorLed(ledEstado);

  // Inicializa o Display
  tft.init();
  tft.setRotation(0); // <-- Alterado para Modo Retrato (240x320)
  tft.fillScreen(TFT_BLACK);

  // Inicializa o barramento SPI alternativo do Touchscreen
  touchSPI.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
  ts.begin(touchSPI);
  ts.setRotation(0); // <-- Alinha a rotação do sensor com a tela

  // Dimensões da tela em modo Retrato (240x320)
  int largura = tft.width();   // Retorna 240
  int altura = tft.height();   // Retorna 320

  // Configura tamanho e centralização do botão para caber perfeitamente em 240 de largura
  btnW = 180; // Largura do botão ligeiramente menor
  btnH = 60;
  btnX = (largura - btnW) / 2; // (240 - 180) / 2 = 30px de margem nas laterais
  btnY = (altura - btnH) / 2;  // Centralizado verticalmente

  // Título do painel ajustado para o topo da tela vertical
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("Controle do LED", largura / 2, 40, 4);

  // Desenha o botão inicial (Azul / Desligado)
  desenharBotao(ledEstado);
  
  ultimoToqueTempo = millis();

  Serial.println("Setup finalizado com sucesso em Modo Retrato!");
}

void loop() {
  unsigned long tempoInativo = millis() - ultimoToqueTempo;

  // --- CONTROLE DE BRILHO INTELIGENTE ---
  if (tempoInativo < TEMPO_ATIVO) {
    analogWrite(SCREEN_BL_PIN, 255);
  } 
  else if (tempoInativo >= TEMPO_ATIVO && tempoInativo < (TEMPO_ATIVO + TEMPO_FADE)) {
    int brilhoAtual = map(tempoInativo, TEMPO_ATIVO, TEMPO_ATIVO + TEMPO_FADE, 255, 0);
    analogWrite(SCREEN_BL_PIN, brilhoAtual);
  } 
  else {
    analogWrite(SCREEN_BL_PIN, 0);
  }

  // --- DETECÇÃO DO TOQUE ---
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    // Filtro contra ruídos físicos
    if (p.x > 100 && p.x < 3900 && p.y > 100 && p.y < 3900 && p.z > 300) {

      // Se a tela estava apagada, o primeiro toque apenas acorda o display
      if (tempoInativo >= TEMPO_ATIVO) {
        ultimoToqueTempo = millis(); 
        analogWrite(SCREEN_BL_PIN, 255); 
        Serial.println("Display acordado!");
        while (ts.touched()) { delay(10); }
        delay(150);
        return; 
      }

      // Traduz coordenadas para pixels no layout 240x320 (Retrato)
      // Ajustado limites do painel resistivo para se alinhar à nova orientação
      int x_touch = map(p.x, 200, 3800, 0, 240);
      int y_touch = map(p.y, 250, 3750, 0, 320);

      // No modo setRotation(0), a biblioteca costuma espelhar o eixo X.
      // Caso o seu toque fique invertido horizontalmente, remova a barra de comentário da linha abaixo:
      // x_touch = 240 - x_touch;

      // Verificação de toque na área do botão único
      if (x_touch > btnX && x_touch < (btnX + btnW) && y_touch > btnY && y_touch < (btnY + btnH)) {
        
        ultimoToqueTempo = millis(); 

        ledEstado = !ledEstado;
        definirCorLed(ledEstado);
        desenharBotao(ledEstado);
        
        Serial.printf("Botao pressionado! LED: %s\n", ledEstado ? "LIGADO" : "DESLIGADO");

        // Debounce (Trava de segurança)
        while (ts.touched()) { delay(10); }
        delay(150); 
      }
    }
  }
}