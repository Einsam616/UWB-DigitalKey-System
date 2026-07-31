//=======================================================================
//文件名称：gpio.c
//功能概要：GPIO底层驱动构件源文件
//芯片类型：STM32F103C8T6
//版权所有：江苏电子-索明何
//版本更新：2023-01-16  V1.0
//=======================================================================
//1.包含本软件构件头文件
#include  "gpio.h"

//2.定义仅用于本文件的全局变量，内部函数声明
//(1)定义GPIO结构体指针数组，存放GPIO基地址
static GPIO_TypeDef  *GPIO_ARR[] = { GPIOA, GPIOB, GPIOC, GPIOD };

//(2)内部函数声明
static void gpio_port_pin(uint16 port_pin, uint8 *port, uint8 *pin);

//3.对外接口函数的定义与实现
//=======================================================================
//函数名称：gpio_init
//函数功能：初始化指定端口引脚为GPIO功能，并设定引脚方向为输入或输出；
//          若为输出，还要设置引脚的输出状态
//函数参数：port_pin: (端口号|引脚号)，如(PTB|5)表示B端口5号引脚
//          dir:   引脚方向（可用宏定义作为函数实参，GPIO_IN为输入，GPIO_OUT为输出）
//          state: 引脚的输出状态（0为低电平，1为高电平）；
//	               引脚方向为输入时，可为0或1	
//函数返回：无
//相关说明：
// (1)可用的GPIO引脚：PA0~PA12、PA15，PB0、PB1、PB3~PB15，PC13~PC15，PD0~PD1 
//    注：PA13、PA14分别用于SWD_DIO、SWD_CLK；PB2用于BOOT1引脚。 
//        若使用内部时钟，则PD0、PD1也可用作GPIO引脚。
// (2)输出采用推挽方式和默认的中速方式
//=======================================================================
void gpio_init(uint16 port_pin, uint8 dir, uint8 state)
{
    //1.定义局部变量
    GPIO_TypeDef  *gpio_ptr;       //GPIO结构体指针，存放引脚对应的GPIO基地址
    uint8  port, pin;              //分别存放端口号和引脚号
	
    //2.解析出port_pin对应的端口号port和引脚号pin，获取对应的GPIO基地址
    gpio_port_pin(port_pin, &port, &pin); 
    gpio_ptr = GPIO_ARR[port];	
	
    //3.使能相应GPIO端口时钟（GPIO使用APB2总线时钟）
    RCC->APB2ENR |= 1 << (port+2);	
	
    //4.禁用JTAG调试，使能SWD调试（使JTAG的PA15、PB3、PB4引脚用作GPIO功能）
    if( port_pin == (PTA|15) || port_pin == (PTB|3) || port_pin == (PTB|4) )
    {
        //1)使能复用功能AFIO时钟（AFIO使用APB2总线时钟）			
        RCC->APB2ENR |= RCC_APB2ENR_AFIOEN; 			
        //2)配置复用功能重映射配置寄存器AFIO_MAPR
        AFIO->MAPR &= ~AFIO_MAPR_SWJ_CFG;              //清除SWJ_CFG位
        AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;   //设置SWJ_CFG位
    }	
		
    //5.若使用内部时钟，则外部时钟引脚PD0(OSC_IN)、PD1(OSC_OUT)可用作GPIO功能
    if( port_pin == (PTD|0) || port_pin == (PTD|1) )
    {
        //1)使能复用功能AFIO时钟（AFIO使用APB2总线时钟）
        RCC->APB2ENR |= RCC_APB2ENR_AFIOEN; 			
        //2)配置复用功能重映射配置寄存器AFIO_MAPR			
        AFIO->MAPR &= ~AFIO_MAPR_PD01_REMAP;       //清除PD01_REMAP位
        AFIO->MAPR |=  AFIO_MAPR_PD01_REMAP;       //设置PD01_REMAP位
    }	
		
    //6.配置寄存器GPIO_CRL和GPIO_CRH，根据参数dir设置引脚方向
    if(pin <= 7)
    {	      
        gpio_ptr->CRL &= ~(3 << (pin*4));	        //清除对应引脚的MODE位
        gpio_ptr->CRL |= dir << (pin*4);          //设置对应引脚的MODE位	
    }
    else if(pin >= 8 && pin <= 15)
    {	
        gpio_ptr->CRH &= ~(3 << (pin-8)*4);	      //清除对应引脚的MODE位
        gpio_ptr->CRH |= dir << (pin-8)*4;        //设置对应引脚的MODE位		
    }
		
    //7.若引脚方向为输出，则根据参数state设置引脚的输出状态
    if(dir == GPIO_OUT)
    {
        //1)配置寄存器GPIO_CRL和GPIO_CRH，设置引脚为推挽输出
        if(pin <= 7)
            gpio_ptr->CRL &= ~(3 << (pin*4 + 2));     //清除对应引脚的CNF位
        else if(pin >= 8 && pin <= 15)
            gpio_ptr->CRH &= ~(3 << ((pin-8)*4 + 2));	//清除对应引脚的CNF位
        //2)配置寄存器GPIO_BRR和GPIO_BSRR，设置引脚输出状态				
        if(state == 0)            
            gpio_ptr->BRR  |= 1 << pin;               //对应引脚输出低电平      
        else                      
            gpio_ptr->BSRR |= 1 << pin; 	            //对应引脚输出高电平		
    }
}

