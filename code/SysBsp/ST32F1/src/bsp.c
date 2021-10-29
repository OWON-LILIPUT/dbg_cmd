
#include "bsp.h"
#include "main.h"

#include <stm32f10x_lib.h> 

//*****************************************************************************
//
// Macros for hardware access, both direct and via the bit-band region.
//
//*****************************************************************************

#define GPIO_ClrBits(m,n) GPIO_ResetBits(m,n)


/************************************************************************/
/*      Global Variables                                                */
/************************************************************************/

/************************************************************************/
/*      static Variables                                                */
/************************************************************************/


/************************************************************************/
/*      Local Prototypes                                                */
/************************************************************************/

/************************************************************************/
/*      Local Functions                                                 */
/************************************************************************/
/* Mcu Reset */
void mcu_software_reset(void)
{
    NVIC_SETPRIMASK();   //�ر����ж�
    NVIC_GenerateSystemReset();
}

static void mcu_rcc_cfg_init(void)
{
    RCC_DeInit();
    RCC_HSEConfig(RCC_HSE_ON);
    if (RCC_WaitForHSEStartUp() == SUCCESS) {
        FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
        FLASH_SetLatency(FLASH_Latency_2);
        RCC_HCLKConfig(RCC_SYSCLK_Div1);
        RCC_PCLK1Config(RCC_HCLK_Div2);
        RCC_PCLK2Config(RCC_HCLK_Div1);
        RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);/* PLLCLK = 8MHz * 9 = 72 MHz */
        RCC_PLLCmd(ENABLE);
        while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
        RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
        while (RCC_GetSYSCLKSource() != 0x08);
    }
#ifdef  VECT_TAB_RAM
    NVIC_SetVectorTable(NVIC_VectTab_RAM,   0x0);
#else
    NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0);
#endif
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
}

/* Nvic �����ж����ȼ� */
enum nvic_name_e{
    NVIC_UART1,
    NVIC_TIM3,
    NVIC_END,
};
struct nvic_cfg_s{
    enum nvic_name_e       IRQFlag;
    NVIC_InitTypeDef       NVIC_Init;
};


const struct nvic_cfg_s nvic_cfg_tbl[] = {
    //            �ж�ID               ��ռ�� �����ȼ� ʹ��
    {NVIC_UART1,   USART1_IRQChannel,       2,   3,     ENABLE },
    {NVIC_TIM3,    TIM3_IRQChannel,         3,   1,     ENABLE },
    {NVIC_END,     0,                       0,   0,     DISABLE},
};

static void mcu_nvic_init(void)
{
    /* ����ԽС���ȼ�Խ��Խ����ռ����Ӧ���� ���ȼ�һ��ʱִ������˳��
    ---------------------------pre-emption----subpriority--
       NVIC_PriorityGroup_0  0:bit��ռ���ȼ� 4bit��Ӧ���ȼ�
       NVIC_PriorityGroup_1  1:bit��ռ���ȼ� 3bit��Ӧ���ȼ� (ʹ��)
       NVIC_PriorityGroup_2  2:bit��ռ���ȼ� 2bit��Ӧ���ȼ�
       NVIC_PriorityGroup_3  3:bit��ռ���ȼ� 1bit��Ӧ���ȼ�
       NVIC_PriorityGroup_4  4:bit��ռ���ȼ� 0bit��Ӧ���ȼ�
    */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//�ȷ��������� �����ø��¼����ȼ���
#if 0
    /* XXX �ж����� */
    NVIC_InitStructure.NVIC_IRQChannel                   = nvic_cfg_tbl[NVIC_UART1].NVIC_Init.NVIC_IRQChannel;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = nvic_cfg_tbl[NVIC_UART1].NVIC_Init.NVIC_IRQChannelPreemptionPriority;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = nvic_cfg_tbl[NVIC_UART1].NVIC_Init.NVIC_IRQChannelSubPriority;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = nvic_cfg_tbl[NVIC_UART1].NVIC_Init.NVIC_IRQChannelCmd;
    NVIC_Init(&NVIC_InitStructure);
#endif
}

