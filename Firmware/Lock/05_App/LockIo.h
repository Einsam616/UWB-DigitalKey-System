#ifndef LOCK_IO_H
#define LOCK_IO_H

#include <stdint.h>

typedef enum
{
    LOCK_LED_LOCKED = 0,
    LOCK_LED_UNLOCKED,
    LOCK_LED_KEY_DETECTED,
    LOCK_LED_WELCOME,
    LOCK_LED_ID_VALID,
    LOCK_LED_COUNT
} LockIoLed;

#define LOCK_LED_BIT(led) ((uint8_t)(1u << (uint8_t)(led)))

/**
 * @brief Initialize PA0-PA4 status LEDs and the lock-side buzzer.
 * @note PB8 uses TIM4_CH3 at 2 kHz and 50 percent duty when enabled.
 */
void LockIo_Init(void);

/** @brief Set one PA0-PA4 status LED. */
void LockIo_SetLed(LockIoLed led, uint8_t enabled);

/** @brief Set all five LEDs from LOCK_LED_BIT values in one update. */
void LockIo_SetLedMask(uint8_t mask);

/**
 * @brief Turn the TIM4_CH3 buzzer PWM on or off.
 * @param enabled Non-zero enables the 2 kHz PB8 waveform.
 */
void LockIo_SetBuzzer(uint8_t enabled);

/**
 * @brief Drive a short non-blocking buzzer pulse.
 * @param tick_ms Current application tick in milliseconds.
 * @param duration_ms Pulse duration.
 */
void LockIo_BeepUntil(uint32_t tick_ms, uint16_t duration_ms);

/**
 * @brief 启动非阻塞蜂鸣序列。
 * @param tick_ms 当前毫秒节拍。
 * @param count 蜂鸣次数，零表示直接停止。
 * @param on_ms 每次鸣叫时间。
 * @param off_ms 相邻两次鸣叫的间隔。
 */
void LockIo_BeepPattern(uint32_t tick_ms, uint8_t count,
                        uint16_t on_ms, uint16_t off_ms);

/** @brief 立即取消当前蜂鸣或蜂鸣序列。 */
void LockIo_CancelBuzzer(void);

/**
 * @brief Service the timed buzzer pulse.
 * @param tick_ms Current application tick in milliseconds.
 */
void LockIo_Service(uint32_t tick_ms);

#endif
