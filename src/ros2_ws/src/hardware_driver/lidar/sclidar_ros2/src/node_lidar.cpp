#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <vector>
#include <thread>

#include "node_lidar.h"
#include "msg_recept.h"
#include "serial_port.h"
#include "lidar_data_processing.h"
#include "point_cloud_optimize.h"
#include "lidar_information.h"
#include "mtime.h"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/u_int16.hpp"
#include <rclcpp/rclcpp.hpp>


//#include "interface/Error.hpp"
using namespace std;

node_lidar_t node_lidar;

//#define ENCRYPTION_T

// using std::placeholders::_1;

node_lidar_t::~node_lidar_t(){
  	if(scan_node_buf)
	{
		delete[] scan_node_buf;
		scan_node_buf = NULL;
	}
	if (globalRecvBuffer)
	{
		delete[] globalRecvBuffer;
		globalRecvBuffer = NULL;
	}
	printf("Lidar Closed");
	node_lidar.serial_port->close();
	node_lidar.lidar_status.lidar_ready = false;
	node_lidar.lidar_status.close_lidar = true;
	flushSerial();

}

/*清空缓存区数据*/
void flushSerial()
{
	if (!node_lidar.lidar_status.isConnected){
		return;
	}
	
	size_t len = node_lidar.serial_port->available();
	
	if (len)
	{
		uint8_t *buffer = static_cast<uint8_t *>(alloca(len * sizeof(uint8_t)));
		size_t bytes_read = node_lidar.serial_port->read_data(buffer, len);
	}

	sleep_ms(20);
}

/*激光雷达启动状态判断函数*/
bool lidar_state_judgment()
{
	static bool status_judge = false;     //整体状态判断
	static bool lidar_flush = false;      //是否已经下发雷达启动指令
	static bool wait_speed_right = false; //是否获取到调速信息
	static bool lidar_start_flag = false; //下发雷达启动指令后的雷达反馈标志

	static uint64_t lidar_status_time = 0; //收到启动指令或者重启指令的时间


	if(node_lidar.lidar_status.lidar_ready != node_lidar.lidar_status.lidar_last_status || node_lidar.lidar_status.close_lidar)
	{
		printf("状态切换\n");
		// RCLCPP_INFO(get_logger(), "状态切换\n");
		node_lidar.lidar_status.close_lidar = false;
		node_lidar.lidar_status.lidar_last_status = node_lidar.lidar_status.lidar_ready;

		lidar_flush = false;
		wait_speed_right = false;
		lidar_start_flag = false;

		lidar_status_time = current_times();
		flushSerial();
	}
	if(node_lidar.lidar_status.lidar_trap_restart)
	{
		printf("状态异常重新启动 %lld\n",lidar_status_time);
		
		node_lidar.lidar_status.lidar_trap_restart = false;

		
		wait_speed_right = false;
		lidar_flush = false;
		lidar_start_flag = false;

		lidar_status_time = current_times();
		node_lidar.serial_port->write_data(end_lidar,4);
	}
	if(node_lidar.lidar_status.lidar_ready && !wait_speed_right)
	{
		
		if(current_times() - lidar_status_time > 1000 && !lidar_flush)
		{
			switch (node_lidar.lidar_general_info.version)
			{
				case M1C1_Mini_v1:
					// printf("V1版本启动雷达\n");
					node_lidar.serial_port->write_data(start_lidar,4);
					wait_speed_right = true;
					break;
				
				case M1C1_Mini_v2:
					// printf("V2版本启动雷达\n");
					node_lidar.serial_port->write_data(start_lidar,4);
					wait_speed_right = true;
					break;
				
				case M1CT_Coin_D2:
					// printf("D2版本启动雷达\n");
					node_lidar.serial_port->write_data(start_lidar,4);
					wait_speed_right = true;
					break;
				
				default:
					break;
			}
		}
		node_lidar.lidar_time.lidar_frequence_abnormal_time = current_times();
		node_lidar.lidar_time.system_start_time = current_times();
	}
	return wait_speed_right;
}

