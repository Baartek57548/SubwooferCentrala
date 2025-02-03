#include <avr/wdt.h>         // Biblioteka watchdoga
#include <LowPower.h>        // Biblioteka LowPower (dla Arduino AVR)
#include <OneWire.h>
#include <DallasTemperature.h>

// =====================
// OPCJA DEBUGOWANIA
// =====================
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
const int BATTERY_PIN = A0;         // Pomiar napięcia akumulatora (dzielnik napięcia)
const int AUDIO_PIN = A1;           // Wejście sygnału audio
const int SUBWOOFER_CTRL_PIN = 2;   // Sterowanie zasilaniem subwoofera
const int ONE_WIRE_BUS = 3;         // Czujnik DS18B20 (1-Wire)
const int FAN_PWM_PIN = 5;          // PWM sterujące wentylatorem
const int TEMP_ALARM_PIN = 6;       // Sterowanie alarmem przegrzania
const int LED_ALARM_PIN = 7;        // Dioda LED sygnalizująca tryb awaryjny

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// =====================
// PARAMETRY POMIARÓW
// =====================
const float VOLTAGE_DIVIDER_FACTOR = 3.0;  // Współczynnik dzielnika napięcia (np. 20kΩ/10kΩ -> 3)
const float BATTERY_LOW_THRESHOLD = 12.0;  // Próg niskiego napięcia [V]
const int AUDIO_THRESHOLD = 50;            // Próg wykrywania audio (wartość ADC)
unsigned long lastAudioDetectedTime = 0;
bool audioTimerActive = false;
unsigned long audioTimerStart = 0;
const unsigned long AUDIO_TIMER_DURATION = 60000; // 60 sekund

const float FAN_TEMP_MAX = 40.0;         // Temperatura, przy której PWM rośnie proporcjonalnie (0–255)
const float FAN_TEMP_MAX_LIMIT = 50.0;     // Powyżej tej temperatury wentylator pracuje na max (PWM = 255)
const float TEMP_ALARM_RELEASE = 35.0;     // Temperatura, przy której alarm przegrzania jest resetowany

// =====================
// DEFINICJA STRUKTURY STANÓW
// =====================
enum SystemState {
  STATE_ACTIVE,    // Aktywny: system monitoruje napięcie, audio, temperaturę – elementy włączone
  STATE_IDLE,      // Spoczynek: warunki nie wymagają aktywności (np. wysoki poziom napięcia, audio obecne)
  STATE_OVERHEAT,  // Awaryjny: temperatura przekracza FAN_TEMP_MAX_LIMIT, włączony alarm
  STATE_SLEEP      // Uśpienie: system przez długi czas pozostaje bez aktywności, przechodzi w tryb oszczędzania energii
};

SystemState systemState = STATE_IDLE;
unsigned long lastActiveTime = 0;  // Czas ostatniej aktywności (dla przejścia w tryb IDLE / SLEEP)

