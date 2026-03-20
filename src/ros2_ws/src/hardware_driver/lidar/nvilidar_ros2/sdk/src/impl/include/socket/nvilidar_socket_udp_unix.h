#if defined(__linux__)

#ifndef _NVILIDAR_SOCKET_UDP_UNIX
#define _NVILIDAR_SOCKET_UDP_UNIX

#include <string>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace nvilidar_socket
{
    class Nvilidar_Socket_UDP
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
        bool                 m_SocketConnect;       //socket是否连接 
        int                  m_SocketHandle;        //handle 
        struct sockaddr_in   m_SocketPara;          //para 
        struct sockaddr_in   m_SocketSndPara;       //send 
    };
};

#endif

#endif