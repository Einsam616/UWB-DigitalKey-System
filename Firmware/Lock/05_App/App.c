#include "App.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "Cc1101.h"
#include "ChineseFont.h"
#include "CompetitionLogo.h"
#include "DebugSerial.h"
#include "DipSwitch.h"
#include "LockIo.h"
#include "PasswordStore.h"
#include "Screen.h"
#include "LockPacket.h"
#include "LockState.h"
#include "TM1637Keypad.h"
#include "delay.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#define LOCK_DISPLAY_MIN_PERIOD_MS 100u
#define LOCK_PACKET_TIMEOUT_MS    3000u
#define LOCK_PASSWORD_TIMEOUT_MS 5000u
#define LOCK_PASSWORD_FEEDBACK_MS 1500u
#define LOCK_MANUAL_UNLOCK_MS     3000u
#define LOCK_PASSWORD_CURSOR_MS   500u
#define LOCK_KEY_SCAN_PERIOD_MS   10u
#define LOCK_KEY_BEEP_MS          35u
#define LOCK_TM1637_WAKE_MS       5000u
#define LOCK_ID_APPLIED_DISPLAY_MS 5000u
#define LOCK_SCREEN_BOOT_TEST     0
#define LOCK_RF_PROBE_PIN         GPIO_Pin_13
#define LOCK_RF_PROBE_PULSE_MS    30u

static uint32_t s_tick_ms;
static uint32_t s_last_packet_ms;
static uint32_t s_next_display_ms;
static uint16_t s_radial_distance_mm;
static int16_t s_angle_deg10;
static uint8_t s_rx_id;
static uint8_t s_pending_id;
static uint8_t s_configured_id;
static uint8_t s_id_pending;
static uint8_t s_id_applied_visible;
static uint32_t s_id_applied_deadline;
static uint8_t s_quality;
static uint8_t s_link_ok;
static uint8_t s_rf_ready;
static uint8_t s_have_position;
static uint8_t s_position_stale;
static uint8_t s_diagnostic;
static uint8_t s_display_dirty;
static uint8_t s_have_sequence;
static uint16_t s_last_sequence;
static uint16_t s_lost_packets;
static LockState s_state;
static const char *s_last_event;
static const char *s_previous_event;
static uint32_t s_rf_probe_deadline;
static uint8_t s_rf_probe_active;
static uint8_t s_indicator_mask;
static uint8_t s_indicator_valid;
static uint8_t s_manual_unlock_active;
static uint32_t s_manual_unlock_deadline;

typedef enum
{
    APP_PASSWORD_IDLE = 0,
    APP_PASSWORD_MANUAL,
    APP_PASSWORD_VERIFY_OLD,
    APP_PASSWORD_ENTER_NEW,
    APP_PASSWORD_CONFIRM_NEW,
    APP_PASSWORD_FEEDBACK
} AppPasswordStage;

static AppPasswordStage s_password_stage;
static AppPasswordStage s_password_feedback_next;
static uint8_t s_password[PASSWORD_STORE_LENGTH];
static uint8_t s_password_input[PASSWORD_STORE_LENGTH];
static uint8_t s_password_new[PASSWORD_STORE_LENGTH];
static uint8_t s_password_input_length;
static uint32_t s_password_last_input_ms;
static uint32_t s_password_feedback_deadline;
static uint32_t s_password_cursor_ms;
static uint8_t s_password_cursor_on;
static uint32_t s_next_key_scan_ms;
static uint8_t s_tm1637_awake;
static uint32_t s_tm1637_sleep_deadline;

typedef struct
{
    uint8_t valid;
    uint8_t unlocked;
    char rx_id[5];
    char configured_id[6];
    const char *auth_text;
    uint16_t auth_color;
    char distance[8];
    uint16_t distance_color;
    char angle[8];
    uint16_t angle_color;
    const char *zone_text;
    uint16_t zone_background;
    uint8_t sensing_entered;
    uint8_t welcome_entered;
    uint8_t unlock_entered;
    const char *link_text;
    const char *last_event;
    const char *previous_event;
    const char *id_status;
    uint16_t id_status_color;
} AppDisplayCache;

static AppDisplayCache s_display_cache;

static void App_ProcessRadio(void);
static void App_ServicePassword(void);
static void App_ServiceManualUnlock(void);
static void App_UpdatePasswordDisplay(void);

/** @brief 唤醒数码管并把无操作熄屏计时延长 5 秒。 */
static void App_WakeTm1637(void)
{
    s_tm1637_awake = 1u;
    s_tm1637_sleep_deadline = s_tick_ms + LOCK_TM1637_WAKE_MS;
    TM1637Keypad_SetDisplayEnabled(1u);
    App_UpdatePasswordDisplay();
}

/** @brief 关闭数码管显示，保留密码状态供下一次按键唤醒。 */
static void App_SleepTm1637(void)
{
    s_tm1637_awake = 0u;
    TM1637Keypad_SetDisplayEnabled(0u);
}

/** @brief 返回无线状态或手动操作共同决定的实际开锁状态。 */
static uint8_t App_IsUnlocked(void)
{
    return (s_manual_unlock_active != 0u ||
            (s_state == LOCK_STATE_UNLOCKED && s_position_stale == 0u)) ? 1u : 0u;
}

/* PC13 is an active-low diagnostic probe during radio bring-up. */
static void App_RfProbeInit(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    gpio.GPIO_Pin = LOCK_RF_PROBE_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);
    GPIO_SetBits(GPIOC, LOCK_RF_PROBE_PIN);
    s_rf_probe_active = 0u;
    s_rf_probe_deadline = 0u;
}

static void App_RfProbePulse(uint32_t tick_ms)
{
    GPIO_ResetBits(GPIOC, LOCK_RF_PROBE_PIN);
    s_rf_probe_active = 1u;
    s_rf_probe_deadline = tick_ms + LOCK_RF_PROBE_PULSE_MS;
}

static void App_RfProbeService(uint32_t tick_ms)
{
    if (s_rf_probe_active != 0u &&
        (int32_t)(tick_ms - s_rf_probe_deadline) >= 0)
    {
        s_rf_probe_active = 0u;
        GPIO_SetBits(GPIOC, LOCK_RF_PROBE_PIN);
    }
}

/**
 * @brief 按红、绿、蓝、黄、黄的实物顺序更新 D1-D5。
 * @note D1 闭锁/错误，D2 开锁，D3 检测到钥匙，D4 迎宾区，D5 身份通过。
 */