// =====================
// FUNKCJA MAPOWANIA FLOAT
// =====================
int mapFloat(float x, float in_min, float in_max, int out_min, int out_max) {
  if (x < in_min) x = in_min;
  if (x > in_max) x = in_max;
  return (int)((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

// =====================
// FUNKCJA DEBUG STATUSU
// =====================
void debugStatus() {
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

// =====================
// FUNKCJA AKTUALIZACJI STANU SYSTEMU
// =====================
void updateSystemState() {
  unsigned long currentTime = millis();

  // Pomiar napięcia akumulatora
  int adcBattery = analogRead(BATTERY_PIN);
  float measuredVoltage = adcBattery * (5.0 / 1023.0);
  float batteryVoltage = measuredVoltage * VOLTAGE_DIVIDER_FACTOR;
  bool batteryLow = (batteryVoltage < BATTERY_LOW_THRESHOLD);

  // Pomiar sygnału audio
  int adcAudio = analogRead(AUDIO_PIN);
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
  
  // Ustal warunek aktywności – gdy napięcie jest niskie lub brak audio
  bool activeCondition = batteryLow || audioTimerActive;
  
  // Jeśli warunki aktywne są spełnione, ustaw stan ACTIVE i resetuj timer bezczynności
  if (activeCondition) {
    systemState = STATE_ACTIVE;
    lastActiveTime = currentTime;
  } 
  // Jeśli system był aktywny, ale przez 30 sekund nie wykryto aktywności, przechodzi w stan IDLE
  else if (currentTime - lastActiveTime > 30000) {
    systemState = STATE_IDLE;
  }
  
  // Jeśli temperatura przekracza granicę, ustaw stan OVERHEAT (ma priorytet)
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);
  if (temperature > FAN_TEMP_MAX_LIMIT) {
    systemState = STATE_OVERHEAT;
  }
  
  // Jeśli system jest przez długi czas (np. 2 minuty) w stanie IDLE, przejdź do trybu SLEEP
  if (systemState == STATE_IDLE && (currentTime - lastActiveTime > 120000)) {
    systemState = STATE_SLEEP;
  }
  
  // Debug: wypisz aktualny stan systemu
  switch(systemState) {
    case STATE_ACTIVE:
      DEBUG_PRINTLN("Stan systemu: ACTIVE");
      break;
    case STATE_IDLE:
      DEBUG_PRINTLN("Stan systemu: IDLE");
      break;
    case STATE_OVERHEAT:
      DEBUG_PRINTLN("Stan systemu: OVERHEAT");
      break;
    case STATE_SLEEP:
      DEBUG_PRINTLN("Stan systemu: SLEEP");
      break;
  }
}

void setup() {
  Serial.begin(9600);
  
  // Konfiguracja pinów
  pinMode(BATTERY_PIN, INPUT);
  pinMode(AUDIO_PIN, INPUT);
  pinMode(SUBWOOFER_CTRL_PIN, OUTPUT);
  pinMode(FAN_PWM_PIN, OUTPUT);
  pinMode(TEMP_ALARM_PIN, OUTPUT);
  pinMode(LED_ALARM_PIN, OUTPUT);
  
  // Upewnij się, że na starcie wszystkie wyjścia są wyłączone
  digitalWrite(SUBWOOFER_CTRL_PIN, LOW);
  digitalWrite(TEMP_ALARM_PIN, LOW);
  digitalWrite(LED_ALARM_PIN, LOW);
  analogWrite(FAN_PWM_PIN, 0);
  
  sensors.begin();
  lastAudioDetectedTime = millis();
  lastActiveTime = millis();
  
  // Inicjalizacja Watchdoga – ustawiony na 2 sekundy
  wdt_enable(WDTO_2S);
  
  DEBUG_PRINTLN("System uruchomiony - debugowanie wlaczone");
}

void loop() {
  // Resetuj watchdog
  wdt_reset();
  
  unsigned long currentTime = millis();
  
  // Aktualizuj stan systemu (napięcie, audio, temperatura)
  updateSystemState();
  
  // Wykonaj zadania w zależności od aktualnego stanu:
  switch(systemState) {
    case STATE_ACTIVE:
      {
        // Aktywne działanie: włącz subwoofer
        digitalWrite(SUBWOOFER_CTRL_PIN, HIGH);
        
        // Pomiar temperatury i sterowanie wentylatorem
        sensors.requestTemperatures();
        float temperature = sensors.getTempCByIndex(0);
        int fanPWM = (temperature <= FAN_TEMP_MAX) ?
                      mapFloat(temperature, 0.0, FAN_TEMP_MAX, 0, 255) : 255;
        analogWrite(FAN_PWM_PIN, fanPWM);
        
        // Zabezpieczenie przed przegrzaniem
        if (temperature > FAN_TEMP_MAX_LIMIT) {
          digitalWrite(TEMP_ALARM_PIN, HIGH);
        } else if (temperature < TEMP_ALARM_RELEASE) {
          digitalWrite(TEMP_ALARM_PIN, LOW);
        }
        
        // Sygnalizacja LED: w trybie OVERHEAT migająca dioda
        if (temperature > FAN_TEMP_MAX_LIMIT) {
          if ((currentTime / 500) % 2 == 0)
            digitalWrite(LED_ALARM_PIN, HIGH);
          else
            digitalWrite(LED_ALARM_PIN, LOW);
        } else {
          digitalWrite(LED_ALARM_PIN, LOW);
        }
        
        DEBUG_PRINT("Temperatura: ");
        DEBUG_PRINT(temperature);
        DEBUG_PRINTLN(" *C");
        DEBUG_PRINT("Fan PWM: ");
        DEBUG_PRINT(fanPWM);
        DEBUG_PRINT(" | Alarm: ");
        DEBUG_PRINTLN(digitalRead(TEMP_ALARM_PIN));
      }
      break;
      
    case STATE_OVERHEAT:
      {
        // W trybie OVERHEAT – alarm pozostaje włączony, subwoofer włączony, wentylator wyłączony (dla ochrony)
        digitalWrite(TEMP_ALARM_PIN, HIGH);
        digitalWrite(SUBWOOFER_CTRL_PIN, HIGH);
        analogWrite(FAN_PWM_PIN, 0);
        if ((currentTime / 500) % 2 == 0)
          digitalWrite(LED_ALARM_PIN, HIGH);
        else
          digitalWrite(LED_ALARM_PIN, LOW);
      }
      break;
      
    case STATE_IDLE:
      {
        // W stanie IDLE wyłączamy wszystkie aktywne elementy
        digitalWrite(SUBWOOFER_CTRL_PIN, LOW);
        digitalWrite(TEMP_ALARM_PIN, LOW);
        digitalWrite(LED_ALARM_PIN, LOW);
        analogWrite(FAN_PWM_PIN, 0);
      }
      break;
      
    case STATE_SLEEP:
      {
        // W trybie SLEEP: oszczędzanie energii
        digitalWrite(SUBWOOFER_CTRL_PIN, LOW);
        digitalWrite(TEMP_ALARM_PIN, LOW);
        digitalWrite(LED_ALARM_PIN, LOW);
        analogWrite(FAN_PWM_PIN, 0);
        DEBUG_PRINTLN("Przechodze w tryb sleep (8 sekund)...");
        // Uśpij mikrokontroler na 8 sekund; ADC oraz BOD zostają wyłączone dla niższego poboru prądu.
        LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
        // Po przebudzeniu wracamy do stanu IDLE, resetujemy timer aktywności.
        systemState = STATE_IDLE;
        lastActiveTime = millis();
      }
      break;
  }
  
  // Dodatkowe debugowanie (np. co około 5 sekund)
  if (currentTime % 5000 < 200) {
    debugStatus();
  }
  
  delay(200); // Pętla musi wykonywać się wystarczająco często, aby watchdog był resetowany
}
