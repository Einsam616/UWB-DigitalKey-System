#ifndef LOCK_APP_H
#define LOCK_APP_H

/**
 * @brief Initialize the lock application and all board-side peripherals.
 */
void App_Init(void);

/**
 * @brief Run one cooperative application step.
 * @note The step polls CC1101, updates the lock state, refreshes the display,
 *       services the buzzer, and delays approximately 1 ms.
 */
void App_Run(void);

#endif