/* System Timer Tick */
volatile unsigned long Tim3Cnt = 0;
void TIM3_IRQHandler(void)// 1mS Fcnt = 12MHz
{
    Tim3Cnt++;
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET ) { /*���TIM3�����жϷ������*/
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update); /*���TIMx�����жϱ�־ */
        admin_1ms_thread_isr();
    }
}

static void tim3_int_init(int arr, int psc)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); /* ʱ��ʹ�� */
    /* TIM3 �ж����� */
    NVIC_InitStructure.NVIC_IRQChannel                   = nvic_cfg_tbl[NVIC_TIM3].NVIC_Init.NVIC_IRQChannel;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = nvic_cfg_tbl[NVIC_TIM3].NVIC_Init.NVIC_IRQChannelPreemptionPriority;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = nvic_cfg_tbl[NVIC_TIM3].NVIC_Init.NVIC_IRQChannelSubPriority;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = nvic_cfg_tbl[NVIC_TIM3].NVIC_Init.NVIC_IRQChannelCmd;
    NVIC_Init(&NVIC_InitStructure);
    /*��ʱ��TIM3��ʼ��*/
    TIM_TimeBaseStructure.TIM_Period = arr - 1; /*���ڵ�ֵ*/
    TIM_TimeBaseStructure.TIM_Prescaler = psc - 1; /* Ԥ��Ƶֵ */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; /*����ʱ�ӷָ�:TDTS = Tck_tim*/
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; /* TIM���ϼ���ģʽ*/
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); /*����ָ���Ĳ�����ʼ��TIMx��ʱ�������λ*/
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE); /*ʹ��ָ����TIM3�ж�,��������ж�*/
    TIM_Cmd(TIM3, ENABLE); /*ʹ��TIMx*/
}


