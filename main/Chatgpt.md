Danke für die Klarstellung. Wenn beim Wechseln des WLAN-Netzwerks die LED nicht blinkt, liegt das sehr wahrscheinlich daran, dass die LED nicht aktiv über WLAN-Events benachrichtigt wird. Aktuell wird der LED-Status offenbar nicht automatisch angepasst, wenn sich die WLAN-Verbindung ändert.

✅ Ziel:
Die LED soll z. B. schnell blinken, wenn kein WLAN verbunden ist, und dauerhaft leuchten oder langsam blinken, wenn WLAN verbunden ist – auch beim Wechseln des Netzwerks.

🔍 Ursachenanalyse
Dein Code enthält:

c
Kopieren
Bearbeiten
my_led_set_wifi_status(true);
Aber nur einmal, nämlich nach my_wifi_init().

→ Das reicht nicht, um spätere Verbindungsabbrüche oder Netzwerkwechsel zu erkennen!

✅ Lösung: WLAN-Status live überwachen oder per Event melden
Du hast 2 Optionen:

Option 1: WLAN-Status regelmäßig abfragen (my_wifi_is_connected())
In led_status_task():

c
Kopieren
Bearbeiten
wifi_connected = my_wifi_is_connected();
➕ Vorteil:
Einfach und funktioniert immer.

🧠 Beispiel-Implementierung in my_wifi.c:
c
Kopieren
Bearbeiten
static bool s_wifi_connected = false;

static void on_wifi_event(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            s_wifi_connected = false;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        s_wifi_connected = true;
    }
}

bool my_wifi_is_connected()
{
    return s_wifi_connected;
}
Dann kannst du es zyklisch in led_status_task() abfragen.

Option 2: LED direkt über WLAN-Events steuern
Du kannst auch die LED direkt aus dem WLAN-Event-Handler steuern.

Beispiel:
c
Kopieren
Bearbeiten
static void on_wifi_event(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            wifi_connected = false;
            my_led_set_wifi_status(false);  // LED-Update hier
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_connected = true;
        my_led_set_wifi_status(true);  // LED-Update hier
    }
}
🔧 Bonus: LED-Funktion erweitern
Du kannst z. B. diese Funktion in my_led.c anbieten:

c
Kopieren
Bearbeiten
void my_led_set_wifi_status(bool connected)
{
    if (connected)
    {
        my_led_blink(1, 1000); // langsam blinken
    }
    else
    {
        my_led_blink(2, 200); // schnell doppelt blinken
    }
}
Oder dauerhaft leuchten bei Erfolg:

c
Kopieren
Bearbeiten
void my_led_set_wifi_status(bool connected)
{
    my_led_set(connected);
}
✅ Empfehlung
Die Kombination aus:

Eventbasiertem WLAN-Status (on_wifi_event) und

LED-Steuerung direkt aus dem Handler

ist am zuverlässigsten. Du kannst es zusätzlich durch zyklische Prüfung ergänzen.

Möchtest du, dass ich dir ein vollständiges my_wifi.c oder my_led.c mit Event-basiertem Statuswechsel zeige?