static void App_UpdateIndicators(void)
{
    uint8_t mask = 0u;

    if (s_manual_unlock_active != 0u)
    {
        /* 手动开锁没有依赖位置帧，D2 绿灯在整个 3 秒内常亮。 */
        mask = LOCK_LED_BIT(LOCK_LED_UNLOCKED);
    }
    else if (s_state == LOCK_STATE_UNLOCKED)
    {
        mask |= LOCK_LED_BIT(LOCK_LED_UNLOCKED);
    }
    else
    {
        mask |= LOCK_LED_BIT(LOCK_LED_LOCKED);
    }
    if (s_manual_unlock_active == 0u)
    {
        if (s_have_position != 0u && s_position_stale == 0u)
        {
            mask |= LOCK_LED_BIT(LOCK_LED_KEY_DETECTED);
        }
        if (s_state == LOCK_STATE_WELCOME &&
            ((s_tick_ms / 500u) & 1u) == 0u)
        {
            mask |= LOCK_LED_BIT(LOCK_LED_WELCOME);
        }
        if (s_link_ok != 0u && s_have_position != 0u &&
            s_rx_id == s_configured_id)
            mask |= LOCK_LED_BIT(LOCK_LED_ID_VALID);
    }
    if (s_indicator_valid == 0u || s_indicator_mask != mask)
    {
        s_indicator_mask = mask;
        s_indicator_valid = 1u;
        LockIo_SetLedMask(mask);
    }
}

/** @brief Return whether a millisecond deadline has been reached. */
static uint8_t App_TimeReached(uint32_t now, uint32_t deadline)
{
    return ((int32_t)(now - deadline) >= 0) ? 1u : 0u;
}

/** @brief Convert the four-bit setting to the user-facing MSB-first text form. */
static void App_FormatId(uint8_t id, char text[5])
{
    text[0] = (char)('0' + ((id >> 3u) & 1u));
    text[1] = (char)('0' + ((id >> 2u) & 1u));
    text[2] = (char)('0' + ((id >> 1u) & 1u));
    text[3] = (char)('0' + (id & 1u));
    text[4] = '\0';
}

/** @brief Track forward sequence gaps and report whether the loss count changed. */
static uint8_t App_TrackSequence(uint16_t sequence)
{
    uint16_t delta;
    uint16_t missed;
    uint16_t old_lost;
    uint32_t total;

    if (s_have_sequence == 0u)
    {
        s_last_sequence = sequence;
        s_have_sequence = 1u;
        return 0u;
    }
    delta = (uint16_t)(sequence - s_last_sequence);
    if (delta == 0u || delta >= 0x8000u) return 0u;
    missed = (uint16_t)(delta - 1u);
    old_lost = s_lost_packets;
    total = (uint32_t)s_lost_packets + missed;
    s_lost_packets = (total > 999u) ? 999u : (uint16_t)total;
    s_last_sequence = sequence;
    return (s_lost_packets != old_lost) ? 1u : 0u;
}

/** @brief Draw a padded text field so shorter updates erase old characters. */
static void App_ShowField(uint16_t x,
                          uint16_t y,
                          uint8_t width_chars,
                          uint8_t size,
                          const char *text,
                          uint16_t color,
                          uint16_t background)
{
    char padded[33];
    uint8_t index = 0u;
    if (width_chars > 32u) width_chars = 32u;
    while (text != 0 && text[index] != '\0' && index < width_chars)
    {
        padded[index] = text[index];
        ++index;
    }
    while (index < width_chars) padded[index++] = ' ';
    padded[index] = '\0';
    Screen_ShowString(x, y, padded, color, background, size);
}

/** @brief 返回 16 点阵中英文混排文本的显示宽度。 */
static uint16_t App_TextWidth16(const char *text)
{
    const uint8_t *bytes = (const uint8_t *)text;
    uint16_t width = 0u;

    while (bytes != 0 && *bytes != 0u)
    {
        if (*bytes < 0x80u)
        {
            width = (uint16_t)(width + 6u);
            bytes++;
        }
        else if ((*bytes & 0xE0u) == 0xC0u && bytes[1] != 0u)
        {
            width = (uint16_t)(width + 16u);
            bytes += 2;
        }
        else if ((*bytes & 0xF0u) == 0xE0u &&
                 bytes[1] != 0u && bytes[2] != 0u)
        {
            width = (uint16_t)(width + 16u);
            bytes += 3;
        }
        else
        {
            width = (uint16_t)(width + 6u);
            bytes++;
        }
    }
    return width;
}

