
void write_ini()
{
    char buf[1024] = {0};
    DWORD ret = GetModuleFileNameA(NULL, buf, sizeof(buf));
    std::string filename_ini(buf);
    filename_ini+=".ini";

    ofstream myfile;
    myfile.open (filename_ini);
    myfile << "[Settings]\n";
    myfile << "Port="<<Port<<"\n";
    myfile << "BaudRate="<<BaudRate<<"\n";
    myfile << "ByteSize="<<ByteSize<<"\n";
    myfile << "Parity="<<Parity<<"\n";
    myfile << "StopBits="<<StopBits<<"\n";
    myfile.close();
}

void read_ini()
{
    char buf[1024] = {0};
    DWORD ret = GetModuleFileNameA(NULL, buf, sizeof(buf));
    std::string filename_ini(buf);
    filename_ini+=".ini";


    std::ifstream myfile (filename_ini);
    std::string ReadLine;
    std::string ReadValue;
    int startchar;
    if (myfile.is_open())
    {

    while ( getline (myfile,ReadLine) )
    {
      startchar=ReadLine.find("=");
      if(startchar<ReadLine.length())
      {
          ReadValue=ReadLine.substr(startchar+1);
          if(ReadLine.find("Port=") == 0)Port=ReadValue;
          if(ReadLine.find("BaudRate=") == 0)BaudRate=ReadValue;
          if(ReadLine.find("ByteSize=") == 0)ByteSize=ReadValue;
          if(ReadLine.find( "Parity=") == 0)Parity=ReadValue;
          if(ReadLine.find("StopBits=") == 0)StopBits=ReadValue;
      }
    }
    myfile.close();
  }
}