/************************************************************************/
/*  激光数据解析线程　Laser data analysis thread                           */
/************************************************************************/
int read_forever()
{	
	node_info  local_buf[128];
	size_t     count = 128;
	node_info  local_scan[1000];
	size_t     scan_count = 0;
	result_t   ans = RESULT_FAIL;
	
	memset(local_scan, 0, sizeof(local_scan));

	node_lidar.lidar_time.scan_time_record = current_times();

	while (1)
	{
		bool state_jugde = lidar_state_judgment();
		if(state_jugde)
		{
			count = 128;
			ans = node_lidar.lidar_data_processing.waitScanData(local_buf, count);
			if(!IS_OK(ans))
			{
				if(current_times()-node_lidar.lidar_time.system_start_time > 3000 )
				{
					if(!node_lidar.lidar_status.lidar_restart_try)
					{
						printf("尝试重启雷达\n");
						node_lidar.lidar_status.lidar_restart_try = true;
						node_lidar.lidar_status.lidar_trap_restart = true;
					}else{
						printf("@@@雷达被卡住\n");
						node_lidar.lidar_status.lidar_abnormal_state |= 0x01;
						usleep(100);
					}
				}	
			}else{
				node_lidar.lidar_status.lidar_restart_try = false;
				node_lidar.lidar_time.system_start_time = current_times();
			}
			for (size_t pos = 0; pos < count; ++pos)
			{
				if (local_buf[pos].sync_flag & LIDAR_RESP_MEASUREMENT_SYNCBIT)
				{
					if ((local_scan[0].sync_flag & LIDAR_RESP_MEASUREMENT_SYNCBIT))
					{
						local_scan[0].stamp = local_buf[pos].stamp;
						local_scan[0].scan_frequence = local_buf[pos].scan_frequence;
		
						/*频率异常超过30秒，触发异常状态*/
						/*
						if(local_scan[0].scan_frequence > 103 || local_scan[0].scan_frequence < 97)
						{
							if(current_times()-node_lidar.lidar_time.lidar_frequence_abnormal_time > 30000)
							{
								node_lidar.lidar_status.lidar_abnormal_state |= 0x02;
							}
						}else{
							node_lidar.lidar_time.lidar_frequence_abnormal_time = current_times();
						}*/
						
						
						node_lidar._lock.lock();
						if((node_lidar.lidar_time.scan_time_current - node_lidar.lidar_time.scan_time_record) > 2000)
						{
							// printf("full----- count=%d,time=%lld\n",scan_count,current_times());
							node_lidar.lidar_time.scan_time_record = node_lidar.lidar_time.scan_time_current;
						}
						memcpy(node_lidar.scan_node_buf, local_scan, scan_count * sizeof(node_info));
						node_lidar.scan_node_count = scan_count;
						node_lidar.lidar_time.scan_time_current = current_times();
						node_lidar._dataEvent.set();
						node_lidar._lock.unlock();
					}
					scan_count = 0;
				}
				local_scan[scan_count++] = local_buf[pos];
				if (scan_count == _countof(local_scan))
				{
					scan_count -= 1;
				}
			}
		}
		else{
			flushSerial();
			delay(100);
		}
		
	}
	return RESULT_OK;
}

/*线程事件同步函数*/
result_t grabScanData(uint32_t timeout) {
	switch (node_lidar._dataEvent.wait(timeout)) {
		case Event::EVENT_TIMEOUT:
			return RESULT_TIMEOUT;

		case Event::EVENT_OK: 
			{
				node_lidar._lock.lock();
				if (node_lidar.scan_node_count == 0) {
					return RESULT_FAIL;
				}
				node_lidar._lock.unlock();
			}
			return RESULT_OK;

		default:
			return RESULT_FAIL;
	}
}

/*处理雷达的线程*/
bool data_handling(LaserScan &outscan)
{
	//node_lidar.lidar_time.tim_scan_start = getTime();	
	if(grabScanData(2000)==RESULT_OK)
	{
		send_lidar_data(outscan);
		// RCLCPP_INFO(rclcpp::get_logger("sclidar"),"data_handling true");
		return true;
	}else{
		// RCLCPP_INFO(rclcpp::get_logger("sclidar"),"data_handling false");
		return false;
	}	
}


