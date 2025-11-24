#include <WiFi.h>
#include <WebServer.h>
#include <arduinoFFT.h>
#include <SPIFFS.h>
#include <Arduino.h>


// ===============================
// CONFIGURACIÓN DE PINES
// ===============================
const int micPin = 35;      // Entrada del micrófono (MAX9814 OUT)
const int redPin = 26;      // PWM para canal Rojo
const int greenPin = 27;    // PWM para canal Verde  
const int bluePin = 33;     // PWM para canal Azul

// ===============================
// VARIABLES DE MODOS DE OPERACIÓN
// ===============================
bool musicMode = false;     // Modo reactivo a música
bool rainbowMode = false;   // Modo arcoíris automático
bool manualMode = true;     // Modo manual (activo por defecto)

// ===============================
// VARIABLES PARA MODO ARCOÍRIS
// ===============================
unsigned long lastColorChange = 0;
int rainbowHue = 0;
// Rainbow controls (ms between color steps, brightness multiplier 0.0-1.0)
unsigned long rainbowIntervalMs = 30; // default 30ms per step
float rainbowBrightness = 1.0; // 1.0 = 100%

// ===============================
// CONFIGURACIÓN FFT (ANÁLISIS DE AUDIO)
// ===============================
const int samples = 512;
const double samplingFrequency = 4000.0;
double vReal[samples];
double vImag[samples];
ArduinoFFT<double> FFT(vReal, vImag, samples, samplingFrequency);

// ===============================
// CONFIGURACIÓN PWM
// ===============================
const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmMax = 255;
// PWM channels para ESP32 (LEDC)
const int redChannel = 0;
const int greenChannel = 1;
const int blueChannel = 2;

// ===============================
// CONFIGURACIÓN WIFI
// ===============================
const char* ssid = "Vladimir05";
const char* password = "Junior25";
WebServer server(80);

// ===============================
// VARIABLES PARA DETECCIÓN DE BEAT
// ===============================
double prevLowEnergy = 0;
double avgLowEnergy = 0;
const double smoothingFactor = 0.9;
unsigned long lastBeatTime = 0;
const int beatHoldTime = 150; // ms
double beatThreshold = 400.0;

// ===============================
// VARIABLES DE COLOR MANUAL
// ===============================
int redVal = 0, greenVal = 0, blueVal = 0;
// Color para modo música (color que parpadeará cuando se detecte beat)
int musicRed = 0, musicGreen = 0, musicBlue = 255; // default azul

// NOTE: Web UI files are served from SPIFFS (`data/` folder).
// The original embedded HTML is removed to keep SPIFFS usage consistent.

