#if defined(__linux__)

#ifndef _NVILIDAR_SOCKET_TCP_UNIX
#define _NVILIDAR_SOCKET_TCP_UNIX

#include <string>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace nvilidar_socket
{
    class Nvilidar_Socket_TCP
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
        bool                 m_SocketConnect;       //socket是否连接 
        int                  m_SocketHandle;        //handle 
        struct sockaddr_in   m_SocketPara;          //para 
    };
}

#endif

#endif