/*处理最新一圈雷达的数据*/
int send_lidar_data(LaserScan &outscan)
{
	node_lidar._lock.lock();

	size_t count = node_lidar.scan_node_count;

	if(count < MAX_SCAN_NODES && count > 0)
	{	
		node_lidar.lidar_time.tim_scan_end = getTime();
		uint64_t scan_time = node_lidar.lidar_general_info.m_PointTime * (count - 1);
		node_lidar.lidar_time.tim_scan_start = node_lidar.lidar_time.tim_scan_end -  scan_time ;

		node_lidar.lidar_block.lidar_zero_count = 0;


		outscan.config.angle_increment = (2.0*M_PI/count);
		outscan.config.min_angle = 0;
		outscan.config.max_angle = 2*M_PI;
		outscan.config.min_range = 0.10;
		outscan.config.max_range = 10.0; //测量的最远距离是10m
		outscan.config.scan_time =  static_cast<float>(scan_time * 1.0 / 1e9);
    	outscan.config.time_increment = outscan.config.scan_time / (double)(count - 1);
		outscan.stamp = node_lidar.lidar_time.tim_scan_start;
		//scan_msg->header.frame_id = node_lidar.lidar_general_info.frame_id;
		//scan_msg->header.stamp = ros::Time::now();

		if(node_lidar.lidar_status.isConnected)
		{
			//outscan.points.clear();
			float range = 0;
			float angle = 0.0;
			uint16_t intensity = 0;
			for (int i = 0; i < count; i++)
			{
				LaserPoint point;
				LaserPoint point_check;
				angle = static_cast<float>((node_lidar.scan_node_buf[i].angle_q6_checkbit >>
											LIDAR_RESP_MEASUREMENT_ANGLE_SHIFT) /64.0f);
				range = (node_lidar.scan_node_buf[i].distance_q2/1000.0f);
				intensity = node_lidar.scan_node_buf[i].sync_quality;

				point_check.angle = angle;
				point_check.range = range;
				point_check.intensity = intensity;
				
				if(0 <= angle && angle <= 360){
					point.range = range;
					point.angle = angle;
					point.intensity = intensity;
				}else{
					point.range = 0.0;
					point.intensity = 0;
					point.angle = 0.0;
				}

				if(range < 0.1)
				{
					node_lidar.lidar_block.lidar_zero_count++;
				}
				outscan.points.push_back(point);
			}

			node_lidar._lock.unlock();

			/*雷达被遮挡判断*/
			
			node_lidar.optimize_lidar.lidar_blocked_judge(count);


			if (node_lidar.lidar_status.FilterEnable)
			{
				node_lidar.optimize_lidar.PointCloudFilter(&outscan);
			}


		}
	}
	// RCLCPP_INFO(rclcpp::get_logger("sclidar"),"unlock");
	node_lidar._lock.unlock();
	return 1;
}

/*设置串口信息的函数*/
bool lidar_set_port()
{
	if (node_lidar.lidar_status.isConnected)
	{
		return true;
	}

	std::cout << "***" << node_lidar.lidar_general_info.port << std::endl;
	node_lidar.serial_port=make_shared<Serial_Port>(node_lidar.lidar_general_info.port,
							node_lidar.lidar_general_info.m_SerialBaudrate,Timeout::simpleTimeout(DEFAULT_TIMEOUT));
	if (!node_lidar.serial_port->open())
	{
		return false;
	}
	node_lidar.lidar_status.isConnected=true;
	sleep_ms(100);
    node_lidar.serial_port->setDTR(0);
	return true;
}

/*初始化函数*/
bool initialize()
{
	if(node_lidar.lidar_status.optimize_enable)
	{
		//求雷达安装位置到扫地机边缘的距离
		node_lidar.optimize_lidar.lidar_blocked_init();
	}

	switch (node_lidar.lidar_general_info.version)
	{
	case M1C1_Mini_v1:
		printf("version M1C1_Mini_v1\n");
		node_lidar.lidar_general_info.m_SerialBaudrate = 115200;
		node_lidar.lidar_data_processing.PackageSampleBytes = 2;
		break;

	case M1C1_Mini_v2:
		printf("version M1C1_Mini_v2\n");
		node_lidar.lidar_general_info.m_SerialBaudrate = 150000;
		node_lidar.lidar_data_processing.PackageSampleBytes = 3;
		node_lidar.lidar_general_info.m_intensities = true;
		break;

	case M1CT_Coin_Plus:
		printf("version M1CT_Coin_Plus\n");
		node_lidar.lidar_general_info.m_SerialBaudrate = 115200;
		node_lidar.lidar_data_processing.PackageSampleBytes = 3;
		break;
	
	case M1CT_Coin_D2:
		printf("version M1CT_D2\n");
		node_lidar.lidar_general_info.m_SerialBaudrate = 230400;
		node_lidar.lidar_data_processing.PackageSampleBytes = 3;
		node_lidar.lidar_general_info.m_intensities = true;
		break;
	
	default:
		break;
	}

	//设置通信串口
	if(!lidar_set_port()){
		printf("lidar_set_port wrong\n");
		return false;
	}

	//获取串口获取每个byte所用的时间
	node_lidar.lidar_general_info.trans_delay = node_lidar.serial_port->getByteTime();
	node_lidar.scan_node_buf = new node_info[1000];
	node_lidar.globalRecvBuffer = new uint8_t[sizeof(node_packages)];
	return true;
}


