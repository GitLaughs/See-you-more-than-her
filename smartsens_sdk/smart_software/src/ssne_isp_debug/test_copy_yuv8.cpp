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

    ssne_tensor_t on_img;

    uint8_t load_flag;  // 0: 当前load偶帧; 1: 当前load奇帧（初始值为0）
    ssne_tensor_t debug_tensor0, debug_tensor1;
    debug_tensor0 = create_tensor(1920, 1280, SSNE_YUV422_16, SSNE_BUF_AI);
    debug_tensor1 = create_tensor(1920, 1280, SSNE_YUV422_16, SSNE_BUF_AI);
    set_isp_debug_config(debug_tensor0, debug_tensor1);
    printf("[isp debug]set config\n");

    int fid = 0;
    while (fid < 10000)
    {
        GetImageData(&on_img, kPipeline0, kSensor0, 0);

        if (fid == 0)
        {
            copy_tensor_buffer(on_img, debug_tensor0);
        }
        else if (fid == 1)
        {
            copy_tensor_buffer(on_img, debug_tensor1);
        }
        else
        {
            get_even_or_odd_flag(load_flag);
            printf("[isp debug]frame %d, flag %d\n", fid, load_flag);
            if (load_flag == 0)
            {
                copy_tensor_buffer(on_img, debug_tensor0);
                printf("[isp debug]copy to even data\n");
            }
            else
            {
                copy_tensor_buffer(on_img, debug_tensor1);
                printf("[isp debug]copy to odd data\n");
            }
            start_isp_debug_load();
            printf("[isp debug]start load\n");
        }

        sleep(0.1);
        fid ++;
    }

    release_tensor(debug_tensor0);
    release_tensor(debug_tensor1);

    CloseOnlinePipeline(kPipeline0);
    return 0;
}


int main()
{
    isp_debug();
    return 0;
}
