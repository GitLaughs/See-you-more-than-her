#include "gpio_test_runner.hpp"

#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <string>
#include <unistd.h>
#include <vector>

#include <smartsoc/gpio_api.h>

namespace {

bool parse_u32(const std::string& value, uint32_t* out) {
    if (!out || value.empty()) return false;

    errno = 0;
    char* end = nullptr;
    unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
    if (errno != 0 || !end || *end != '\0') return false;
    if (parsed > std::numeric_limits<uint32_t>::max()) return false;
    *out = static_cast<uint32_t>(parsed);
    return true;
}

bool parse_pin(const std::string& value, int* pin) {
    char* end = nullptr;
    long parsed = std::strtol(value.c_str(), &end, 10);
    if (!end || *end != '\0') return false;
    if (parsed != 0 && parsed != 2 && parsed != 8 && parsed != 9 && parsed != 10) return false;
    *pin = static_cast<int>(parsed);
    return true;
}

bool parse_mode(const std::string& value, GpioTestMode* mode) {
    if (value == "tx") {
        *mode = GpioTestMode::Tx;
        return true;
    }
    if (value == "rx") {
        *mode = GpioTestMode::Rx;
        return true;
    }
    if (value == "loop") {
        *mode = GpioTestMode::Loop;
        return true;
    }
    return false;
}

uint16_t pin_to_mask(int pin) {
    switch (pin) {
        case 0: return GPIO_PIN_0;
        case 2: return GPIO_PIN_2;
        case 8: return GPIO_PIN_8;
        case 9: return GPIO_PIN_9;
        case 10: return GPIO_PIN_10;
        default: return 0;
    }
}

const char* mode_name(GpioTestMode mode) {
    switch (mode) {
        case GpioTestMode::Tx: return "tx";
        case GpioTestMode::Rx: return "rx";
        case GpioTestMode::Loop: return "loop";
    }
    return "unknown";
}

bool is_uart_mux_pin(int pin) {
    return pin == 0 || pin == 2;
}

void log_timestamp_prefix() {
    using clock = std::chrono::system_clock;
    const clock::time_point now = clock::now();
    const std::time_t now_time = clock::to_time_t(now);
    const long millis = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);

    std::tm local_time{};
#if defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    localtime_r(&now_time, &local_time);
#else
    const std::tm* tmp = std::localtime(&now_time);
    if (tmp) local_time = *tmp;
#endif

    std::fprintf(stderr,
                 "[gpio_test][%04d-%02d-%02d %02d:%02d:%02d.%03ld] ",
                 local_time.tm_year + 1900,
                 local_time.tm_mon + 1,
                 local_time.tm_mday,
                 local_time.tm_hour,
                 local_time.tm_min,
                 local_time.tm_sec,
                 millis);
}

void log_line(const char* format, ...) {
    log_timestamp_prefix();
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fputc('\n', stderr);
}

bool gpio_call_ok(int rc, const char* operation, int pin) {
    if (rc == GPIO_SUCCESS) return true;
    log_line("pin=%d %s failed rc=%d", pin, operation, rc);
    return false;
}

bool prepare_gpio_pin(gpio_handle_t gpio, int pin, uint16_t pin_mask, gpio_mode_t mode) {
    if (!gpio) return false;
    if (is_uart_mux_pin(pin)) {
        if (!gpio_call_ok(gpio_set_alternate(gpio, pin_mask, GPIO_AF_INPUT_NONE, GPIO_AF_OUTPUT_NONE),
                          "gpio_set_alternate(GPIO_AF_INPUT_NONE, GPIO_AF_OUTPUT_NONE)",
                          pin)) {
            return false;
        }
    }
    if (!gpio_call_ok(gpio_set_enable(gpio, pin_mask, true), "gpio_set_enable(true)", pin)) return false;
    if (!gpio_call_ok(gpio_set_mode(gpio, pin_mask, mode), "gpio_set_mode", pin)) return false;
    return true;
}

void restore_pin_mux_if_needed(gpio_handle_t gpio, int pin, uint16_t pin_mask) {
    if (!gpio || !is_uart_mux_pin(pin)) return;
    if (pin == 0) {
        gpio_set_alternate(gpio, pin_mask, GPIO_AF_INPUT_NONE, GPIO_AF_OUTPUT_UART_TX0);
    } else if (pin == 2) {
        gpio_set_alternate(gpio, pin_mask, GPIO_AF_INPUT_UART_RX0, GPIO_AF_OUTPUT_NONE);
    }
}