int node_start(int argc, char **argv)
{

	
	rclcpp::init(argc, argv);
	auto node = rclcpp::Node::make_shared("sclidar");
	
	node->declare_parameter("port","/dev/ttyUSB0");
	node->get_parameter("port", node_lidar.lidar_general_info.port);

	node->declare_parameter("baudrate",115200);
	node->get_parameter("baudrate", node_lidar.lidar_general_info.m_SerialBaudrate);

	node->declare_parameter("frame_id","laser");
	node->get_parameter("frame_id", node_lidar.lidar_general_info.frame_id);

	node->declare_parameter("version",1);
	node->get_parameter("version", node_lidar.lidar_general_info.version);

	rclcpp::Publisher<std_msgs::msg::String>::SharedPtr error_pub;
	error_pub = node->create_publisher<std_msgs::msg::String>("lsd_error", 10);
	std_msgs::msg::String pubdata;	
	
	bool ret_init = initialize();

	if(!ret_init)
	{
		RCLCPP_ERROR(rclcpp::get_logger("sclidar"),"node_lidar init error");
		pubdata.data="node_lidar init error\n";
		error_pub->publish(pubdata);
		return -1;
	}
	RCLCPP_INFO(rclcpp::get_logger("sclidar"),"sclidar start scan");
	/*读取激光雷达数据的线程*/
	thread t1(read_forever);
	t1.detach();
	
	node_lidar.lidar_status.lidar_ready = true;
	rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr laser_pub;
	laser_pub = node->create_publisher<sensor_msgs::msg::LaserScan>("scan", 1);
	
	while(rclcpp::ok())
	{
		if(node_lidar.lidar_status.lidar_abnormal_state != 0)
		{
			if(node_lidar.lidar_status.lidar_abnormal_state & 0x01)
			{
				pubdata.data="node_lidar is trapped\n";
				error_pub->publish(pubdata);
				printf("异常状态1---trapped\n");
			}
			if(node_lidar.lidar_status.lidar_abnormal_state & 0x02)
			{
				pubdata.data="node_lidar frequence abnormal\n";
				error_pub->publish(pubdata);
				printf("异常状态2---frequence abnormal\n");
			}
			if(node_lidar.lidar_status.lidar_abnormal_state & 0x04)
			{
				pubdata.data="node_lidar is blocked\n";
				error_pub->publish(pubdata);
				printf("异常状态3---blocked\n");
			}
			node_lidar.serial_port->write_data(end_lidar,4);
			node_lidar.lidar_status.lidar_ready = false;

			sleep(1);
		}
		LaserScan scan;
		if(data_handling(scan))
		{
			auto scan_msg = std::make_shared<sensor_msgs::msg::LaserScan>();

			scan_msg->ranges.resize(scan.points.size());
			scan_msg->intensities.resize(scan.points.size());

			scan_msg->header.stamp.sec = RCL_NS_TO_S(scan.stamp);;
			scan_msg->header.stamp.nanosec = scan.stamp - RCL_S_TO_NS(scan_msg->header.stamp.sec);
			scan_msg->header.frame_id = node_lidar.lidar_general_info.frame_id;

			scan_msg->angle_min = scan.config.min_angle;
			scan_msg->angle_max = scan.config.max_angle;
			scan_msg->angle_increment = scan.config.angle_increment;
			scan_msg->scan_time = scan.config.scan_time;
			scan_msg->time_increment = scan.config.time_increment;
			scan_msg->range_min = scan.config.min_range;
			scan_msg->range_max = scan.config.max_range;
			for(int i=0; i < scan.points.size(); i++) {
				scan_msg->ranges[i] = scan.points[i].range;
				scan_msg->intensities[i] = scan.points[i].intensity;
			}
			laser_pub->publish(*scan_msg);
    	}
	}
	node_lidar.serial_port->write_data(end_lidar,4);
	return 0;
}

