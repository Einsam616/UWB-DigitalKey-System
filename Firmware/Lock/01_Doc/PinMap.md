# Lock Pin Map

Target: STM32F103C8T6, 3.3 V logic.

| Module | Signal | MCU pin | Direction/mode | Peripheral/AF | Electrical note |
|---|---|---|---|---|---|
| Indicator LED | D1 | PA0 | push-pull output | GPIO | active low; run/heartbeat |
| Indicator LED | D2 | PA1 | push-pull output | GPIO | active low; valid wireless link |
| Indicator LED | D3 | PA2 | push-pull output | GPIO | active low; welcome zone |
| Indicator LED | D4 | PA3 | push-pull output | GPIO | active low; locked |
| Indicator LED | D5 | PA4 | push-pull output | GPIO | active low; unlocked |
| CC1101 | GDO0 | PA5 | input pulldown | GPIO | packet sync/status |
| Lock ID switch | SW1 | PA6 | input pull-up | GPIO | active low; ON=1; ID bit3 |
| Lock ID switch | SW2 | PA7 | input pull-up | GPIO | active low; ON=1; ID bit2 |
| Screen | CS1 | PA8 | push-pull output | GPIO | active low |
| Debug | USART1 TX/RX | PA9/PA10 | AF output / input pull-up | USART1 | 115200 baud, 8N1 |
| Screen | RES | PA11 | push-pull output | GPIO | active low reset |
| Screen | DC | PA12 | push-pull output | GPIO | command/data select |
| SWD | SWDIO/SWCLK | PA13/PA14 | debug reserved | SWD | keep connected for download |
| Screen | CS2 | PA15 | push-pull output | GPIO | held high; JTAG disabled |
| Screen | BLK | PB0 | push-pull output | GPIO/PWM capable | backlight gate |
| Boot | BOOT1 | PB2 | board pull-down | GPIO | leave board default |
| CC1101 | SPI1 SCK | PB3 | AF push-pull | SPI1 remap | JTAG disabled, SWD kept |
| CC1101 | SPI1 MISO/GDO1 | PB4 | input pull-up | SPI1 remap | shared module status line |
| CC1101 | SPI1 MOSI | PB5 | AF push-pull | SPI1 remap | module data input |
| Extension | TM1637 CLK | PB6 | reserved | GPIO | wiring retained; not initialized yet |
| Extension | TM1637 IO | PB7 | reserved | GPIO | wiring retained; not initialized yet |
| Buzzer | PWM | PB8 | AF push-pull | TIM4_CH3 | 2 kHz, 50% duty while active |
| Lock ID switch | SW3 | PB9 | input pull-up | GPIO | active low; ON=1; ID bit1 |
| Lock ID switch | SW4 | PB10 | input pull-up | GPIO | active low; ON=1; ID bit0 |
| CC1101 | CSN | PB12 | push-pull output | GPIO | active low |
| Screen | SPI2 SCK | PB13 | push-pull output | software SPI | library-compatible clock |
| Screen | SPI2 MISO | PB14 | reserved input | GPIO | retained for future readback |
| Screen | SPI2 MOSI | PB15 | push-pull output | software SPI | library-compatible data |
| Touch | PEN interrupt | PC13 | reserved input | EXTI candidate | not used by lock state machine |

## Required remap

CC1101 uses the fully remapped SPI1 pins. The driver enables:

```c
RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
GPIO_PinRemapConfig(GPIO_Remap_SPI1, ENABLE);
```

This disables JTAG but keeps PA13/PA14 SWD available.

The screen library uses its original software-SPI timing on PB13/PB15. It writes the LCD through CS1/PA8 and keeps CS2/PA15 high for the reserved touch side. PB14 is reserved and is not read by the current page renderer.
