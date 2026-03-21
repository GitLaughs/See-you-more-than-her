#if defined(_WIN32)

#ifndef _NVILIDAR_SOCKET_TCP_WIN
#define _NVILIDAR_SOCKET_TCP_WIN

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

#define NVILIDAR_SOCKET_TCP_API __declspec(dllexport)


namespace nvilidar_socket
{
    class NVILIDAR_SOCKET_TCP_API Nvilidar_Socket_TCP
    {
    public:
		Nvilidar_Socket_TCP();
        ~Nvilidar_Socket_TCP();
        void tcpInit(const char *addr, unsigned short port);
        bool tcpOpen();
        void tcpClose();
        bool istcpOpen();        //串口是否打开 
        int  tcpReadAvaliable(); //读可读字节的长度 
        int  tcpReadData(const uint8_t *data,int len);
        int  tcpWriteData(const uint8_t *data,int len);        //写数据  
        void tcpFlush();         //刷新数据 
    private:
        WSADATA              m_hWSAData;            // Windows
        sockaddr_in          m_SocketPara;          //参数信息
        SOCKET               m_SocketHandle;        //handle 
        bool                 m_SocketConnect;       //socket是否连接 
    };
};

#endif

#endif