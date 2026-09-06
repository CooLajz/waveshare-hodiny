#pragma once

// Produkční start se z této funkce vrátí pouze s ověřeným Wi-Fi připojením.
// Pokud údaje chybějí nebo připojení selže, funkce obsluhuje samostatný
// onboarding režim až do restartu zařízení.
void wifiOnboardingRequireConnection();