bool read_pin_state(gpio_handle_t gpio, int pin, uint16_t pin_mask, bool* high) {
    if (!gpio || !high) return false;
    uint16_t state_mask = 0;
    if (!gpio_call_ok(gpio_read_pin(gpio, pin_mask, &state_mask), "gpio_read_pin", pin)) return false;
    *high = (state_mask & pin_mask) != 0;
    return true;
}

int run_tx_mode(gpio_handle_t gpio, int pin, uint16_t pin_mask, const GpioTestConfig& config) {
    if (!prepare_gpio_pin(gpio, pin, pin_mask, GPIO_MODE_OUTPUT)) return 1;

    const uint32_t half_period_us = config.period_ms * 1000;
    const uint64_t total_cycles = (static_cast<uint64_t>(config.duration_s) * 1000ULL) / config.period_ms;
    gpio_pin_state_t state = GPIO_PIN_RESET;

    log_line("mode=tx pin=%d period_ms=%u duration_s=%u cycles=%llu",
             pin,
             config.period_ms,
             config.duration_s,
             static_cast<unsigned long long>(total_cycles));

    if (!gpio_call_ok(gpio_write_pin(gpio, pin_mask, state), "gpio_write_pin(reset)", pin)) return 1;

    for (uint64_t cycle = 0; cycle < total_cycles; ++cycle) {
        state = (state == GPIO_PIN_RESET) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        if (!gpio_call_ok(gpio_write_pin(gpio, pin_mask, state), "gpio_write_pin(toggle)", pin)) return 1;
        log_line("mode=tx pin=%d cycle=%llu level=%s",
                 pin,
                 static_cast<unsigned long long>(cycle + 1),
                 state == GPIO_PIN_SET ? "HIGH" : "LOW");
        usleep(half_period_us);
    }

    gpio_write_pin(gpio, pin_mask, GPIO_PIN_RESET);
    log_line("mode=tx pin=%d finished", pin);
    return 0;
}

int run_rx_mode(gpio_handle_t gpio, int pin, uint16_t pin_mask, const GpioTestConfig& config) {
    if (!prepare_gpio_pin(gpio, pin, pin_mask, GPIO_MODE_INPUT)) return 1;

    const uint32_t poll_us = config.period_ms * 1000;
    const uint64_t max_samples = (static_cast<uint64_t>(config.duration_s) * 1000ULL) / config.period_ms;
    bool current_high = false;
    bool previous_high = false;

    if (!read_pin_state(gpio, pin, pin_mask, &previous_high)) return 1;
    log_line("mode=rx pin=%d period_ms=%u duration_s=%u initial=%s",
             pin,
             config.period_ms,
             config.duration_s,
             previous_high ? "HIGH" : "LOW");

    for (uint64_t sample = 0; sample < max_samples; ++sample) {
        usleep(poll_us);
        if (!read_pin_state(gpio, pin, pin_mask, &current_high)) return 1;
        if (current_high != previous_high) {
            log_line("mode=rx pin=%d sample=%llu edge=%s",
                     pin,
                     static_cast<unsigned long long>(sample + 1),
                     current_high ? "RISING" : "FALLING");
            previous_high = current_high;
        }
    }

    log_line("mode=rx pin=%d finished final=%s", pin, previous_high ? "HIGH" : "LOW");
    return 0;
}

