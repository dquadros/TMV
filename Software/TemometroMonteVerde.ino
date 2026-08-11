/*
  "Termômetro Monte Verde"
  Termômetro construído com
  - placa ESP32-C6-LCD-1.47 da Waveshare
  - sensor de temperatura HDC1080
  - cinco LEDs RGB (WS2812B)

  (C) 2026, Daniel Quadros
*/

// SNTP - Obtenção da hora - Não usado neste teste

#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include "secrets.h"
#include "SNTP.h"

WiFiUDP wifiudp;
SNTP sntp;

// Display

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>

#define TFT_CS        14
#define TFT_RST       21
#define TFT_DC        15
#define TFT_MOSI      6
#define TFT_SCLK      7
#define TFT_BL        22

#define cor565(r,g,b) ((r<<11)|(g<<5)|b)
#define FUNDO_TELA    cor565(7,31,7)
#define FUNDO_TERM    cor565(27,54,27)
#define FUNDO_TEXTO   cor565(3,25,3)
#define COR_SOMBRA    cor565(27,54,27)

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// LEDs RGB

#include <FastLED.h>

#define NUM_LEDS 5
#define RGB_PIN 5
CRGB ledsRGB[NUM_LEDS];
#define LED_JANELA_BAIXO 0
#define LED_JANELA_CIMA 1
#define LED_SOL1 2
#define LED_SOL2 3
#define LED_SOL3 4

#define BUILTIN_RGB_PIN 8
CRGB builtinLED[1];

// Sensor de Temperatura

#include <Wire.h>  
  
// Endereco I2C do HDC1080
#define ADDR      0x40  

// Ligação do HDC1080
#define PIN_SDA 3
#define PIN_SCL 4
  
// Registradores do HDC1080
#define REG_TEMP  0  
#define REG_HUM   1  
#define REG_CONF  2  
#define REG_MANID 0xFE
#define REG_DEVID 0xFF

// Botão BOOT
#define BOOT_PIN 9
int demoMode;

//--------------------------------------------------
// Iniciação
//--------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Inica os LEDs
  FastLED.addLeds<WS2812, BUILTIN_RGB_PIN, RGB>(builtinLED, 1);
  builtinLED[0] = CRGB::Red;   // Indica que pode apertar BOOT para ativar demo
  FastLED.addLeds<WS2812, RGB_PIN, GRB>(ledsRGB, NUM_LEDS);
  for (int i = 0; i < NUM_LEDS; i++) {
    ledsRGB[i] = CRGB::Black; 
  }
  FastLED.show();

  // Inicia sensor de temperatura
  initHDC1080();

  // Inicia a tela
  initScreen();

  // Inicia SNTP
  //Serial.println("Conectando");
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
  }
  //Serial.println("");
  //Serial.print("Connectado, IP = ");
  //Serial.println(WiFi.localIP());
  sntp.init (&wifiudp, -10800);

  // Verifica se deve rodar no modo demonstração
  delay(2000);
  pinMode (BOOT_PIN, INPUT_PULLUP);
  demoMode = digitalRead(BOOT_PIN) == LOW;

  builtinLED[0] = CRGB::Black;   // Indica que não pode mais apertar BOOT para ativar demo
  FastLED.show();
}


//--------------------------------------------------
// Loop principal
//--------------------------------------------------
void loop() {
  static struct tm *local_time = NULL;
  static int hora = 0;
  static int minuto = 0;
  float temp;

  if (demoMode) {
    struct tm tempo_emulado;
    tempo_emulado.tm_hour = hora;
    tempo_emulado.tm_min = minuto;
    if (hora < 12) {
      temp = 20.0f + hora/10.0f;
    } else {
      temp = 21.2f - (hora-12)/10.0f;
    }
    atualizaLEDs(hora, minuto);
    updateScreen(&tempo_emulado, temp);
    minuto += 1;
    if (minuto == 60) {
      minuto = 0;
      hora += 1;
      if (hora == 24) {
        hora = 0;
      }
    }
    delay(50);
  } else {
    sntp.update();
    if (sntp.valid()) {
      time_t agora = sntp.time();
      local_time = localtime(&agora);
    }
    temp = readHDC1080();
    atualizaLEDs(local_time->tm_hour, local_time->tm_min);
    updateScreen(local_time, temp);
    delay(10000);
  }
}