/** @brief 保存已经绘制的短字符串，供下一帧比较。 */
static void App_CopyText(char *destination, uint8_t capacity,
                         const char *source)
{
    uint8_t index = 0u;

    if (capacity == 0u) return;
    while (source != 0 && source[index] != '\0' &&
           index < (uint8_t)(capacity - 1u))
    {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

/** @brief 原位覆盖一个固定宽度中文字段，不先清空整块区域。 */
static void App_ShowChineseField(uint16_t x,
                                 uint16_t y,
                                 uint16_t width,
                                 const char *text,
                                 uint16_t color,
                                 uint16_t background)
{
    uint16_t text_width = App_TextWidth16(text);

    ChineseFont_ShowText(x, y, text, color, background, 1u);
    if (text_width < width)
    {
        Screen_FillRect((uint16_t)(x + text_width), (uint16_t)(y + 1u),
                        (uint16_t)(width - text_width), 16u, background);
    }
    Screen_FillRect(x, y, width, 1u, background);
}

/** @brief Draw the static 320x240 lock dashboard. */
static void App_DrawLayout(void)
{
    Screen_Clear(SCREEN_BLACK);
    Screen_FillRect(0u, 0u, SCREEN_WIDTH, 40u, SCREEN_NAVY);
    CompetitionLogo_Draw(4u, 4u);
    ChineseFont_ShowText(58u, 1u, ZH_TITLE_PREFIX,
                         SCREEN_WHITE, SCREEN_NAVY, 1u);
    ChineseFont_ShowText(58u, 20u, ZH_TITLE_SYSTEM,
                         SCREEN_WHITE, SCREEN_NAVY, 1u);
    Screen_DrawRect(4u, 43u, 312u, 42u, SCREEN_DGRAY);
    Screen_DrawRect(4u, 89u, 312u, 28u, SCREEN_DGRAY);
    Screen_DrawRect(4u, 121u, 312u, 47u, SCREEN_DGRAY);
    Screen_DrawLine(0u, 172u, 319u, 172u, SCREEN_DGRAY);
    Screen_DrawLine(0u, 197u, 319u, 197u, SCREEN_DGRAY);

    ChineseFont_ShowText(8u, 47u, ZH_LABEL_RX_ID,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(160u, 47u, ZH_LABEL_SET_ID,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(8u, 67u, ZH_LABEL_AUTH,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(8u, 95u, ZH_LABEL_DISTANCE,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(168u, 95u, ZH_LABEL_BEARING,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    Screen_ShowString16(152u, 95u, "cm", SCREEN_LGRAY, SCREEN_BLACK, 1u);
    Screen_ShowString16(294u, 95u, "deg", SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(8u, 126u, ZH_LABEL_CURRENT_ZONE,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(174u, 126u, ZH_LABEL_LOCK_STATE,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(8u, 146u, ZH_LABEL_ZONE_ENTRY,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(76u, 146u, ZH_ZONE_NAME_SENSING,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(144u, 146u, ZH_ZONE_NAME_WELCOME,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(212u, 146u, ZH_ZONE_NAME_UNLOCK,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(8u, 176u, ZH_LABEL_RF,
                         SCREEN_CYAN, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(8u, 202u, ZH_LABEL_LAST_EVENT,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    Screen_ShowString16(80u, 202u, "1:", SCREEN_LGRAY, SCREEN_BLACK, 1u);
    Screen_ShowString16(80u, 221u, "2:", SCREEN_LGRAY, SCREEN_BLACK, 1u);
}

/** @brief Return the current identity-verification text and color. */
static const char *App_AuthText(uint16_t *color)
{
    if (s_manual_unlock_active != 0u)
    {
        *color = SCREEN_GREEN;
        return ZH_STATUS_PASS;
    }
    if (s_diagnostic != 0u)
    {
        *color = SCREEN_CYAN;
        return ZH_STATUS_TEST;
    }
    if (s_have_position == 0u)
    {
        *color = SCREEN_YELLOW;
        return ZH_STATUS_WAIT;
    }
    if (s_position_stale != 0u)
    {
        *color = SCREEN_YELLOW;
        return ZH_STATUS_WAIT;
    }
    if (s_rx_id == s_configured_id)
    {
        *color = SCREEN_GREEN;
        return ZH_STATUS_PASS;
    }
    *color = SCREEN_RED;
    return ZH_STATUS_ERROR;
}

/** @brief Return the current zone label and its background color. */
static const char *App_ZoneText(uint16_t *background)
{
    if (s_manual_unlock_active != 0u)
    {
        *background = SCREEN_DGREEN;
        return ZH_ZONE_UNLOCK;
    }
    if (s_diagnostic != 0u)
    {
        *background = SCREEN_DCYAN;
        return ZH_ZONE_RADIO_TEST;
    }
    if (s_have_position == 0u || s_position_stale != 0u)
    {
        *background = SCREEN_DGRAY;
        return ZH_ZONE_WAITING;
    }
    if (s_state == LOCK_STATE_UNLOCKED)
    {
        *background = SCREEN_DGREEN;
        return ZH_ZONE_UNLOCK;
    }
    if (s_state == LOCK_STATE_WELCOME)
    {
        *background = SCREEN_OLIVE;
        return ZH_ZONE_WELCOME;
    }
    if (s_state == LOCK_STATE_SENSING)
    {
        *background = SCREEN_DCYAN;
        return ZH_ZONE_SENSING;
    }
    if (s_state == LOCK_STATE_OUTSIDE)
    {
        *background = SCREEN_PURPLE;
        return ZH_ZONE_OUTSIDE;
    }
    if (s_state == LOCK_STATE_INVALID)
    {
        *background = SCREEN_MAROON;
        return ZH_ZONE_BAD_ID;
    }
    *background = SCREEN_DGRAY;
    return ZH_ZONE_OUT_OF_RANGE;
}

/** @brief 原位更新页眉中的开闭锁徽标。 */
static void App_ShowLockBadge(uint8_t unlocked)
{
    uint16_t background = (unlocked != 0u) ? SCREEN_DGREEN : SCREEN_MAROON;
    const char *text = (unlocked != 0u) ? ZH_LOCK_OPEN : ZH_LOCK_CLOSED;

    ChineseFont_ShowText(256u, 11u, text, SCREEN_WHITE, background, 1u);
    Screen_FillRect(244u, 8u, 72u, 4u, background);
    Screen_FillRect(244u, 28u, 72u, 4u, background);
    Screen_FillRect(244u, 12u, 12u, 16u, background);
    Screen_FillRect(304u, 12u, 12u, 16u, background);
}

/** @brief 原位更新带背景色的当前位置字段。 */
static void App_ShowZoneField(const char *text, uint16_t background)
{
    uint16_t text_width = App_TextWidth16(text);

    ChineseFont_ShowText(88u, 126u, text, SCREEN_WHITE, background, 1u);
    Screen_FillRect(82u, 125u, 78u, 2u, background);
    Screen_FillRect(82u, 127u, 6u, 16u, background);
    if (text_width < 72u)
    {
        Screen_FillRect((uint16_t)(88u + text_width), 127u,
                        (uint16_t)(72u - text_width), 16u, background);
    }
}

/** @brief 原位更新一个三区进入状态，绿色为“是”，深灰为“否”。 */
static void App_ShowZoneFlag(uint16_t x, uint8_t entered)
{
    uint16_t background = (entered != 0u) ? SCREEN_DGREEN : SCREEN_DGRAY;

    ChineseFont_ShowText((uint16_t)(x + 36u), 146u,
                         (entered != 0u) ? ZH_YES : ZH_NO,
                         SCREEN_WHITE, background, 1u);
    Screen_FillRect((uint16_t)(x + 34u), 145u, 20u, 2u, background);
    Screen_FillRect((uint16_t)(x + 34u), 147u, 2u, 16u, background);
    Screen_FillRect((uint16_t)(x + 52u), 147u, 2u, 16u, background);
}

/** @brief 只更新内容发生变化的动态字段。 */
static void App_ShowDisplay(void)
{
    char configured_id[6];
    char rx_id[5];
    char distance[8];
    char angle[8];
    uint16_t field_color;
    uint16_t distance_color;
    uint16_t angle_color;
    uint16_t zone_background;
    uint16_t id_status_color;
    uint16_t cm;
    uint16_t mm_digit;
    int32_t angle_abs;
    char angle_sign;
    const char *zone_text;
    const char *link_text;
    const char *auth_text;
    const char *id_status;
    const char *last_event;
    const char *previous_event;
    uint8_t unlocked;
    uint8_t sensing_entered;
    uint8_t welcome_entered;
    uint8_t unlock_entered;
    uint8_t force;

    s_display_dirty = 0u;
    force = (s_display_cache.valid == 0u) ? 1u : 0u;

    App_FormatId(s_pending_id, configured_id);
    if (s_have_position != 0u)
    {
        App_FormatId(s_rx_id, rx_id);
    }
    else
    {
        rx_id[0] = '-';
        rx_id[1] = '-';
        rx_id[2] = '-';
        rx_id[3] = '-';
        rx_id[4] = '\0';
    }
    if (s_id_pending != 0u)
    {
        configured_id[4] = '*';
        configured_id[5] = '\0';
    }
    auth_text = App_AuthText(&field_color);

    if (s_have_position != 0u)
    {
        cm = (uint16_t)(s_radial_distance_mm / 10u);
        mm_digit = (uint16_t)(s_radial_distance_mm % 10u);
        (void)sprintf(distance, "%04u.%u", (unsigned int)cm,
                      (unsigned int)mm_digit);
        distance_color = (s_position_stale == 0u) ? SCREEN_WHITE : SCREEN_LGRAY;

        angle_abs = (s_angle_deg10 < 0) ? -(int32_t)s_angle_deg10 : (int32_t)s_angle_deg10;
        angle_sign = (s_angle_deg10 < 0) ? '-' : '+';
        (void)sprintf(angle, "%c%03lu.%lu", angle_sign,
                      (unsigned long)(angle_abs / 10),
                      (unsigned long)(angle_abs % 10));
        angle_color = (s_position_stale == 0u) ? SCREEN_WHITE : SCREEN_LGRAY;
    }
    else
    {
        App_CopyText(distance, (uint8_t)sizeof(distance), "----.-");
        App_CopyText(angle, (uint8_t)sizeof(angle), "+---.-");
        distance_color = SCREEN_LGRAY;
        angle_color = SCREEN_LGRAY;
    }

    zone_text = App_ZoneText(&zone_background);
    sensing_entered = (s_manual_unlock_active == 0u &&
                      s_state == LOCK_STATE_SENSING) ? 1u : 0u;
    welcome_entered = (s_manual_unlock_active == 0u &&
                       s_state == LOCK_STATE_WELCOME) ? 1u : 0u;
    unlock_entered = App_IsUnlocked();

    if (s_rf_ready == 0u) link_text = ZH_RF_FAULT;
    else if (s_link_ok == 0u) link_text = ZH_STATUS_WAIT;
    else link_text = ZH_RF_NORMAL;
    if (s_id_pending != 0u)
    {
        id_status = ZH_ID_PENDING;
        id_status_color = SCREEN_YELLOW;
    }
    else if (s_id_applied_visible != 0u)
    {
        id_status = ZH_ID_APPLIED;
        id_status_color = SCREEN_GREEN;
    }
    else
    {
        id_status = 0;
        id_status_color = SCREEN_BLACK;
    }
    unlocked = App_IsUnlocked();
    last_event = s_last_event;
    previous_event = s_previous_event;

    if (force != 0u || s_display_cache.unlocked != unlocked)
    {
        App_ShowLockBadge(unlocked);
        ChineseFont_ShowText(218u, 126u,
                             (unlocked != 0u) ? ZH_LOCK_OPEN : ZH_LOCK_CLOSED,
                             (unlocked != 0u) ? SCREEN_GREEN : SCREEN_RED,
                             SCREEN_BLACK, 1u);
        s_display_cache.unlocked = unlocked;
        App_ProcessRadio();
    }
    if (force != 0u || strcmp(s_display_cache.rx_id, rx_id) != 0)
    {
        App_ShowField(64u, 46u, 4u, 2u, rx_id, SCREEN_WHITE, SCREEN_BLACK);
        App_CopyText(s_display_cache.rx_id,
                     (uint8_t)sizeof(s_display_cache.rx_id), rx_id);
        App_ProcessRadio();
    }
    if (force != 0u || strcmp(s_display_cache.configured_id, configured_id) != 0)
    {
        App_ShowField(216u, 46u, 5u, 2u, configured_id,
                      SCREEN_WHITE, SCREEN_BLACK);
        App_CopyText(s_display_cache.configured_id,
                     (uint8_t)sizeof(s_display_cache.configured_id), configured_id);
        App_ProcessRadio();
    }
    if (force != 0u || s_display_cache.auth_text != auth_text ||
        s_display_cache.auth_color != field_color)
    {
        App_ShowChineseField(82u, 67u, 48u, auth_text,
                             field_color, SCREEN_BLACK);
        s_display_cache.auth_text = auth_text;
        s_display_cache.auth_color = field_color;
        App_ProcessRadio();
    }
    if (force != 0u || s_display_cache.id_status != id_status ||
        s_display_cache.id_status_color != id_status_color)
    {
        App_ShowChineseField(144u, 67u, 168u, id_status,
                             id_status_color, SCREEN_BLACK);
        s_display_cache.id_status = id_status;
        s_display_cache.id_status_color = id_status_color;
        App_ProcessRadio();
    }
    if (force != 0u || strcmp(s_display_cache.distance, distance) != 0 ||
        s_display_cache.distance_color != distance_color)
    {
        App_ShowField(80u, 94u, 6u, 2u, distance,
                      distance_color, SCREEN_BLACK);
        App_CopyText(s_display_cache.distance,
                     (uint8_t)sizeof(s_display_cache.distance), distance);
        s_display_cache.distance_color = distance_color;
        App_ProcessRadio();
    }
    if (force != 0u || strcmp(s_display_cache.angle, angle) != 0 ||
        s_display_cache.angle_color != angle_color)
    {
        App_ShowField(222u, 94u, 6u, 2u, angle,
                      angle_color, SCREEN_BLACK);
        App_CopyText(s_display_cache.angle,
                     (uint8_t)sizeof(s_display_cache.angle), angle);
        s_display_cache.angle_color = angle_color;
        App_ProcessRadio();
    }
    if (force != 0u || s_display_cache.zone_text != zone_text ||
        s_display_cache.zone_background != zone_background)
    {
        App_ShowZoneField(zone_text, zone_background);
        s_display_cache.zone_text = zone_text;
        s_display_cache.zone_background = zone_background;
        App_ProcessRadio();
    }
    if (force != 0u || s_display_cache.sensing_entered != sensing_entered)
    {
        App_ShowZoneFlag(76u, sensing_entered);
        s_display_cache.sensing_entered = sensing_entered;
        App_ProcessRadio();
    }
    if (force != 0u || s_display_cache.welcome_entered != welcome_entered)
    {
        App_ShowZoneFlag(144u, welcome_entered);
        s_display_cache.welcome_entered = welcome_entered;
        App_ProcessRadio();
    }
    if (force != 0u || s_display_cache.unlock_entered != unlock_entered)
    {
        App_ShowZoneFlag(212u, unlock_entered);
        s_display_cache.unlock_entered = unlock_entered;
        App_ProcessRadio();
    }
    if (force != 0u || s_display_cache.link_text != link_text)
    {
        App_ShowChineseField(50u, 176u, 120u, link_text,
                             SCREEN_CYAN, SCREEN_BLACK);
        s_display_cache.link_text = link_text;
        App_ProcessRadio();
    }
    if (force != 0u || s_display_cache.last_event != last_event)
    {
        App_ShowChineseField(96u, 202u, 216u, last_event,
                             SCREEN_WHITE, SCREEN_BLACK);
        s_display_cache.last_event = last_event;
        App_ProcessRadio();
    }
    if (force != 0u || s_display_cache.previous_event != previous_event)
    {
        App_ShowChineseField(96u, 221u, 216u, previous_event,
                             SCREEN_LGRAY, SCREEN_BLACK);
        s_display_cache.previous_event = previous_event;
        App_ProcessRadio();
    }
    s_display_cache.valid = 1u;
}

/** @brief Push one message into the two-line recent-event history. */
static void App_RecordEvent(const char *event)
{
    if (event == 0 || *event == '\0') return;
    if (s_last_event != 0 && strcmp(s_last_event, event) == 0) return;

    s_previous_event = s_last_event;
    s_last_event = event;
    s_display_dirty = 1u;
    s_next_display_ms = s_tick_ms;
}

/** @brief Update the retained event message after a state transition. */
static void App_RecordStateEvent(LockState old_state, LockState new_state)
{
    if (new_state == old_state) return;
    if (s_manual_unlock_active != 0u) return;
    if (new_state == LOCK_STATE_UNLOCKED) App_RecordEvent(ZH_EVENT_UNLOCK);
    else if (new_state == LOCK_STATE_WELCOME) App_RecordEvent(ZH_EVENT_ENTER_WELCOME);
    else if (new_state == LOCK_STATE_INVALID) App_RecordEvent(ZH_EVENT_BAD_ID);
    else if (new_state == LOCK_STATE_OUTSIDE) App_RecordEvent(ZH_EVENT_OUTSIDE);
    else if (old_state == LOCK_STATE_UNLOCKED) App_RecordEvent(ZH_EVENT_AUTO_LOCK);
    else if (new_state == LOCK_STATE_SENSING) App_RecordEvent(ZH_EVENT_ENTER_SENSING);
    else if (new_state == LOCK_STATE_LOCKED) App_RecordEvent(ZH_EVENT_LEAVE_SENSING);
}

/** @brief 清空当前输入缓存并把光标放到下一次输入位置。 */
static void App_PasswordClearInput(void)
{
    (void)memset(s_password_input, 0, sizeof(s_password_input));
    s_password_input_length = 0u;
    s_password_cursor_on = 1u;
    s_password_cursor_ms = s_tick_ms;
}

/** @brief 只在 TM1637 上显示当前密码阶段、已输入数字和下一位光标。 */
static void App_UpdatePasswordDisplay(void)
{
    if (s_password_stage == APP_PASSWORD_IDLE ||
        s_password_stage == APP_PASSWORD_MANUAL)
    {
        TM1637Keypad_ShowInputDigits(s_password_input,
                                     (s_password_stage == APP_PASSWORD_MANUAL) ?
                                     s_password_input_length : 0u,
                                     s_password_cursor_on);
    }
    else if (s_password_input_length != 0u)
    {
        TM1637Keypad_ShowInputDigits(s_password_input,
                                     s_password_input_length,
                                     s_password_cursor_on);
    }
    else if (s_password_stage == APP_PASSWORD_VERIFY_OLD)
    {
        TM1637Keypad_ShowPrompt('O');
    }
    else if (s_password_stage == APP_PASSWORD_ENTER_NEW)
    {
        TM1637Keypad_ShowPrompt('N');
    }
    else if (s_password_stage == APP_PASSWORD_CONFIRM_NEW)
    {
        TM1637Keypad_ShowPrompt('R');
    }
    else
    {
        TM1637Keypad_ClearDisplay();
    }
}

/** @brief 切换密码输入阶段，并在数码管上显示对应提示。 */
static void App_PasswordEnterStage(AppPasswordStage stage)
{
    s_password_stage = stage;
    App_PasswordClearInput();
    s_password_last_input_ms = s_tick_ms;
    App_UpdatePasswordDisplay();
}

/** @brief 检查四位输入是否与目标密码完全一致。 */
static uint8_t App_PasswordMatches(const uint8_t target[PASSWORD_STORE_LENGTH])
{
    uint8_t index;

    if (s_password_input_length != PASSWORD_STORE_LENGTH) return 0u;
    for (index = 0u; index < PASSWORD_STORE_LENGTH; ++index)
    {
        if (s_password_input[index] != target[index]) return 0u;
    }
    return 1u;
}

/** @brief 显示 P/F 反馈并安排下一阶段，不阻塞无线接收。 */
static void App_PasswordFeedback(AppPasswordStage next_stage,
                                 char status,
                                 uint8_t beep_count)
{
    App_WakeTm1637();
    s_password_feedback_next = next_stage;
    s_password_stage = APP_PASSWORD_FEEDBACK;
    s_password_feedback_deadline = s_tick_ms + LOCK_PASSWORD_FEEDBACK_MS;
    s_password_cursor_on = 0u;
    TM1637Keypad_ShowStatus(status);
    if (status == 'F') App_RecordEvent(ZH_EVENT_PASSWORD_ERROR);
    if (beep_count == 1u)
        LockIo_BeepUntil(s_tick_ms, 200u);
    else
        LockIo_BeepPattern(s_tick_ms, beep_count, 80u, 80u);
}

/** @brief 启动三秒手动开锁，主循环仍继续接收无线数据。 */
static void App_StartManualUnlock(void)
{
    App_WakeTm1637();
    s_manual_unlock_active = 1u;
    s_manual_unlock_deadline = s_tick_ms + LOCK_MANUAL_UNLOCK_MS;
    s_password_stage = APP_PASSWORD_IDLE;
    App_PasswordClearInput();
    App_RecordEvent(ZH_EVENT_MANUAL_UNLOCK);
    TM1637Keypad_ShowStatus('P');
    LockIo_BeepUntil(s_tick_ms, 200u);
}

/** @brief 处理密码确认键对应的手动开锁和修改密码流程。 */
static void App_SubmitPassword(void)
{
    uint8_t save_ok;

    if (s_password_input_length != PASSWORD_STORE_LENGTH)
    {
        App_PasswordFeedback(s_password_stage, 'F', 2u);
        return;
    }
    if (s_password_stage == APP_PASSWORD_MANUAL)
    {
        if (App_PasswordMatches(s_password) != 0u)
            App_StartManualUnlock();
        else
        {
            App_PasswordFeedback(APP_PASSWORD_MANUAL, 'F', 2u);
        }
    }
    else if (s_password_stage == APP_PASSWORD_VERIFY_OLD)
    {
        if (App_PasswordMatches(s_password) != 0u)
            App_PasswordEnterStage(APP_PASSWORD_ENTER_NEW);
        else
        {
            App_PasswordFeedback(APP_PASSWORD_VERIFY_OLD, 'F', 2u);
        }
    }
    else if (s_password_stage == APP_PASSWORD_ENTER_NEW)
    {
        (void)memcpy(s_password_new, s_password_input,
                     sizeof(s_password_new));
        App_PasswordEnterStage(APP_PASSWORD_CONFIRM_NEW);
    }
    else if (s_password_stage == APP_PASSWORD_CONFIRM_NEW)
    {
        if (App_PasswordMatches(s_password_new) == 0u)
        {
            App_PasswordFeedback(APP_PASSWORD_ENTER_NEW, 'F', 3u);
            return;
        }
        save_ok = PasswordStore_Save(s_password_new);
        if (save_ok == 0u)
        {
            App_PasswordFeedback(APP_PASSWORD_ENTER_NEW, 'F', 3u);
            return;
        }
        (void)memcpy(s_password, s_password_new, sizeof(s_password));
        App_PasswordFeedback(APP_PASSWORD_IDLE, 'P', 2u);
    }
}

/** @brief 读取一次按键并更新密码输入缓冲区。 */
static void App_ProcessPasswordKey(char key)
{
    App_WakeTm1637();
    s_password_last_input_ms = s_tick_ms;
    if (key == '*')
    {
        s_password_stage = APP_PASSWORD_IDLE;
        App_PasswordClearInput();
        App_UpdatePasswordDisplay();
        return;
    }
    if (key == 'A')
    {
        if (s_password_input_length != 0u)
        {
            --s_password_input_length;
            s_password_last_input_ms = s_tick_ms;
            s_password_cursor_ms = s_tick_ms;
            s_password_cursor_on = 1u;
        }
        App_UpdatePasswordDisplay();
        return;
    }
    if (key >= '0' && key <= '9')
    {
        if (s_password_input_length < PASSWORD_STORE_LENGTH)
        {
            s_password_input[s_password_input_length++] =
                (uint8_t)(key - '0');
            s_password_last_input_ms = s_tick_ms;
            s_password_cursor_ms = s_tick_ms;
            s_password_cursor_on = 1u;
            App_UpdatePasswordDisplay();
        }
        return;
    }
    if (key == '#') App_SubmitPassword();
}

/** @brief 轮询密码键盘、光标闪烁和输入超时。 */
static void App_ServicePassword(void)
{
    char key;

    if (s_manual_unlock_active != 0u) return;

    if (s_password_stage == APP_PASSWORD_IDLE)
    {
        if (s_tm1637_awake == 0u)
        {
            /* 休眠时不改写段码，避免光标闪烁把显示重新点亮。 */
        }
        else if (App_TimeReached(s_tick_ms, s_tm1637_sleep_deadline) != 0u)
        {
            App_SleepTm1637();
        }
        else if ((s_tick_ms - s_password_cursor_ms) >= LOCK_PASSWORD_CURSOR_MS)
        {
            s_password_cursor_ms = s_tick_ms;
            s_password_cursor_on = (uint8_t)(s_password_cursor_on == 0u);
            App_UpdatePasswordDisplay();
        }
        if (App_TimeReached(s_tick_ms, s_next_key_scan_ms) == 0u) return;
        s_next_key_scan_ms = s_tick_ms + LOCK_KEY_SCAN_PERIOD_MS;
        key = TM1637Keypad_GetPressedKey();
        if (key != TM1637_KEYPAD_NO_KEY)
        {
            /* Acknowledge every debounced key press; submit/result tones below
             * intentionally replace this short confirmation tone. */
            LockIo_BeepUntil(s_tick_ms, LOCK_KEY_BEEP_MS);
            App_WakeTm1637();
            if (key >= '0' && key <= '9')
            {
                App_PasswordEnterStage(APP_PASSWORD_MANUAL);
                App_ProcessPasswordKey(key);
            }
            else if (key == 'D')
            {
                App_PasswordEnterStage(APP_PASSWORD_VERIFY_OLD);
            }
        }
        return;
    }

    if (s_password_stage == APP_PASSWORD_FEEDBACK)
    {
        if (App_TimeReached(s_tick_ms, s_password_feedback_deadline) != 0u)
        {
            TM1637Keypad_ClearDisplay();
            if (s_password_feedback_next == APP_PASSWORD_IDLE)
            {
                s_password_stage = APP_PASSWORD_IDLE;
                App_PasswordClearInput();
                App_UpdatePasswordDisplay();
            }
            else
                App_PasswordEnterStage(s_password_feedback_next);
        }
        return;
    }

    if ((s_tick_ms - s_password_last_input_ms) >= LOCK_PASSWORD_TIMEOUT_MS)
    {
        s_password_stage = APP_PASSWORD_IDLE;
        App_PasswordClearInput();
        App_SleepTm1637();
        return;
    }
    if ((s_tick_ms - s_password_cursor_ms) >= LOCK_PASSWORD_CURSOR_MS)
    {
        s_password_cursor_ms = s_tick_ms;
        s_password_cursor_on = (uint8_t)(s_password_cursor_on == 0u);
        App_UpdatePasswordDisplay();
    }
    if (App_TimeReached(s_tick_ms, s_next_key_scan_ms) == 0u) return;
    s_next_key_scan_ms = s_tick_ms + LOCK_KEY_SCAN_PERIOD_MS;
    key = TM1637Keypad_GetPressedKey();
    if (key != TM1637_KEYPAD_NO_KEY)
    {
        LockIo_BeepUntil(s_tick_ms, LOCK_KEY_BEEP_MS);
        App_ProcessPasswordKey(key);
    }
}

/** @brief 三秒手动开锁结束后恢复无线状态或等待状态。 */
static void App_ServiceManualUnlock(void)
{
    if (s_manual_unlock_active == 0u ||
        App_TimeReached(s_tick_ms, s_manual_unlock_deadline) == 0u) return;

    s_manual_unlock_active = 0u;
    App_PasswordClearInput();
    App_WakeTm1637();
    App_UpdatePasswordDisplay();
    if (s_link_ok == 0u || s_have_position == 0u)
        App_RecordEvent(ZH_EVENT_LINK_TIMEOUT);
    else if (s_state == LOCK_STATE_WELCOME)
        App_RecordEvent(ZH_EVENT_ENTER_WELCOME);
    else if (s_state == LOCK_STATE_SENSING)
        App_RecordEvent(ZH_EVENT_ENTER_SENSING);
    else if (s_state == LOCK_STATE_INVALID)
        App_RecordEvent(ZH_EVENT_BAD_ID);
    else if (s_state == LOCK_STATE_OUTSIDE)
        App_RecordEvent(ZH_EVENT_OUTSIDE);
    else if (s_state == LOCK_STATE_UNLOCKED)
        App_RecordEvent(ZH_EVENT_UNLOCK);
    else
        App_RecordEvent(ZH_EVENT_AUTO_LOCK);
}

/** @brief Process one packet currently buffered in the CC1101 RX FIFO. */
static void App_ProcessRadio(void)
{
    uint8_t payload[60];
    uint8_t length;
    uint8_t old_rx_id;
    uint16_t old_distance;
    int16_t old_angle;
    int result;
    char debug[96];
    LockPacket packet;
    LockState old_state;

    if (s_rf_ready == 0u) return;
    result = Cc1101_ReceivePacket(payload, (uint8_t)sizeof(payload), &length);
    if (result != 1) return;
    /* A pulse here means the CC1101 received a CRC-valid air packet. */
    App_RfProbePulse(s_tick_ms);
    if (LockPacket_Decode(payload, length, &packet) == 0u) return;

    s_last_packet_ms = s_tick_ms;
    s_link_ok = 1u;
    if (packet.kind == LOCK_PACKET_DIAGNOSTIC)
    {
        s_diagnostic = 1u;
        s_have_position = 0u;
        s_position_stale = 0u;
        s_state = LOCK_STATE_LOCKED;
        if (s_manual_unlock_active == 0u) App_RecordEvent(ZH_EVENT_RADIO_TEST);
        s_display_dirty = 1u;
        DebugSerial_WriteText("RX TEST:666\r\n");
        return;
    }

    s_diagnostic = 0u;
    (void)App_TrackSequence(packet.sequence);
    if ((packet.flags & 0x01u) == 0u)
    {
        s_have_position = 0u;
        s_position_stale = 0u;
        s_state = LOCK_STATE_LOCKED;
        if (s_manual_unlock_active == 0u) App_RecordEvent(ZH_EVENT_POSITION_INVALID);
        s_display_dirty = 1u;
        DebugSerial_WriteText("RX POSITION INVALID -> LOCKED\r\n");
        return;
    }

    old_state = s_state;
    old_rx_id = s_rx_id;
    old_distance = s_radial_distance_mm;
    old_angle = s_angle_deg10;
    s_have_position = 1u;
    s_position_stale = 0u;
    s_rx_id = packet.key_id;
    s_radial_distance_mm = packet.radial_distance_mm;
    s_angle_deg10 = packet.angle_deg10;
    s_quality = packet.quality;
    s_state = LockState_Classify(s_radial_distance_mm, s_angle_deg10,
                                 (s_rx_id == s_configured_id) ? 1u : 0u,
                                 old_state);
    App_RecordStateEvent(old_state, s_state);
    if (s_state != old_state)
    {
        if (s_manual_unlock_active == 0u && s_state == LOCK_STATE_WELCOME)
            LockIo_BeepUntil(s_tick_ms, 80u);
        if (s_manual_unlock_active == 0u && s_state == LOCK_STATE_UNLOCKED)
            LockIo_BeepUntil(s_tick_ms, 180u);
    }
    if (s_state != old_state || s_rx_id != old_rx_id ||
        s_radial_distance_mm != old_distance || s_angle_deg10 != old_angle)
    {
        s_display_dirty = 1u;
    }

    (void)sprintf(debug, "RX SEQ=%u LOST=%u ID=%u R=%umm A=%d.%ddeg Q=%u STATE=%s\r\n",
                  (unsigned int)packet.sequence,
                  (unsigned int)s_lost_packets,
                  (unsigned int)s_rx_id,
                  (unsigned int)s_radial_distance_mm,
                  (int)(s_angle_deg10 / 10),
                  (int)(((s_angle_deg10 < 0) ? -(int32_t)s_angle_deg10 :
                         (int32_t)s_angle_deg10) % 10),
                  (unsigned int)s_quality,
                  LockState_Name(s_state));
    DebugSerial_WriteText(debug);
}

/**
 * @brief 处理门锁侧拨码变化，并用稳定后的 ID 立即重新判定。
 */
static void App_ServiceConfiguredId(void)
{
    uint8_t events = DipSwitch_Service(s_tick_ms);

    if ((events & DIP_SWITCH_EVENT_CHANGED) != 0u)
    {
        char dip_debug[48];

        s_pending_id = DipSwitch_GetId();
        s_id_pending = (s_pending_id != s_configured_id) ? 1u : 0u;
        if (s_id_pending != 0u)
        {
            s_id_applied_visible = 0u;
        }
        else
        {
            s_id_applied_visible = 1u;
            s_id_applied_deadline = s_tick_ms + LOCK_ID_APPLIED_DISPLAY_MS;
        }
        App_RecordEvent((s_id_pending != 0u) ?
                        ZH_ID_PENDING : ZH_EVENT_SET_CONFIRMED);
        (void)sprintf(dip_debug, "DIP CHANGED ID=%u%u%u%u\r\n",
                      (unsigned int)((s_pending_id >> 3u) & 1u),
                      (unsigned int)((s_pending_id >> 2u) & 1u),
                      (unsigned int)((s_pending_id >> 1u) & 1u),
                      (unsigned int)(s_pending_id & 1u));
        DebugSerial_WriteText(dip_debug);
        s_display_dirty = 1u;
        s_next_display_ms = s_tick_ms;
    }
    if ((events & DIP_SWITCH_EVENT_APPLY) != 0u)
    {
        LockState old_state = s_state;

        s_pending_id = DipSwitch_GetId();
        s_configured_id = s_pending_id;
        s_id_pending = 0u;
        s_id_applied_visible = 1u;
        s_id_applied_deadline = s_tick_ms + LOCK_ID_APPLIED_DISPLAY_MS;
        if (s_manual_unlock_active == 0u) App_RecordEvent(ZH_EVENT_SET_CONFIRMED);
        LockIo_BeepUntil(s_tick_ms, 50u);
        if (s_have_position != 0u && s_position_stale == 0u)
        {
            s_state = LockState_Classify(s_radial_distance_mm, s_angle_deg10,
                                         (s_rx_id == s_configured_id) ? 1u : 0u,
                                         old_state);
            App_RecordStateEvent(old_state, s_state);
            if (s_state == LOCK_STATE_WELCOME && old_state != LOCK_STATE_WELCOME)
                LockIo_BeepUntil(s_tick_ms, 80u);
            if (s_state == LOCK_STATE_UNLOCKED && old_state != LOCK_STATE_UNLOCKED)
                LockIo_BeepUntil(s_tick_ms, 180u);
            if (s_state == LOCK_STATE_INVALID) LockIo_CancelBuzzer();
        }
        s_display_dirty = 1u;
        s_next_display_ms = s_tick_ms;
    }
    if (s_id_pending == 0u && s_id_applied_visible != 0u &&
        App_TimeReached(s_tick_ms, s_id_applied_deadline) != 0u)
    {
        s_id_applied_visible = 0u;
        s_display_dirty = 1u;
        s_next_display_ms = s_tick_ms;
    }
}

/** @brief Lock the output state when the wireless key times out. */
static void App_ServiceTimeout(void)
{
    if (s_link_ok != 0u &&
        (s_tick_ms - s_last_packet_ms) >= LOCK_PACKET_TIMEOUT_MS)
    {
        s_link_ok = 0u;
        s_position_stale = (s_have_position != 0u) ? 1u : 0u;
        s_quality = 0u;
        s_diagnostic = 0u;
        s_have_sequence = 0u;
        s_state = LOCK_STATE_LOST;
        if (s_manual_unlock_active == 0u) App_RecordEvent(ZH_EVENT_LINK_TIMEOUT);
        s_display_dirty = 1u;
        LockIo_CancelBuzzer();
        DebugSerial_WriteText("LINK LOST -> LOCKED\r\n");
    }
}

/** @brief Initialize radio, screen, and lock-side outputs. */
void App_Init(void)
{
    s_tick_ms = 0u;
    s_last_packet_ms = 0u;
    s_next_display_ms = 0u;
    s_radial_distance_mm = 0u;
    s_angle_deg10 = 0;
    s_rx_id = 0u;
    s_pending_id = 0u;
    s_configured_id = 0u;
    s_id_pending = 0u;
    s_id_applied_visible = 1u;
    s_id_applied_deadline = LOCK_ID_APPLIED_DISPLAY_MS;
    s_quality = 0u;
    s_link_ok = 0u;
    s_rf_ready = 0u;
    s_have_position = 0u;
    s_position_stale = 0u;
    s_diagnostic = 0u;
    s_display_dirty = 1u;
    s_have_sequence = 0u;
    s_last_sequence = 0u;
    s_lost_packets = 0u;
    s_state = LOCK_STATE_LOCKED;
    s_last_event = ZH_EVENT_POWER_ON;
    s_previous_event = "";
    s_indicator_mask = 0u;
    s_indicator_valid = 0u;
    s_manual_unlock_active = 0u;
    s_manual_unlock_deadline = 0u;
    s_password_stage = APP_PASSWORD_IDLE;
    s_password_feedback_next = APP_PASSWORD_IDLE;
    s_password_input_length = 0u;
    s_password_last_input_ms = 0u;
    s_password_feedback_deadline = 0u;
    s_password_cursor_ms = 0u;
    s_password_cursor_on = 1u;
    s_next_key_scan_ms = 0u;
    s_tm1637_awake = 0u;
    s_tm1637_sleep_deadline = 0u;
    (void)memset(&s_display_cache, 0, sizeof(s_display_cache));
    (void)PasswordStore_Load(s_password);
    App_RfProbeInit();

    LockIo_Init();
    TM1637Keypad_Init();
    App_UpdatePasswordDisplay();
    App_SleepTm1637();
    DipSwitch_Init();
    s_pending_id = DipSwitch_GetId();
    s_configured_id = s_pending_id;
    App_UpdateIndicators();

    DebugSerial_Init(115200u);
    {
        char dip_debug[64];
        (void)sprintf(dip_debug, "DIP ID=%u%u%u%u APPLY=PB9\r\n",
                      (unsigned int)((s_pending_id >> 3u) & 1u),
                      (unsigned int)((s_pending_id >> 2u) & 1u),
                      (unsigned int)((s_pending_id >> 1u) & 1u),
                      (unsigned int)(s_pending_id & 1u));
        DebugSerial_WriteText(dip_debug);
    }
    Screen_Init();
#if (LOCK_SCREEN_BOOT_TEST == 1)
    Screen_BackLight(1u);
    Screen_Clear(SCREEN_RED);
    delay_ms(350u);
    Screen_Clear(SCREEN_GREEN);
    delay_ms(350u);
    Screen_Clear(SCREEN_BLUE);
    delay_ms(350u);
#endif
    App_DrawLayout();
    App_ShowDisplay();
    Screen_BackLight(1u);
    s_rf_ready = (Cc1101_Init() == 0) ? 1u : 0u;
    App_UpdateIndicators();
    DebugSerial_WriteText((s_rf_ready != 0u) ?
                          "LOCK READY, CC1101 OK\r\n" :
                          "LOCK READY, CC1101 NOT FOUND\r\n");
    s_display_dirty = 1u;
    App_ShowDisplay();
}

/** @brief Poll all devices and refresh dirty display fields cooperatively. */
void App_Run(void)
{
    App_RfProbeService(s_tick_ms);
    LockIo_Service(s_tick_ms);
    App_ServiceConfiguredId();
    App_ProcessRadio();
    App_ServiceTimeout();
    App_ServiceManualUnlock();
    App_ServicePassword();
    App_UpdateIndicators();
    if (s_display_dirty != 0u &&
        App_TimeReached(s_tick_ms, s_next_display_ms) != 0u)
    {
        s_next_display_ms = s_tick_ms + LOCK_DISPLAY_MIN_PERIOD_MS;
        App_ShowDisplay();
    }
    delay_ms(1u);
    ++s_tick_ms;
}