/* Uart 0 */
static void usart1_init(unsigned long baudrate)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    GPIO_InitStructure.GPIO_Pin   =  GPIO_Pin_9;             //PA9
    GPIO_InitStructure.GPIO_Speed =  GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  =  GPIO_Mode_AF_PP;        //�����������
    GPIO_Init(GPIOA, &GPIO_InitStructure);                   //TX��ʼ��
    GPIO_InitStructure.GPIO_Pin   =  GPIO_Pin_10;            //PA10
    GPIO_InitStructure.GPIO_Mode  =  GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    /* Enable the USART1 Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel                   = nvic_cfg_tbl[NVIC_UART1].NVIC_Init.NVIC_IRQChannel;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = nvic_cfg_tbl[NVIC_UART1].NVIC_Init.NVIC_IRQChannelPreemptionPriority;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = nvic_cfg_tbl[NVIC_UART1].NVIC_Init.NVIC_IRQChannelSubPriority;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = nvic_cfg_tbl[NVIC_UART1].NVIC_Init.NVIC_IRQChannelCmd;
    NVIC_Init(&NVIC_InitStructure);
    //USART_InitStructure.USART_BaudRate = 921600;
    USART_InitStructure.USART_BaudRate   = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits   = USART_StopBits_1;
    USART_InitStructure.USART_Parity     = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;//��Rx���պ�Tx���͹���
    USART_Init(USART1, &USART_InitStructure);                    //��ʼ��
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);  //�������ж�
    USART_Cmd(USART1, ENABLE);                                   //��������
}

volatile unsigned long uart1_cnt = 0,txd1_cnt = 0, rxd1_cnt = 0;
void USART1_IRQHandler(void)
{
    //�����ַ�����
    uart1_cnt++;
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {     //�жϷ��������ж�
        char rxd;
        USART_ClearITPendingBit(USART1,   USART_IT_RXNE);        //����жϱ�־
        rxd = USART_ReceiveData(USART1) & 0xFF;
        uart_rxd1_thread_isr(rxd);
        rxd1_cnt++;
    }
    if (USART_GetITStatus(USART1, USART_IT_TXE) != RESET) {
        USART_ClearITPendingBit(USART1, USART_IT_TXE);
        uart_txd1_thread_isr();
        txd1_cnt++;
    }
}
void uart1_txd_int_en(char en)
{
    if (en) {
        USART_ITConfig(USART1, USART_IT_TXE, ENABLE);  //�������Ϳ����ж�
    } else {
        USART_ITConfig(USART1, USART_IT_TXE, DISABLE); //�������ڶ��������ݹرշ����ж�
    }
}
void uart1_send_wait(char ch)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET); // ���ͻ���Ĵ����Ƿ�Ϊ��
    USART_SendData(USART1, ch);
}
void uart1_send(char send_data)
{
    //ʹ�÷��Ϳ��ж� ֱ��д�����ݷ���
    USART_SendData(USART1, send_data);
}

////////////////////////////////////////////////////////////////////////////////////
enum gpio_name_e{
    PIN_KEY1,
    PIN_KEY2,
    PIN_BEEP,
    PIN_LAMP,
    PIN_END,
};
struct gpio_cfg_s {
    enum gpio_name_e  GpioFlag;
    int               GpioEn;  //�Ƿ�ʹ�ܹܽų�ʼ��
    int               UpPowVal;//�ϵ�����״̬
    GPIO_TypeDef     *GPIO_PRT;
    GPIO_InitTypeDef  GPIO_INIT;
};

const struct gpio_cfg_s gpio_cfg_tbl[] = {
    {PIN_KEY1        , 0  , 0, GPIOA , GPIO_Pin_0  , GPIO_Speed_50MHz , GPIO_Mode_IN_FLOATING},
    {PIN_KEY2        , 0  , 0, GPIOA , GPIO_Pin_1  , GPIO_Speed_50MHz , GPIO_Mode_IN_FLOATING},
    {PIN_BEEP        , 1  , 1, GPIOB , GPIO_Pin_10 , GPIO_Speed_50MHz , GPIO_Mode_Out_OD     }, //����III�ſ�����ܽ�
    {PIN_LAMP        , 0  , 0, GPIOE , GPIO_Pin_8  , GPIO_Speed_50MHz , GPIO_Mode_Out_PP     },
    {PIN_END         , 0  , 0, 0     , 0           , GPIO_Speed_50MHz , GPIO_Mode_IN_FLOATING}
};

static void mcu_gpio_cfg_init(void)
{
    int i;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
#ifdef MCU_STM32F3_2KB
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
#endif
    AFIO->MAPR = (0x00FFFFFF & AFIO->MAPR) | 0x04000000; //�ر�JTAG
    for (i = 0; gpio_cfg_tbl[i].GPIO_PRT; i++) {
        if (gpio_cfg_tbl[i].GpioEn) {
            if ((gpio_cfg_tbl[i].GPIO_INIT.GPIO_Mode == GPIO_Mode_Out_OD) || (gpio_cfg_tbl[i].GPIO_INIT.GPIO_Mode == GPIO_Mode_Out_PP)) {
                if (gpio_cfg_tbl[i].UpPowVal) {
                    GPIO_SetBits(gpio_cfg_tbl[i].GPIO_PRT, gpio_cfg_tbl[i].GPIO_INIT.GPIO_Pin);
                } else {
                    GPIO_ClrBits(gpio_cfg_tbl[i].GPIO_PRT, gpio_cfg_tbl[i].GPIO_INIT.GPIO_Pin);
                }
            }
            GPIO_Init(gpio_cfg_tbl[i].GPIO_PRT, (GPIO_InitTypeDef *)&gpio_cfg_tbl[i].GPIO_INIT);
        }
    }
}

/* InPort */
int GetKey1(void)
{
    if (gpio_cfg_tbl[PIN_KEY1].GpioEn) {
        if (GPIO_ReadInputDataBit(gpio_cfg_tbl[PIN_KEY1].GPIO_PRT, gpio_cfg_tbl[PIN_KEY1].GPIO_INIT.GPIO_Pin) == 0) {
            return 1;
        }
    }
    return 0;
}

