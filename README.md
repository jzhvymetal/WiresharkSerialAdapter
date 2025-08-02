# WiresharkSerialAdapter
Wireshark Serial Adapter for Windows. This add-on allows you to use Wireshark with a serial adapter to sniff serial data.

Copy `WireSharkSerialAdapter.exe` to the `\Wireshark\extcap` directory.

If multiple adapters are needed, make copies of `WireSharkSerialAdapter.exe` (e.g., `WireSharkSerialAdapter01.exe`, `WireSharkSerialAdapter02.exe`, etc.).

The adapter works by sniffing serial data using extcap, delivering it to Wireshark payloads via either user-defined DLT (147–162) or RTAC Serial (250).

RTAC requires less setup within Wireshark since it is included as a DLT. Both DLT methods require selecting the appropriate dissector for decoding. For RTAC, this is done by right-clicking a frame in Wireshark and selecting **Decode As**. For user-defined DLT (147–162), the protocol is selected when configuring the DLT under **payload protocol**.

## RTAC Serial(250)
![alt text](https://github.com/jzhvymetal/WiresharkSerialAdapter/blob/main/02_Wireshark%20Serial%20Adapter-Using%20RTAC%20Serial%20DLT.png)

## DLT(147-162)
![alt text](https://github.com/jzhvymetal/WiresharkSerialAdapter/blob/main/02_Wireshark%20Serial%20Adapter-Using%20User%20DLT.png)

## Hardware
Any serial adapter will work.  Any caching or latency timing need to be kept to the minimum.  Below are some examples hardware architectures.  Also shown below is how to disable latency timing on SE TCSMCNAM002P and TSXCUSB485.

![alt text](https://github.com/jzhvymetal/WiresharkSerialAdapter/blob/main/00_Wireshark%20Serial%20Adapter%20RS485%20HARDWARE.png)

![alt text](https://github.com/jzhvymetal/WiresharkSerialAdapter/blob/main/00A_Wireshark%20Serial%20Adapter%20RS485%20DISABLE%20LATENCY%20TIMER.png)

## Software Settings

![alt text](https://github.com/jzhvymetal/WiresharkSerialAdapter/blob/main/99_Wireshark%20Serial%20Adapter-Software%20Settings.png)

---

<ins>**Interframe Timing Detection**</ins>

* **Event**: Uses the serial adapter's event mechanism to detect when data has been received.
* **Polling**: Uses timer-based polling to detect when data has been received.

---

<ins>**Interframe Timebase**</ins>

* **Multiplier: 1x Modbus Character**: Uses the Modbus timing specification, multiplied by the *Interframe Multiplier* setting, to detect the end of a frame.
* **Multiplier: 1x Character**: Uses the time per serial character (calculated based on baud rate, data bits, parity, and stop bits), multiplied by the *Interframe Multiplier* setting, to detect the end of a frame.
* **Delay Only**: Uses only the delay specified in *Interframe Delay (µs)* to detect the end of a frame.

---

<ins>**Interframe Multiplier**</ins>

* Used as a time multiplier when selecting an *Interframe Timebase* mode that relies on a multiplier.

---

<ins>**Interframe Delay (µs)**</ins>

* Used as an additional delay when detecting the end of a frame. This delay is added to any other timing used by the selected *Interframe Timebase* mode.

---

<ins>**Interframe Correction**</ins>

* **Modbus CRC**: Used in conjunction with end-of-frame detection. This checks each frame for a valid Modbus CRC. If the CRC is valid, the frame is presented to Wireshark. If not, the data is further analyzed to find the point where a valid CRC frame begins. This helps compensate for imprecise interframe timing.
* **None**: No frame correction is applied.

---


### **Compiling Instructions--Based on: [Visual Studio Code – C++ with MinGW](https://code.visualstudio.com/docs/cpp/config-mingw)**

1. **Install Visual Studio Code**

   Download from:
   [https://code.visualstudio.com/download](https://code.visualstudio.com/download)

2. **Install the C/C++ extension for VS Code**

   Open the Extensions view (`Ctrl+Shift+X`), then search for **C++** and install the extension by Microsoft.

3. **Install MSYS2 and the MinGW-w64 toolchain**

   Download and install from:
   [https://www.msys2.org](https://www.msys2.org)

4. **Install the required MinGW packages**

   Open the **MSYS2 UCRT64 terminal**, then run:

   ```bash
   pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
   ```

5. **Add MinGW to your system PATH**

   Open Command Prompt as Administrator and run:

   ```cmd
   setx path "%path%;C:\msys64\ucrt64\bin"
   ```

6. **Verify your MinGW installation**

   Close and reopen the Command Prompt, then run:

   ```bash
   gcc --version  
   g++ --version  
   gdb --version
   ```

7. **Compile `WireSharkSerialAdapter.cpp` in VS Code**

        1. Open the project folder in VS Code
        2. Press `Ctrl+F5` to build and run
        3. When prompted, select **g++** as the compiler




