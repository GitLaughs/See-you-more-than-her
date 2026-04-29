#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class GpioTestMode {
    Tx,
    Rx,
    Loop,
};

struct GpioTestConfig {
    int pin = -1;
    GpioTestMode mode = GpioTestMode::Tx;
    uint32_t period_ms = 500;
    uint32_t duration_s = 10;
};

bool ParseGpioTestArgs(const std::vector<std::string>& args,
                       GpioTestConfig* config,
                       std::string* error_message);

int RunGpioTest(const GpioTestConfig& config);
