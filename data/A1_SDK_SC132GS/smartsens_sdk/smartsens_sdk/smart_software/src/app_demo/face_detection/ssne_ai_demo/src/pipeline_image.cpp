#include "../include/common.hpp"

namespace {
constexpr uint16_t kSdkNativeWidth = 720;
constexpr uint16_t kSdkNativeHeight = 1280;
}

bool IMAGEPROCESSOR::Initialize(std::array<int, 2>* in_img_shape)
{
    img_shape = *in_img_shape;
    format_online = SSNE_YUV422_16;

    uint16_t img_width = static_cast<uint16_t>(img_shape[0]);
    uint16_t img_height = static_cast<uint16_t>(img_shape[1]);
    int set_result = OnlineSetOutputImage(kPipeline0, format_online, img_width, img_height);
    if (set_result != 0) {
        printf("[ERROR] Failed to set online output image: %d, size=%ux%u\n",
               set_result, img_width, img_height);
        return false;
    }

    int open_result = OpenOnlinePipeline(kPipeline0);
    if (open_result != 0 && (img_width != kSdkNativeWidth || img_height != kSdkNativeHeight)) {
        printf("[WARN] Failed to open online pipeline: %d, retry native size=%ux%u\n",
               open_result, kSdkNativeWidth, kSdkNativeHeight);
        img_width = kSdkNativeWidth;
        img_height = kSdkNativeHeight;
        set_result = OnlineSetOutputImage(kPipeline0, format_online, img_width, img_height);
        if (set_result != 0) {
            printf("[ERROR] Failed to set native online output image: %d, size=%ux%u\n",
                   set_result, img_width, img_height);
            return false;
        }
        open_result = OpenOnlinePipeline(kPipeline0);
    }

    if (open_result != 0) {
        printf("[ERROR] Failed to open online pipeline: %d\n", open_result);
        return false;
    }

    img_shape = {img_width, img_height};
    opened = true;
    printf("[INFO] open online pipe0: %d, format=%u, size=%ux%u\n",
           open_result, format_online, img_width, img_height);
    return true;
}

bool IMAGEPROCESSOR::GetImage(ssne_tensor_t* img_sensor)
{
    if (!opened) {
        return false;
    }

    int capture_code = GetImageData(img_sensor, kPipeline0, kSensor0, false);
    if (capture_code != 0) {
        printf("[IMAGEPROCESSOR] Get Invalid Image from kPipeline0: %d\n", capture_code);
        return false;
    }
    return true;
}

void IMAGEPROCESSOR::Release()
{
    if (opened) {
        CloseOnlinePipeline(kPipeline0);
        opened = false;
        printf("[INFO] OnlinePipe closed!\n");
    }
}
