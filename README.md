---

# Argus Infusion Pump - Hardware Control (Zephyr RTOS) ⚙️💉

This repository contains the low-level firmware for the Argus Infusion Pump, designed to run on STM32 family microcontrollers (Blackpill). Developed on top of the **Zephyr RTOS**, this module is responsible for all Hard Real-Time critical operations, including motor control, sensor acquisition, and safety management (hardware-level alarms).

This firmware acts as the main actuator and sensing node, receiving commands and reporting telemetry to the **IoT Gateway (Raspberry Pi)** through an isolated communication bus.

## ✨ Architecture & Key Features

* **Zephyr RTOS:** Utilization of preemptive threads, message queues, and hardware timers to ensure deterministic execution of the infusion control.
* **Closed-Loop Control:** Integration of the `motor_driver` with the `encoder` to ensure absolute volumetric accuracy and stall detection (mechanical jam).
* **Critical Analog Monitoring (`adc_driver`):** High-speed reading for pressure sensors (occlusion detection) and optical/ultrasonic sensors (air bubble detection).
* **Robust Communication Protocol:** Data exchange via SPI/UART with the Linux Gateway, encapsulated by `protocol_defs.h` and strictly validated by a Cyclic Redundancy Check (`utl_crc16.c`) to prevent the injection of commands corrupted by EMI noise.
* **Secondary OTA (Firmware Update):** Implementation of a native `ota_handler` that allows the Linux Gateway to flash new versions of this firmware onto the STM32 remotely, without physical intervention.
* **Isolated State Machine (`logic_engine`):** The control logic (Infusing, Paused, Alarm, KVO) runs independently of the Gateway. If communication with Linux fails, the pump automatically enters a safe state.

## 📂 Project Structure

The codebase follows the standard Zephyr module structure:

* **`app.overlay`**: Device Tree Overlay. Maps the STM32 peripherals (Motor Pins, ADC, SPI, I2C) to generic Zephyr APIs.
* **`prj.conf`**: Kconfig configurations (Enables drivers, thread stack sizes, C++ support, logging).
* **`include/` & `src/**`:
* `hub.*`: Central orchestration point for threads and RTOS message routing.
* `logic_engine.*`: Finite State Machine (FSM) that dictates the pump's clinical behavior.
* `motor_driver.*` & `encoder.*`: Stepper motor control and real position reading.
* `adc_driver.*`: Abstraction for sampling critical sensors.
* `cmd.*` & `protocol_defs.h`: Routing of commands received from the Gateway.
* `ota_handler.*`: Internal Flash memory write logic for updates.


* **`utl/`**: Critical utility functions.
* `utl_crc16.*`: Data integrity validation (Safety-critical).


* **`test/`**: C++ scripts (`ota_master.cpp`, `spi_loopback.cpp`) used by the Gateway/Host PC to simulate and validate the communication buses against the STM32.

## 🚀 How to Build and Flash

This project utilizes the `west` meta-tool from the Zephyr ecosystem.

1. **Compile the Firmware:**
```bash
west build -b blackpill_f411ce .

```


*(Replace the base board if you are using the F401 variant).*
2. **Flash via ST-Link/DFU:**
```bash
west flash

```


*Alternatively, use the `flash_firmware.txt` script if you are using custom OpenOCD tools.*

## 🛠️ Authorship

Developed by **Stephan Costa Barros**.

---

