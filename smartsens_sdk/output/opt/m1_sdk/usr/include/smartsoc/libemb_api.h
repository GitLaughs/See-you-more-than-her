#pragma once
#include <stdint.h>
#include <stddef.h>

extern "C"{

typedef struct emb_user_data{
    int data_length;
    void *data;
}emb_user_data_t;

/**
 * @brief 打开emb
 * @param emb_usr_data 输入要显示在图像上的数据
 * @return  错误码
 */
int Start_Emb(emb_user_data_t emb_usr);

/**
 * @brief 关闭emb
 * @return 错误码
 */
int Close_Emb();

/**
 * @brief 更新emb数据
 * @return 错误码
 */
int Emb_Updata(emb_user_data_t emb_usr);

/**
 * @brief 检查emb运行状态
 * @return错误码
 */
int Emb_Check();

/**
 * @brief 检查emb运行结果CRC校验
 * @return 错误码
 */
int Check_EmbCRCReslut();

}
