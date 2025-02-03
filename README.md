# SubwooferCentrala
Monitoruje napięcie akumulatora (włącza subwoofer poniżej 12V), wykrywa brak sygnału audio (włącza subwoofer na 60s), mierzy temperaturę (steruje wentylatorem), aktywuje alarm powyżej 50°C, wyświetla dane, używa watchdog do resetu, oszczędza energię przez tryb uśpienia i zmniejszenie częstotliwości pomiarów.



Opis działania kodu
Ten kod steruje urządzeniem, które ma za zadanie monitorować temperaturę, napięcie akumulatora oraz wykrywać sygnał audio, a także zarządzać wentylatorem i subwooferem na podstawie tych danych. Poniżej szczegółowy opis funkcjonalności poszczególnych części programu.

1. Pomiar napięcia akumulatora
W kodzie zastosowano dzielnik napięcia, aby mierzyć napięcie akumulatora, który zasilany jest napięciem do 14,5V. Arduino odczytuje to napięcie z pinu A0 przy pomocy funkcji analogRead(). Wartość z analogowego wejścia jest następnie przekształcana na wartość napięcia akumulatora, biorąc pod uwagę dzielnik napięcia (rezystory: 10kΩ i 4.7kΩ). Jeżeli napięcie spadnie poniżej 12V, subwoofer zostaje włączony.



int sensorValue = analogRead(pin_voltage);
voltage = sensorValue * (5.0 / 1023.0) * ((10000 + 4700) / 4700);  // Obliczenie napięcia
2. Wykrywanie sygnału audio
Kod odczytuje poziom sygnału audio z pinu A1 przy pomocy funkcji analogRead(). Jeżeli poziom sygnału jest poniżej progu (500), oznacza to, że sygnał audio zniknął (np. cisza), a wtedy subwoofer jest włączany na 60 sekund (za pomocą funkcji millis(), która mierzy czas). Jeśli sygnał audio jest wykrywany (poziom powyżej 500), subwoofer jest wyłączany.



if (audioLevel < 500) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      digitalWrite(pin_subwoofer, HIGH);  // Włączenie subwoofera na 60s
    }
} else {
    digitalWrite(pin_subwoofer, LOW);  // Wyłączenie subwoofera
}
3. Pomiar temperatury
Czujnik temperatury DS18B20 podłączony do pinu D2 jest używany do pomiaru temperatury w otoczeniu. Biblioteka DallasTemperature komunikuje się z tym czujnikiem i zwraca zmierzoną temperaturę w stopniach Celsjusza. Temperatura jest odczytywana co iterację pętli i wykorzystywana do sterowania wentylatorem.



sensors.requestTemperatures();
temperature = sensors.getTempCByIndex(0);
4. Sterowanie wentylatorem
Wentylator jest sterowany za pomocą PWM (sygnał o zmiennej szerokości impulsu) na pinie D6. Wentylator działa proporcjonalnie do temperatury:

Jeśli temperatura jest poniżej 35°C, wentylator jest wyłączony (PWM = 0).
Jeśli temperatura jest między 35°C a 40°C, wentylator działa proporcjonalnie (wzrost prędkości z 0 do pełnej prędkości).
Jeżeli temperatura przekroczy 40°C, wentylator działa na pełnej prędkości (PWM = 255).
Jeśli temperatura osiągnie 50°C lub więcej, wentylator również działa na pełnych obrotach, a dodatkowo włączany jest alarm (pin pin_alarm zostaje włączony).



if (temperature >= 50) {
    analogWrite(pin_fan, 255);  // Wentylator na pełnych obrotach
    digitalWrite(pin_alarm, HIGH);  // Włącz alarm
} else if (temperature >= 40) {
    analogWrite(pin_fan, 255);  // Wentylator na pełnych obrotach
    digitalWrite(pin_alarm, HIGH);  // Włącz alarm
} else if (temperature >= 35) {
    fanSpeed = map(temperature, 35, 40, 128, 255);  // Proporcjonalne sterowanie prędkością wentylatora
    analogWrite(pin_fan, fanSpeed);
    digitalWrite(pin_alarm, HIGH);  // Włącz alarm
} else if (temperature < 35) {
    analogWrite(pin_fan, 0);  // Wentylator wyłączony
    digitalWrite(pin_alarm, LOW);  // Wyłącz alarm
}
5. Sterowanie alarmem
Gdy temperatura przekroczy 50°C, włączany jest alarm (pin pin_alarm), który może np. załączyć diodę LED lub buzzer, sygnalizując krytyczną temperaturę. Alarm pozostaje aktywny, aż temperatura spadnie poniżej 35°C, wtedy alarm jest wyłączany.



if (temperature >= 50) {
    digitalWrite(pin_alarm, HIGH);  // Włącz alarm
} else if (temperature < 35) {
    digitalWrite(pin_alarm, LOW);  // Wyłącz alarm
}



6. Wyświetlanie danych na monitorze szeregowym
Dane dotyczące napięcia akumulatora, temperatury oraz poziomu sygnału audio są wyświetlane w monitorze szeregowym co 500 ms. Pomaga to w monitorowaniu stanu systemu.


Serial.print("Napięcie: ");
Serial.println(voltage);
Serial.print("Temperatura: ");
Serial.println(temperature);
Serial.print("Poziom audio: ");
Serial.println(audioLevel);


Podsumowanie funkcji programu:
Pomiar napięcia akumulatora: Monitorowanie napięcia akumulatora z wykorzystaniem dzielnika napięcia. Subwoofer jest włączany, gdy napięcie spadnie poniżej 12V.
Wykrywanie sygnału audio: Jeśli nie wykryto sygnału audio, subwoofer jest załączany na 60 sekund.
Pomiar temperatury: Odczyt temperatury z czujnika DS18B20. Temperatury wpływają na sterowanie wentylatorem oraz aktywację alarmu.
Sterowanie wentylatorem: Wentylator jest sterowany proporcjonalnie do temperatury, z pełnym włączeniem przy temperaturze 50°C.
Alarm: Jeśli temperatura przekroczy 50°C, uruchamiany jest alarm, który utrzymuje się, aż temperatura spadnie poniżej 35°C.
Podsumowanie ogólne:
System monitoruje napięcie akumulatora, temperaturę oraz wykrywa sygnał audio.
Na podstawie tych danych steruje wentylatorem (w zależności od temperatury) oraz subwooferem (w zależności od napięcia i sygnału audio).
W przypadku wysokiej temperatury, system włącza alarm i wentylator na pełnych obrotach, aby zapobiec przegrzaniu.
