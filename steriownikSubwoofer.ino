#include <avr/wdt.h>         // Biblioteka watchdoga
#include <OneWire.h>
#include <DallasTemperature.h>

// =====================
// OPCJA DEBUGOWANIA
// =====================
// Aby włączyć debugowanie, ustaw poniższą linię na 1
#define DEBUG 1

#if DEBUG
  #define DEBUG_PRINT(x)     Serial.print(x)
  #define DEBUG_PRINTLN(x)   Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// =====================
// DEFINICJE PINÓW
// =====================
// Pomiar napięcia akumulatora (dzielnik napięcia)
const int BATTERY_PIN = A0;    
// Wejście sygnału audio (po odpowiedniej obróbce – np. obwód prostowniczy)
const int AUDIO_PIN = A1;      

// Wyjście sterujące zasilaniem subwoofera (np. przez przekaźnik lub tranzystor)
const int SUBWOOFER_CTRL_PIN = 2;

// Czujnik temperatury DS18B20 (1-Wire)
const int ONE_WIRE_BUS = 3;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Wyjście PWM sterujące wentylatorem (pin obsługujący PWM, np. D5)
const int FAN_PWM_PIN = 5;

// Wyjście alarmowe przy zbyt wysokiej temperaturze (np. sterowanie przekaźnikiem)
const int TEMP_ALARM_PIN = 6;

// Dioda LED sygnalizująca tryb awaryjny (np. D7)
const int LED_ALARM_PIN = 7;

// =====================
// PARAMETRY POMIARÓW
// =====================
// Parametry dzielnika napięcia: przykładowo R1 = 20kΩ, R2 = 10kΩ
// V_bat = V_measured * (R1+R2)/R2 = V_measured * 3
const float VOLTAGE_DIVIDER_FACTOR = 3.0;
const float BATTERY_LOW_THRESHOLD = 12.0;  // [V]

// Parametry wykrywania sygnału audio
const int AUDIO_THRESHOLD = 50;  // wartość z ADC – do dostosowania
unsigned long lastAudioDetectedTime = 0;
bool audioTimerActive = false;
unsigned long audioTimerStart = 0;
const unsigned long AUDIO_TIMER_DURATION = 60000; // 60 sekund

// Parametry temperaturowe
const float FAN_TEMP_MAX = 40.0;         // Temperatura, przy której przy regulacji PWM osiągamy 255
const float FAN_TEMP_MAX_LIMIT = 50.0;     // Powyżej tej temperatury wentylator pracuje na max (PWM = 255)
const float TEMP_ALARM_RELEASE = 35.0;     // Temperatura, przy której alarm zostanie wyłączony

// Flaga alarmu przegrzania
bool overTempActive = false;

