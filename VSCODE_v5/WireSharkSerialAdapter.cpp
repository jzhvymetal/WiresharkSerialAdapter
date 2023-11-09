#include <iostream>
#include <windows.h>
#include <unistd.h>
#include <math.h> //USED FOR CEIL
#include <vector>
#include <queue>
#include <chrono> //Used for System Time
#include <fstream> //USED for File Handling
#include <stdio.h>

using namespace std;

const double ProgramVersion=2.0;
const std::string WireSharkAdapterName="Serial Port Adapter";
const boolean GuiMenu=true;
std::string FileName="";
std::string Port="6";
std::string BaudRate="19200";
std::string ByteSize="8";
std::string StopBits="1";
std::string Parity="NONE";
std::string FrameTiming="event";
std::string FrameTimebase="modbus_multi";
std::string FrameMulti="2.0";
std::string FrameDelay="0.0";
std::string FrameCorrect="none";
std::string WiresharkDLT="250";
std::string CaptureOutputPipeName="";
std::string ControlInPipeName="";
std::string ControlOutPipeName="";

#define DLT_RTAC_SERIAL 250
static HANDLE hComSerial = INVALID_HANDLE_VALUE;
static HANDLE hCommReadThread = INVALID_HANDLE_VALUE;
static HANDLE hCommFrameReadyEvent = INVALID_HANDLE_VALUE;
static HANDLE hCommFrameQueuedEvent= INVALID_HANDLE_VALUE;
static HANDLE hCaptureOutputPipe = INVALID_HANDLE_VALUE;     /* pipe handle */
static HANDLE hControlInPipe = INVALID_HANDLE_VALUE;     /* pipe handle */
static HANDLE hControlInThread = INVALID_HANDLE_VALUE;    
static HANDLE hControlInEvent = INVALID_HANDLE_VALUE;    
static HANDLE hControlOutPipe = INVALID_HANDLE_VALUE;     /* pipe handle */

typedef unsigned char typeFrameByte;
typedef std::vector<typeFrameByte> typeFrameVector;
typedef std::queue< typeFrameVector > typeFrameQueue;
typeFrameQueue  InputQueue;
std::vector<std::string> ComPortStringVector;
typeFrameVector FragmentVector;
typeFrameVector SerialFrameVector;

void SendWireSharkControl(BYTE bControlNumber, BYTE bCommand, std::string sPayload);

void print_hex(typeFrameByte frame[], int len, bool space=false)
{
  unsigned int RX_INT;
  for (int pos = 0; pos < len; pos++)
  {
    RX_INT=(unsigned char) frame[pos];
    cerr << hex << ((RX_INT<16)?"0":"") << (RX_INT & 0xFF);
    if(space)cerr<<" ";
  }
}

