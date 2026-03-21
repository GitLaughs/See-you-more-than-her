#include "nvilidar_process.h"
#include <list>
#include <string>
#include "myconsole.h"
#include "mytimer.h"
#include "mystring.h"
#include <iostream> 
#include <istream> 
#include <sstream>

namespace nvilidar
{
	LidarProcess::LidarProcess(LidarCommTypeEnum comm, std::string name_ip, uint32_t port_baud)
	{
		//communicate type 
		LidarCommType = comm;

		//lidar para 
		Nvilidar_UserConfigTypeDef  cfg;
		//get the default para 
		LidarDefaultUserConfig(cfg);

		if (USE_SERIALPORT == comm)
		{
			cfg.serialport_name = name_ip;		//serialport name 
			cfg.serialport_baud = port_baud;	//serialport baud  

			lidar_serial.LidarLoadConfig(cfg);	//serialport  
		}
		else if (USE_SOCKET == comm)
		{
			cfg.ip_addr = name_ip;
			cfg.lidar_udp_port = port_baud;
			
			lidar_udp.LidarLoadConfig(cfg);		//network para  
			lidar_net_cfg.LidarLoadConfig(cfg);	//network config para   
		}
	}
	LidarProcess::~LidarProcess()
	{

	}

	//lidar init,for sync para ,get communicate state 
	bool LidarProcess::LidarInitialialize()
	{
		if (USE_SERIALPORT == LidarCommType)
		{
			return lidar_serial.LidarInitialialize();
		}
		else if (USE_SOCKET == LidarCommType)
		{
			return lidar_udp.LidarInitialialize();
		}
		return false;
	}

	//turn on the lidar  
	bool LidarProcess::LidarTurnOn()
	{
		if (USE_SERIALPORT == LidarCommType)
		{
			return lidar_serial.LidarTurnOn();
		}
		else if (USE_SOCKET == LidarCommType)
		{
			return lidar_udp.LidarTurnOn();
		}
		return false;
	}

	//ture off the lidar 
	bool LidarProcess::LidarTurnOff()
	{
		if (USE_SERIALPORT == LidarCommType)
		{
			return lidar_serial.LidarTurnOff();
		}
		else if (USE_SOCKET == LidarCommType)
		{
			return lidar_udp.LidarTurnOff();
		}
		return false;
	}

	//get lidar one circle data   
	bool LidarProcess::LidarSamplingProcess(LidarScan &scan, uint32_t timeout)
	{
		bool ret_state = false;							//return states 
		bool get_point_state = false;					//get point states 
		static uint32_t  no_response_times = 0;			//cannot receive data times 
		static uint32_t  auto_reconnect_times = 0;		//auto reconnect times 

		//get point from serialport or socket 
		if (USE_SERIALPORT == LidarCommType)
		{	
			get_point_state = lidar_serial.LidarSamplingProcess(scan, timeout);
		}
		else if (USE_SOCKET == LidarCommType)
		{	
			get_point_state = lidar_udp.LidarSamplingProcess(scan, timeout);
		}

		//get no res times 
		if(auto_reconnect_flag)			//auto reconnect 
		{
			ret_state = true;

			if(get_point_state)
			{
				no_response_times = 0;  	
			}
			else 
			{
				scan.points.clear();		  //clear points

				no_response_times++;
				if (no_response_times >= 10)  //max 20 seconds 
				{
					no_response_times = 0;

					//auto reconnect 
					bool reconnect = LidarAutoReconnect();
					if(true == reconnect)
					{
						auto_reconnect_times = 0;
					}
					else 
					{
						auto_reconnect_times ++;
						nvilidar::console.warning("Auto Reconnect %d......",auto_reconnect_times);
					}
				}	
			}
		}
		else 
		{
			ret_state = true;	

			if(get_point_state)
			{
				no_response_times = 0; 			 
			}
			else 
			{
				scan.points.clear();		  //clear points 

				no_response_times++;
				if (no_response_times >= 10)  //max 20 seconds 
				{
					ret_state = false;			  //no connect,return false,quit the point state 
					no_response_times = 0;
				}		
			}
		}

		return ret_state;
	}

	//quit  
	void LidarProcess::LidarCloseHandle()
	{
		if (USE_SERIALPORT == LidarCommType)
		{
			lidar_serial.LidarCloseHandle();
		}
		else if (USE_SOCKET == LidarCommType)
		{
			lidar_udp.LidarCloseHandle();
		}
	}

	//auto reconnect 
	bool LidarProcess::LidarAutoReconnect()
	{
		LidarCloseHandle();			//first,close the connect 
		delayMS(500);				//delay for thread close 
		if(false == LidarInitialialize())		//thread init 
		{
			return false;
		}
		if(false == LidarTurnOn())			//open lidar output point 
		{
			return false;
		}
		return true;
	}
	

	//=========================parameter sync=================================

