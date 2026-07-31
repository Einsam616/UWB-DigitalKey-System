#include "App.h"

#include <stdio.h>
#include <stdint.h>

#include "Cc1101.h"
#include "ChineseFont.h"
#include "DebugSerial.h"
#include "DipSwitch.h"
#include "LockIo.h"
#include "Screen.h"
#include "LockPacket.h"
#include "LockState.h"
#include "delay.h"

#define LOCK_DISPLAY_MIN_PERIOD_MS 200u
#define LOCK_PACKET_TIMEOUT_MS    1000u
#define LOCK_SCREEN_BOOT_TEST     0

static uint32_t s_tick_ms;
static uint32_t s_last_packet_ms;
static uint32_t s_next_display_ms;
static uint16_t s_distance_mm;
static int16_t s_angle_deg10;
static uint8_t s_rx_id;
static uint8_t s_config_id;
static uint8_t s_quality;
static uint8_t s_link_ok;
static uint8_t s_rf_ready;
static uint8_t s_have_position;
static uint8_t s_diagnostic;
static uint8_t s_display_dirty;
static uint8_t s_have_sequence;
static uint16_t s_last_sequence;
static uint16_t s_lost_packets;
static LockState s_state;
static const char *s_last_event;

/** @brief Apply the five front-panel indicator meanings to D1-D5. */
static void App_UpdateIndicators(void)
{
    uint8_t mask = LOCK_LED_BIT(LOCK_LED_RUN);

    if (s_rf_ready != 0u && s_link_ok != 0u)
    {
        mask |= LOCK_LED_BIT(LOCK_LED_LINK);
    }
    if (s_state == LOCK_STATE_WELCOME)
    {
        mask |= LOCK_LED_BIT(LOCK_LED_WELCOME);
    }
    if (s_state == LOCK_STATE_UNLOCKED)
    {
        mask |= LOCK_LED_BIT(LOCK_LED_UNLOCKED);
    }
    else
    {
        mask |= LOCK_LED_BIT(LOCK_LED_LOCKED);
    }
    LockIo_SetLedMask(mask);
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

/** @brief Clear and redraw one fixed-width UTF-8 Chinese field. */
static void App_ShowChineseField(uint16_t x,
                                 uint16_t y,
                                 uint16_t width,
                                 const char *text,
                                 uint16_t color,
                                 uint16_t background)
{
    Screen_FillRect(x, y, width, 17u, background);
    ChineseFont_ShowText(x, y, text, color, background, 1u);
}

/** @brief Draw the static 320x240 lock dashboard. */
static void App_DrawLayout(void)
{
    Screen_Clear(SCREEN_BLACK);
    Screen_FillRect(0u, 0u, SCREEN_WIDTH, 32u, SCREEN_NAVY);
    ChineseFont_ShowTitle24(8u, 4u, ZH_TITLE_DIGITAL_KEY_LOCK,
                            SCREEN_WHITE, SCREEN_NAVY);
    Screen_DrawRect(4u, 38u, 312u, 48u, SCREEN_DGRAY);
    Screen_DrawRect(4u, 90u, 312u, 70u, SCREEN_DGRAY);
    Screen_DrawLine(0u, 199u, 319u, 199u, SCREEN_DGRAY);
}

/** @brief Return the current identity-verification text and color. */
static const char *App_AuthText(uint16_t *color)
{
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
    if (s_rx_id == s_config_id)
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
    if (s_diagnostic != 0u)
    {
        *background = SCREEN_DCYAN;
        return ZH_ZONE_RADIO_TEST;
    }
    if (s_have_position == 0u)
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

/** @brief Draw all dynamic fields required by the C题 judging items. */
static void App_ShowDisplay(void)
{
    char line[33];
    char set_id[5];
    char rx_id[5];
    uint16_t field_color;
    uint16_t zone_background;
    uint16_t cm;
    uint16_t mm_digit;
    int32_t angle_abs;
    char angle_sign;
    const char *zone_text;
    const char *link_text;
    const char *auth_text;

    App_FormatId(s_config_id, set_id);
    if (s_link_ok != 0u && s_have_position != 0u)
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

    Screen_FillRect(240u, 4u, 76u, 24u,
                    (s_state == LOCK_STATE_UNLOCKED) ? SCREEN_DGREEN : SCREEN_MAROON);
    ChineseFont_ShowText(254u, 8u,
                         (s_state == LOCK_STATE_UNLOCKED) ?
                         ZH_LOCK_OPEN : ZH_LOCK_CLOSED,
                         SCREEN_WHITE,
                         (s_state == LOCK_STATE_UNLOCKED) ?
                         SCREEN_DGREEN : SCREEN_MAROON, 1u);

    ChineseFont_ShowText(10u, 44u, ZH_LABEL_RX_ID,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    App_ShowField(66u, 44u, 4u, 2u, rx_id, SCREEN_WHITE, SCREEN_BLACK);
    ChineseFont_ShowText(166u, 44u, ZH_LABEL_SET_ID,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    App_ShowField(222u, 44u, 4u, 2u, set_id, SCREEN_WHITE, SCREEN_BLACK);
    ChineseFont_ShowText(10u, 66u, ZH_LABEL_AUTH,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    auth_text = App_AuthText(&field_color);
    App_ShowChineseField(90u, 66u, 64u, auth_text,
                         field_color, SCREEN_BLACK);

    ChineseFont_ShowText(16u, 98u, ZH_LABEL_DISTANCE,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(16u, 132u, ZH_LABEL_ANGLE,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);

    if (s_have_position != 0u)
    {
        cm = (uint16_t)(s_distance_mm / 10u);
        mm_digit = (uint16_t)(s_distance_mm % 10u);
        (void)sprintf(line, "%04u.%ucm", (unsigned int)cm, (unsigned int)mm_digit);
        App_ShowField(82u, 94u, 12u, 3u, line, SCREEN_WHITE, SCREEN_BLACK);

        angle_abs = (s_angle_deg10 < 0) ? -(int32_t)s_angle_deg10 : (int32_t)s_angle_deg10;
        angle_sign = (s_angle_deg10 < 0) ? '-' : '+';
        (void)sprintf(line, "%c%03lu.%ludeg", angle_sign,
                      (unsigned long)(angle_abs / 10),
                      (unsigned long)(angle_abs % 10));
        App_ShowField(82u, 128u, 12u, 3u, line, SCREEN_WHITE, SCREEN_BLACK);
    }
    else
    {
        App_ShowField(82u, 94u, 12u, 3u, "----.-cm", SCREEN_LGRAY, SCREEN_BLACK);
        App_ShowField(82u, 128u, 12u, 3u, "+---.-deg", SCREEN_LGRAY, SCREEN_BLACK);
    }

    zone_text = App_ZoneText(&zone_background);
    Screen_FillRect(0u, 164u, SCREEN_WIDTH, 31u, zone_background);
    ChineseFont_ShowText(8u, 172u, ZH_LABEL_ZONE,
                         SCREEN_WHITE, zone_background, 1u);
    ChineseFont_ShowText(56u, 172u, zone_text,
                         SCREEN_WHITE, zone_background, 1u);

    if (s_rf_ready == 0u) link_text = ZH_RF_FAULT;
    else if (s_link_ok == 0u) link_text = ZH_STATUS_WAIT;
    else link_text = ZH_RF_NORMAL;
    Screen_FillRect(0u, 201u, SCREEN_WIDTH, 19u, SCREEN_BLACK);
    ChineseFont_ShowText(8u, 203u, ZH_LABEL_RF,
                         SCREEN_CYAN, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(50u, 203u, link_text,
                         SCREEN_CYAN, SCREEN_BLACK, 1u);
    if (s_link_ok != 0u && s_have_position != 0u)
    {
        (void)sprintf(line, "Q:%03u", (unsigned int)s_quality);
    }
    else
    {
        (void)sprintf(line, "Q:---");
    }
    App_ShowField(126u, 203u, 5u, 2u, line, SCREEN_CYAN, SCREEN_BLACK);
    (void)sprintf(line, "L:%03u", (unsigned int)s_lost_packets);
    App_ShowField(198u, 203u, 5u, 2u, line, SCREEN_CYAN, SCREEN_BLACK);

    Screen_FillRect(0u, 223u, SCREEN_WIDTH, 17u, SCREEN_BLACK);
    ChineseFont_ShowText(8u, 223u, ZH_LABEL_LAST_EVENT,
                         SCREEN_LGRAY, SCREEN_BLACK, 1u);
    ChineseFont_ShowText(84u, 223u, s_last_event,
                         SCREEN_WHITE, SCREEN_BLACK, 1u);
    s_display_dirty = 0u;
}

/** @brief Update the retained event message after a state transition. */
static void App_RecordStateEvent(LockState old_state, LockState new_state)
{
    if (new_state == old_state) return;
    if (new_state == LOCK_STATE_UNLOCKED) s_last_event = ZH_EVENT_UNLOCK;
    else if (new_state == LOCK_STATE_WELCOME) s_last_event = ZH_EVENT_ENTER_WELCOME;
    else if (new_state == LOCK_STATE_INVALID) s_last_event = ZH_EVENT_BAD_ID;
    else if (new_state == LOCK_STATE_OUTSIDE) s_last_event = ZH_EVENT_OUTSIDE;
    else if (old_state == LOCK_STATE_UNLOCKED) s_last_event = ZH_EVENT_AUTO_LOCK;
    else if (new_state == LOCK_STATE_SENSING) s_last_event = ZH_EVENT_ENTER_SENSING;
    else if (new_state == LOCK_STATE_LOCKED) s_last_event = ZH_EVENT_LEAVE_SENSING;
}

/** @brief Process one packet currently buffered in the CC1101 RX FIFO. */
static void App_ProcessRadio(void)
{
    uint8_t payload[60];
    uint8_t length;
    uint8_t old_rx_id;
    uint8_t old_quality;
    uint8_t loss_changed;
    uint16_t old_distance;
    int16_t old_angle;
    int result;
    char debug[96];
    LockPacket packet;
    LockState old_state;

    if (s_rf_ready == 0u) return;
    result = Cc1101_ReceivePacket(payload, (uint8_t)sizeof(payload), &length);
    if (result != 1) return;
    if (LockPacket_Decode(payload, length, &packet) == 0u) return;

    s_last_packet_ms = s_tick_ms;
    s_link_ok = 1u;
    if (packet.kind == LOCK_PACKET_DIAGNOSTIC)
    {
        s_diagnostic = 1u;
        s_have_position = 0u;
        s_state = LOCK_STATE_LOCKED;
        s_last_event = ZH_EVENT_RADIO_TEST;
        s_display_dirty = 1u;
        DebugSerial_WriteText("RX TEST:666\r\n");
        return;
    }

    s_diagnostic = 0u;
    loss_changed = App_TrackSequence(packet.sequence);
    if ((packet.flags & 0x01u) == 0u)
    {
        s_have_position = 0u;
        s_state = LOCK_STATE_LOCKED;
        s_last_event = ZH_EVENT_POSITION_INVALID;
        s_display_dirty = 1u;
        DebugSerial_WriteText("RX POSITION INVALID -> LOCKED\r\n");
        return;
    }

    old_state = s_state;
    old_rx_id = s_rx_id;
    old_distance = s_distance_mm;
    old_angle = s_angle_deg10;
    old_quality = s_quality;
    s_have_position = 1u;
    s_rx_id = packet.key_id;
    s_distance_mm = packet.radial_distance_mm;
    s_angle_deg10 = packet.angle_deg10;
    s_quality = packet.quality;
    s_state = LockState_Classify(s_distance_mm, s_angle_deg10,
                                 (s_rx_id == s_config_id) ? 1u : 0u,
                                 old_state);
    App_RecordStateEvent(old_state, s_state);
    if (s_state != old_state)
    {
        if (s_state == LOCK_STATE_WELCOME) LockIo_BeepUntil(s_tick_ms, 80u);
        if (s_state == LOCK_STATE_UNLOCKED) LockIo_BeepUntil(s_tick_ms, 180u);
    }
    if (s_state != old_state || s_rx_id != old_rx_id ||
        s_distance_mm != old_distance || s_angle_deg10 != old_angle ||
        s_quality != old_quality || loss_changed != 0u)
    {
        s_display_dirty = 1u;
    }

    (void)sprintf(debug, "RX SEQ=%u LOST=%u ID=%u D=%umm A=%d.%ddeg Q=%u STATE=%s\r\n",
                  (unsigned int)packet.sequence,
                  (unsigned int)s_lost_packets,
                  (unsigned int)s_rx_id,
                  (unsigned int)s_distance_mm,
                  (int)(s_angle_deg10 / 10),
                  (int)(((s_angle_deg10 < 0) ? -(int32_t)s_angle_deg10 :
                         (int32_t)s_angle_deg10) % 10),
                  (unsigned int)s_quality,
                  LockState_Name(s_state));
    DebugSerial_WriteText(debug);
}

/** @brief Revalidate the current key whenever the lock-side DIP setting changes. */
static void App_ServiceConfiguredId(void)
{
    uint8_t new_id = DipSwitch_ReadValue();
    LockState old_state;

    if (new_id == s_config_id) return;
    s_config_id = new_id;
    s_last_event = ZH_EVENT_SET_CHANGED;
    if (s_have_position != 0u)
    {
        old_state = s_state;
        s_state = LockState_Classify(s_distance_mm, s_angle_deg10,
                                     (s_rx_id == s_config_id) ? 1u : 0u,
                                     old_state);
        if (s_state == LOCK_STATE_WELCOME && old_state != LOCK_STATE_WELCOME)
            LockIo_BeepUntil(s_tick_ms, 80u);
        if (s_state == LOCK_STATE_UNLOCKED && old_state != LOCK_STATE_UNLOCKED)
            LockIo_BeepUntil(s_tick_ms, 180u);
        if (s_state == LOCK_STATE_INVALID) LockIo_SetBuzzer(0u);
    }
    s_display_dirty = 1u;
}

/** @brief Lock the output state when the wireless key times out. */
static void App_ServiceTimeout(void)
{
    if (s_link_ok != 0u &&
        (s_tick_ms - s_last_packet_ms) > LOCK_PACKET_TIMEOUT_MS)
    {
        s_link_ok = 0u;
        s_have_position = 0u;
        s_diagnostic = 0u;
        s_have_sequence = 0u;
        s_state = LOCK_STATE_LOST;
        s_last_event = ZH_EVENT_LINK_TIMEOUT;
        s_display_dirty = 1u;
        LockIo_SetBuzzer(0u);
        DebugSerial_WriteText("LINK LOST -> LOCKED\r\n");
    }
}

/** @brief Initialize radio, screen, and lock-side outputs. */
void App_Init(void)
{
    s_tick_ms = 0u;
    s_last_packet_ms = 0u;
    s_next_display_ms = 0u;
    s_distance_mm = 0u;
    s_angle_deg10 = 0;
    s_rx_id = 0u;
    s_config_id = 0u;
    s_quality = 0u;
    s_link_ok = 0u;
    s_rf_ready = 0u;
    s_have_position = 0u;
    s_diagnostic = 0u;
    s_display_dirty = 1u;
    s_have_sequence = 0u;
    s_last_sequence = 0u;
    s_lost_packets = 0u;
    s_state = LOCK_STATE_LOCKED;
    s_last_event = ZH_EVENT_POWER_ON;

    LockIo_Init();
    DipSwitch_Init();
    s_config_id = DipSwitch_ReadValue();
    LockIo_SetLedMask(LOCK_LED_BIT(LOCK_LED_RUN) |
                      LOCK_LED_BIT(LOCK_LED_LINK) |
                      LOCK_LED_BIT(LOCK_LED_WELCOME) |
                      LOCK_LED_BIT(LOCK_LED_LOCKED) |
                      LOCK_LED_BIT(LOCK_LED_UNLOCKED));
    delay_ms(150u);
    App_UpdateIndicators();

    DebugSerial_Init(115200u);
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
    LockIo_Service(s_tick_ms);
    App_ServiceConfiguredId();
    App_ProcessRadio();
    App_ServiceTimeout();
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
