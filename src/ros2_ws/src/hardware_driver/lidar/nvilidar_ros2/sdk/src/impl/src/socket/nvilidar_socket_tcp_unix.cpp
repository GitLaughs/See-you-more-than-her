#if defined(__linux__)

#include "socket/nvilidar_socket_tcp_unix.h"
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
        // 创建设备类型 本雷达需要 IPV4,STREAM,TCP方式  
        m_SocketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if(m_SocketHandle == -1)
		{
			//printf("invalid socket!");
		}

        // 保存ip 端口等信息
        m_SocketPara.sin_family =  AF_INET;
        m_SocketPara.sin_port =  htons(port);
        m_SocketPara.sin_addr.s_addr = inet_addr(addr);
    }

    // 打开连接  
    bool Nvilidar_Socket_TCP::tcpOpen()
    {
        unsigned long ul=1;
        // 判断有无连接  
        if(!istcpOpen())
        {
            ul=1;
            /*设置套接字为非阻塞*/
            if (ioctl(m_SocketHandle, FIONBIO, &ul) < 0) 
            {
                 //fprintf(stderr, "Set flags error:%s\n", strerror(errno));
                 close(m_SocketHandle);
                 return false;
            }

            //默认false
            m_SocketConnect = false;

            // 连接设备 
            int rc = connect(m_SocketHandle, (sockaddr *)&m_SocketPara, sizeof(m_SocketPara));
            if(rc < 0)
            {
                if(errno != EINPROGRESS)
                {
                    return false;
                }

                //设置时间 
                fd_set fdr, fdw;
                int err = 0;
                // unsigned int errlen = sizeof(err);
                struct timeval timeout;

                /*正在处理连接*/
                FD_ZERO(&fdr);
                FD_ZERO(&fdw);
                FD_SET(m_SocketHandle, &fdr);
                FD_SET(m_SocketHandle, &fdw);
                timeout.tv_sec = 5;
                timeout.tv_usec = 0;
                rc = select(m_SocketHandle + 1, &fdr, &fdw, NULL, &timeout);
                //printf("rc is: %d\n", rc);

                /*select调用失败*/
                if (rc < 0) 
                {
                    close(m_SocketHandle);
                    return false;
                }
                //调用成功 
                else if(0 == rc)
                {
                    close(m_SocketHandle);
                    return false;
                }
            }
            
            /*设置套接字为阻塞*/
            ul=0;
            if (ioctl(m_SocketHandle, FIONBIO, &ul) < 0) 
            {
                //fprintf(stderr, "Set flags error:%s\n", strerror(errno));
                close(m_SocketHandle);
                return false;
            }

            //printf("socket connect ok\n");

            m_SocketConnect = true;
            return true;
        }
        m_SocketConnect = true;
        return true;
    }

    // 关闭连接  
    void Nvilidar_Socket_TCP::tcpClose()
    {
        if(istcpOpen())
        {
            m_SocketConnect = false;
            close(m_SocketHandle);
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
        if(!istcpOpen())
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

        int ret;

        ret = send(m_SocketHandle,(char *)data,len,0);

        return ret;
    }

    // 刷新数据 
    void Nvilidar_Socket_TCP::tcpFlush()
    {
        return;
    }
}

#endif