	//lidar data  sync 
	void  LidarProcess::LidarParaSync(Nvilidar_UserConfigTypeDef &cfg)
	{
		cfg.storePara.samplingRate = (uint32_t)(cfg.sampling_rate * 1000);		// * 1000
		cfg.storePara.angleOffset = (uint16_t)(cfg.angle_offset * 64 + 0.5);	//角度偏移 	实际与雷达的  64倍 U16 	
		cfg.storePara.isHasSensitive = cfg.sensitive;							//是否带有信号质量 
		cfg.storePara.aimSpeed = (uint16_t)(cfg.aim_speed * 100 + 0.5);			//N Hz 实际与雷达的  100倍 U16 
		cfg.storePara.tailingLevel = cfg.tailing_level;							//拖尾等级 
		cfg.storePara.apdValue = cfg.apd_value;									//apd value 

		//ingnore array apart 
		std::vector<float> elems;
		std::stringstream ss(cfg.ignore_array_string);
		std::string number;
		while (std::getline(ss, number, ',')) {
			elems.push_back(atof(number.c_str()));
		}
		cfg.ignore_array = elems;

		//data to filter 
		if (cfg.ignore_array.size() % 2)
		{
			nvilidar::console.error("ignore array is odd need be even");
		}
		for (uint16_t i = 0; i < cfg.ignore_array.size(); i++)
		{
			if (cfg.ignore_array[i] < -180.0 && cfg.ignore_array[i] > 180.0)
			{
				nvilidar::console.error("ignore array should be between 0 and 360");
			}
		}

		//get auto connect state 
		auto_reconnect_flag = cfg.auto_reconnect;
	}

	//origin data 
	void  LidarProcess::LidarDefaultUserConfig(Nvilidar_UserConfigTypeDef &cfg)
	{
		//lidar model 
		cfg.lidar_model_name = NVILIDAR_VP300;
		//para to config 
		cfg.serialport_baud = 921600;
		cfg.serialport_name = "/dev/nvilidar";
		cfg.ip_addr = "192.168.1.200";		//192.168.1.200 lidar default ip 
		cfg.lidar_udp_port = 8100;			//8100 is lidar default port,use udp  
		cfg.config_tcp_port = 8200;			//8200 is lidar default config para port,use tcp  
		cfg.frame_id = "laser_frame";
		cfg.resolution_fixed = false;		//one circle same points  
		cfg.auto_reconnect = true;			//auto connect  
		cfg.reversion = false;				//add 180.0 state 
		cfg.inverted = false;				//mirror 
		cfg.angle_max = 180.0;
		cfg.angle_min = -180.0;
		cfg.range_max = 64.0;
		cfg.range_min = 0;
		cfg.aim_speed = 10.0;				//10Hz
		cfg.sampling_rate = 10;				//10k
		cfg.sensitive = false;				//default dont't use sensitive 
		cfg.tailing_level = 6;				//tailing level 
		cfg.angle_offset_change_flag = false;	//change angle offset flag
		cfg.angle_offset = 0.0;				//angle offset 
		cfg.apd_change_flag = false;		//can change apd value,default false
		cfg.apd_value = 500;				//change apd value 
		cfg.ignore_array_string = "";		//filter some angle 
		//过滤点信息 
		cfg.filter_jump_enable = true;		//jump point filter 
		cfg.filter_jump_value_min = 3;		//min filter 
		cfg.filter_jump_value_max = 25;		//max filter 

		LidarParaSync(cfg);
	}

	//==========================get serialport list=======================================
	std::string LidarProcess::LidarGetSerialList()
	{
		std::string port;       
		std::vector<NvilidarSerialPortInfo> ports = nvilidar::LidarDriverSerialport::getPortList();      
		std::vector<NvilidarSerialPortInfo>::iterator it;

		//列表信息
		if (ports.empty())
		{
			nvilidar::console.show("Not Lidar was detected.");
			return 0;
		}
		else if (1 == ports.size())
		{
			it = ports.begin();
			port = (*it).portName;
		}
		else
		{
			int id = 0;
			for (it = ports.begin(); it != ports.end(); it++)
			{
				nvilidar::console.show("%d. %s  %s\n", id, it->portName.c_str(), it->description.c_str());
				id++;
			}
			while (1)
			{
				nvilidar::console.show("Please select the lidar port:");
				std::string number;
				std::cin >> number;

				//参数不合法 
				if ((size_t)atoi(number.c_str()) >= ports.size())
				{
					continue;
				}
				//参数配置 
				it = ports.begin();
				id = atoi(number.c_str());

				//查找  
				port = ports.at(id).portName;

				break;
			}
		}

		return port;
	}

	//================================other interface for network=============================================
	bool LidarProcess::LidarSetNetConfig(std::string ip, std::string gateway, std::string mask)
	{
		// Nvilidar_NetConfigTypeDef net_cfg;

		//建立连接  
		bool state = lidar_net_cfg.LidarNetConfigConnect();
		if (false == state)
		{
			nvilidar::console.warning("connect to config port error!");
			return 0;
		}

		//set ip  
		//ip = "192.168.1.201";
		//gateway = "192.168.1.1";
		//mask = "255.255.255.0";
		state = lidar_net_cfg.LidarNetConfigWrite(ip, gateway, mask);
		if (false == state)
		{
			nvilidar::console.warning("set ip error!");
			return false;
		}
		delayMS(1000);
		//read ip address 
		state = lidar_net_cfg.LidarNetConfigRead(ip, gateway, mask);
		if (false == state)
		{
			nvilidar::console.warning("read ip error!");
			return false;
		}
		nvilidar::console.message("read net para:");
		nvilidar::console.message("ip:%s, gate:%s, mask:%s", ip.c_str(), gateway.c_str(), mask.c_str());

		//disconnect  
		lidar_net_cfg.LidarNetConfigDisConnect();

		return true;
	}



	//=============================ROS interface,for reload the parameter ===========================================
	void LidarProcess::LidarReloadPara(Nvilidar_UserConfigTypeDef cfg)
	{
		LidarParaSync(cfg);
		
		if (USE_SERIALPORT == LidarCommType)
		{
			lidar_serial.LidarLoadConfig(cfg);	//serialport  
		}
		else if (USE_SOCKET == LidarCommType)
		{		
			lidar_udp.LidarLoadConfig(cfg);	//network socket  
			lidar_net_cfg.LidarLoadConfig(cfg);	//config para 
		}
	}
}





