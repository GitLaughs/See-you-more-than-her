#pragma once

#include "nvilidar_def.h"
#include "socket/nvilidar_socket.h"
#include <string>
#include <vector>
#include <stdint.h>

#if defined(_WIN32)
#include <conio.h>
#include <process.h>
#include <tlhelp32.h>
#include <sys/utime.h>
#include <io.h>
#include <direct.h>
#else
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#endif


#define NVILIDAR_NET_CONFIG_FRAMEHEAD      0xC0		//帧头 
#define NVILIDAR_NET_CONFIG_WRITE_CMD      0x01        //写命令       link: C0 01
#define NVILIDAR_NET_CONFIG_READ_CMD       0x02        //读命令       like: C0 02 C0 00 11 22 33 44 55 66 77 88 99 AA BB CC CRC FF
#define NVILIDAR_NET_CONFIG_FRAMEEND       0xFF        //结尾字符

//包对齐
#pragma pack(push)
#pragma pack(1)

//雷达网络配置参数
typedef struct
{
    uint8_t     IP_addr[4];
    uint8_t     GateWay[4];
    uint8_t     Mask[4];
}Nvilidar_NetConfigTypeDef;


//应答时的数据结构模式
struct Nvilidar_NetConfig_ProtocolHeader {
  uint8_t  startByte;           //起始命令 1byte
  uint8_t  cmd;                 //命令字   1Byte
  uint16_t length;              //长度    2byte
};

//应答时的数据结构模式
struct Nvilidar_NetConfig_ProtocolTail {
  uint8_t  crc;                 //校验   1Byte
  uint8_t  endByte;             //结尾   1byte
};

//应答数据信息 
struct Nvilidar_NetConfig_Protocol_NormalResponseData
{
	uint8_t  cmd;                 //命令字   1Byte
	uint16_t length;              //长度    2byte
	uint8_t	 dataInfo[100];		  //100 ram 
};

#pragma pack(pop)


//---定义库信息 VS系列的生成库文件  
#ifdef WIN32
	#define NVILIDAR_NET_CONFIG_API __declspec(dllexport)
#else
	#define NVILIDAR_NET_CONFIG_API
#endif // ifdef WIN32

namespace nvilidar
{
    //lidar driver 
	class  NVILIDAR_NET_CONFIG_API LidarDriverNetConfig
    {
		public:
			LidarDriverNetConfig();                //构造函数
			~LidarDriverNetConfig();           //析构函数

			void LidarLoadConfig(Nvilidar_UserConfigTypeDef cfg);
			bool LidarNetConfigWrite(std::string ip,std::string gate,std::string mask);
    		bool LidarNetConfigRead(std::string &ip,std::string &gate,std::string &mask);
			bool LidarNetConfigConnect();
			bool LidarNetConfigDisConnect();

		private:
			bool NetConfigConnect(std::string ip_addr, uint16_t port);  //串口初始化
			void NetConfigDisconnect();      //关闭串口
			bool SendTCP(const uint8_t *data, size_t size);      //发送接口  私有类
			bool SendCommand(uint8_t cmd, const void *payload = NULL,uint16_t payloadsize = 0);
			bool GetDataResponse(void);		//解包（普通数据解包）
			std::string ip_char2str(uint8_t *in);	//ip转std string
			bool ip_str2char(std::string in,uint8_t*out);
			std::string int2str(int i);

			//----------------------串口类---------------------------
			nvilidar_socket::Nvilidar_Socket_TCP net_config_tcp;
			Nvilidar_UserConfigTypeDef net_config_cfg;
			Nvilidar_NetConfigTypeDef   net_config_para;
			bool  m_CommOpen = false;
    };
}
