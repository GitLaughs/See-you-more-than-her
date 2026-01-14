#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#include "smartsoc/ssne_api.h"


int isp_debug()
{
    // pipe0
    OnlineSetBinning(kPipeline0, kDownSample1x, kDownSample1x);
    OnlineSetOutputImage(kPipeline0, SSNE_YUV422_16, 1920, 1280);
    OpenOnlinePipeline(kPipeline0);

    uint8_t load_flag;  // 0: 当前load偶帧; 1: 当前load奇帧（初始值为0）
    ssne_tensor_t debug_tensor0, debug_tensor1;

    int fid = 0;
    while (fid < 10000)
    {
        if (fid == 0)
        {
            GetImageData(&debug_tensor0, kPipeline0, kSensor0, 0);
        }
        else if (fid == 1)
        {
            GetImageData(&debug_tensor1, kPipeline0, kSensor0, 0);
            set_isp_debug_config(debug_tensor0, debug_tensor1);
            printf("[isp debug]set config\n");
        }
        else
        {
            get_even_or_odd_flag(load_flag);
            printf("[isp debug]frame %d, flag %d\n", fid, load_flag);
            if (load_flag == 0)
            {
                GetImageData(&debug_tensor0, kPipeline0, kSensor0, 0);
                printf("[isp debug]write even data\n");
            }
            else
            {
                GetImageData(&debug_tensor1, kPipeline0, kSensor0, 0);
                printf("[isp debug]write odd data\n");
            }
            start_isp_debug_load();
            printf("[isp debug]start load\n");
        }

        sleep(0.1);
        fid ++;
    }

    CloseOnlinePipeline(kPipeline0);
    return 0;
}


int main()
{
    isp_debug();
    return 0;
}
