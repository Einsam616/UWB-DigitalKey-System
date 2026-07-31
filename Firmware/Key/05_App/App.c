#include "App.h"

#include <stdio.h>
#include <stdint.h>

#include "Cc1101.h"
#include "OLEDI2C.h"
#include "DipSwitch.h"
#include "PositionSolver.h"
#include "UwbParser.h"
#include "UwbSerial.h"
#include "delay.h"

#define APP_UWB_BAUD_RATE       256000u
#define APP_TAG_BASELINE_MM     500.0f
#define APP_LEFT_TAG_ID         0x4E21u
#define APP_RIGHT_TAG_ID        0x4E22u
#define APP_PAIR_MAX_AGE_MS     6000u
#define APP_OLED_PERIOD_MS      250u
#define APP_BIND_FIRST_DELAY_MS 500u
#define APP_BIND_RETRY_MS       1000u
#define APP_BIND_MAX_ATTEMPTS   5u
#define APP_UWB_BIND_COMMAND    "AT+TWLT=1,0004,4E21,4E22,FFFF,FFFF\r\n"
#define APP_RADIO_PERIOD_MS     100u
#define APP_RADIO_RETRY_MS      1000u
#define APP_RADIO_FRAME_SIZE    12u
#define APP_RADIO_MAGIC         0xD5u
#define APP_RADIO_VERSION       0x01u
#define APP_CYLINDER_RADIUS_MM  300u
#define APP_POSITION_QUALITY    100u

static UwbLineParser s_parser;
static uint32_t s_left_mm;
static uint32_t s_right_mm;
static uint32_t s_left_time;
static uint32_t s_right_time;
static uint32_t s_uart_bytes;
static uint32_t s_valid_frames;
static uint32_t s_tick_ms;
static uint32_t s_next_oled_ms;
static uint8_t s_left_valid;
static uint8_t s_right_valid;
static uint8_t s_uwb_overflow;
static uint8_t s_bind_attempts;
static uint32_t s_next_bind_ms;
static uint8_t s_radio_ready;
static uint8_t s_radio_tx_ok;
static uint16_t s_radio_sequence;
static uint32_t s_next_radio_ms;
static uint32_t s_next_radio_retry_ms;

/** @brief 判断当前时刻是否已到达截止时刻。 */
static uint8_t App_TimeReached(uint32_t now, uint32_t deadline)
{
    return (((int32_t)(now - deadline)) >= 0) ? 1u : 0u;
}

/** @brief 计算与 Lock 端一致的 CRC-8/0x07 应用层校验。 */
static uint8_t App_Crc8(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0u;
    uint8_t index;
    uint8_t bit;

    for (index = 0u; index < length; ++index)
    {
        crc = (uint8_t)(crc ^ data[index]);
        for (bit = 0u; bit < 8u; ++bit)
        {
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1u) ^ 0x07u)
                                  : (uint8_t)(crc << 1u);
        }
    }
    return crc;
}

/** @brief 构造 Lock 端使用的 12 字节固定定位帧。 */
static void App_BuildRadioFrame(uint8_t frame[APP_RADIO_FRAME_SIZE],
                                uint8_t key_id,
                                uint16_t radial_distance_mm,
                                int16_t angle_deg10)
{
    uint16_t raw_angle = (uint16_t)angle_deg10;
    frame[0] = APP_RADIO_MAGIC;
    frame[1] = APP_RADIO_VERSION;
    frame[2] = (uint8_t)(key_id & 0x0Fu);
    frame[3] = 0x01u;
    frame[4] = (uint8_t)radial_distance_mm;
    frame[5] = (uint8_t)(radial_distance_mm >> 8u);
    frame[6] = (uint8_t)raw_angle;
    frame[7] = (uint8_t)(raw_angle >> 8u);
    frame[8] = (uint8_t)s_radio_sequence;
    frame[9] = (uint8_t)(s_radio_sequence >> 8u);
    frame[10] = APP_POSITION_QUALITY;
    frame[11] = App_Crc8(frame, (uint8_t)(APP_RADIO_FRAME_SIZE - 1u));
}

/** @brief 向 OLED 写入固定宽度的一行，避免旧字符残留。 */
static void App_ShowLine(uint8_t page, const char *text)
{
    char line[22];
    uint8_t index = 0u;
    while (text != 0 && text[index] != '\0' && index < 21u)
    {
        line[index] = text[index];
        index++;
    }
    while (index < 21u) line[index++] = ' ';
    line[index] = '\0';
    (void)OLEDI2C_ShowString(0u, page, line);
}

