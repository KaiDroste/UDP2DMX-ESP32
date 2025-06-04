Todos:
- Optional: eigene WiFi-Initialisierung statt protocol_examples_common.h.
- dmx_driver_install() mit Fehlerprüfung aufrufen.
- Falls LED ständig blinkt, prüfe ob dmx_error tatsächlich nötig ist.
- [x] RGB einbinden
- [x] Fading für RGB?
- CW WW einbinden
- dimming curves
- Button einbinden alles an (30 %) / alles aus
- [ ] Fading Ruckelt Eventuell lässt sich das noch mit Chatgpt ausdiskutieren? 
  - [ ] Fading ruckelt nur auf niedrigen Dimmstufen Kann an den Lampen oder Dimmern liegen lässt sich eventuell durch verschiedene Dimmkurven anpassen.
- [ ] Fallback Wifi für Loxone einrichten
- [ ] ESP DMX Netzwerknamen vergeben damit ich keine IP adresse mehr benötige



# UDP2DMX Gateway (ESP32)

This code is developed to use a DMX controller with an ESP32 as UDP2DMX gateway. The idea is to use the ESP32 via udp to allow communcation between the Loxone home automation system and the DMX light controller. 

This code is based on the following code:
- ESP IDF (v5.0)
- ESP DMX Library

The Protocol used in the Code was published by Robert Lechner? for the Arduino to DMX gateway and is partwise adopted to this code. 

## Getting startet 
For using the code the esp-idf needs to be installed. This code uses ESP-IDF V5.0 since this is the latest version supported by the ESP-DMX library. 

Use menuconfig to configure Wifi and the ESP pins. 
''' idf.py menuconfig '''
- Configure Wifi
- Configure RX and TX Pins of MAX 485
- Configure LED Pin
- Configure Button Pin


When the configuration is done run build and flash to flash the firmware to the ESP. 

After uploading the firmware you can monitor the ESP with '''idf.py monitor'''

## Testing the DMX communication
For testing the communication via DMX use the jupyter notebook "testudp.ipynb" Change the IP in the notebook to the ESPs IP (can be found in the log). After sending an command the ESP will show in the log the command. Also the LED light (if configured) should blink. 

## Configuring the ESP in Loxone 



