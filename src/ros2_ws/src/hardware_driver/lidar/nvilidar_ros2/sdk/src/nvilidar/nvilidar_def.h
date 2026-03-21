#ifndef _NVILIDAR_DEF_H_
#define _NVILIDAR_DEF_H_

#include <stdint.h>
#include "nvilidar_protocol.h"
#include <string>


//======================================basic parameter============================================ 

//SDK version 
#define NVILIDAR_SDKVerision     "1.0.9"

//PI def
#ifndef M_PI
#define M_PI        3.14159265358979323846
#endif 

//other 
#define NVILIDAR_DEFAULT_TIMEOUT     2000    //default timeout 
#define NVILIDAR_POINT_TIMEOUT		 2000	 //one circle time  for example, the lidar speed is 10hz ,the timeout must smaller the 100ms


//lidar model  list 
typedef enum
{
	NVILIDAR_Unknow = 0,		//unknow lidar 
   	NVILIDAR_VP300,				//lidar VP300
	NVILIDAR_VP350,				//lidar VP350
   	NVILIDAR_Tail,
}LidarModelListEnumTypeDef;


//======================================other parameters============================================ 

//lidar current state 
struct Nvilidar_PackageStateTypeDef
{
	bool m_CommOpen;              	//serialport open flag 
	bool m_Scanning;                //lidar is scanning data 
	uint8_t last_device_byte;       //last byte 
};

//stored para for lidar
struct  Nvilidar_StoreConfigTypeDef
{
	uint8_t     isHasSensitive;         //has sensitive 
	uint16_t    aimSpeed;               //motor aim speed == x100
	uint32_t    samplingRate;           //sampling rate == x1
	int16_t     angleOffset;            //angle offset == x64
	uint8_t     tailingLevel;           //tailling level 0-max 20-min
	uint16_t    apdValue;				//apd value 
};

//数据信息 
struct Nvilidar_DeviceInfo
{
	std::string m_SoftVer;				//software version
	std::string m_HardVer;				//hardware version
	std::string m_ProductName;			//product name 
	std::string m_SerialNum;			//serialnumber 
};

//雷达配置参数
struct  Nvilidar_UserConfigTypeDef
{
	LidarModelListEnumTypeDef  lidar_model_name;	//lidar model name 

	std::string frame_id;				//ID
	std::string serialport_name;		//serialport name 
	int    		serialport_baud;		//serialport baudrate 
	std::string ip_addr;				//ip addr for net convert
	int    		lidar_udp_port;			//ip port for net convert
	int    		config_tcp_port;		//ip port for config net para 
	bool		auto_reconnect;			//auto reconnect 
    bool		reversion;				//add 180.0 
	bool		inverted;				//turn backwards(if it is true)
	double		angle_max;				//angle max value for lidar 
	double		angle_min;				//angle min value for lidar  
	double		range_max;				//measure distance max value for lidar  
	double		range_min;				//measure distance min value for lidar  
	double 		aim_speed;				//lidar aim speed   
	int			sampling_rate;			//sampling rate  
	bool		sensitive;				//is contain sensitive  
	int			tailing_level;			//tailling level  
	bool		apd_change_flag;		//is enable to change apd value  
	int			apd_value;				//default apd value 
	bool 		angle_offset_change_flag;  //is enable to change angle offset 
	double 		angle_offset;			//angle offset 

	std::string ignore_array_string;	//filter angle ,string,like ,
	std::vector<float> ignore_array;	//filter angle to array list 

	bool 		resolution_fixed;		//is good resolution  
	Nvilidar_DeviceInfo			deviceInfo;	//lidar info 
	Nvilidar_StoreConfigTypeDef	storePara;	//lidar needed to store  

	bool 		filter_jump_enable;		//is needed to filter jump point 
	int 		filter_jump_value_min;	//filter min value 
	int 		filter_jump_value_max;	//filter max value
};

//共用体
union Nvilidar_PackageBufTypeDef
{
	uint8_t buf[1200];
	Nvilidar_Node_Package_Quality        pack_qua;
	Nvilidar_Node_Package_No_Quality     pack_no_qua;
};

