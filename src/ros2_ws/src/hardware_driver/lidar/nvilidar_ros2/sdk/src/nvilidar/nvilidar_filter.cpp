#include "nvilidar_filter.h"
#include <list>
#include <string>
#include <iostream> 
#include <istream> 
#include <sstream>
#include <math.h>

namespace nvilidar
{
	LidarFilter::LidarFilter()
	{

	}
	LidarFilter::~LidarFilter()
	{

	}

	//雷达加载参数  
	void LidarFilter::LidarFilterLoadPara(Nvilidar_UserConfigTypeDef cfg)
	{
		lidar_cfg = cfg;
	}

	//雷达过滤跳动点信息 
	void LidarFilter::LidarJumpFilter(std::vector<Nvilidar_Node_Info> &in)
	{
		size_t i = 0;


		//异常值  退出解析 
		if(in.size() <= 4)
		{
			return;
		}

		//正常值  进行过滤管理信息 
		while(i < (in.size()-3))
		{
			if(((abs(in.at(i+1).lidar_distance - in.at(i).lidar_distance) >= lidar_cfg.filter_jump_value_min)
							&&(abs(in.at(i+1).lidar_distance - in.at(i).lidar_distance) <= lidar_cfg.filter_jump_value_max))
				&& ((abs(in.at(i+1).lidar_distance - in.at(i+2).lidar_distance) >= lidar_cfg.filter_jump_value_min)
							&& (abs(in.at(i+1).lidar_distance - in.at(i+2).lidar_distance) <= lidar_cfg.filter_jump_value_max)))
			{
				in.at(i+1).lidar_distance = (in.at(i).lidar_distance + in.at(i+2).lidar_distance) / 2;
				i += 2;

				//printf("filter:%d\n",in.at(i+1).lidar_distance);
			}
			else
			{
				i++;
			}
		}
	}
}





