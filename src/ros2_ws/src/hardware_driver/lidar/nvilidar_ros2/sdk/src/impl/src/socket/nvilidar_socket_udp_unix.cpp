#if defined(__linux__)

#include "socket/nvilidar_socket_udp_unix.h"
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/ioctl.h>


namespace nvilidar_socket
{ 
	Nvilidar_Socket_UDP::Nvilidar_Socket_UDP()
    {
        m_SocketConnect = false;
    }
	Nvilidar_Socket_UDP::~Nvilidar_Socket_UDP()
    {
        m_SocketConnect = false;
    }

    // 初始化 
    bool Nvilidar_Socket_UDP::udpInit(const char *addr, unsigned short port)
    { 
         m_SocketConnect = false;

        // 创建设备类型 本雷达需要 IPV4,STREAM,TCP方式  
        m_SocketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if(m_SocketHandle == -1)
		{
            m_SocketConnect = false;
            return false;
			//printf("invalid socket!");
		}

        // 保存ip 端口等信息
        m_SocketPara.sin_family =  AF_INET;
        m_SocketPara.sin_port =  htons(port);
        m_SocketPara.sin_addr.s_addr = INADDR_ANY;

        if (-1 == bind(m_SocketHandle, (sockaddr*)&m_SocketPara, sizeof(m_SocketPara)))
		{
            return false;
		}

        //发送 
		m_SocketSndPara.sin_family = AF_INET;
		m_SocketSndPara.sin_port = htons(port);
		m_SocketSndPara.sin_addr.s_addr = inet_addr(addr);

		int opt_state = 1;
		if (-1 == setsockopt(m_SocketHandle, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt_state, sizeof(opt_state)))
		{
			return false;
		}

        m_SocketConnect = true;

        return true;
    }

    // 判断有没有连接  
    bool Nvilidar_Socket_UDP::isudpOpen()
    {
        return m_SocketConnect;
    }

    //关闭socket  
    void Nvilidar_Socket_UDP::udpClose()
    {
        if(m_SocketConnect)
        {
            m_SocketConnect = false;
            close(m_SocketHandle);
        }
    }

     // 读可读字节的长度   未用此项 
    int Nvilidar_Socket_UDP::udpReadAvaliable()
    {
        return 8192;
    }

    // 读socket数据  
    int Nvilidar_Socket_UDP::udpReadData(const uint8_t *data,int len)
    {
        int ret = 0;
        if(!isudpOpen())
        {
            return -1;
        }
        socklen_t para_len = sizeof(m_SocketPara);
        ret = recvfrom(m_SocketHandle, (char *)data, len, 0, (struct sockaddr *)&m_SocketPara, &para_len);

        return ret;
    }

    // 写socket数据  
    int Nvilidar_Socket_UDP::udpWriteData(const uint8_t *data,int len)
    {
        if(!isudpOpen())
        {
            return -1;
        }

        int ret;

        ret = sendto(m_SocketHandle,(char *)data,len,0,(struct sockaddr*)&m_SocketSndPara,sizeof(m_SocketSndPara)) ;

        return ret;
    }


}

#endif