//=======================================================================
//函数名称：gpio_set
//函数功能：在初始化指定端口引脚为GPIO输出功能后，设置指定引脚的输出状态
//函数参数：port_pin: (端口号|引脚号)，如(PTB|5)表示B端口5号引脚
//          state:    引脚的输出状态（0为低电平，1为高电平）
//函数返回：无
//=======================================================================
void gpio_set(uint16 port_pin, uint8 state)
{
    //1.定义局部变量
    GPIO_TypeDef  *gpio_ptr;      //GPIO结构体指针，存放引脚对应的GPIO基地址
    uint8  port, pin;             //分别存放端口号和引脚号
	
    //2.解析出port_pin对应的端口号port和引脚号pin，获取对应的GPIO基地址
    gpio_port_pin(port_pin, &port, &pin); 
    gpio_ptr = GPIO_ARR[port];	
	
    //3.根据参数state设置引脚的输出状态
    if(state == 0)            
        gpio_ptr->BRR  = 1 << pin;    //对应引脚输出低电平  
    else                      
        gpio_ptr->BSRR = 1 << pin;    //对应引脚输出高电平
}

//=======================================================================
//函数名称：gpio_get
//函数功能：在初始化指定端口引脚为GPIO输入功能后，获取指定引脚的输入状态
//函数参数：port_pin: (端口号|引脚号)，如(PTB|5) 表示B端口5号引脚
//函数返回：指定端口引脚的输入状态（0或1）
//=======================================================================
uint8 gpio_get(uint16 port_pin)
{
    //1.定义局部变量
    GPIO_TypeDef  *gpio_ptr;        //GPIO结构体指针，存放引脚对应的GPIO基地址
    uint8  port, pin;               //分别存放端口号和引脚号
    uint8  value;                   //存放引脚的状态
	
    //2.解析出port_pin对应的端口号port和引脚号pin，获取对应的GPIO基地址
    gpio_port_pin(port_pin, &port, &pin); 
    gpio_ptr = GPIO_ARR[port];	
	
    //3.获取并返回引脚的状态
    value = (uint8)(BGET(gpio_ptr->IDR, pin));  //读取端口输入数据寄存器对应引脚位
    return  value;
}

//=======================================================================
//函数名称：gpio_reverse
//函数功能：在初始化指定端口引脚为GPIO输出功能后，反转引脚的输出状态
//函数参数：port_pin: (端口号|引脚号)，如(PTB|5)表示B端口5号引脚
//函数返回：无
//=======================================================================
void gpio_reverse(uint16 port_pin)
{
    //1.定义局部变量
    GPIO_TypeDef  *gpio_ptr;      //GPIO结构体指针，存放引脚对应的GPIO基地址
    uint8  port, pin;             //分别存放端口号和引脚号
	
    //2.解析出port_pin对应的端口号port和引脚号pin，获取对应的GPIO基地址
    gpio_port_pin(port_pin, &port, &pin); 
    gpio_ptr = GPIO_ARR[port];
	
    //3.反转指定引脚输出状态
    if(BGET(gpio_ptr->ODR, pin) == 0)    //若引脚输出低电平时，则反转为高电平
        gpio_ptr->BSRR |= 1 << pin;       			
    else                                 //若引脚输出高电平时，则反转为低电平
        gpio_ptr->BRR  |= 1 << pin; 			
}