// ===============================
// CONFIGURACIÓN WIFI
// ===============================
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Conectando a WiFi...");

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n⚠️ No se pudo conectar, iniciando modo AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_RGB_Control", "12345678");
    Serial.print("Red creada: ESP32_RGB_Control - IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

// ===============================
// APLICAR COLOR A LOS LEDs
// ===============================
void applyColor(int r, int g, int b) {
  ledcWrite(redChannel, r);
  ledcWrite(greenChannel, g);
  ledcWrite(blueChannel, b);
}

// ===============================
// DETECCIÓN DE BEAT Y REACCIÓN
// ===============================
void detectBeatAndReact() {
  static unsigned long lastSampleTime = 0;
  unsigned long now = micros();

  if (now - lastSampleTime < (1000000.0 / samplingFrequency)) return;
  lastSampleTime = now;

  // Lectura y procesamiento de muestras de audio
  double avg = 0;
  for (int i = 0; i < samples; i++) {
    vReal[i] = analogRead(micPin);
    avg += vReal[i];
    vImag[i] = 0;
    delayMicroseconds(50); // evita saturar ADC
  }

  avg /= samples;
  for (int i = 0; i < samples; i++) {
    vReal[i] -= avg;
    if (vReal[i] < 60) vReal[i] = 0; // ignora ruido
  }

  // Análisis FFT
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude();

  // Cálculo de energía en bajas frecuencias (20-120 Hz)
  double lowEnergy = 0;
  for (int i = 2; i < samples / 2; i++) {
    double freq = i * (samplingFrequency / samples);
    if (freq >= 20 && freq <= 120) lowEnergy += vReal[i];
  }

  // Detección de beat
  avgLowEnergy = smoothingFactor * avgLowEnergy + (1.0 - smoothingFactor) * lowEnergy;
  bool beatDetected = (lowEnergy - prevLowEnergy) > beatThreshold;
  prevLowEnergy = lowEnergy;

  // Reacción al beat
  if (beatDetected && millis() - lastBeatTime > beatHoldTime) {
    lastBeatTime = millis();
    // Aplicar color seleccionado para modo música
    int rv = constrain(musicRed, 0, pwmMax);
    int gv = constrain(musicGreen, 0, pwmMax);
    int bv = constrain(musicBlue, 0, pwmMax);
    ledcWrite(redChannel, rv);
    ledcWrite(greenChannel, gv);
    ledcWrite(blueChannel, bv);
  } else {
    // Desvanecimiento suave
    int fade = map(millis() - lastBeatTime, 0, beatHoldTime, 255, 0);
    fade = constrain(fade, 0, 255);
    // desvanecer desde el color de música hacia apagado
    float f = fade / 255.0;
    int rv = (int)constrain(musicRed * f, 0, pwmMax);
    int gv = (int)constrain(musicGreen * f, 0, pwmMax);
    int bv = (int)constrain(musicBlue * f, 0, pwmMax);
    ledcWrite(redChannel, rv);
    ledcWrite(greenChannel, gv);
    ledcWrite(blueChannel, bv);
  }
}

// ===============================
// APLICAR COLOR ARCOÍRIS (HSV to RGB)
// ===============================
void applyRainbowColor(int hue) {
  float r, g, b;
  // Conversión simple HSV a RGB
  int region = hue / 60;
  float f = (hue / 60.0) - region;
  float q = 1 - f;

  switch(region) {
    case 0: r=1; g=f; b=0; break;
    case 1: r=q; g=1; b=0; break;
    case 2: r=0; g=1; b=f; break;
    case 3: r=0; g=q; b=1; break;
    case 4: r=f; g=0; b=1; break;
    default: r=1; g=0; b=q; break;
  }

  // Escribir en los canales LEDC (rojo, verde, azul)
  int rv = (int)(r * 255 * rainbowBrightness);
  int gv = (int)(g * 255 * rainbowBrightness);
  int bv = (int)(b * 255 * rainbowBrightness);
  rv = constrain(rv, 0, pwmMax);
  gv = constrain(gv, 0, pwmMax);
  bv = constrain(bv, 0, pwmMax);
  ledcWrite(redChannel, rv);
  ledcWrite(greenChannel, gv);
  ledcWrite(blueChannel, bv);
}

// ===============================
// CONFIGURACIÓN INICIAL
// ===============================
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  // Configuración de PWM (ESP32 LEDC)
  ledcSetup(redChannel, pwmFreq, pwmResolution);
  ledcAttachPin(redPin, redChannel);
  ledcSetup(greenChannel, pwmFreq, pwmResolution);
  ledcAttachPin(greenPin, greenChannel);
  ledcSetup(blueChannel, pwmFreq, pwmResolution);
  ledcAttachPin(bluePin, blueChannel);

  setupWiFi();

  // Montar SPIFFS antes de iniciar el servidor
  if (!SPIFFS.begin(true)) {
    Serial.println("Error montando SPIFFS");
  } else {
    Serial.println("SPIFFS montado correctamente");
  }

  // ===============================
  // ENDPOINTS DEL SERVIDOR WEB
  // ===============================
  
  // Página principal
  server.on("/", []() {
    if (!SPIFFS.exists("/index.html")) {
      server.send(404, "text/plain", "index.html not found");
      return;
    }
    File file = SPIFFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  });

  server.on("/script.js", []() {
    if (!SPIFFS.exists("/script.js")) {
      server.send(404, "text/plain", "script.js not found");
      return;
    }
    File file = SPIFFS.open("/script.js", "r");
    server.streamFile(file, "application/javascript");
    file.close();
  });

  server.on("/style.css", []() {
    if (!SPIFFS.exists("/style.css")) {
      server.send(404, "text/plain", "style.css not found");
      return;
    }
    File file = SPIFFS.open("/style.css", "r");
    server.streamFile(file, "text/css");
    file.close();
  });

  server.on("/iro.min.js", []() {
    if (!SPIFFS.exists("/iro.min.js")) {
      server.send(404, "text/plain", "iro.min.js not found");
      return;
    }
    File file = SPIFFS.open("/iro.min.js", "r");
    server.streamFile(file, "application/javascript");
    file.close();
  });


  // Establecer color manual
  server.on("/setColor", HTTP_GET, []() {
    if (server.hasArg("R") && server.hasArg("G") && server.hasArg("B")) {

        int r = server.arg("R").toInt();
        int g = server.arg("G").toInt();
        int b = server.arg("B").toInt();
        
        //Actualiza valor para el modo manual
        redVal = r;
        greenVal = g;
        blueVal = b;

        // Llama a tu función existente que enciende la tira LED
        applyColor(r, g, b);

        server.send(200, "text/plain", "OK");
    }
    else {
        server.send(400, "text/plain", "Missing parameters");
    }
});


  // Cambiar modo de operación
  server.on("/setMode", []() {
    String mode = server.arg("m");
    
    if (mode == "music") {
      musicMode = true;
      rainbowMode = false;
      manualMode = false;
    } 
    else if (mode == "rainbow") {
      rainbowMode = true;
      musicMode = false;
      manualMode = false;
    } 
    else {
      manualMode = true;
      musicMode = false;
      rainbowMode = false;
    }
    
    server.send(200, "text/plain", "Mode set");
  });
 
  // Ajustar sensibilidad del beat
  server.on("/setBeatThreshold", []() {
    if (server.hasArg("value")) {
      beatThreshold = server.arg("value").toInt();
      Serial.print("Umbral de beat actualizado: ");
      Serial.println(beatThreshold);
    }
    server.send(200, "text/plain", "OK");
  });

    // Establecer color para modo música
    server.on("/setMusicColor", HTTP_GET, []() {
      if (server.hasArg("R") && server.hasArg("G") && server.hasArg("B")) {
        musicRed = server.arg("R").toInt();
        musicGreen = server.arg("G").toInt();
        musicBlue = server.arg("B").toInt();
        Serial.print("Music color set: "); Serial.print(musicRed); Serial.print(","); Serial.print(musicGreen); Serial.print(","); Serial.println(musicBlue);
      }
      server.send(200, "text/plain", "OK");
    });

    // Obtener color actual para modo música (JSON)
    server.on("/getMusicColor", HTTP_GET, []() {
      String payload = "{";
      payload += "\"R\":" + String(musicRed) + ",";
      payload += "\"G\":" + String(musicGreen) + ",";
      payload += "\"B\":" + String(musicBlue);
      payload += "}";
      server.send(200, "application/json", payload);
    });

  // Ajustar velocidad del arcoíris (ms entre pasos)
  server.on("/setRainbowSpeed", HTTP_GET, []() {
    if (server.hasArg("value")) {
      unsigned long v = server.arg("value").toInt();
      if (v < 5) v = 5;
      if (v > 2000) v = 2000;
      rainbowIntervalMs = v;
      Serial.print("Rainbow interval ms: "); Serial.println(rainbowIntervalMs);
    }
    server.send(200, "text/plain", "OK");
  });

  // Ajustar brillo del arcoíris (0-100 percent)
  server.on("/setRainbowBrightness", HTTP_GET, []() {
    if (server.hasArg("value")) {
      int p = server.arg("value").toInt();
      if (p < 0) p = 0;
      if (p > 100) p = 100;
      rainbowBrightness = p / 100.0f;
      Serial.print("Rainbow brightness: "); Serial.println(rainbowBrightness);
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("Servidor listo.");
} 

// ===============================
// LOOP PRINCIPAL
// ===============================
void loop() {
  server.handleClient();

  if (musicMode) {
    detectBeatAndReact();
  }
  else if (rainbowMode) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastColorChange > rainbowIntervalMs) { // velocidad de transición
      rainbowHue = (rainbowHue + 1) % 360;
      applyRainbowColor(rainbowHue);
      lastColorChange = currentMillis;
    }
  }
  else if (manualMode) {
    // El color ya se aplica cuando se cambia, no necesita acción adicional
    applyColor(redVal, greenVal, blueVal);
  }

  yield();
  delay(5);
}