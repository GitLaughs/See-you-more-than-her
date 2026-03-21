#if defined(_WIN32)

#ifndef _NVILIDAR_SOCKET_UDP_WIN
#define _NVILIDAR_SOCKET_UDP_WIN

#include <string>
#include <stdio.h>   /* Standard input/output definitions */
#include <sys/stat.h>
#include <io.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <ws2def.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#define NVILIDAR_SOCKET_UDP_API __declspec(dllexport)


namespace nvilidar_socket
{
    class NVILIDAR_SOCKET_UDP_API Nvilidar_Socket_UDP
    {
    public:
		Nvilidar_Socket_UDP();
        ~Nvilidar_Socket_UDP();
        bool udpInit(const char *addr, unsigned short port);
        void udpClose();         //关掉udp通信  
        bool isudpOpen();        //是否打开 
        int  udpReadAvaliable(); //读可读字节的长度 
        int  udpReadData(const uint8_t *data,int len);
        int  udpWriteData(const uint8_t *data,int len);        //写数据  

    private:
        WSADATA              m_hWSAData;            // Windows
        sockaddr_in          m_SocketPara;          //参数信息
		sockaddr_in          m_SocketSndPara;          //参数信息
        SOCKET               m_SocketHandle;        //handle 
        bool                 m_SocketConnect;       //socket是否连接 
    };
};

#endif

#endif