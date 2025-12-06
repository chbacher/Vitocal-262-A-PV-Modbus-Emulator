# Vitocal-262-A-PV-Modbus-Emulator

Modbus RTU Slave emulator for the Viessmann ALE3D5F Energy Meter. This application deployed on a Raspberry Pi with a USB RS485 adapter (e.g. DSD TECH SH U10) dynamically simulates power registers (R37 R42 R47) to enable PV surplus utilization within the Vitocal 262-A heat pump. Power data is injected via a separate TCP/IP socket connection.


## Why I Built This

After growing frustrated with Viessmann's restrictive and expensive approach—and repeatedly encountering the same "official meter required" response from both my electrician and Viessmann support—I decided to develop this emulator solution.

It is important to emphasize that **this is not my original idea.** The concept originates from the work of Flocki and other contributors in the OpenWB forum. However, their original code was outdated and incompatible with newer Vitocal heat pump models and their updated Modbus protocol. This project builds upon and modernizes those early efforts—refactoring, documenting, and simplifying the implementation to create a reliable and accessible solution.

My goal is to make it easier for other frustrated Viessmann customers to achieve seamless smart-home integration without relying on expensive proprietary hardware. 

---

## The Problem: Achieving True PV Surplus Control with the Vitocal 262-A

The Viessmann Vitocal 262-A heat pump family supports photovoltaic (PV) surplus usage (Eigenstromnutzung) for domestic hot water heating. However, Viessmann's implementation requires communication with a dedicated, proprietary energy meter (a rebranded Saia-Burgess ALE3D5F) via Modbus RTU.

This mandatory requirement creates practical issues for users seeking flexible smart home integration:

1. **High Cost and Limited Integration:** The required energy meter is expensive and functions as a closed, inflexible component, making seamless integration with modern energy management systems (e.g., Loxone, openWB, Home Assistant) difficult.

2. **Vendor Lock-in:** Official Viessmann support typically insists on the proprietary meter as the only supported method to enable PV surplus mode, dismissing alternative or custom integration approaches.

3. **Limited Control Logic:** Relying solely on the heat pump's internal logic—whether via Modbus or SG-Ready contacts—only provides fixed or coarse switching thresholds (e.g., activating only the heat pump, or activating both the heat pump and electric heating element). It does not allow dynamic, granular, or externally calculated surplus power control.

---

## The Solution: Modbus RTU Emulation

This project eliminates the dependency on the proprietary energy meter. By running a Modbus RTU Slave emulator on an affordable controller (such as a Raspberry Pi Zero 2 W or similar Linux device) with a standard USB-to-RS485 adapter, we can accurately simulate the presence of the required energy meter.

An external energy management system can inject real-time, calculated PV surplus power values directly into the Modbus registers (R37, R42, R47, and R50) that the Vitocal 262-A continuously polls.

This approach enables full, customized control—allowing users to define precise activation thresholds, such as:

-   Engaging the heat pump at **800 W** of simulated surplus power
-   Activating both the heat pump and electric heating element at **3.0 kW** of simulated surplus

Crucially, this approach does not require physically modifying the heat pump's hardware. However, please be aware of the following:

### ⚠️ Warranty Implications

Using this emulation method **may void or affect the warranty** on your Vitocal 262‑A heat pump or any connected Viessmann equipment, as you are deviating from the manufacturer's prescribed installation and communication method. Viessmann may refuse service or support if they determine that non‑approved components or software were used.

### ⚠️ No Liability Assumed

This project is provided as open‑source software for educational and experimental use. The author **assumes no responsibility or liability** for any damages, malfunctions, safety issues, or financial losses that may result from using this solution.

### ⚡ Electrical Safety Warning

Working with mains voltage, RS485 wiring, and heat pump equipment carries **serious risk of electric shock, fire, or equipment damage.** If you are not qualified or confident in handling electrical installations, **you must consult a licensed electrician** to install and connect any cables between the emulator and the heat pump. **Never work on live circuits.**

---

## Hardware Requirements (What I Used)

To build this setup, you might need:

- **Controller:** Raspberry Pi Zero 2 W running [DietPi OS](https://dietpi.com/) (lightweight, optimized for Raspberry Pi)
- **Modbus RTU Adapter:** DSD TECH SH U10 (USB to RS485) (e.g. Amazon, Aliexpress)
- **Modbus Cable:** Shielded 3-wire cable (A+, B-, Gnd) for RS485 connection (Amazon)
- **Modbus Connector:** Terminal Block Connector 5.08 mm / Straight Pin Header Socket (Aliexpress)
- **Power Supply:** Standard 5V USB-C power adapter for the Raspberry Pi
- **Network:** Optional: Additional USB-to-Ethernet adapter (Wi-Fi would also work) (Amazon, Aliexpress)

**Note:** You can substitute the controller with any Linux-capable device (e.g., standard Raspberry Pi, old laptop, or NAS). Same goes with the OS.
