//========================================================================
//文件名称：timer_pwm_incap.h
//功能概要：Timer_PWM_INCAP底层驱动构件头文件
//芯片类型：STM32F103C8T6
//版权所有：江苏电子-索明何
//版本更新：2023-12-10  V1.1
//========================================================================
#ifndef  _TIMER_PWM_INCAP_H        //防止重复定义（开头）
#define  _TIMER_PWM_INCAP_H

//1.头文件包含
#include  "common.h"               //包含公共要素软件构件头文件

//2.宏定义
//(1)TIMER号宏定义
#define  TIMER1   0
#define  TIMER2   1
#define  TIMER3   2
#define  TIMER4   3

//(2)TIMER时钟频率
#define  TIMER_CLK_MHZ    1        //1MHz
#define  TIMER_CLK_KHZ    1000     //1000kHz

//(3)端口号宏定义，采用左移8位是为了使端口号位于TIMER通道引脚号的高8位
#define  PTA    (0<<8)
#define  PTB    (1<<8)
#define  PTC    (2<<8)
#define  PTD    (3<<8)

//(4)TIMER通道使用的引脚宏定义（由实际使用的引脚决定）
//以下各个TIMER的通道引脚映射号取值依据：AFIO_MAPR寄存器的说明
#define  TIMER1_REMAP   0          //TIMER1通道引脚映射号，取0
#if (TIMER1_REMAP == 0)
  #define  TIMER1_CH1   (PTA|8)    //TIMER1_CH1通道：PTA8
  #define  TIMER1_CH2   (PTA|9)    //TIMER1_CH2通道：PTA9
  #define  TIMER1_CH3   (PTA|10)   //TIMER1_CH3通道：PTA10
  #define  TIMER1_CH4   (PTA|11)   //TIMER1_CH4通道：PTA11
#endif

#define  TIMER2_REMAP   0          //TIMER2通道引脚映射号，取0~3
#if (TIMER2_REMAP == 0)
  #define  TIMER2_CH1   (PTA|0)    //TIMER2_CH1通道：PTA0
  #define  TIMER2_CH2   (PTA|1)    //TIMER2_CH2通道：PTA1
  #define  TIMER2_CH3   (PTA|2)    //TIMER2_CH3通道：PTA2
  #define  TIMER2_CH4   (PTA|3)    //TIMER2_CH4通道：PTA3
#elif (TIMER2_REMAP == 1)
  #define  TIMER2_CH1   (PTA|15)   //TIMER2_CH1通道：PTA15
  #define  TIMER2_CH2   (PTB|3)    //TIMER2_CH2通道：PTB3
  #define  TIMER2_CH3   (PTA|2)    //TIMER2_CH3通道：PTA2
  #define  TIMER2_CH4   (PTA|3)    //TIMER2_CH4通道：PTA3
#elif (TIMER2_REMAP == 2)
  #define  TIMER2_CH1   (PTA|0)    //TIMER2_CH1通道：PTA0
  #define  TIMER2_CH2   (PTA|1)    //TIMER2_CH2通道：PTA1
  #define  TIMER2_CH3   (PTB|10)   //TIMER2_CH3通道：PTB10
  #define  TIMER2_CH4   (PTB|11)   //TIMER2_CH4通道：PTB11
#elif (TIMER2_REMAP == 3)
  #define  TIMER2_CH1   (PTA|15)   //TIMER2_CH1通道：PTA15
  #define  TIMER2_CH2   (PTB|3)    //TIMER2_CH2通道：PTB3
  #define  TIMER2_CH3   (PTB|10)   //TIMER2_CH3通道：PTB10
  #define  TIMER2_CH4   (PTB|11)   //TIMER2_CH4通道：PTB11
#endif

#define  TIMER3_REMAP   0          //TIMER3通道引脚映射号，取0或2
#if (TIMER3_REMAP == 0)
  #define  TIMER3_CH1   (PTA|6)    //TIMER3_CH1通道：PTA6
  #define  TIMER3_CH2   (PTA|7)    //TIMER3_CH2通道：PTA7
  #define  TIMER3_CH3   (PTB|0)    //TIMER3_CH3通道：PTB0
  #define  TIMER3_CH4   (PTB|1)    //TIMER3_CH4通道：PTB1
#elif (TIMER3_REMAP == 2)
  #define  TIMER3_CH1   (PTB|4)    //TIMER3_CH1通道：PTB4
  #define  TIMER3_CH2   (PTB|5)    //TIMER3_CH2通道：PTB5
  #define  TIMER3_CH3   (PTB|0)    //TIMER3_CH3通道：PTB0
  #define  TIMER3_CH4   (PTB|1)    //TIMER3_CH4通道：PTB1
#endif

