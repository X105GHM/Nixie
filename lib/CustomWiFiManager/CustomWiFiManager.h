#pragma once
#include "ewm/EasyWiFiManager.hpp"

/*
  EasyWiFiManager (EWM) – kompakte WLAN-Setup-Library für ESP32

  Zweck:
  - Verbindet automatisch mit bekannten WLANs (aus Credential-Store).
  - Falls keine Verbindung möglich ist, startet ein Konfigurations-Portal als Captive Portal.

  Ablauf:
  1) Beim Start versucht EWM alle gespeicherten Zugangsdaten (Priorität: 0 = höchste).
  2) Wenn kein Netzwerk klappt, wird ein SoftAP (WIFI_AP_STA) gestartet und ein Web-Portal bereitgestellt.
  3) Über das Portal kann man WLANs scannen, hinzufügen, löschen, sortieren und eine Verbindung anstoßen.
  4) Sobald der ESP32 im STA-Modus verbunden ist, läuft ein Countdown:
     - Nach Ablauf wird der AP beendet (nur noch WIFI_STA).
     - Optional kann die Seite danach automatisch schließen oder umleiten (z. B. auf mDNS / IP).

  UI/Portal:
  - Captive Portal Endpunkte für Android/iOS/Windows sind enthalten (generate_204, hotspot-detect, etc.).
  - PortalUiConfig erlaubt generische UI-Konfiguration (mDNS Host, Redirect/Close-Verhalten, Timing).

  Thread-Safety:
  - WiFi-Operationen werden über einen Mutex (WiFiLock) geschützt, damit parallel laufende Tasks nicht kollidieren.

  Optional:
  - ConnectivityMonitor kann regelmäßig Internet-Konnektivität prüfen (z. B. Ping/Probe) und bei Ausfall reagieren.

  Typischer Einsatz:
  - In deinem Projekt EWM konfigurieren (Hostname, AP-SSID, Probe, UI-Config) und ewm.begin() aufrufen.
  - Danach kann der Rest der Anwendung unabhängig vom Setup-Portal laufen.
*/
