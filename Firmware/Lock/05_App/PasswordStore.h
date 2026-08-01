#ifndef PASSWORD_STORE_H
#define PASSWORD_STORE_H

#include <stdint.h>

#define PASSWORD_STORE_LENGTH 4u

/**
 * @brief 从内部 Flash 读取四位密码。
 * @param password 输出四个 0 到 9 的数字。
 * @return 记录有效返回 1，否则写入默认密码 1234 并返回 0。
 */
uint8_t PasswordStore_Load(uint8_t password[PASSWORD_STORE_LENGTH]);

/**
 * @brief 擦写内部 Flash 最后一页并保存四位密码。
 * @param password 四个 0 到 9 的数字。
 * @return 写入并回读成功返回 1，失败返回 0。
 */
uint8_t PasswordStore_Save(const uint8_t password[PASSWORD_STORE_LENGTH]);

#endif