//包信息
typedef struct 
{
	uint16_t packageIndex;         //单包采样点索引位置信息
	Nvilidar_PackageBufTypeDef  packageBuffer;    //包信息（实际内容）
	bool     packageErrFlag;       //包错误标记信息
	uint16_t packageCheckSumGet;   //校验值获取
	uint16_t packageCheckSumCalc;  //校验值计算
	uint16_t  packageFreq;          //雷达转速信息
	int16_t  packageTemp;          //雷达温度信息
	uint32_t packagePointTime;     //2点时间间隔
	uint16_t packageFirstAngle;    //起始采样角
	uint16_t packageLastAngle;     //结束采样角
	float    packageAngleDiffer;   //每2个点之间的角度差
	float    packageLastAngleDiffer; //最后一次算的2点角度的差值
	uint8_t  packagePointDistSize; //一个点对应的字节的大小信息
	bool     packageHas0CAngle;    //是否为0度角
	bool     packageHasTemp;       //是否当前位置为温度
	bool     packageHas0CFirst;    //第一个字节 判断是否是0度角
	bool     packageHasTempFirst;  //第一个字节 判断是否为温度信息
	uint16_t  package0CIndex;      //0度角索引（目前协议为非单独封包）
	uint64_t packageStamp;		   //接收完本包的时间 
	uint16_t packagePointNum;	   //一包的点数信息 
}Nvilidar_PointViewerPackageInfoTypeDef;

//接收信息 (只用于接收暂存 无其它用)
typedef struct
{
	bool	recvFinishFlag;
	Nvilidar_Protocol_DeviceInfo lidar_device_info;//雷达接收并且返回的数据  
	Nvilidar_Protocol_GetPara    lidar_get_para;	//获取参数信息 
	uint8_t     isHasSensitive;					//有信号质量信息
	uint16_t    aimSpeed;						//转速信息 x100
	uint32_t    samplingRate;					//采样率x1
	int16_t     angleOffset;					//角度偏移x64
	uint8_t     tailingLevel;					//拖尾等级
	uint16_t    apdValue;						//apd值信息 
	uint8_t     saveFlag;						//是否保存成功了 
}NvilidarRecvInfoTypeDef;

//一圈点信息 
typedef struct
{
	uint64_t  startStamp;			//One Lap Start Timestamp 
	uint64_t  stopStamp;			//One Lap Stop Timestamp 
	std::vector<Nvilidar_Node_Info>  lidarCircleNodePoints;	//lidar point data
}CircleDataInfoTypeDef;



//======================================输出数据信息============================================ 
/**
 * @brief The Laser Point struct
 * @note angle unit: rad.\n
 * range unit: meter.\n
 */
typedef struct {
	/// lidar angle. unit(rad)
	float angle;
	/// lidar range. unit(m)
	float range;
	/// lidar intensity
	float intensity;
} NviLidarPoint;

/**
 * @brief A struct for returning configuration from the NVILIDAR
 * @note angle unit: rad.\n
 * time unit: second.\n
 * range unit: meter.\n
 */
typedef struct {
	/// Start angle for the laser scan [rad].  0 is forward and angles are measured clockwise when viewing NVILIDAR from the top.
	float min_angle;
	/// Stop angle for the laser scan [rad].   0 is forward and angles are measured clockwise when viewing NVILIDAR from the top.
	float max_angle;
	/// angle resoltuion [rad]
	float angle_increment;
	/// Scan resoltuion [s]
	float time_increment;
	/// Time between scans
	float scan_time;
	/// Minimum range [m]
	float min_range;
	/// Maximum range [m]
	float max_range;
} NviLidarConfig;


typedef struct {
	/// System time when first range was measured in nanoseconds
	uint64_t stamp;
	/// Array of lidar points
	std::vector<NviLidarPoint> points;
	/// Configuration of scan
	NviLidarConfig config;
} LidarScan;



#endif
