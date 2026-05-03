#include "../include/common.hpp"

namespace {
constexpr uint16_t kCaptureWidth = 720;
constexpr uint16_t kCaptureHeight = 1280;
}

bool IMAGEPROCESSOR::Initialize(std::array<int, 2>* in_img_shape)
{
    img_shape = *in_img_shape;
    format_online = SSNE_YUV422_16;

    int set_result = OnlineSetOutputImage(kPipeline0, format_online, kCaptureWidth, kCaptureHeight);
    if (set_result != 0) {
        printf("[ERROR] Failed to set online output image: %d\n", set_result);
        return false;
    }

    int open_result = OpenOnlinePipeline(kPipeline0);
    if (open_result != 0) {
        printf("[ERROR] Failed to open online pipeline: %d\n", open_result);
        return false;
    }

    img_shape = {kCaptureWidth, kCaptureHeight};
    opened = true;
    printf("[INFO] open online pipe0: %d, format=%u, size=%ux%u\n",
           open_result, format_online, kCaptureWidth, kCaptureHeight);
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