DWORD WINAPI ComReadThreadFunc(LPVOID lpParam)
{
    std::string mPORT ("\\\\.\\COM" + Port);
    DWORD       mBaudRate=std::stoi(BaudRate);
    BYTE        mByteSize=std::stoi(ByteSize);
    BYTE        mStopBits=ONESTOPBIT;
    if (StopBits.compare("2")==0) mStopBits=TWOSTOPBITS;
    BYTE        mParity=NOPARITY;
    if (Parity.compare("NONE")==0) mParity=NOPARITY;
    if (Parity.compare("ODD")==0)  mParity=ODDPARITY;
    if (Parity.compare("EVEN")==0) mParity=EVENPARITY;

    hComSerial = CreateFileA( mPORT.c_str(), // TEXT("\\\\.\\COM3"),
    GENERIC_READ | GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE, // 0,    // exclusive access
    NULL, // default security attributes
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL, //FILE_ATTRIBUTE_NORMAL //FILE_FLAG_OVERLAPPED
    NULL
    );

    // Do some basic settings
    BOOL fSuccess;
    DCB serialParams = { 0 };
    serialParams.DCBlength = sizeof(DCB);
    fSuccess=GetCommState(hComSerial, &serialParams);

    serialParams.BaudRate = mBaudRate; //CBR_115200;
    serialParams.ByteSize = mByteSize; //8;
    serialParams.Parity = mParity; //EVENPARITY;
    serialParams.StopBits = mStopBits; //ONESTOPBIT;
    //set flow control="hardware"
    serialParams.fOutxCtsFlow=false;
    serialParams.fOutxDsrFlow=false;
    serialParams.fDtrControl=0;
    serialParams.fDsrSensitivity=false;
    serialParams.fTXContinueOnXoff=false;
    serialParams.fOutX=false;
    serialParams.fInX=false;
    serialParams.fNull=false;
    serialParams.fRtsControl=0;

    fSuccess=SetCommState(hComSerial, &serialParams);
    GetCommState(hComSerial, &serialParams);


    // Modbus states that a baud rate higher than 19200 must use a fixed 750 us
    // for inter character time out and 1.75 ms for a frame delay.
    // For baud rates below 19200 the timing is more critical and has to be calculated.
    // E.g. 9600 baud in a 10 bit packet is 960 characters per second
    // In milliseconds this will be 960characters per 1000ms. So for 1 character
    // 1000ms/960characters is 1.04167ms per character and finally modbus states an
    // intercharacter must be 1.5T or 1.5 times longer than a normal character and thus
    // 1.5T = 1.04167ms * 1.5 = 1.5625ms. A frame delay is 3.5T.
    double dblBitsPerByte = 1 + serialParams.ByteSize + ( serialParams.StopBits==TWOSTOPBITS? 2 : 1 ) + ( serialParams.Parity>0? 1 : 0 );
    double Serial_1_Char=dblBitsPerByte / (double)serialParams.BaudRate * 1000000.0f; //us per char
    if(serialParams.BaudRate>19200 && strcmp(FrameTimebase.c_str(), "modbus_multi") == 0)
    {
        Serial_1_Char=500;
    }
    double ReadFrameDelay=std::stod(FrameDelay);
    double ReadFrameMulti=std::stod(FrameMulti);

    double ReadTimeOut=((ReadFrameMulti * Serial_1_Char)+ReadFrameDelay);
    if (strcmp(FrameTimebase.c_str(), "char_delay") == 0)ReadTimeOut=ReadFrameDelay;


    // Set timeouts 
    COMMTIMEOUTS timeout = { 0 };
    if(strcmp(FrameTiming.c_str(), "polling") == 0)
    {
        // ReadIntervalTimeout with a value of MAXDWORD, combined with zero values for both the 
        // ReadTotalTimeoutConstant and ReadTotalTimeoutMultiplier members, specifies 
        // that the read operation is to return immediately with the bytes that 
        // have already been received, even if no bytes have been received.  
        timeout.ReadIntervalTimeout = MAXDWORD;
        timeout.ReadTotalTimeoutMultiplier = 0;
        timeout.ReadTotalTimeoutConstant =  0; 
        SendWireSharkControl(4, 2,"Frame Timing Polling\n");
    }
    else
    {
        //One of the following below occurs when the ReadFile function is called if an application sets 
        // ReadIntervalTimeout=MAXDWORD;
        // ReadTotalTimeoutMultiplier=MAXDWORD; 
        // MAXDWORD > ReadTotalTimeoutConstant > 0 

        // *If there are any bytes in the input buffer, ReadFile returns immediately with the bytes in the buffer.
        // *If there are no bytes in the input buffer, ReadFile waits until a byte arrives and then returns immediately.
        // *If no bytes arrive within the time specified by ReadTotalTimeoutConstant, ReadFile times out.
        timeout.ReadIntervalTimeout = MAXDWORD; 
        timeout.ReadTotalTimeoutMultiplier = MAXDWORD;
        timeout.ReadTotalTimeoutConstant =  1; //Fixed 1ms Timeout
        SendWireSharkControl(4, 2,"Frame Timing Event\n");
    }
    timeout.WriteTotalTimeoutMultiplier = 0;
    timeout.WriteTotalTimeoutConstant = 0;
    SetCommTimeouts(hComSerial, &timeout);

    typeFrameByte RX_CHAR[1024];
    int RX_LEN;
    DWORD dwCommEvent;

    if (!SetCommMask(hComSerial, EV_RXCHAR));   // Error setting communications event mask.
    PurgeComm(hComSerial, PURGE_RXCLEAR|PURGE_TXCLEAR);
    ReadFile(hComSerial, &RX_CHAR, 1023, (LPDWORD)((void *)&RX_LEN), NULL);
    
    bool b;
    double EV_Timeout=0;
    LARGE_INTEGER freq, start, end;

    while(hComSerial !=INVALID_HANDLE_VALUE)
    {
        if (WaitCommEvent(hComSerial, &dwCommEvent, NULL))
        { 
            do
            {
                RX_LEN=0;
                b=ReadFile(hComSerial, &RX_CHAR, 1023, (LPDWORD)((void *)&RX_LEN), NULL);
                if (RX_LEN>0 && b)
                {
                    // A Data has been read; process it.
                    SerialFrameVector.insert(SerialFrameVector.end(), RX_CHAR, RX_CHAR+RX_LEN);
                    QueryPerformanceCounter(&start);
                    EV_Timeout = 0;
                }
                else
                {
                    if(timeout.WriteTotalTimeoutConstant==0) 
                    {
                        usleep(static_cast<useconds_t>(ReadTimeOut/2)); //Wait half the readtimeout to free CPU resource and wait for more data to be recieved
                        QueryPerformanceFrequency(&freq);
                        QueryPerformanceCounter(&end);
                        // subtract before dividing to improve precision
                        EV_Timeout = static_cast<double>(end.QuadPart - start.QuadPart) / static_cast<double>(freq.QuadPart/1000000);
                    }
                    else
                    {
                        EV_Timeout = EV_Timeout + 1000;  //Using fixed time of 1ms for ReadFile timeout
                    }
                }
            } 
            while(EV_Timeout<ReadTimeOut);
            
            if(!SerialFrameVector.empty())
            {                
                SetEvent(hCommFrameReadyEvent); //Send Frame to be buffered to Queue
                WaitForSingleObject(hCommFrameQueuedEvent, INFINITE);    // indefinite wait for event frame Queue Event 
                ResetEvent(hCommFrameQueuedEvent);
                SerialFrameVector.clear();
            }
        }
    }
    return 0;
}

int CreateComThread()
{
    int RetValue=0;

    hCommReadThread = CreateThread(NULL, 0, ComReadThreadFunc, NULL, 0, NULL);

    hCommFrameQueuedEvent = CreateEvent( 
    NULL,               // default security attributes
    TRUE,               // manual-reset event
    FALSE,              // initial state is nonsignaled
    TEXT("FrameQueuedEvent")  // object name
    ); 

    hCommFrameReadyEvent = CreateEvent( 
    NULL,               // default security attributes
    TRUE,               // manual-reset event
    FALSE,              // initial state is nonsignaled
    TEXT("CommFrameReadyEvent")  // object name
    ); 

    if (    hCommReadThread == INVALID_HANDLE_VALUE || 
            hCommFrameReadyEvent == INVALID_HANDLE_VALUE || 
            hCommReadThread==NULL || 
            hCommFrameReadyEvent==NULL ||
            hCommFrameQueuedEvent==NULL || 
            hCommFrameQueuedEvent==NULL)
    {
        cout<<"Creating Thread Error";
        CloseHandle(hCommReadThread);
        CloseHandle(hCommFrameReadyEvent);
        CloseHandle(hCommFrameQueuedEvent);
    }
    else
    {
        RetValue=1;
    }

    return(RetValue);
}