#define  TIMER4_REMAP   0          //TIMER4通道引脚映射号，取0
#if (TIMER4_REMAP == 0)
  #define  TIMER4_CH1   (PTB|6)    //TIMER2_CH1通道：PTB6
  #define  TIMER4_CH2   (PTB|7)    //TIMER2_CH2通道：PTB7
  #define  TIMER4_CH3   (PTB|8)    //TIMER2_CH3通道：PTB8
  #define  TIMER4_CH4   (PTB|9)    //TIMER2_CH4通道：PTB9
#endif

//(5)PWM极性和对齐方式宏定义
#define  PWM_P        1            //正极性（平时电平为低电平，有效电平为高电平）
#define  PWM_N        0            //负极性（平时电平为高电平，有效电平为低电平）
#define  PWM_EDGE     1            //边沿对齐
#define  PWM_CENTER   0            //中心对齐

//(6)输入捕获模式宏定义
#define  CAP_UP       1            //上升沿捕获
#define  CAP_DOWN     2            //下降沿捕获

//3.对外接口函数声明
//(1)PWM对外接口函数声明
//========================================================================
//函数名称：timer_pwm_init
//函数功能：对指定的TIMER通道进行PWM初始化
//函数参数：TIMERx_CHy: Timer号_通道号（可用宏定义作为函数实参，
//                      TIMER1_CH1~TIMER1_CH4，TIMER2_CH1~TIMER2_CH4，
//                      TIMER3_CH1~TIMER3_CH4，TIMER4_CH1~TIMER4_CH4）
//          pol:   PWM极性选择（可用宏定义作为函数实参，PWM_P为正极性，PWM_N为负极性）
//          align: PWM对齐方式选择（可用宏定义作为函数实参，
//                 PWM_EDGE为边沿对齐，PWM_CENTER为中心对齐）
//          period: PWM周期，单位为us，合理范围为100~65535 us
//          duty:   高电平的占空比（用0~100表示0%~100%）
//函数返回：无
//相关说明：在TIMER时钟频率（计数频率）为1MHz时，计数周期为1us。为了能实现100%的
//          占空比，对于16位TIMER，PWM周期最大值取65535 us。
//          为了能实现1%的占空比，PWM周期最小值为100us，此时对应的计数次数为1次。
//函数调用示例：
//   初始化TIMER2_CH1通道PWM，正极性、边沿对齐、周期为200us、占空比为50%
//   timer_pwm_init(TIMER2_CH1, PWM_P, PWM_EDGE, 200, 50);
//========================================================================
void timer_pwm_init(uint16 TIMERx_CHy, uint8 pol, uint8 align, uint32 period, uint8 duty);

//========================================================================
//函数名称：timer_pwm_update
//函数功能：更新指定的PWM通道输出高电平的占空比
//函数参数：TIMERx_CHy: Timer号_通道号（可用宏定义作为函数实参，
//                      TIMER1_CH1~TIMER1_CH4，TIMER2_CH1~TIMER2_CH4，
//                      TIMER3_CH1~TIMER3_CH4，TIMER4_CH1~TIMER4_CH4）
//          duty: 高电平的占空比（用0~100表示0%~100%）
//函数返回：无
//函数调用示例：
//   timer_pwm_update(TIMER2_CH1, 30);  //将TIMER2_CH1通道PWM输出高电平占空比更新为30%
//========================================================================
void timer_pwm_update(uint16 TIMERx_CHy, uint8 duty);


//(2)输入捕获INCAP对外接口函数声明
//========================================================================
//函数名称：timer_incap_init
//函数功能：对指定的Timer通道进行输入捕获初始化
//函数参数：TIMERx_CHy: TIMER号_通道号（可用宏定义作为函数实参，
//                      TIMER1_CH1~TIMER1_CH4，TIMER2_CH1~TIMER2_CH4，
//                      TIMER3_CH1~TIMER3_CH4，TIMER4_CH1~TIMER4_CH4）
//         capmode:     输入捕获模式（可用宏定义作为函数实参，
//                      CAP_UP为上升沿捕获，CAP_DOWN为下降沿捕获）
//函数返回：无
//相关说明：
//  (1)在TIMER时钟频率（计数频率）为1MHz时，计数周期为1us。16位TIMER的计数器
//     寄存器的最大值为65535，其溢出周期最大为65535us。因此，可测量的脉冲输入信号
//     最大周期不宜超过65535us。
//  (2)当输入的脉冲信号周期小于定时器的溢出周期时，
//     脉冲信号的周期或脉宽 = 对应的计数次数 * TIMER时钟周期 
//                          = 对应的计数次数 / TIMER时钟频率
//函数调用示例：初始化TIMER3_CH2通道为输入捕获功能，并采用上升沿捕获
//   timer_incap_init(TIMER3_CH2, CAP_UP);  
//========================================================================
void timer_incap_init(uint16 TIMERx_CHy, uint8 capmode);