/** @brief Read and display the four-bit PB12-PB15 DIP-switch ID. */
static void App_ShowDipId(void)
{
    uint8_t bits[4];
    char line[8];

    DipSwitch_ReadBits(bits);
    line[0] = 'I';
    line[1] = 'D';
    line[2] = ':';
    line[3] = (char)('0' + bits[0]);
    line[4] = (char)('0' + bits[1]);
    line[5] = (char)('0' + bits[2]);
    line[6] = (char)('0' + bits[3]);
    line[7] = '\0';
    App_ShowLine(0u, line);
}

/** @brief 显示等待或溢出状态。 */
static void App_ShowWaiting(void)
{
    const char *status;
    char angle_line[10] = {'A', ':', '-', '-', '-', '.', '-', OLED_I2C_DEGREE_CHAR, '\0'};
    App_ShowDipId();
    App_ShowLine(2u, "D:---.-cm");
    App_ShowLine(4u, angle_line);
    if (s_uwb_overflow != 0u) status = "STATUS: UART OVF";
    else if (s_uart_bytes == 0u && s_bind_attempts < APP_BIND_MAX_ATTEMPTS)
        status = "STATUS: BINDING";
    else if (s_uart_bytes == 0u) status = "STATUS: NO UART";
    else if (s_valid_frames == 0u) status = "STATUS: BAD FRAME";
    else if (s_left_valid != 0u || s_right_valid != 0u) status = "STATUS: ONE TAG";
    else status = "STATUS: WAIT";
    if (s_left_valid != 0u && s_right_valid != 0u &&
        ((s_tick_ms - s_left_time) > APP_PAIR_MAX_AGE_MS ||
         (s_tick_ms - s_right_time) > APP_PAIR_MAX_AGE_MS))
    {
        status = "STATUS: STALE";
    }
    App_ShowLine(6u, status);
}

/** @brief Bind the two measured tags after every anchor power-up. */
static void App_ServiceUwbBinding(void)
{
    if (s_valid_frames != 0u)
    {
        s_bind_attempts = APP_BIND_MAX_ATTEMPTS;
        return;
    }
    if (s_bind_attempts >= APP_BIND_MAX_ATTEMPTS ||
        App_TimeReached(s_tick_ms, s_next_bind_ms) == 0u)
    {
        return;
    }

    (void)UwbSerial_WriteText(APP_UWB_BIND_COMMAND);
    s_bind_attempts++;
    s_next_bind_ms = s_tick_ms + APP_BIND_RETRY_MS;
}

/** @brief 依据两路距离计算中心距离和方位角并刷新 OLED。 */
static void App_ShowPosition(void)
{
    PositionResult result;
    int32_t angle_deg10;
    int32_t angle_abs;
    char angle_sign;
    char line[24];
    uint32_t center_mm_display;
    uint32_t center_cm;
    uint32_t center_cm_tenth;

    if (PositionSolver_Calculate((float)s_left_mm, (float)s_right_mm,
                                 APP_TAG_BASELINE_MM, &result) == 0u)
    {
        App_ShowWaiting();
        App_ShowLine(6u, "STATUS: GEOM ERR");
        return;
    }
    angle_deg10 = (result.angle_deg >= 0.0f) ?
                  (int32_t)(result.angle_deg * 10.0f + 0.5f) :
                  (int32_t)(result.angle_deg * 10.0f - 0.5f);
    angle_abs = (angle_deg10 < 0) ? -angle_deg10 : angle_deg10;
    angle_sign = (angle_deg10 < 0) ? '-' : '+';
    center_mm_display = (uint32_t)result.center_mm;
    center_cm = center_mm_display / 10u;
    center_cm_tenth = center_mm_display % 10u;

    (void)sprintf(line, "D:%lu.%lucm", (unsigned long)center_cm,
                  (unsigned long)center_cm_tenth);
    App_ShowDipId();
    App_ShowLine(2u, line);
    (void)sprintf(line, "A:%c%ld.%ld%c", angle_sign,
                  (long)(angle_abs / 10), (long)(angle_abs % 10),
                  OLED_I2C_DEGREE_CHAR);
    App_ShowLine(4u, line);
    if (s_radio_ready != 0u && s_radio_tx_ok != 0u)
        App_ShowLine(6u, "STATUS: RF TX OK");
    else
        App_ShowLine(6u, "STATUS: RF ERROR");
}

/** @brief 轮询 UART 环形缓冲区并更新左右 Tag 的最新距离。 */
static void App_ProcessUwb(void)
{
    uint8_t value;
    UwbMeasurement measurement;
    while (UwbSerial_ReadByte(&value) != 0u)
    {
        s_uart_bytes++;
        if (UwbParser_Consume(&s_parser, value, &measurement) != 0u)
        {
            if (measurement.tag_id == APP_LEFT_TAG_ID)
            {
                s_left_mm = measurement.range0_mm;
                s_left_time = s_tick_ms;
                s_left_valid = 1u;
                s_valid_frames++;
            }
            else if (measurement.tag_id == APP_RIGHT_TAG_ID)
            {
                s_right_mm = measurement.range0_mm;
                s_right_time = s_tick_ms;
                s_right_valid = 1u;
                s_valid_frames++;
            }
        }
    }
    if (UwbSerial_TakeOverflow() != 0u) s_uwb_overflow = 1u;
}