int run_loop_mode(gpio_handle_t gpio, int pin, uint16_t pin_mask, const GpioTestConfig& config) {
    if (!prepare_gpio_pin(gpio, pin, pin_mask, GPIO_MODE_OUTPUT)) return 1;

    const uint32_t period_us = config.period_ms * 1000;
    const uint64_t total_cycles = (static_cast<uint64_t>(config.duration_s) * 1000ULL) / config.period_ms;
    gpio_pin_state_t state = GPIO_PIN_RESET;

    log_line("mode=loop pin=%d period_ms=%u duration_s=%u cycles=%llu",
             pin,
             config.period_ms,
             config.duration_s,
             static_cast<unsigned long long>(total_cycles));

    for (uint64_t cycle = 0; cycle < total_cycles; ++cycle) {
        state = (state == GPIO_PIN_RESET) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        if (!gpio_call_ok(gpio_write_pin(gpio, pin_mask, state), "gpio_write_pin(loop)", pin)) return 1;

        bool observed_high = false;
        if (!read_pin_state(gpio, pin, pin_mask, &observed_high)) return 1;

        const bool expected_high = state == GPIO_PIN_SET;
        log_line("mode=loop pin=%d cycle=%llu write=%s read=%s match=%s",
                 pin,
                 static_cast<unsigned long long>(cycle + 1),
                 expected_high ? "HIGH" : "LOW",
                 observed_high ? "HIGH" : "LOW",
                 observed_high == expected_high ? "yes" : "no");
        if (observed_high != expected_high) {
            gpio_write_pin(gpio, pin_mask, GPIO_PIN_RESET);
            return 1;
        }
        usleep(period_us);
    }

    gpio_write_pin(gpio, pin_mask, GPIO_PIN_RESET);
    log_line("mode=loop pin=%d finished", pin);
    return 0;
}

}  // namespace

bool ParseGpioTestArgs(const std::vector<std::string>& args,
                       GpioTestConfig* config,
                       std::string* error_message) {
    if (!config || !error_message) return false;

    *config = GpioTestConfig{};
    bool have_pin = false;
    bool have_mode = false;

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--pin") {
            if (i + 1 >= args.size() || !parse_pin(args[++i], &config->pin)) {
                *error_message = "invalid --pin, expected one of 0/2/8/9/10; usage: --gpio-test --pin 8 --mode tx [--period-ms 500] [--duration-s 10]";
                return false;
            }
            have_pin = true;
        } else if (arg == "--mode") {
            if (i + 1 >= args.size() || !parse_mode(args[++i], &config->mode)) {
                *error_message = "invalid --mode, expected tx|rx|loop; usage: --gpio-test --pin 8 --mode tx [--period-ms 500] [--duration-s 10]";
                return false;
            }
            have_mode = true;
        } else if (arg == "--period-ms") {
            if (i + 1 >= args.size() || !parse_u32(args[++i], &config->period_ms) || config->period_ms == 0) {
                *error_message = "invalid --period-ms, expected positive integer";
                return false;
            }
        } else if (arg == "--duration-s") {
            if (i + 1 >= args.size() || !parse_u32(args[++i], &config->duration_s) || config->duration_s == 0) {
                *error_message = "invalid --duration-s, expected positive integer";
                return false;
            }
        } else {
            *error_message = std::string("unknown argument: ") + arg;
            return false;
        }
    }

    if (!have_pin) {
        *error_message = "missing --pin; usage: --gpio-test --pin 8 --mode tx [--period-ms 500] [--duration-s 10]";
        return false;
    }
    if (!have_mode) {
        *error_message = "missing --mode; usage: --gpio-test --pin 8 --mode tx [--period-ms 500] [--duration-s 10]";
        return false;
    }
    if (config->mode == GpioTestMode::Rx) {
        config->period_ms = 500;
    }
    return true;
}

int RunGpioTest(const GpioTestConfig& config) {
    const uint16_t pin_mask = pin_to_mask(config.pin);
    if (pin_mask == 0) {
        std::fprintf(stderr, "[gpio_test] unsupported pin %d\n", config.pin);
        return 1;
    }

    gpio_handle_t gpio = gpio_init();
    if (!gpio) {
        std::fprintf(stderr, "[gpio_test] gpio_init failed\n");
        return 1;
    }

    log_line("start mode=%s pin=%d period_ms=%u duration_s=%u",
             mode_name(config.mode),
             config.pin,
             config.period_ms,
             config.duration_s);

    int rc = 1;
    switch (config.mode) {
        case GpioTestMode::Tx:
            rc = run_tx_mode(gpio, config.pin, pin_mask, config);
            break;
        case GpioTestMode::Rx:
            rc = run_rx_mode(gpio, config.pin, pin_mask, config);
            break;
        case GpioTestMode::Loop:
            rc = run_loop_mode(gpio, config.pin, pin_mask, config);
            break;
    }

    restore_pin_mux_if_needed(gpio, config.pin, pin_mask);
    gpio_close(gpio);
    log_line("exit rc=%d", rc);
    return rc;
}
