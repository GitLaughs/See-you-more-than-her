/*
 * @Filename: utils.hpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-12-30 14-57-47
 * @Copyright (c) 2025 SmartSens
 */
#pragma once

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
    #define LOG_DEBUG(fmt_str, ...) \
        printf("[DEBUG] " fmt_str, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt_str, ...) // 编译时移除
#endif


