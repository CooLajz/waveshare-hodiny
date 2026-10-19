# Změny firmware

## 2.1.0 — 2026-10-19

- Jeden společný firmware pro Waveshare ESP32-S3-Touch-LCD-2.1 a ESP32-S3-Touch-LCD-2.8C. Deska se rozpozná automaticky, při instalaci ji nemusíte vybírat.
- Podpora dotykového řadiče GT911 a odpovídající inicializace panelu 2.8C, včetně správného směru svislých gest.
- Rozpoznaný model desky a dotykový řadič jsou uvedené ve webové diagnostice.
- Červený noční režim předpovědi zachovává černé pozadí i při chybějících datech.
- Společná konfigurace nástrojů pro vývojový build, release a USB upload na Apple Silicon.
- Aktualizované české a anglické podklady a instalační stránka pro obě desky. Starší firmware před verzí 2.1.0 podporuje pouze model 2.1.

Testovací buildy byly ověřené na obou modelech, včetně OTA aktualizace. Na 2.8C prošla také čistá instalace a Improv Serial provisioning přes oba USB-C konektory.
