# WiresharkSerialAdapter
Wireshark Serial Adapter for Windows.  This addon is is to use wireshark with a serial adapter to sniff serial data.

Copy WireSharkSerialApapter.exe to the \Wireshark\extcap directory

If multiple adapters are needed make copies of WireSharkSerialAdapter.exe (Ex:  WireSharkSerialAdapter01.exe, WireSharkSerialAdapter02.exe....) 


The Adapter works by sniffing serial data by using [extcap](https://www.wireshark.org/docs/man-pages/extcap.html) into Wireshark frames by using either User defined DLT(147-162) or RTAC Serial(250).  
RTAC requires less setup within Wireshark as it is included as DLT.  Both require Right clicking on the frame within Wireshark and clicking Decode As and picking the protocol for decoding. 

## RTAC Serial(250)
![alt text](https://github.com/jzhvymetal/WiresharkSerialAdapter/blob/main/02_Wireshark%20Serial%20Adapter-Using%20RTAC%20Serial%20DLT.png)

## DLT(147-162)
![alt text](https://github.com/jzhvymetal/WiresharkSerialAdapter/blob/main/02_Wireshark%20Serial%20Adapter-Using%20User%20DLT.png)

## Hardware
Any serial adapter will work.  Any caching or latency timing need to be kept to the minimum.  Below are some examples hardware architectures.  Also shown below is how to disable latency timing on SE TCSMCNAM002P and TSXCUSB485.

![alt text](https://github.com/jzhvymetal/WiresharkSerialAdapter/blob/main/00_Wireshark%20Serial%20Adapter%20RS485%20HARDWARE.png)

![alt text](https://github.com/jzhvymetal/WiresharkSerialAdapter/blob/main/00A_Wireshark%20Serial%20Adapter%20RS485%20DISABLE%20LATENCY%20TIMER.png)

