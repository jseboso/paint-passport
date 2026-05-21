# Firmware Setup & Development Guide

## Arduino IDE Configuration

### 1. Board & Package Setup
- **Board Package:** ESP32 by Espressif Systems (Install via **Boards Manager**)
- **Target Board:** Select your specific ESP32 board (e.g., `ESP32S3 Dev Module`)

---

### 2. Tools Menu Settings

Ensure the following configuration is selected under the **Tools** menu before flashing:

| Setting | Value |
|---|---|
| **PSRAM** | **OPI PSRAM** *(8MB embedded PSRAM is OPI/octal despite quad flash)* |
| **Flash Size** | **4MB (32Mb)** |
| **Flash Mode** | **QIO 80MHz** |
| **Partition Scheme** | **Default 4MB with spiffs (1.2MB APP / 1.5MB SPIFFS)** |
| **USB CDC On Boot** | **Disabled** |
| **Upload Speed** | `921600` |

---