int GetKey2(void)
{
    if (gpio_cfg_tbl[PIN_KEY2].GpioEn) {
        if (GPIO_ReadInputDataBit(gpio_cfg_tbl[PIN_KEY2].GPIO_PRT, gpio_cfg_tbl[PIN_KEY2].GPIO_INIT.GPIO_Pin) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Out Port*/
void SetBeepIO(int val)
{
    if (gpio_cfg_tbl[PIN_BEEP].GpioEn) {
        if (val) {
            GPIO_ClrBits(gpio_cfg_tbl[PIN_BEEP].GPIO_PRT, gpio_cfg_tbl[PIN_BEEP].GPIO_INIT.GPIO_Pin);
        } else {
            GPIO_SetBits(gpio_cfg_tbl[PIN_BEEP].GPIO_PRT, gpio_cfg_tbl[PIN_BEEP].GPIO_INIT.GPIO_Pin);
        }
    }
}

void SetLampIO(int val)
{
    if (gpio_cfg_tbl[PIN_LAMP].GpioEn) {
        if (val) {
            GPIO_ClrBits(gpio_cfg_tbl[PIN_LAMP].GPIO_PRT, gpio_cfg_tbl[PIN_LAMP].GPIO_INIT.GPIO_Pin);
        } else {
            GPIO_SetBits(gpio_cfg_tbl[PIN_LAMP].GPIO_PRT, gpio_cfg_tbl[PIN_LAMP].GPIO_INIT.GPIO_Pin);
        }
    }
}

/************************************************************************/
/*     dbg_cmd Interface                                                */
/************************************************************************/
#include "dbg_cmd.h"
#ifdef DBG_CMD_EN
static void mcu_bsp_msg(void)
{
    RCC_ClocksTypeDef Clock;
    RCC_GetClocksFreq(&Clock);
    DBG_CMD_PRN("SYSCLK_Frequency: %dMHz\r\n", Clock.SYSCLK_Frequency / 1000000u);
    DBG_CMD_PRN("HCLK_Frequency  : %dMHz\r\n", Clock.HCLK_Frequency   / 1000000u);
    DBG_CMD_PRN("PCLK2_Frequency : %dMHz\r\n", Clock.PCLK2_Frequency  / 1000000u);
    DBG_CMD_PRN("PCLK1_Frequency : %dMHz\r\n", Clock.PCLK1_Frequency  / 1000000u);
    DBG_CMD_PRN("ADCCLK_Frequency: %dMHz\r\n", Clock.ADCCLK_Frequency / 1000000u);
}

static void int_cnt_msg(void)
{
    DBG_CMD_PRN("SysTick  :%lu\r\n", Tim3Cnt);
    DBG_CMD_PRN("txd1_cnt :%lu\r\n", txd1_cnt);
    DBG_CMD_PRN("rxd1_cnt :%lu\r\n", rxd1_cnt);
    DBG_CMD_PRN("uart1_cnt:%lu\r\n", uart1_cnt);
}
static bool dbg_cmd_func()
{
    if (dbg_cmd_exec("help", "", "")) {
        DBG_CMD_PRN(".McuBsp\r\n");
        return false;
    }
    if (dbg_cmd_exec(".mcubsp", "", "")) {
        dbg_cmd_print_msg_en();
    }
    if (dbg_cmd_exec("mcubspmsg", "", "")) {
        mcu_bsp_msg();
        return true;
    }
    if (dbg_cmd_exec("intcntmsg", "", "")) {
        int_cnt_msg();
        return true;
    }
    if (dbg_cmd_exec("reset", "", "")) {
        mcu_software_reset();
        return true;
    }
    return false;
}
#endif
/************************************************************************/
/*      Application Interface                                           */
/************************************************************************/
void mcu_bsp_init(void)
{
    mcu_rcc_cfg_init();
    mcu_nvic_init();     //�����Ƚ������ȼ������
    mcu_gpio_cfg_init();
    tim3_int_init(6000, 12); // 1mS
    usart1_init(115200);
#ifdef DBG_CMD_EN
    dbg_cmd_add_list((CMD_FUNC_T)dbg_cmd_func);
#endif
}