//=======================================================================
//函数名称：gpio_pull
//函数功能：在初始化指定端口引脚为GPIO输入功能后，选择引脚内部上拉/下拉电阻
//函数参数：port_pin:   (端口号|引脚号)，如(PTB|5)表示B端口5号引脚
//          pull_select: 引脚内部上拉/下拉选择（可用宏定义作为函数实参，PULL_UP为上拉，
//                       PULL_DWON为下拉）
//函数返回：无
//相关说明：STM32F103C8T6的所有GPIO引脚都有内部上拉电阻和下拉电阻
//=======================================================================
void gpio_pull(uint16 port_pin, uint8 pull_select)
{
    //1.定义局部变量
    GPIO_TypeDef  *gpio_ptr;      //GPIO结构体指针，存放引脚对应的GPIO基地址
    uint8  port, pin;             //分别存放端口号和引脚号
	
    //2.解析出port_pin对应的端口号port和引脚号pin，获取对应的GPIO基地址
    gpio_port_pin(port_pin, &port, &pin); 
    gpio_ptr = GPIO_ARR[port];	
	
    //3.根据pull_select设置引脚使用输入上拉或下拉
    //1)配置寄存器GPIO_CRL和GPIO_CRH，设置引脚为输入上拉/下拉
    if(pin <= 7)
    {			
        gpio_ptr->CRL &= ~(3 << (pin*4 + 2));       //清除对应引脚的CNF位
        gpio_ptr->CRL |=   2 << (pin*4 + 2);        //设置对应引脚的CNF位
    }
    else if(pin >= 8 && pin <= 15)
    {
        gpio_ptr->CRH &= ~(3 << ((pin-8)*4 + 2));   //清除对应引脚的CNF位
        gpio_ptr->CRH |=   2 << ((pin-8)*4 + 2);    //设置对应引脚的CNF位
    }
    //2)配置寄存器GPIO_BSRR和GPIO_BRR，选择上拉或下拉电阻
    if(pull_select == PULL_UP)                      //选用上拉电阻
        gpio_ptr->BSRR |= 1 << pin;                 //设置对应引脚位   
    else                                            //选用下拉电阻
        gpio_ptr->BRR  |= 1 << pin; 	              //设置对应引脚位
}

//=======================================================================
//函数名称：gpio_out_speed
//函数功能：在初始化指定端口引脚为GPIO输出功能后，设置引脚输出速度
//函数参数：port_pin: (端口号|引脚号)，如(PTB|5) 表示B端口5号引脚
//          speed:    引脚输出速度选择（可用宏定义作为函数实参，LOW_SPEED为低速，
//                    MEDIUM_SPEED为中速，HIGH_SPEED为高速）
//函数返回：无
//=======================================================================
void gpio_out_speed(uint16 port_pin, uint8 speed)
{
    //1.定义局部变量
    GPIO_TypeDef  *gpio_ptr;      //GPIO结构体指针，存放引脚对应的GPIO基地址
    uint8  port, pin;             //分别存放端口号和引脚号
	
    //2.解析出port_pin对应的端口号port和引脚号pin，获取对应的GPIO基地址
    gpio_port_pin(port_pin, &port, &pin); 
    gpio_ptr = GPIO_ARR[port];	
	
    //3.配置寄存器GPIO_CRL和GPIO_CRH，根据参数speed设置引脚输出速度
    if(pin <= 7)
    {	
        gpio_ptr->CRL &= ~(3 << pin*4);	    //清除对应引脚的MODE位
        gpio_ptr->CRL |= speed << pin*4;    //设置对应引脚的MODE位
    }
    else if(pin >= 8 && pin <= 15)
    {			
        gpio_ptr->CRH &= ~(3 << pin*4);	    //清除对应引脚的MODE位
        gpio_ptr->CRH |= speed << pin*4;    //设置对应引脚的MODE位			
    }
}

//4.内部函数的定义与实现
//=======================================================================
//函数名称：gpio_port_pin
//函数功能：对传入参数port_pin进行解析，得出并传回引脚对应的端口号与引脚号
//          例如，(PTB|5)对应的端口号*port=1（PTB），引脚号*pin=5
//函数参数：port_pin: (端口号|引脚号)，如(PTB|5)表示B端口5号引脚
//          *port:    用于传回端口号（0~3对应PTA~PTD）
//		      *pin:     用于传回引脚号（0~15）
//=======================================================================
static void gpio_port_pin(uint16 port_pin, uint8 *port, uint8 *pin)
{
    *port = (uint8)(port_pin >> 8);     //获取端口号
    *pin = (uint8) port_pin;            //获取引脚号
}
