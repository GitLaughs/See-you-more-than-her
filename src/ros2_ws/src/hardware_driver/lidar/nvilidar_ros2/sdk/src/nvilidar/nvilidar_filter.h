#pragma once

#include "nvilidar_def.h"
#include "nvilidar_protocol.h"

//---定义库信息 VS系列的生成库文件  
#ifdef WIN32
	#define NVILIDAR_FILTER_API __declspec(dllexport)
#else
	#define NVILIDAR_FILTER_API
#endif // ifdef WIN32


namespace nvilidar
{
    //lidar driver 
	class  NVILIDAR_FILTER_API LidarFilter
    {
		public:
			LidarFilter();		
			~LidarFilter();

			void LidarFilterLoadPara(Nvilidar_UserConfigTypeDef cfg);			//加载参数信息
			void LidarJumpFilter(std::vector<Nvilidar_Node_Info> &in);		//跳动点过滤 

		private:
			Nvilidar_UserConfigTypeDef     lidar_cfg;				//雷达型号
    };
}