//========================================================================
//函数名称：timer_incap_mode
//函数功能：对指定的TIMER通道进行捕获模式选择
//函数参数：TIMERx_CHy: Timer号_通道号（可用宏定义作为函数实参，
//                      TIMER1_CH1~TIMER1_CH4，TIMER2_CH1~TIMER2_CH4，
//                      TIMER3_CH1~TIMER3_CH4，TIMER4_CH1~TIMER4_CH4）
//             capmode: 输入捕获模式（可用宏定义作为函数实参，
//                      CAP_UP为上升沿捕获，CAP_DOWN为下降沿捕获）
//函数返回：无
//函数调用示例：
//   timer_incap_mode(TIMER3_CH2, CAP_DOWN);   //为TIMER3_CH2通道选择下降沿捕获
//========================================================================
void timer_incap_mode(uint16 TIMERx_CHy, uint8 capmode);

//========================================================================
//函数名称：timer_incap_get_value
//函数功能：获取TIMERx_CHy通道值寄存器的值（通道输入捕获值）
//函数参数：TIMERx_CHy: Timer号_通道号（可用宏定义作为函数实参，
//                      TIMER1_CH1~TIMER1_CH4，TIMER2_CH1~TIMER2_CH4，
//                      TIMER3_CH1~TIMER3_CH4，TIMER4_CH1~TIMER4_CH4）
//函数返回：TIMERx_CHy通道值寄存器的值（通道输入捕获值）
//相关说明：当TIMER发生通道事件（通道引脚捕获到输入信号的跳变沿）时，
//          计数器CNT的值将锁存到通道值寄存器CCRx中。
//函数调用示例：
//   获取TIMER3_CH2通道的输入捕获值，并将其保存至变量cap_value中
//   cap_value = timer_incap_get_value(TIMER3_CH2); 
//========================================================================
uint16 timer_incap_get_value(uint16 TIMERx_CHy);

//========================================================================
//函数名称：timer_ch_int_enable
//函数功能：使能TIMERx_CHy的通道中断
//函数参数：TIMERx_CHy: Timer号_通道号（可用宏定义作为函数实参，
//                      TIMER1_CH1~TIMER1_CH4，TIMER2_CH1~TIMER2_CH4，
//                      TIMER3_CH1~TIMER3_CH4，TIMER4_CH1~TIMER4_CH4）
//函数返回：无
//相关说明：当TIMER通道捕获到输入信号跳变沿（输入）时，将生成通道事件，通道标志
//          CCxIF由硬件置1。
//          若使能TIMER中断和通道中断，则当通道标志CCxIF=1时产生通道中断。
//函数调用示例：
//   timer_ch_int_enable(TIMER3_CH2);    //使能TIMER3_CH2通道中断
//========================================================================
void timer_ch_int_enable(uint16 TIMERx_CHy);

//========================================================================
//函数名称：timer_ch_int_disable
//函数功能：禁止TIMERx_CHy的通道中断
//函数参数：TIMERx_CHy: Timer号_通道号（可用宏定义作为函数实参，
//                      TIMER1_CH1~TIMER1_CH4，TIMER2_CH1~TIMER2_CH4，
//                      TIMER3_CH1~TIMER3_CH4，TIMER4_CH1~TIMER4_CH4）
//函数返回：无
//函数调用示例：
//   timer_ch_int_disable(TIMER3_CH2);   //禁止TIMER3_CH2通道中断
//========================================================================
void timer_ch_int_disable(uint16 TIMERx_CHy);

//========================================================================
//函数名称：timer_chf_get
//函数功能：获取TIMERx_CHy通道标志的值
//函数参数：TIMERx_CHy: Timer号_通道号（可用宏定义作为函数实参，
//                      TIMER1_CH1~TIMER1_CH4，TIMER2_CH1~TIMER2_CH4，
//                      TIMER3_CH1~TIMER3_CH4，TIMER4_CH1~TIMER4_CH4）
//函数返回：1表示有通道事件发生，0表示无通道事件发生
//函数调用示例：
//    timer_chf_get(TIMER3_CH2);       //获取TIMER3_CH2通道标志值
//========================================================================
uint8 timer_chf_get(uint16 TIMERx_CHy);

//========================================================================
//函数名称：timer_chf_clear
//函数功能：清除TIMERx_CHy通道标志
//函数参数：TIMERx_CHy: Timer号_通道号（可用宏定义作为函数实参，
//                      TIMER1_CH1~TIMER1_CH4，TIMER2_CH1~TIMER2_CH4，
//                      TIMER3_CH1~TIMER3_CH4，TIMER4_CH1~TIMER4_CH4）
//函数返回：无
//相关说明：TIMER通道用于输入捕获时，可通过调用本函数（对通道标志位CCxIF写0）或
//          timer_incap_get_value函数（读取通道值寄存器CCRx），对通道标志CCxIF清零
//函数调用示例：
//   timer_chf_clear(TIMER3_CH2);     //清除TIMER3_CH2通道标志
//========================================================================
void timer_chf_clear(uint16 TIMERx_CHy);

#endif                 //防止重复定义（结尾）
