# SubwooferCentrala
Monitoruje napięcie akumulatora (włącza subwoofer poniżej 12V), wykrywa brak sygnału audio (włącza subwoofer na 60s), mierzy temperaturę (steruje wentylatorem), aktywuje alarm powyżej 50°C, wyświetla dane,

Podsumowanie funkcji programu:
Pomiar napięcia akumulatora: Monitorowanie napięcia akumulatora z wykorzystaniem dzielnika napięcia. Subwoofer nie jest włączany, gdy napięcie spadnie poniżej 12V.
Wykrywanie sygnału audio: Jeśli wykryto sygnał audio, subwoofer jest załączany na 60 sekund.
Pomiar temperatury: Odczyt temperatury z czujnika DHT11 (taki miałem). Temperatury wpływają na sterowanie wentylatorem oraz aktywację alarmu.
Sterowanie wentylatorem: Wentylator jest sterowany proporcjonalnie do temperatury, z pełnym włączeniem przy temperaturze 50°C.
Alarm: Jeśli temperatura przekroczy 60°C, uruchamiany jest alarm, który utrzymuje się, aż temperatura spadnie poniżej 45°C.
Podsumowanie ogólne:
System monitoruje napięcie akumulatora, temperaturę oraz wykrywa sygnał audio.
Na podstawie tych danych steruje wentylatorem (w zależności od temperatury) oraz subwooferem (w zależności od napięcia i sygnału audio).
W przypadku wysokiej temperatury, system włącza alarm i wentylator na pełnych obrotach, aby zapobiec przegrzaniu.
