#include "nvilidar_driver_net_config.h"
#include "socket/nvilidar_socket.h"
#include <list>
#include <string>
#include "myconsole.h"
#include "mytimer.h"
#include "mystring.h"

namespace nvilidar
{
	LidarDriverNetConfig::LidarDriverNetConfig()
	{
		m_CommOpen = false;       //serial default close 
	}

	LidarDriverNetConfig::~LidarDriverNetConfig()
	{
		NetConfigDisconnect();
	}

	void LidarDriverNetConfig::LidarLoadConfig(Nvilidar_UserConfigTypeDef cfg)
	{
		net_config_cfg = cfg;                   //para avaliable 
	}

	//network para config 
	bool LidarDriverNetConfig::LidarNetConfigWrite(std::string ip,std::string gate,std::string mask)
	{	
		Nvilidar_NetConfigTypeDef cfg;

		//change string to ip 
		ip_str2char(ip,&cfg.IP_addr[0]);
		ip_str2char(gate,&cfg.GateWay[0]);
		ip_str2char(mask,&cfg.Mask[0]);

		//show 
		printf("write_ip:%d.%d.%d.%d,gate:%d.%d.%d.%d,mask:%d.%d.%d.%d\n",
			cfg.IP_addr[0],cfg.IP_addr[1],cfg.IP_addr[2],cfg.IP_addr[3],
			cfg.GateWay[0],cfg.GateWay[1],cfg.GateWay[2],cfg.GateWay[3],
			cfg.Mask[0],cfg.Mask[1],cfg.Mask[2],cfg.Mask[3]);	
		
		//send data 
		if(!SendCommand(NVILIDAR_NET_CONFIG_WRITE_CMD,(void *)&cfg,sizeof(Nvilidar_NetConfigTypeDef)))
		{	
			return false;
		}
		//wait for response 
		delayMS(500);
		//read the response data 
		if(false == GetDataResponse())
		{
			return false;
		}

		//result 
		return true;
	}

	//get config para 
    bool LidarDriverNetConfig::LidarNetConfigRead(std::string &ip,std::string &gate,std::string &mask)
	{
		//send data  
		if(!SendCommand(NVILIDAR_NET_CONFIG_READ_CMD))
		{
			return false;
		}
		
		//wait for response 
		delayMS(500);
		//read the response data 
		if(false == GetDataResponse())
		{
			return false;
		}

		//ip to string 
		ip = ip_char2str(&net_config_para.IP_addr[0]);
		gate = ip_char2str(&net_config_para.GateWay[0]);
		mask = ip_char2str(&net_config_para.Mask[0]);	

		return true;
	}

	//连接 
	bool LidarDriverNetConfig::LidarNetConfigConnect()
	{
		//初始化网络  
		if(! NetConfigConnect(net_config_cfg.ip_addr,net_config_cfg.config_tcp_port))	
		{
			NetConfigDisconnect();
			return false;
		}
		return true;
	}

	//关闭连接  
	bool LidarDriverNetConfig::LidarNetConfigDisConnect()
	{
		//断掉连接 
		NetConfigDisconnect();
		return true;
	}

	//---------------------------------------私有类及接口---------------------------------
	//启动雷达串口
	bool LidarDriverNetConfig::NetConfigConnect(std::string ip_addr, uint16_t port)
	{
		net_config_tcp.tcpInit(ip_addr.c_str(),port);
		net_config_tcp.tcpOpen();
		
		if (net_config_tcp.istcpOpen())
		{
			m_CommOpen = true;
			delayMS(100);
			//printf("connet ok\n");
			
			return true;
		}
		//printf("connet fail\n");
		m_CommOpen = false;
		
		return false;
	}

	//关闭雷达串口接口 
	void LidarDriverNetConfig::NetConfigDisconnect()
	{
		//closeThread();		//关闭线程 
		net_config_tcp.tcpClose();
		m_CommOpen = false;
	}

	//发送串口
	bool LidarDriverNetConfig::SendTCP(const uint8_t *data, size_t size)
	{
		if (!m_CommOpen)
		{
			return false;
		}

		if (data == NULL || size == 0)
		{
			return false;
		}

		
		//写数据 直到写完为止  
		size_t r;
		while (size) 
		{
			r = net_config_tcp.tcpWriteData(data,size);
			if (r < 1) 
			{
				return false;
			}

			size -= r;
			data += r;
		}

		return true;
	}

	//雷达发送数据
	bool LidarDriverNetConfig::SendCommand(uint8_t cmd, const void *payload, uint16_t payloadsize)
	{
		uint8_t pkt_header[sizeof(Nvilidar_NetConfig_ProtocolHeader)];
		Nvilidar_NetConfig_ProtocolHeader *header = reinterpret_cast<Nvilidar_NetConfig_ProtocolHeader * >(pkt_header);
		uint8_t checksum = 0;
		uint8_t pkt_tail = NVILIDAR_NET_CONFIG_FRAMEEND;     //包尾

		//串口未打开 返回失败
		if (!m_CommOpen)
		{
			return false;
		}

		//长命令
		header->startByte = NVILIDAR_NET_CONFIG_FRAMEHEAD;
		header->cmd = cmd;
		header->length = payloadsize;

		//计算校验值
		if(payloadsize > 0)
		{
			for (size_t pos = 0; pos < payloadsize; pos++)
			{
				checksum ^= ((uint8_t *)payload)[pos];
			}
		}
		else 
		{
			checksum = 0;
		}

		//开始进行发送
		SendTCP(pkt_header, 4) ;       //包头 长度信息
		if(payloadsize > 0)
		{
			SendTCP((const uint8_t *)payload, payloadsize);   //发送数据信息
		}
		SendTCP(&checksum, 1);         //校验值
		SendTCP(&pkt_tail, 1);         //校验值

		return true;
	}