/** @brief 判断左右距离是否来自近期的同一轮测量。 */
static uint8_t App_PairValid(void)
{
    uint32_t age_left;
    uint32_t age_right;
    if (s_left_valid == 0u || s_right_valid == 0u) return 0u;
    age_left = s_tick_ms - s_left_time;
    age_right = s_tick_ms - s_right_time;
    return (age_left <= APP_PAIR_MAX_AGE_MS && age_right <= APP_PAIR_MAX_AGE_MS) ? 1u : 0u;
}

/** @brief 将最新 UWB 位置按 10 Hz 通过已验证的 CC1101 链路发送。 */
static void App_ServiceRadio(void)
{
    uint8_t frame[APP_RADIO_FRAME_SIZE];
    PositionResult result;
    uint32_t center_mm;
    uint32_t radial_mm;
    int32_t angle_deg10;

    if (App_TimeReached(s_tick_ms, s_next_radio_ms) == 0u) return;
    s_next_radio_ms = s_tick_ms + APP_RADIO_PERIOD_MS;

    if (s_radio_ready == 0u)
    {
        if (App_TimeReached(s_tick_ms, s_next_radio_retry_ms) == 0u) return;
        s_radio_ready = (Cc1101_Init() == 0) ? 1u : 0u;
        s_next_radio_retry_ms = s_tick_ms + APP_RADIO_RETRY_MS;
        if (s_radio_ready == 0u) return;
    }
    if (App_PairValid() == 0u) return;
    if (PositionSolver_Calculate((float)s_left_mm, (float)s_right_mm,
                                 APP_TAG_BASELINE_MM, &result) == 0u)
    {
        return;
    }

    center_mm = (uint32_t)result.center_mm;
    radial_mm = (center_mm > APP_CYLINDER_RADIUS_MM) ?
                (center_mm - APP_CYLINDER_RADIUS_MM) : 0u;
    if (radial_mm > 65535u) radial_mm = 65535u;
    angle_deg10 = (result.angle_deg >= 0.0f) ?
                  (int32_t)(result.angle_deg * 10.0f + 0.5f) :
                  (int32_t)(result.angle_deg * 10.0f - 0.5f);

    App_BuildRadioFrame(frame, DipSwitch_ReadValue(), (uint16_t)radial_mm,
                        (int16_t)angle_deg10);
    if (Cc1101_SendPacket(frame, APP_RADIO_FRAME_SIZE) == 0)
    {
        ++s_radio_sequence;
        s_radio_tx_ok = 1u;
    }
    else
    {
        s_radio_ready = 0u;
        s_radio_tx_ok = 0u;
        s_next_radio_retry_ms = s_tick_ms + APP_RADIO_RETRY_MS;
    }
}

/** @brief 初始化本次 01 工程的所有模块。 */
void App_Init(void)
{
    DipSwitch_Init();
    UwbParser_Reset(&s_parser);
    UwbSerial_Init(APP_UWB_BAUD_RATE);
    (void)OLEDI2C_Init();
    s_left_valid = 0u;
    s_right_valid = 0u;
    s_uart_bytes = 0u;
    s_valid_frames = 0u;
    s_uwb_overflow = 0u;
    s_bind_attempts = 0u;
    s_tick_ms = 0u;
    s_next_bind_ms = APP_BIND_FIRST_DELAY_MS;
    s_next_oled_ms = 0u;
    s_radio_sequence = 0u;
    s_radio_tx_ok = 0u;
    s_next_radio_ms = 0u;
    s_next_radio_retry_ms = APP_RADIO_RETRY_MS;
    s_radio_ready = (Cc1101_Init() == 0) ? 1u : 0u;
    App_ShowWaiting();
}

/** @brief 轮询 UWB 并以 1 ms 节拍更新 OLED。 */
void App_Run(void)
{
    App_ServiceUwbBinding();
    App_ProcessUwb();
    App_ServiceRadio();
    if (App_TimeReached(s_tick_ms, s_next_oled_ms) != 0u)
    {
        s_next_oled_ms = s_tick_ms + APP_OLED_PERIOD_MS;
        if (App_PairValid() != 0u) App_ShowPosition();
        else App_ShowWaiting();
    }
    delay_ms(1u);
    s_tick_ms++;
}
