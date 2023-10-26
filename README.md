# WiresharkSerialAdapter
Wireshark Serial Adapter for Windows.  This addon is is to use wireshark with a serial adapter to sniff serial data.

Copy WireSharkSerialApapter.exe to the \Wireshark\extcap directory

If multiple adapters are needed make copies of WireSharkSerialAdapter.exe (Ex:  WireSharkSerialAdapter01.exe, WireSharkSerialAdapter02.exe....) 


The Adapter works by sniffing serial data by using [extcap](https://www.wireshark.org/docs/man-pages/extcap.html) into Wireshark payloads by using either User defined DLT(147-162) or RTAC Serial(250).  

RTAC requires less setup within Wireshark as it is included as a DLT.  Both DLT methods require selecting the required desector for decoding.  For RTAC this is done by right clicking on the frame within Wireshark and clicking **Decode AS**.  For User defined DLT(147-162) the protocol is selected when configing the DLT under **payload protocol**.

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

<ins>Interframe Timing Detection</ins>
- **Event:** Use the serial adapters event to detect when data has been received.
- **Polling**:  Uses timer based polling to detect when data has been received.

<ins>Interframe Timebase</ins>
- **Multipler**: 1x Modbus Character:  Uses the Modbus time based on specification multipled by the Interframe Multipler setting for detection of end of frame.
- **Multipler**: 1x Character:  Uses time per serial character(calulated by baud rate, Byte, Parity, Stop bits) multipled by the Interframe Multipler setting for detection of end of frame.
- **Delay Only**:  Uses on the delay specified in the Interframe Delay(us) for detection of end of frame.

<ins>Interframe Multipler</ins>
- Used as time multipler when selecting one of the Interframe Timebase using multipler.

<ins>Interframe Delay(us)</ins>
- Use as delay on detection of end of frame.  This time is added to any additional time to detect the end of frame on all Timebase selected.

<ins>Interframe Correction</ins>
- **Modbus CRC**:  This is used in conjuction with detecting the end of frames.  This will check the frame for the correct Modbus CRC.  If the frame has the correct CRC it will present the frame to Wireshark.   If the CRC is not correct it will analyze the frame until the point that a correct CRC frame is detected.  This help if the Interframe Timing Detection is not precise to detect the Interframe period.
- **None**:  No frame correction is applied.

## How to Compile
https://code.visualstudio.com/docs/cpp/config-mingw


Simplified from link above:
1.  Install Visual Studio Code.
 <br />&emsp; https://code.visualstudio.com/download
2.  Install the C/C++ extension for VS Code. 
<br />&emsp; Extensions view (Ctrl+Shift+X). You can install the C/C++ extension by searching for 'C++'
3.  Installing the MinGW-w64 toolchain
<br />&emsp; https://www.msys2.org
4.  In this terminal, install the MinGW-w64 toolchain by running the following command:
<br />&emsp; pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
5.  Add the path to your MinGW-w64 bin folder to the Windows PATH environment variable by using the following steps:
<br />&emsp; Open cmd as admin type setx path "%path%;C:\msys64\ucrt64\bin"
6.  Check your MinGW installation
<br />&emsp;Close existing cmd and open new type following:
<br />&emsp;&emsp; gcc --version
<br />&emsp;&emsp; g++ --version
<br />&emsp;&emsp; gdb --version
8.   Compile WireSharkSerialAdapter.cpp
<br />&emsp; 1. CTRL+F5
<br />&emsp; 2. select g++