// =====================
// FUNKCJA MAPOWANIA FLOAT
// =====================
int mapFloat(float x, float in_min, float in_max, int out_min, int out_max) {
  if (x < in_min) x = in_min;
  if (x > in_max) x = in_max;
  return (int)((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

// =====================
// FUNKCJA DEBUG
// =====================
void debugStatus() {
  // Tutaj można dodać dodatkowe funkcje raportowania np. zapisywanie do EEPROM
  // lub wysyłanie przez inny interfejs. Na razie wypisujemy tylko podstawowe informacje.
  DEBUG_PRINT("Bateria: ");
  int adcBattery = analogRead(BATTERY_PIN);
  float measuredVoltage = adcBattery * (5.0 / 1023.0);
  float batteryVoltage = measuredVoltage * VOLTAGE_DIVIDER_FACTOR;
  DEBUG_PRINT(batteryVoltage);
  DEBUG_PRINTLN(" V");

  DEBUG_PRINT("Audio ADC: ");
  int adcAudio = analogRead(AUDIO_PIN);
  DEBUG_PRINTLN(adcAudio);
}

void setup() {
  Serial.begin(9600);
  
  // Ustawienie trybów pinów
  pinMode(BATTERY_PIN, INPUT);
  pinMode(AUDIO_PIN, INPUT);
  pinMode(SUBWOOFER_CTRL_PIN, OUTPUT);
  pinMode(FAN_PWM_PIN, OUTPUT);
  pinMode(TEMP_ALARM_PIN, OUTPUT);
  pinMode(LED_ALARM_PIN, OUTPUT);
  
  // Upewnij się, że wyjścia są na początku wyłączone
  digitalWrite(SUBWOOFER_CTRL_PIN, LOW);
  digitalWrite(TEMP_ALARM_PIN, LOW);
  digitalWrite(LED_ALARM_PIN, LOW);
  analogWrite(FAN_PWM_PIN, 0);
  
  // Inicjalizacja czujnika DS18B20
  sensors.begin();
  
  // Inicjalizacja czasu ostatniego wykrycia audio
  lastAudioDetectedTime = millis();
  
  // ===============================
  // Inicjalizacja Watchdoga
  // ===============================
  // Ustaw watchdog na okres 2 sekund. Jeżeli w ciągu 2 sekund watchdog nie zostanie zresetowany,
  // nastąpi reset mikrokontrolera.
  wdt_enable(WDTO_2S);
  
  DEBUG_PRINTLN("System uruchomiony - debugowanie wlaczone");
}

void loop() {
  // Reset watchdoga na początku każdej iteracji pętli
  wdt_reset();
  
  unsigned long currentTime = millis();

  // ============
  // 1. POMIAR NAPIĘCIA AKAUMULATORA
  // ============
  int adcBattery = analogRead(BATTERY_PIN);
  float measuredVoltage = adcBattery * (5.0 / 1023.0);
  float batteryVoltage = measuredVoltage * VOLTAGE_DIVIDER_FACTOR;
  
  DEBUG_PRINT("Napiecie akumulatora: ");
  DEBUG_PRINT(batteryVoltage);
  DEBUG_PRINTLN(" V");

  // Warunek niskiego napięcia: TRUE, gdy napięcie < 12V
  bool batteryLow = (batteryVoltage < BATTERY_LOW_THRESHOLD);

  // ============
  // 2. WYKRYWANIE SYGNAŁU AUDIO
  // ============
  int adcAudio = analogRead(AUDIO_PIN);
  DEBUG_PRINT("Audio ADC: ");
  DEBUG_PRINTLN(adcAudio);
  
  bool audioPresent = (adcAudio > AUDIO_THRESHOLD);
  if (audioPresent) {
    lastAudioDetectedTime = currentTime;
    audioTimerActive = false;
  } else {
    if (!audioTimerActive && (currentTime - lastAudioDetectedTime >= 1000)) {
      audioTimerActive = true;
      audioTimerStart = currentTime;
      DEBUG_PRINTLN("Brak sygnalu audio - wlaczam subwoofer na 60s");
    }
    if (audioTimerActive && (currentTime - audioTimerStart >= AUDIO_TIMER_DURATION)) {
      audioTimerActive = false;
      DEBUG_PRINTLN("60s minelo - wylaczam tryb audio");
    }
  }
  
  // System aktywny, gdy napięcie jest niskie LUB brak audio przez 60s
  bool systemActive = batteryLow || audioTimerActive;
  digitalWrite(SUBWOOFER_CTRL_PIN, systemActive ? HIGH : LOW);

  // ============
  // 3. POMIAR TEMPERATURY I STEROWANIE WENTYLATOREM
  // ============
  if (systemActive) {
    sensors.requestTemperatures();
    float temperature = sensors.getTempCByIndex(0);
    DEBUG_PRINT("Temperatura: ");
    DEBUG_PRINT(temperature);
    DEBUG_PRINTLN(" *C");
    
    int fanPWM = 0;
    if (temperature <= FAN_TEMP_MAX) {
      fanPWM = mapFloat(temperature, 0.0, FAN_TEMP_MAX, 0, 255);
    } else {
      fanPWM = 255;
    }
    analogWrite(FAN_PWM_PIN, fanPWM);
    
    // ============
    // 4. ZABEZPIECZENIE PRZECIWKO PRZEGRZANIU
    // ============
    if (temperature > FAN_TEMP_MAX_LIMIT) {
      digitalWrite(TEMP_ALARM_PIN, HIGH);
      overTempActive = true;
    } else if (overTempActive && temperature < TEMP_ALARM_RELEASE) {
      digitalWrite(TEMP_ALARM_PIN, LOW);
      overTempActive = false;
    }
    
    // Sygnalizacja LED: w trybie awaryjnym (przegrzanie) zapal diodę LED (migająca)
    if (overTempActive) {
      if ((currentTime / 500) % 2 == 0)
        digitalWrite(LED_ALARM_PIN, HIGH);
      else
        digitalWrite(LED_ALARM_PIN, LOW);
    } else {
      digitalWrite(LED_ALARM_PIN, LOW);
    }
    
    DEBUG_PRINT("Fan PWM: ");
    DEBUG_PRINT(fanPWM);
    DEBUG_PRINT(" | Alarm: ");
    DEBUG_PRINTLN(digitalRead(TEMP_ALARM_PIN));
  } else {
    analogWrite(FAN_PWM_PIN, 0);
    digitalWrite(TEMP_ALARM_PIN, LOW);
    digitalWrite(LED_ALARM_PIN, LOW);
  }
  
  // Możliwość dodatkowego raportowania stanu systemu
  #if DEBUG
    // Na przykład, wywołanie funkcji debugStatus() co kilka sekund:
    if (currentTime % 5000 < 200) {  // co około 5 sekund
      debugStatus();
    }
  #endif
  
  delay(200);  // pętla musi wykonywać się częściej niż co 2 sekundy, aby watchdog był resetowany
}