int CRC16_2(typeFrameByte  frame[], int len)
{
  int crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++)
  {
    crc ^= (int)frame[pos];          // XOR byte into least sig. byte of crc

    for (int i = 8; i != 0; i--) {    // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;
        crc&= 0x7FFF;                  // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }
   // Reverse byte order.
  crc = ((crc  << 8) | (crc >> 8)) & 0xFFFF;
  return crc;
}

bool CRC_OK(typeFrameByte  frame[], int len)
{
    unsigned int CALC_CRC;
    unsigned int FRAME_CRC;
    if(len>2)
    {
        CALC_CRC=CRC16_2(frame,len-2);
        FRAME_CRC=(unsigned int) frame[len-2]<< 8 | (unsigned int) frame[len-1];
    }
    return((CALC_CRC==FRAME_CRC)&&(len>2));
}


int FindModbusFrameEnd(typeFrameByte frame[], int len)
{
    int ReturnVal=0;
    int QueryCrcLoc=0;
    int ResponseCrcLoc=0;

    int cur_len=len;
    typeFrameByte *cur_frame=frame;
    int frame_start=0;

    bool ResponseSizeOK=FALSE;
    bool QuerySizeOK=FALSE;
    bool ResponseCrcOK=FALSE;
    bool QueryCrcOK=FALSE;

    for(; (frame_start<len-2) && ReturnVal==0; frame_start++)
    {
      ResponseSizeOK=FALSE;
      QuerySizeOK=FALSE;
      ResponseCrcOK=FALSE;
      QueryCrcOK=FALSE;
      QueryCrcLoc=0;
      ResponseCrcLoc=0;

      cur_len=len-frame_start;
      cur_frame=frame+frame_start;
      if(cur_len>=3)  //Smallest possible frame is 3 bytes because 2 bytes need for CRC
      {
        typeFrameByte ModbusFuction=cur_frame[1];
        switch ( ModbusFuction )
        {
            case 1:     //Read Coil Status (FC=01)
            case 2:     //Read Input Status (FC=02)
            case 3:     //Read Holding Registers (FC=03)
            case 4:     //Read Input Registers (FC=04)
                QueryCrcLoc=6;
                if(cur_len-1>2)ResponseCrcLoc=(int)cur_frame[2]+ 3;
                break;
            case 5:     //Force Single Coil (FC=05)
            case 6:     //Preset Single Register (FC=06)
                QueryCrcLoc=6;
                ResponseCrcLoc=6;
                break;
            case 11:        //Fetch Comm Event Counter (FC=11)
                QueryCrcLoc=2;
                ResponseCrcLoc=6;
                break;
            case 12:        //Fetch Comm Event Log (FC=012)
            case 17:        //Report Response ID (FC=017)
                QueryCrcLoc=2;
                if(cur_len-1>2)ResponseCrcLoc=(int)cur_frame[2]+ 3;
                break;
            case 15:        //Force Multiple Coils (FC=15)
            case 16:        //Preset Multiple Registers (FC=16)
                if(cur_len-1>6)QueryCrcLoc=(int)cur_frame[6]+ 7;
                ResponseCrcLoc=6;
                break;
            case 20:        //Read General Reference (FC=20)
            case 21:        //Write General Reference (FC=21)
                if(cur_len-1>3)QueryCrcLoc=(int)cur_frame[2]+ 3;
                if(cur_len-1>3)ResponseCrcLoc=(int)cur_frame[2]+ 3;
                break;
            case 22:        //Mask Write 4X Register (FC=22)
                QueryCrcLoc=8;
                ResponseCrcLoc=8;
                break;
            case 23:        //Preset Read/Write Multiple Registers (FC=23)
                if(cur_len-1>10)QueryCrcLoc=(int)cur_frame[10]+ 11;
                if(cur_len-1>3)ResponseCrcLoc=(int)cur_frame[2]+ 3;
                break;
            case 24:        //Read FIFO Queue (FC=24)
                QueryCrcLoc=4;
                if(cur_len-1>4)ResponseCrcLoc=(int)cur_frame[4]+ 4;
                break;
            default:
                if((0x80 & cur_frame[1])>0)
                {
                    ResponseCrcLoc=3;
                    QueryCrcLoc=ResponseCrcLoc;  //Not Possible Request use Response size
                }
                else
                {
                    //Could be unknown function try finding CRC
                    for(int i=3; ((i<cur_len-2) && ReturnVal==0); i++)
                    {
                            if (CRC_OK(cur_frame, i))
                            {
                                ReturnVal=i;
                            }
                    }
                }
                break;
            }
      }
        ResponseSizeOK=(ResponseCrcLoc>0 && ResponseCrcLoc+2<=cur_len);
        QuerySizeOK=(QueryCrcLoc>0 && QueryCrcLoc+2<=cur_len);
        ResponseCrcOK=FALSE;
        QueryCrcOK=FALSE;

        if(ResponseSizeOK)
        {
            ResponseCrcOK=CRC_OK(cur_frame, ResponseCrcLoc+2);
            if(ResponseCrcOK)ReturnVal=ResponseCrcLoc+2;
        }

        if(QuerySizeOK)
        {
            QueryCrcOK=CRC_OK(cur_frame, QueryCrcLoc+2);
            if(QueryCrcOK)ReturnVal=QueryCrcLoc+2;
        }
    }
    if(frame_start!=1 && ReturnVal>0)ReturnVal=-1*(frame_start-1); //Frame has fragment prefix but found valid frame after
    return(ReturnVal);
}

void SendWireSharkControl(BYTE bControlNumber, BYTE bCommand, std::string sPayload)
{
    typeFrameVector OutVector;
    typeFrameByte *ptr;

    char cSyncPipeIndication='T';
    ptr=(typeFrameByte *) &cSyncPipeIndication;
    OutVector.insert(OutVector.end(), ptr, ptr + sizeof(cSyncPipeIndication));
    BYTE bZero=0;
    ptr=(typeFrameByte *) &bZero;
    OutVector.insert(OutVector.end(), ptr, ptr + sizeof(bZero));
    WORD wMessageLength=sPayload.length() + 2;
    int x = 1;
    if(*(char*)&x==1) //Little Ended Swap Bytes for network order
    {
        wMessageLength = (wMessageLength>>8)|((wMessageLength&0xff)<<8);
    }
    ptr=(typeFrameByte *) &wMessageLength;
    OutVector.insert(OutVector.end(), ptr, ptr + sizeof(wMessageLength));
    ptr=(typeFrameByte *) &bControlNumber;
    OutVector.insert(OutVector.end(), ptr, ptr + sizeof(bControlNumber));
    ptr=(typeFrameByte *) &bCommand;
    OutVector.insert(OutVector.end(), ptr, ptr + sizeof(bCommand));
    if(sPayload.length()>0) 
    {
        ptr=(typeFrameByte *) sPayload.data();
        OutVector.insert(OutVector.end(), ptr, ptr + sPayload.length());
    }

    DWORD dwWritten;
    WriteFile(hControlOutPipe, OutVector.data(), OutVector.size(), &dwWritten, NULL);

}

typeFrameVector WireSharkPacket(typeFrameByte  frame[], int len)
{
    static bool xHeaderWritten=false;
    typeFrameVector OutVector;
    typeFrameByte *ptr;
    DWORD network=std::stoul(WiresharkDLT);


    if (!xHeaderWritten)
    {
        xHeaderWritten=true; //Write Header Once
        

        DWORD magic_number = 0xa1b2c3d4; /* magic number */
        WORD version_major = 2; /* major version number */
        WORD version_minor = 4; /* minor version number */
        DWORD thiszone = 0;       /* GMT to local correction */
        DWORD sigfigs = 0;       /* accuracy of timestamps */
        DWORD snaplen = 65535;   /* max length of captured packets, in octets */
        

        ptr=(typeFrameByte *) &magic_number;
        OutVector.insert(OutVector.end(), ptr, ptr + sizeof(magic_number));
        ptr=(typeFrameByte *) &version_major;
        OutVector.insert(OutVector.end(), ptr, ptr + sizeof(version_major));
        ptr=(typeFrameByte *) &version_minor;
        OutVector.insert(OutVector.end(), ptr, ptr + sizeof(version_minor));
        ptr=(typeFrameByte *) &thiszone;
        OutVector.insert(OutVector.end(), ptr, ptr + sizeof(thiszone));
        ptr=(typeFrameByte *) &sigfigs;
        OutVector.insert(OutVector.end(), ptr, ptr + sizeof(sigfigs));
        ptr=(typeFrameByte *) &snaplen;
        OutVector.insert(OutVector.end(), ptr, ptr + sizeof(snaplen));
        ptr=(typeFrameByte *) &network;
        OutVector.insert(OutVector.end(), ptr, ptr + sizeof(network));
    }

    auto time_now = chrono::system_clock::now();
    auto epoch_now = std::chrono::duration_cast<std::chrono::seconds>(time_now.time_since_epoch()).count();
    auto us_now=std::chrono::duration_cast<std::chrono::microseconds>(time_now.time_since_epoch()).count();
    us_now=us_now-epoch_now*1000000;

    DWORD dwEPOCH=(DWORD)epoch_now;
    DWORD dwMicroSec=(DWORD)us_now;
    DWORD dwPacketLen1=(DWORD)len;
    DWORD dwPacketLen2=(DWORD)len;
     if(network==DLT_RTAC_SERIAL)
     {
        dwPacketLen1=(DWORD)len+12;
        dwPacketLen2=(DWORD)len+12;
     }

    ptr=(typeFrameByte *) &dwEPOCH;
    OutVector.insert(OutVector.end(), ptr, ptr + sizeof(dwEPOCH));
    ptr=(typeFrameByte *) &dwMicroSec;
    OutVector.insert(OutVector.end(), ptr, ptr + sizeof(dwMicroSec));
    ptr=(typeFrameByte *) &dwPacketLen1;
    OutVector.insert(OutVector.end(), ptr, ptr + sizeof(dwPacketLen1));
    ptr=(typeFrameByte *) &dwPacketLen2;
    OutVector.insert(OutVector.end(), ptr, ptr + sizeof(dwPacketLen2));
     if(network==DLT_RTAC_SERIAL)
     {
        DWORD dwRTAC_RelativeTimeStampL=0;
        DWORD dwDuration=0;
        //Reverse Bytes to use proper value
        dwDuration=(dwDuration & 0x000000FFU) << 24 | (dwDuration & 0x0000FF00U) << 8 | (dwDuration & 0x00FF0000U) >> 8 | (dwDuration & 0xFF000000U) >> 24;
        DWORD dwRTAC_RelativeTimeStampR=dwDuration;
        BYTE  bRTAC_SerialEventType=0x06; // CAPTURE_COMPLETE
        BYTE  bRTAC_UARTState=0;
        WORD  wRTAC_Footer=0;
        ptr=(typeFrameByte *) &dwRTAC_RelativeTimeStampL;
        OutVector.insert(OutVector.end(), ptr, ptr + sizeof(dwRTAC_RelativeTimeStampL));
        ptr=(typeFrameByte *) &dwRTAC_RelativeTimeStampR;
        OutVector.insert(OutVector.end(), ptr, ptr + sizeof(dwRTAC_RelativeTimeStampR));
        ptr=(typeFrameByte *) &bRTAC_SerialEventType;
        OutVector.insert(OutVector.end(), ptr, ptr + sizeof(bRTAC_SerialEventType));
        ptr=(typeFrameByte *) &bRTAC_UARTState;
        OutVector.insert(OutVector.end(), ptr, ptr + sizeof(bRTAC_UARTState));
        ptr=(typeFrameByte *) &wRTAC_Footer;
        OutVector.insert(OutVector.end(), ptr, ptr + sizeof(wRTAC_Footer));
     }
    OutVector.insert(OutVector.end(), frame, frame+len);

    return(OutVector);
}

void Output_Frame(typeFrameByte frame[], int len)
{
    //print_hex(frame, len);
    //cout<<" I: "<<InputQueue.size();

    typeFrameVector OutVector=WireSharkPacket(frame, len);
    //DEBUG
    //print_hex(OutVector.data() , OutVector.size());
    //cout<<endl<<endl;
    DWORD dwWritten;
    WriteFile(hCaptureOutputPipe, OutVector.data(), OutVector.size(), &dwWritten, NULL);

}

void ProcessModbusFragmentFrames()
{
    //Finds and fixes fragmented frames based on ModbusCRC 
    while(!InputQueue.empty())
    {
    //Frame CRC was checked is good.  Should be valid frame
    if(CRC_OK(InputQueue.front().data() , InputQueue.front().size()))
    {
        //Fragment frame with no valid CRC
        if (!FragmentVector.empty())
        {
            //Output Fragment frame or invalid CRC
            Output_Frame(FragmentVector.data() , FragmentVector.size());
            //Clear fragment
            FragmentVector.clear();
        }
        //Output a valid frame with good CRC.  Interframe time was correct
        Output_Frame(InputQueue.front().data() , InputQueue.front().size());
    }
    else  //Bad CRC need to try to Collect more fragments or find new frame with correct CRC
    {
        int frame_len;
        int FrameEndLoc=0;
        frame_len=InputQueue.front().size();
        typeFrameByte *frame=InputQueue.front().data();
        FragmentVector.insert(FragmentVector.end(), frame, frame+frame_len);
        bool FoundModbusFrame=false;

        do
        {
            frame_len=FragmentVector.size();
            frame=FragmentVector.data();

            FrameEndLoc=FindModbusFrameEnd(frame,frame_len);
            if (FrameEndLoc>0)
            {
                //Found a Modbus Frame
                FoundModbusFrame=true;
                //Output a found valid frame with good CRC.  Interframe time not correct.  Could be cause by serial port being buffered
                Output_Frame(frame , FrameEndLoc);
                    //Remote the bytes of the good frame
                FragmentVector.erase(FragmentVector.begin(), FragmentVector.begin()+FrameEndLoc);
            }
            if(FrameEndLoc<0)  //Current function Code frame length valid but CRCs not valid so find new CRC frame
            {
                //Negative number is location end of fragment
                FrameEndLoc=-1*FrameEndLoc;
                //Output Fragment frame or invalid CRC
                Output_Frame(frame, FrameEndLoc);
                //remove bad fragment
                FragmentVector.erase(FragmentVector.begin(), FragmentVector.begin()+FrameEndLoc);
            }
        }
        while(FrameEndLoc!=0 && !FragmentVector.empty()); //Frame was found check loop again to check remaining fragment
    }
    InputQueue.pop();
    }
}

void ProcessFrames()
{
     while(1)
     {
         DWORD dwWaitResult;
        dwWaitResult = WaitForSingleObject(hCommFrameReadyEvent, INFINITE);    // indefinite wait for event
        typeFrameVector FrameVector=SerialFrameVector;
        ResetEvent(hCommFrameReadyEvent);
        SetEvent(hCommFrameQueuedEvent);

         if(strcmp(FrameCorrect.c_str(), "modbus_crc")==0) 
         {
            InputQueue.push(FrameVector);
            ProcessModbusFragmentFrames();
         }
         else
         {
             Output_Frame(FrameVector.data() , FrameVector.size());
         }
    }
}

char *filename_remove_path(
    const char *filename_in)
{
    char *filename_out = (char *) filename_in;

    /* allow the device ID to be set */
    if (filename_in) {
        filename_out = strrchr(filename_in, '\\');
        if (!filename_out) {
            filename_out = strrchr(filename_in, '/');
        }
        /* go beyond the slash */
        if (filename_out) {
            filename_out++;
        } else {
            /* no slash in filename */
            filename_out = (char *) filename_in;
        }
    }

    return filename_out;
}



HANDLE CreateNamedPipe(std::string& pipe_name)
{
    HANDLE hPipe = INVALID_HANDLE_VALUE; 
    /* create the pipe */
    while (hPipe == INVALID_HANDLE_VALUE)
    {
        /* use CreateFile rather than CreateNamedPipe */
        hPipe = CreateFileA(
            pipe_name.c_str(),
            GENERIC_READ |
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
        if (hPipe != INVALID_HANDLE_VALUE) {
            break;
        }
        /* if an error occured at handle creation */
        if (!WaitNamedPipeA(pipe_name.c_str(), 20000)) {
            printf("Could not open pipe: waited for 20sec!\n"
                "If this message was issued before the 20sec finished,\n"
                "then the pipe doesn't exist!\n");
            return hPipe;
        }
    }
    ConnectNamedPipe(hPipe, NULL);

    return hPipe;
}

bool StringReplace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

void print_extcap_config_comport()
{
    std::string  argString="";

    argString+="arg {number=0}{call=--port}{display=Port}{type=selector}\n";

    //Up to 255 COM ports are supported so we iterate through all of them seeing
    //if we can open them or if we fail to open them, get an access denied or general error error.
    //Both of these cases indicate that there is a COM port at that number.
    for (int i=1; i<256; i++)
        {
        //Form the Raw device name
        char szPort[32];
        char argSubString[256];
        argSubString[0] = TEXT('\0');
        szPort[0] = TEXT('\0');
        sprintf (szPort, "\\\\.\\COM%u", i);

        //Try to open the port
        HANDLE hCom=CreateFileA(szPort, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
        DWORD dwError = GetLastError();
        if (hCom != INVALID_HANDLE_VALUE)
        {
          //The port was opened successfully
          CloseHandle(hCom);
          sprintf(argSubString,"value {arg=0}{value=%u}{display=COM%u}{default=false}\n",i,i);
          argString+=argSubString;
        }
        else
        {
            //Check to see if the error was because some other app had the port open or a general failure
            if (dwError == ERROR_ACCESS_DENIED || dwError == ERROR_GEN_FAILURE || dwError == ERROR_SHARING_VIOLATION || dwError == ERROR_SEM_TIMEOUT)
            {
                sprintf(argSubString,"value {arg=0}{value=%u}{display=COM%u (IN USE)}{default=false}\n",i,i);
                argString+=argSubString;
            }
        }
    }
    printf("%s", argString.c_str());
}

void print_extcap_config_baud()
{
    std::string  argString="";
    argString+="arg {number=1}{call=--baud}{display=Baud Rate}{type=selector}\n";
    argString+="value {arg=1}{value=1200}{display=1200}{default=false}\n";
    argString+="value {arg=1}{value=2400}{display=2400}{default=false}\n";
    argString+="value {arg=1}{value=4800}{display=4800}{default=false}\n";
    argString+="value {arg=1}{value=9600}{display=9600}{default=false}\n";
    argString+="value {arg=1}{value=14400}{display=14400}{default=false}\n";
    argString+="value {arg=1}{value=19200}{display=19200}{default=true}\n";
    argString+="value {arg=1}{value=38400}{display=38400}{default=false}\n";
    argString+="value {arg=1}{value=56000}{display=56000}{default=false}\n";
    argString+="value {arg=1}{value=57600}{display=57600}{default=false}\n";
    argString+="value {arg=1}{value=76800}{display=76800}{default=false}\n";
    argString+="value {arg=1}{value=115200}{display=115200}{default=false}\n";
    argString+="value {arg=1}{value=128000}{display=128000}{default=false}\n";
    argString+="value {arg=1}{value=256000}{display=256000}{default=false}\n";
    printf("%s", argString.c_str());
}

void print_extcap_config_bytesize()
{
    std::string  argString="";  
    argString+="arg {number=2}{call=--byte}{display=Byte Size}{type=selector}\n";
    argString+="value {arg=2}{value=8}{display=8}{default=true}\n";
    argString+="value {arg=2}{value=7}{display=7}{default=false}\n";
    printf("%s", argString.c_str());
}

void print_extcap_config_parity()
{
    std::string  argString="";  
    argString+="arg {number=3}{call=--parity}{display=Parity}{type=selector}\n";
    argString+="value {arg=3}{value=NONE}{display=NONE}{default=true}\n";
    argString+="value {arg=3}{value=ODD}{display=ODD}{default=false}\n";
    argString+="value {arg=3}{value=EVEN}{display=EVEN}{default=false}\n";
    printf("%s", argString.c_str());
}

void print_extcap_config_stopbits()
{
    std::string  argString="";  
    argString+="arg {number=4}{call=--stop}{display=Stop Bits}{type=selector}\n";
    argString+="value {arg=4}{value=1}{display=1}{default=true}\n";
    argString+="value {arg=4}{value=2}{display=2}{default=false}\n";
    printf("%s", argString.c_str());
}

void print_extcap_config_interframe()
{
    std::string  argString="";  
    argString+="arg {number=5}{call=--frame_timing}{display=Interframe Timing Detection}{type=selector}\n";
    argString+="value {arg=5}{value=event}{display=Event}{default=true}\n";
    argString+="value {arg=5}{value=polling}{display=Polling}{default=false}\n";
    argString+="arg {number=6}{call=--frame_timebase}{display=Interframe Timebase}{type=selector}\n";
    argString+="value {arg=6}{value=modbus_multi}{display=Multipler :1X Modbus Character}{default=true}\n";
    argString+="value {arg=6}{value=char_multi}{display=Multipler: 1X Character}{default=false}\n";
    argString+="value {arg=6}{value=char_delay}{display=Delay Only}{default=false}\n";
    argString+="arg {number=7}{call=--frame_multi}{display=Interframe Multipler}{type=double}{range=1,15}{default=3.0}\n";
    argString+="arg {number=8}{call=--frame_delay}{display=Interframe Delay(us)}{type=double}{range=0,15}{default=0.0}\n";
    argString+="arg {number=9}{call=--frame_correct}{display=Interframe Correction}{type=selector}\n";
    argString+="value {arg=9}{value=none}{display=None}{default=true}\n";
    argString+="value {arg=9}{value=modbus_crc}{display=Modbus CRC}{default=false}\n";
    printf("%s", argString.c_str());
}

void print_extcap_config_dlt()
{
    std::string  argString="";  
    argString+="arg {number=10}{call=--dlt}{display=Wireshark DLT}{type=selector}\n";
    argString+="value {arg=10}{value=250}{display=250:  RTAC Serial}{default=true}\n";
    argString+="value {arg=10}{value=147}{display=147:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=148}{display=148:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=149}{display=149:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=150}{display=150:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=151}{display=151:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=152}{display=152:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=153}{display=153:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=154}{display=154:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=155}{display=155:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=156}{display=156:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=157}{display=157:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=158}{display=158:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=159}{display=159:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=160}{display=160:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=161}{display=161:  User DLT}{default=false}\n";
    argString+="value {arg=10}{value=162}{display=162:  User DLT}{default=false}\n";
    printf("%s", argString.c_str());
}

void print_extcap_config()
{
    print_extcap_config_comport();
    print_extcap_config_baud();
    print_extcap_config_bytesize();
    print_extcap_config_parity();
    print_extcap_config_stopbits();
    print_extcap_config_interframe();
    print_extcap_config_dlt();
}

static void print_help(std::string  filename) {
    std::string  argString="";  

    argString+="Created by Joel Zupancic\n";
    argString+="This is free software!\n";
    argString+="The Source is free!\n";
    argString+= "There is NO warranty!\n";
    argString+="\n";
    argString+="This program captures packets from a serial\n";
    argString+="interface and save them to a file or named pipe.\n";
    argString+="If the program is copied to the Wireshark\\extcap\\ \n";
    argString+="directory it will show up as a interface in the \n";
    argString+="Wireshark GUI with option to configrue the interface.\n";
    argString+="\n";
    argString+="*****Command Line Options*****\n";
    argString+="--port     Supported values: COM1, COM2......\n";
    argString+="--baud     Supported values: 1200-256000\n";
    argString+="--byte     Supported values: 7 OR 8 \n";
    argString+="--parity   Supported values: NONE, ODD, EVEN \n";
    argString+="--stop     Supported values: 1 OR 2 \n";
    argString+="--fifo     Name Pipe output. Supported values: \\\\.\\pipe\\wireshark\n";
 //   argString+="--file     Name file output. Supported values: C:\\temp\\myserial.cap\n";
    argString+="\n";
    argString+="*****Wireshare GUI Options*****\n";
    argString+="--capture  Start the capture routine from interface.\n";
    argString+="--extcap-interfaces Provide a list of interfaces to capture from.\n";
    argString+="--extcap-interface Provide the interface to capture from.\n";
    argString+="--extcap-dlts Provide a list of dlts for the given interface.\n";
    argString+="--extcap-config Provide a list of configurations for the given interface.\n";
 //   argString+="--extcap-capture-filter Used together with capture to provide a capture filter.\n"
    argString+="--fifo Use together with capture to provide the fifo to dump data to.\n";
    argString+="--extcap-control-in Used to get control messages from toolbar.\n";
    argString+="--extcap-control-out Used to send control messages to toolbar.\n";
    argString+="--extcap-version Shows the version of this utility.\n";
    argString+="--extcap-reload-option Reload elements for the given option.\n";

    printf("%s", argString.c_str());
}

void print_extcap_interfaces()
{
     /* note: format for Wireshark ExtCap */
    std::string  argString=""; 
    argString+="interface {value=" + WireSharkAdapterName+ +"_"+ FileName + "}{display=" +  WireSharkAdapterName + " ("+ FileName +")}\n"; 
    
    //Test GUI Menu Interface
    if(GuiMenu)
    {
        argString+="extcap {version=1.0}{display=Example extcap interface}\n"; 
        argString+="control {number=0}{type=string}{display=Message}\n";
        argString+="control {number=1}{type=selector}{display=Time delay}{tooltip=Time delay between packages}\n";
        argString+="control {number=2}{type=boolean}{display=Verify}{default=true}{tooltip=Verify package content}\n";
        argString+="control {number=3}{type=button}{display=Turn on}{tooltip=Turn on or off}\n";
        argString+="control {number=4}{type=button}{role=logger}{display=Log}{tooltip=Show capture log}\n";
        argString+="value {control=1}{value=1}{display=1 sec}\n";
        argString+="value {control=1}{value=2}{display=2 sec}{default=true}\n";
    }
    printf("%s", argString.c_str());
}

void print_extcap_dlt(std::string sInterface)
{
     /* note: format for Wireshark ExtCap */
    std::string  argString="dlt {number=147}{name=" + WireSharkAdapterName +"_"+ FileName + "}{display=" +  WireSharkAdapterName + " ("+ FileName +")}\n"; 
    printf("%s", argString.c_str());
}

bool ParseMainArg(int argc, char *argv[])
{
     int argi = 0;
    bool Wireshark_Capture=false;
    bool PrintArgExit=false;
    std::string sExtcapInterface="";
    std::string sArgMainOpt="";

     /* decode any command line parameters */
    FileName = filename_remove_path(argv[0]);

    for (argi = 1; argi < argc && !PrintArgExit; argi++) 
    {   
        if (strcmp(argv[argi], "--help") == 0) {print_help(FileName);PrintArgExit=true;}  
        if (strcmp(argv[argi], "--version") == 0) {printf("%d", ProgramVersion);PrintArgExit=true;} 
        if (strcmp(argv[argi], "--extcap-interfaces") == 0) {print_extcap_interfaces();PrintArgExit=true;} 
        if (strcmp(argv[argi], "--extcap-config") == 0) {print_extcap_config();PrintArgExit=true;}
        if (strcmp(argv[argi], "--extcap-dlts") == 0) {sArgMainOpt="--extcap-dlts";PrintArgExit=true;}  //Special Case--extcap-interface is required
        if (strcmp(argv[argi], "--capture") == 0)  {Wireshark_Capture = true; } //When parameter is used it will Start capature!  

        int iCaseIndex=0;
        std::string sArgOpt="";
        std::string *pArgOptValue;
        bool xExitCaseLoop=false;

        do {
            switch(iCaseIndex) 
            {
            case 0:
                sArgOpt="--fifo";
                pArgOptValue=&CaptureOutputPipeName;
                break;
            case 1:
                sArgOpt="--extcap-control-in";
                pArgOptValue=&ControlInPipeName;
                break;
            case 2:
                sArgOpt="--extcap-control-out";
                pArgOptValue=&ControlOutPipeName;
                break;                                
            case 3:
                sArgOpt="--extcap-interface";
                pArgOptValue=&sExtcapInterface;
                break;
            case 4:
                sArgOpt="--port";
                pArgOptValue=&Port;
                break;
            case 5:
                sArgOpt="--baud";
                pArgOptValue=&BaudRate;
                break;
            case 6:
                sArgOpt="--byte";
                pArgOptValue=&ByteSize;
                break;    
            case 7:
                sArgOpt="--parity";
                pArgOptValue=&Parity;
                break;    
            case 8:
                sArgOpt="--stop";
                pArgOptValue=&StopBits;
                break;   
            case 9:
                sArgOpt="--frame_timing";
                pArgOptValue=&FrameTiming;
                break;       
            case 10:
                sArgOpt="--frame_timebase";
                pArgOptValue=&FrameTimebase;
                break;   
            case 11:
                sArgOpt="--frame_multi";
                pArgOptValue=&FrameMulti;
                break;
            case 12:
                sArgOpt="--frame_delay";
                pArgOptValue=&FrameDelay;
                break;   
            case 13:
                sArgOpt="--frame_correct";
                pArgOptValue=&FrameCorrect;
                break;    
            case 14:
                sArgOpt="--dlt";
                pArgOptValue=&WiresharkDLT;
                break;   

                                                     
            default:
                xExitCaseLoop=true;
            }

            //Check if second part of ARG is valid
            if  (sArgOpt.compare(argv[argi]) == 0 && !xExitCaseLoop) 
            {
                xExitCaseLoop=true; //Arg Found Exit Case Loop
                argi++;
                if (argi >= argc) 
                {
                    PrintArgExit=true;  //Arg was not passed correctly Exit routine
                    std::string  argString="";  
                    argString=sArgOpt + " requires a value to be provided";
                    printf("%s", argString.c_str());
                }
                else
                {
                    *pArgOptValue=argv[argi];
                } 
            }
            iCaseIndex++;
        }
        while (!xExitCaseLoop);
      //End of For Statement
    }

    if(sArgMainOpt.compare("--extcap-dlts")==0)  //Special Case--extcap-interface is required
    {
        if(sExtcapInterface.empty())
        {
            std::string  argString="";  
            argString=sArgMainOpt + " requires interface to be provided";
            printf("%s", argString.c_str());
        }
        else
            print_extcap_dlt(sExtcapInterface);
    }

    return (Wireshark_Capture && !PrintArgExit); 
}



DWORD WINAPI ControlInThreadFunc(LPVOID lpParam)
{

    OVERLAPPED ovl;
    hControlInEvent=ovl.hEvent=CreateEvent(NULL, TRUE, FALSE, NULL);

     HANDLE hPipe = INVALID_HANDLE_VALUE; 
    /* create the pipe */
    while (hPipe == INVALID_HANDLE_VALUE)
    {
        /* use CreateFile rather than CreateNamedPipe */
        hPipe = CreateFileA(
            ControlInPipeName.c_str(),
            GENERIC_READ |
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL);
        if (hPipe != INVALID_HANDLE_VALUE) {
            break;
        }
        /* if an error occured at handle creation */
        if (!WaitNamedPipeA(ControlInPipeName.c_str(), 20000)) {
            printf("Could not open pipe: waited for 20sec!\n"
                "If this message was issued before the 20sec finished,\n"
                "then the pipe doesn't exist!\n");
            return 0;
        }
    }
    ConnectNamedPipe(hPipe, NULL);

typeFrameVector FrameVector;
typeFrameByte RX_CHAR[1024];
DWORD RX_LEN=0;

char SyncPipeChar;
WORD wMessageLength=0;
BYTE ControlNumber;
BYTE Command;

while(1)
{ /* read loop */
    BOOL b = ::ReadFile(hPipe, &RX_CHAR, sizeof(RX_CHAR), (LPDWORD)((void *)&RX_LEN), &ovl);
    if(!b)
    { /* failed */
        DWORD err = ::GetLastError();
        if(err == ERROR_IO_PENDING)
            { /* pending */
            b = ::GetOverlappedResult(hPipe, &ovl, &RX_LEN, TRUE);
            if(!b)
                { /* wait failed */
                DWORD err = ::GetLastError();
                //  wnd->PostMessage(UWM_REPORT_ERROR, (WPARAM)err);
                //  running = FALSE;
                continue;
                } /* wait failed */
            } /* pending */
        else
        { /* some other error */
            //wnd->PostMessage(UWM_REPORT_ERROR, (WPARAM)err);
            // running = FALSE;
            continue;
        } /* some other error */
    } /* failed */

    if(RX_LEN > 0)
    { /* has data */
        // wnd->PostMessage(UWM_HAVE_DATA, (WPARAM)bytesRead, (LPARAM)buffer);
        FrameVector.insert(FrameVector.end(), RX_CHAR, RX_CHAR+RX_LEN);
    } /* has data */


    if(FrameVector.size()>=6) 
    {
        SyncPipeChar=FrameVector.data()[0];
        int x = 1;
        if(*(char*)&x==1) //Little Ended Swap Bytes for network order
            wMessageLength=(FrameVector.data()[3])|((FrameVector.data()[2]&0xff)<<8);
        else
            wMessageLength=(FrameVector.data()[2])|((FrameVector.data()[3]&0xff)<<8);
        ControlNumber=FrameVector.data()[4];
        Command=FrameVector.data()[5];


        if(FrameVector.size()>= wMessageLength+4)
        {
            std::string sPayload((const char*)&(FrameVector.data()[6]), wMessageLength-2);;
            string sOutput="";
            sOutput+= "SyncPipeChar: " +  to_string(SyncPipeChar);
            sOutput+=" wMessageLength: " + to_string(wMessageLength);
            sOutput+=" ControlNumber: " + to_string(ControlNumber);
            sOutput+=" FrameVectorSize: " + to_string(FrameVector.size());
            sOutput+=" Command: "+ to_string(Command);
            sOutput+="\nPayload: "+ sPayload +"\n\n";            
            SendWireSharkControl(4, 2,sOutput);
            FrameVector.clear();
        }
    } 
} /* read loop */

}


int CreateControlInThread(void)
{
    int RetValue=0;
    hControlInThread = CreateThread(NULL, 0, ControlInThreadFunc, NULL, 0, NULL);
    if (hControlInThread == INVALID_HANDLE_VALUE || hControlInThread==NULL)
    {
        cout<<"Creating Thread Error";
        CloseHandle(hControlInThread);
    }
    else
    {
        RetValue=1;
    }
    return(RetValue);
}

void on_main_exit(void )
{
    CloseHandle(hComSerial);
    CloseHandle(hCommReadThread);
    CloseHandle(hCommFrameReadyEvent);
    CloseHandle(hCommFrameQueuedEvent);
    CloseHandle(hCaptureOutputPipe);
    CloseHandle(hControlOutPipe);
    CloseHandle(hControlInThread);
    CloseHandle(hControlInEvent);  
    CloseHandle(hControlInPipe); 
}

uint64_t SetMinimumTimerResolution()
{
    static NTSTATUS(__stdcall *NtQueryTimerResolution)(OUT PULONG MinimumResolution, OUT PULONG MaximumResolution, OUT PULONG ActualResolution) = (NTSTATUS(__stdcall*)(PULONG, PULONG, PULONG))GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryTimerResolution");
    static NTSTATUS(__stdcall *NtSetTimerResolution)(IN ULONG RequestedResolution, IN BOOLEAN Set, OUT PULONG ActualResolution) = (NTSTATUS(__stdcall*)(ULONG, BOOLEAN, PULONG))GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetTimerResolution");

    if ((NtQueryTimerResolution == nullptr) || (NtSetTimerResolution == nullptr))
        return 0;

    ULONG MinimumResolution, MaximumResolution, ActualResolution;
    NTSTATUS ns = NtQueryTimerResolution(&MinimumResolution, &MaximumResolution, &ActualResolution);
    if (ns == 0)
    {
        ns = NtSetTimerResolution(std::min(MinimumResolution, MaximumResolution), TRUE, &ActualResolution);
        if (ns == 0)
            return (ActualResolution * 100);
    }
    return 1000000;
} 

int main(int argc, char *argv[])
{
   //
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_PROCESSED_INPUT);
    
    ULONG currentRes;
    if (ParseMainArg(argc, argv)==0) 
    {
        return(0);
    }
    else
    {
        atexit(on_main_exit);
        SetMinimumTimerResolution();
        hCaptureOutputPipe=CreateNamedPipe(CaptureOutputPipeName);
        CreateControlInThread();
        hControlOutPipe=CreateNamedPipe(ControlOutPipeName);
        CreateComThread();
        ProcessFrames();
    }

    return 0;
}


