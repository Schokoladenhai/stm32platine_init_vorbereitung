#include "drivers/servo/servo.hpp"

namespace drivers {
namespace {

// Takt, mit dem der Timer zählt. Bei APB1-Vorteiler != 1 laufen die APB1-Timer mit doppeltem PCLK1.
uint32_t timer_tick_hz(TIM_HandleTypeDef* htim) {
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
        pclk *= 2;
    }
    return pclk / (htim->Instance->PSC + 1);
}

}  // namespace

bool Servo::start() {
    if (config_.htim == nullptr || config_.frequency_hz == 0 || config_.min_us >= config_.max_us) {
        return false;
    }
    // Pulsbreiten in µs gehen nur, wenn ein Timerschritt genau 1 µs ist.
    if (timer_tick_hz(config_.htim) != 1000000) {
        return false;
    }
    const uint32_t period_us = 1000000u / config_.frequency_hz;
    if (config_.max_us >= period_us) {
        return false;
    }
    __HAL_TIM_SET_AUTORELOAD(config_.htim, period_us - 1);
    set_pulse_us(config_.center_us);
    if (HAL_TIM_PWM_Start(config_.htim, config_.channel) != HAL_OK) {
        return false;
    }
    running_ = true;
    return true;
}

void Servo::set_pulse_us(uint16_t pulse_us) {
    if (pulse_us < config_.min_us) pulse_us = config_.min_us;
    if (pulse_us > config_.max_us) pulse_us = config_.max_us;
    pulse_us_ = pulse_us;
    __HAL_TIM_SET_COMPARE(config_.htim, config_.channel, pulse_us);
}

void Servo::stop() {
    HAL_TIM_PWM_Stop(config_.htim, config_.channel);
    running_ = false;
}

}  // namespace drivers
