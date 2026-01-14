#include <assert.h>
#include "../include/common.hpp"
#include <iostream>


void MobileNetV2::Initialize(std::string& model_path) {
    char* model_path_char = const_cast<char*>(model_path.c_str());
    std::cout << "model_file name " << model_path_char << std::endl;
    model_id = ssne_loadmodel(model_path_char, SSNE_STATIC_ALLOC);
}

void MobileNetV2::Predict(ssne_tensor_t input[], ssne_tensor_t output[]) {
    //auto start = std::chrono::high_resolution_clock::now();
    // 前向推理
    if (ssne_inference(model_id, 1, input))
    {
        fprintf(stderr, "ssne inference fail!\n");
    }
    ssne_getoutput(model_id, 1, output);
}

void MobileNetV2::Release()
{

}
