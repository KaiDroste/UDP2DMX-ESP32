# 🌐 UDP2DMX Gateway (ESP32)

A lightweight and flexible UDP-to-DMX gateway based on the ESP32 platform, designed to bridge the **Loxone Home Automation System** with **DMX512-based lighting systems** via UDP over Wi-Fi.

This project enables direct communication between Loxone and DMX devices using a well-defined, lightweight UDP protocol. It is ideal for smart home installations where wireless DMX control is required.

---

## ⚙️ Key Features

- ✅ ESP-IDF 5.0 support  
- ✅ Based on the open-source [ESP-DMX library](https://github.com/someweisguy/esp_dmx)  
- ✅ UDP-based protocol (compatible with Loxone UDP protocol)  
- ✅ Easily testable via Jupyter notebook  

---

## 🚀 Getting Started

### 1. Install ESP-IDF

Make sure ESP-IDF v5.0 is installed and properly set up. For installation instructions, refer to the [official ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

---

### 2. Clone the Repository

```bash
git clone "link to this Repo"
cd udp2dmx-esp32
```

---

### 3. Configure Project with `menuconfig`

Launch the ESP-IDF configuration menu:

```bash
idf.py menuconfig
```

Set the following options:

- 📶 **Wi-Fi Configuration**  
  - SSID / Password
  - Select between multiple Wi-Fi networks if applicable

- 🎚️ **DMX Configuration**  
  - TX Pin (to MAX485)
  - RX Pin (optional, for monitoring or debugging)

- 💡 **LED Configuration**  
  - GPIO for status LED (blinks on DMX activity)

- 🔘 **Button Configuration**  
  - GPIO for physical mode switching (if required)

---

### 4. Build and Flash Firmware

```bash
idf.py build
idf.py flash
```

---

### 5. Monitor Output

```bash
idf.py monitor
```

After flashing, the device will connect to Wi-Fi and output its IP address via serial console. Note this IP for testing.

---

## 🧪 DMX Communication Test

You can use the included **`testudp.ipynb`** Jupyter Notebook to send test UDP commands:

1. Open the notebook in Jupyter
2. Set the IP address of your ESP32 device
3. Send UDP packets to test DMX channels
4. Watch serial monitor logs or connected DMX lights to verify output

💡 **Tip**: The status LED (if configured) will blink to confirm incoming UDP traffic.

---

## 🏠 Loxone Integration

To use the gateway in your **Loxone Config**:

1. Use **Virtual Outputs** to send UDP commands to the ESP32's IP.
2. Format the command according to the protocol (see below).
3. Configure each DMX channel or RGB output as needed.

---

## 📡 UDP Protocol Overview

The device expects UDP packets in the following formats:

- **DMXP** — Set DMX value with optional fade  
  Example: `DMXC1#255#10` → Set channel 1 to value 255 with fade time 10
- **DMXC** — RGB control on 3 consecutive channels  
  Example: `DMXR2#128128128` → Set channels 2, 3, 4 to RGB(128,128,128)


---

## 🔆 LED Behavior – Summary

| Condition                               | LED Behavior                | Description                                   |
| --------------------------------------- | --------------------------- | --------------------------------------------- |
| **Wi-Fi connected**<br>**No DMX error** | **Off (constantly)**        | Normal operation                              |
| **Wi-Fi disconnected**                  | **Slow blinking**           | 500 ms on / 500 ms off                        |
| **DMX error active**                    | **Fast blinking**           | 100 ms on / 100 ms off                        |
| **User action**<br>(e.g. Wi-Fi switch)  | **Short blinking sequence** | `n` blinks with `delay_ms` (e.g. 2× 50 ms)    |
| **Network selection via button**        | **1–3 blinks**              | Number of blinks = selected network index + 1 |



---
## 📋 Requirements

- ESP32 board (e.g., ESP32 DevKitC)
- RS485 transceiver (e.g., MAX485)
- DMX-compatible light or fixture
- Python + Jupyter (optional, for testing)

---

## 🙏 Credits

- [ESP-DMX Library](https://github.com/someweisguy/esp_dmx) by [@someweisguy](https://github.com/someweisguy)
- UDP DMX protocol by **Robert Lechner**

---

## 📃 License

MIT License — see `LICENSE` file for details.