	//普通数据解包 
	bool LidarDriverNetConfig::GetDataResponse(void)
	{
		uint16_t  recvPos = 0;											//当前接到的位置信息
		uint8_t   crc = 0;												//CRC校验值 
		Nvilidar_NetConfig_Protocol_NormalResponseData		normalResponseData;				//常规数据应答  
		uint8_t   readBuf[200];
		bool      ret = false;

		//memset 
		memset((char *)&normalResponseData,0x00,sizeof(Nvilidar_NetConfig_Protocol_NormalResponseData));

		//读数据  
		int len = net_config_tcp.tcpReadData(readBuf,200);

		//读错误或者长度为0  
		if(len <= 0)
		{
			return false;
		}

		//字节查找信息 
		for (int j = 0; j < len; j++)
		{
			uint8_t byte = readBuf[j];

			switch (recvPos)
			{
				case 0:		//第一个字节  
				{
					if (byte == NVILIDAR_NET_CONFIG_FRAMEHEAD)
					{
						recvPos++;
						break;
					}
					else
					{
						break;
					}
				}
				case 1:		//第2个字节  
				{
					if (
							(byte == NVILIDAR_NET_CONFIG_WRITE_CMD) ||
							(byte == NVILIDAR_NET_CONFIG_READ_CMD) 
					   )
					{
						normalResponseData.cmd = byte;
						recvPos++;
					}
					else
					{
						normalResponseData.cmd = 0;
						recvPos=0;
					}
					break;
				}
				case 2:		//第3个字节  
				{
					normalResponseData.length = byte;
					recvPos++;
					break;
				}
				case 3:		//第4个字节  
				{
					normalResponseData.length += byte * 256;
					recvPos++;
					break;
				}
				default:	//第5个及后续所有字节 
				{
					if (recvPos < normalResponseData.length + sizeof(Nvilidar_NetConfig_ProtocolHeader))			//中间有效数据  
					{
						crc ^= byte;
						normalResponseData.dataInfo[recvPos - sizeof(Nvilidar_NetConfig_ProtocolHeader)] = byte;

						recvPos++;
					}
					else if (recvPos == normalResponseData.length + sizeof(Nvilidar_NetConfig_ProtocolHeader))	//校验  
					{
						if (byte != crc)
						{
							recvPos = 0;
							break;
						}

						recvPos++;
					}
					else if (recvPos == normalResponseData.length + sizeof(Nvilidar_NetConfig_ProtocolHeader) + 1)
					{
						if (byte != NVILIDAR_NET_CONFIG_FRAMEEND)
						{
							break;
						}

						//调用协议解析接口 
						//协议解析  
						switch(normalResponseData.cmd)
						{
							case NVILIDAR_NET_CONFIG_WRITE_CMD:
							{
								if (normalResponseData.length != sizeof(Nvilidar_NetConfigTypeDef))
								{
									break;
								}

								memcpy((char *)(&net_config_para), normalResponseData.dataInfo, normalResponseData.length);
								ret = true;
							}
							case NVILIDAR_NET_CONFIG_READ_CMD:
							{
								if (normalResponseData.length != sizeof(Nvilidar_NetConfigTypeDef))
								{
									break;
								}

								memcpy((char *)(&net_config_para), normalResponseData.dataInfo, normalResponseData.length);
								ret = true;							
							}
						}
					}
					break;
				}
			}
		}
		return ret;
	}

	//字符转数组 
	std::string LidarDriverNetConfig::int2str(int i)
	{
		char t[10] = {0};
		sprintf(t, "%d", i);
		return t;
	}

	//IP地址转换 
	std::string LidarDriverNetConfig::ip_char2str(uint8_t *in)
	{
		std::string ret = "";
		int i = 0;
		while( i < 4 )
		{
			int a = in[i];
			if( i < 3)
			{
				ret += int2str(a) + ".";
			}
			else
			{
				ret += int2str(a);
			}

			i ++;
		}		
		return ret;
	}

	//IP地址转换 
	bool LidarDriverNetConfig::ip_str2char(std::string in,uint8_t*out)
	{
		char* p = (char*)in.c_str();
		unsigned char tmp = 0;
		unsigned char index = 0;
		
		while(p != NULL)
		{
			if (*p != '\0' && *p != '.') 
			{
				tmp = tmp * 10 + *p - '0';
			}
			else 
			{
				//tmp 
				out[index] = tmp;
				tmp = 0;
				index++;

				if(index >= 4)
				{
					break;
				}
        	}
			p++;
		}
		
		return true;
	}
}