//--------------------------------------------------
// LEDs RGB
void atualizaLEDs(int hora, int minuto) {
  int t = hora*60 + minuto;
  CRGB val;

  // SOL: apagado das 19 às 5, aumenta intensidade das 5 às 12 e depois baixa até as 19
  //      LED_SOL1 das 5 às  10
  //      LED_SOL2 das 10 às 14
  //      LED_SOL3 das 15 às 19
  ledsRGB[LED_SOL1] = ledsRGB[LED_SOL2] = ledsRGB[LED_SOL3] = CRGB(0,0,0);
  int rg = 0, b = 0;
  float k;
  int led = (hora < 10) ? LED_SOL1 : (hora < 14) ? LED_SOL2 : LED_SOL3;
  if ((hora >= 5) && (hora < 12)) {
    k = (t - 5*60)/60.0f;
    rg = (int) (2*k*k + k);
    b = (int) (k*k/2);
    ledsRGB[led] = CRGB(rg, rg, b);
  } else if ((hora >= 12) && (hora < 19)) {
    k = 7.0 - (t - 12*60)/60.0f;
    rg = 2*k*k + k;
    b = (int) (k*k/2);
    ledsRGB[led] = CRGB(rg, rg, b);
  }

  // Janela de baixo: acesa das 6:15 às 7:30 e das 18:00 às 21:00
  if (((t >= (6*60+15)) && (t < (7*60+30))) || ((t >= 18*60) && (t < 21*60))) {
    ledsRGB[LED_JANELA_BAIXO] = CRGB(20,20,30);
  } else {
    ledsRGB[LED_JANELA_BAIXO] = CRGB(0,0,0);
  }

  // Janela de cima: acesa das 6:00 às 7:00 e das 20:00 às 22
  if (((t >= 6*60) && (t < 7*60)) || ((t >= 20*60) && (t < 22*60))) {
    ledsRGB[LED_JANELA_CIMA] = CRGB(30,30,10);
  } else {
    ledsRGB[LED_JANELA_CIMA] = CRGB(0,0,0);
  }

  FastLED.show();
}

//--------------------------------------------------
// Sensor de Temperatura
//--------------------------------------------------

// iniciação
void initHDC1080() {
  Wire.begin(PIN_SDA, PIN_SCL);  

  // Verifica IDs de Fabricante e Dispositivo
  uint16_t manID = ReadReg16(REG_MANID);
  uint16_t devID = ReadReg16(REG_DEVID);
  //Serial.print ("Fabricante: ");
  //Serial.print (manID, HEX);
  //Serial.print (" Dispositivo: ");
  //Serial.println (devID, HEX);
}

// leitura da temperatura
float readHDC1080() {
  uint16_t r;
  float temp, humid;

  // Dispara conversao
  Wire.beginTransmission(ADDR);  
  Wire.write(REG_TEMP);  
  Wire.endTransmission();

  // Espera conversao
  delay(20);

  // Pega resultado e mostra
  Wire.requestFrom(ADDR, 4);
  r = Wire.read() << 8;
  r |= Wire.read();  
  temp = r*165.0/65536.0 - 40.0;
  r = Wire.read() << 8;
  r |= Wire.read();  
  humid = r*100.0/65536.0;
  return temp;
}

// Le registrador de 16 bits
int16_t ReadReg16 (byte reg)  
{  
  uint16_t val;  
    
  // Seleciona register
  Wire.beginTransmission(ADDR);  
  Wire.write(reg);  
  Wire.endTransmission();  
  
  // Le valor
  Wire.requestFrom(ADDR, 2);  
  val = Wire.read() << 8;  
  val |= Wire.read();  
  return (int16_t) val;  
} 

//--------------------------------------------------
// Display
//--------------------------------------------------

// Iniciação
void initScreen() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI);
  pinMode (TFT_BL, OUTPUT);
  digitalWrite (TFT_BL, HIGH);

  tft.init(172, 320);
  tft.fillScreen(FUNDO_TELA);
  tft.setRotation(2);

  tft.fillRoundRect(40, 60, 80, 210, 16, FUNDO_TERM);
  tft.drawRoundRect(40, 60, 80, 210, 16, ST77XX_BLACK);
  tft.fillCircle(90, 250, 10, ST77XX_RED);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_BLACK);
  int t = 50;
  for (int y = 100; y <= 220; y += 24) {
    tft.drawLine(74, y, 86, y, ST77XX_BLACK);
    tft.setCursor (46, y-4);
    tft.print(t);
    t -= 10;
  }
  for (int y = 112; y < 220; y += 24) {
    tft.drawLine(80, y, 86, y, ST77XX_BLACK);
  }
  tft.fillRect(89, 80, 4, 160, ST77XX_WHITE);
  tft.fillRoundRect(89, 220, 4, 20, 2, ST77XX_RED);
}

// atualiza a tela
void updateScreen(struct tm *local_time, float temp) {
  // Atualiza a hora
  tft.fillRoundRect(16, 8, 136, 42, 10, FUNDO_TEXTO);
  if (local_time != NULL) {
    char hora[10];
    sprintf(hora, "%02d:%02d", local_time->tm_hour, local_time->tm_min);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(4);
    tft.setCursor (20, 14);
    tft.print(hora);
    tft.setTextColor(COR_SOMBRA);
    tft.setCursor (21, 15);
    tft.print(hora);
  }

  // Atualiza imagem do termometro
  if (temp < 0) {
    temp = 0;
  } else if (temp > 50) {
    temp = 50;
  }
  int tam = (int) (24.0f*temp)/10.0f + 0.5f;
  tft.fillRect(89, 80, 4, 160, ST77XX_WHITE);
  tft.fillRoundRect(89, 220-tam, 4, tam+20, 2, ST77XX_RED);

  // Atualiza temperatura
  tft.fillRoundRect(38, 284, 86, 24, 6, FUNDO_TEXTO);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor (46, 290);
  tft.setTextSize(2);
  char temperatura[10];
  sprintf (temperatura, "%.1f C", temp);
  tft.print(temperatura);
}
