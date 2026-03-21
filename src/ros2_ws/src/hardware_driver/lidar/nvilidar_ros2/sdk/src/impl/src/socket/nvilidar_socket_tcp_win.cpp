#if defined(_WIN32)
#include "socket/nvilidar_socket_tcp_win.h"
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
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

#pragma comment(lib, "ws2_32.lib")


namespace nvilidar_socket
{ 
	Nvilidar_Socket_TCP::Nvilidar_Socket_TCP()
    {
        m_SocketConnect = false;
    }
	Nvilidar_Socket_TCP::~Nvilidar_Socket_TCP()
    {
        m_SocketConnect = false;
    }

    // 初始化 
    void Nvilidar_Socket_TCP::tcpInit(const char *addr, unsigned short port)
    {
        // socket版本号 
        memset(&m_hWSAData, 0, sizeof(m_hWSAData));
        WSAStartup(MAKEWORD(2, 2), &m_hWSAData);

        // 创建设备类型 本雷达需要 IPV4,STREAM,TCP方式  
        m_SocketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if(m_SocketHandle == INVALID_SOCKET)
		{
			printf("invalid socket!");
		}

        // 保存ip 端口等信息
        m_SocketPara.sin_family =  AF_INET;
        m_SocketPara.sin_port =  htons(port);
        m_SocketPara.sin_addr.S_un.S_addr = inet_addr(addr);
    }

    // 打开连接  
    bool Nvilidar_Socket_TCP::tcpOpen()
    {
        // 判断有无连接  
        if(!istcpOpen())
        {
			//设置非阻塞方式连接
			unsigned long ul = 1;
			int ret = ioctlsocket(m_SocketHandle, FIONBIO, (unsigned long*)&ul);
			if (ret == SOCKET_ERROR)
			{
				return false;
			}

            // 连接设备 
			int rc = connect(m_SocketHandle, (sockaddr *)&m_SocketPara, sizeof(m_SocketPara));
  
			//非阻塞模式  则认为其它错误  有问题 
			if (rc < 0 && (WSAGetLastError() != WSAEWOULDBLOCK))
			{
				WSACleanup();
				closesocket(m_SocketHandle);
				return false;
			}

			//select 模型，即设置超时
			fd_set fdr, fdw;
			struct timeval timeout;

			FD_ZERO(&fdr);
			FD_ZERO(&fdw);
			FD_SET(m_SocketHandle, &fdr);
			FD_SET(m_SocketHandle, &fdw);
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;

			rc = select(m_SocketHandle + 1,&fdr,&fdw,NULL,&timeout);

			if (rc < 0)
			{
				closesocket(m_SocketHandle);
				return false;
			}
			//设回阻塞模式
			ul = 0;
			ret = ioctlsocket(m_SocketHandle, FIONBIO, (unsigned long*)&ul);
			if (ret == SOCKET_ERROR) 
			{
				closesocket(m_SocketHandle);
				return false;
			}

			int on = 1;
			//setsockopt(m_SocketHandle, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(on));

			//设置发送超时6秒
			//int TimeOut = 6000;
			//if (setsockopt(m_SocketHandle, SOL_SOCKET, SO_SNDTIMEO, (char *)&TimeOut, sizeof(TimeOut)) == SOCKET_ERROR) {
			//	return false;
			//}

			////设置接收超时6秒
			//TimeOut = 6000;
			//if (setsockopt(m_SocketHandle, SOL_SOCKET, SO_RCVTIMEO, (char *)&TimeOut, sizeof(TimeOut)) == SOCKET_ERROR) {
			//	return false;
			//}

            m_SocketConnect = true;
        }
        return true;
    }

    // 关闭连接  
    void Nvilidar_Socket_TCP::tcpClose()
    {
        if(m_SocketConnect)
        {
	        m_SocketConnect = false;	
	        closesocket(m_SocketHandle);
			WSACleanup();
        }
    }

    // 判断有没有连接  
    bool Nvilidar_Socket_TCP::istcpOpen()
    {
        #if 0
            int is_ok;
            char buffer[1];       

            /* MSG_PEEK表示从输入队列中读数据但并不将数据从输入队列中移除 */
            recv(m_SocketHandle, buffer, 1, MSG_PEEK);
            is_ok = (WSAECONNRESET != WSAGetLastError());

            return is_ok;
        #else 
            return m_SocketConnect;
        #endif 
    }

     // 读可读字节的长度   未用此项 
    int Nvilidar_Socket_TCP::tcpReadAvaliable()
    {
        return 8192;
    }

    // 读socket数据  
    int Nvilidar_Socket_TCP::tcpReadData(const uint8_t *data,int len)
    {
		if (!istcpOpen())
		{
			return -1;
		}
        int ret = recv(m_SocketHandle, (char *)data, len, 0); 

        return ret;
    }

    // 写socket数据  
    int Nvilidar_Socket_TCP::tcpWriteData(const uint8_t *data,int len)
    {
        if(!istcpOpen())
        {
            return -1;
        }

        int ret = send(m_SocketHandle,(char *)data,len,0);

        return ret;
    }

    // 刷新数据 
    void Nvilidar_Socket_TCP::tcpFlush()
    {
        return;
    }
}

#endif

