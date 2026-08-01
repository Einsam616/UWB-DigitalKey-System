#include "PasswordStore.h"

#include "stm32f10x_flash.h"

#define PASSWORD_STORE_PAGE_ADDRESS 0x0800FC00u
#define PASSWORD_STORE_MAGIC        0x31535750u
#define PASSWORD_STORE_CHECK_XOR    0xA55AA55Au

/** @brief 将四个密码数字封装到一个 32 位字中。 */
static uint32_t PasswordStore_Pack(const uint8_t password[PASSWORD_STORE_LENGTH])
{
    return (uint32_t)password[0] |
           ((uint32_t)password[1] << 8u) |
           ((uint32_t)password[2] << 16u) |
           ((uint32_t)password[3] << 24u);
}

/** @brief 检查四个密码字节是否都处于 0 到 9。 */
static uint8_t PasswordStore_IsValid(const uint8_t password[PASSWORD_STORE_LENGTH])
{
    uint8_t index;

    for (index = 0u; index < PASSWORD_STORE_LENGTH; ++index)
    {
        if (password[index] > 9u) return 0u;
    }
    return 1u;
}

/** @brief 从打包字恢复四个密码数字。 */
static void PasswordStore_Unpack(uint32_t packed,
                                 uint8_t password[PASSWORD_STORE_LENGTH])
{
    password[0] = (uint8_t)packed;
    password[1] = (uint8_t)(packed >> 8u);
    password[2] = (uint8_t)(packed >> 16u);
    password[3] = (uint8_t)(packed >> 24u);
}

/** @brief 从内部 Flash 读取密码，记录无效时返回默认值 1234。 */
uint8_t PasswordStore_Load(uint8_t password[PASSWORD_STORE_LENGTH])
{
    const volatile uint32_t *record =
        (const volatile uint32_t *)PASSWORD_STORE_PAGE_ADDRESS;
    uint32_t packed = record[1];

    PasswordStore_Unpack(packed, password);
    if (record[0] == PASSWORD_STORE_MAGIC &&
        record[2] == (packed ^ PASSWORD_STORE_CHECK_XOR) &&
        PasswordStore_IsValid(password) != 0u)
    {
        return 1u;
    }

    password[0] = 1u;
    password[1] = 2u;
    password[2] = 3u;
    password[3] = 4u;
    return 0u;
}

/** @brief 擦写内部 Flash 最后一页并回读校验密码记录。 */
uint8_t PasswordStore_Save(const uint8_t password[PASSWORD_STORE_LENGTH])
{
    const volatile uint32_t *record =
        (const volatile uint32_t *)PASSWORD_STORE_PAGE_ADDRESS;
    uint32_t packed;
    FLASH_Status status;

    if (PasswordStore_IsValid(password) == 0u) return 0u;
    packed = PasswordStore_Pack(password);

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    status = FLASH_ErasePage(PASSWORD_STORE_PAGE_ADDRESS);
    if (status == FLASH_COMPLETE)
        status = FLASH_ProgramWord(PASSWORD_STORE_PAGE_ADDRESS,
                                   PASSWORD_STORE_MAGIC);
    if (status == FLASH_COMPLETE)
        status = FLASH_ProgramWord(PASSWORD_STORE_PAGE_ADDRESS + 4u, packed);
    if (status == FLASH_COMPLETE)
        status = FLASH_ProgramWord(PASSWORD_STORE_PAGE_ADDRESS + 8u,
                                   packed ^ PASSWORD_STORE_CHECK_XOR);
    FLASH_Lock();

    if (status != FLASH_COMPLETE) return 0u;
    return (record[0] == PASSWORD_STORE_MAGIC &&
            record[1] == packed &&
            record[2] == (packed ^ PASSWORD_STORE_CHECK_XOR)) ? 1u : 0u;
}
