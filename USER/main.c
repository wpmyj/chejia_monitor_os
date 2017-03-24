#include "sys.h" 	
#include "delay.h"	
#include "led.h"
#include "includes.h"
#include "usart.h"
#include "simulate_usart.h"
#include "timer.h"
#include "sdio_sdcard.h"
#include "ILI93xx.h"
#include "gps.h"
#include "can.h"
#include "exfuns.h"
#include "ff.h"
#include "w25qxx.h"  
#include "gprs_jiankong.h"

/////////////////////////UCOSII任务设置///////////////////////////////////

//GPS任务
//设置任务优先级
#define UBLOX_TASK_PRIO       		2 
//设置任务堆栈大小
#define UBLOX_STK_SIZE  				512
//任务堆栈
OS_STK UBLOX_TASK_STK[UBLOX_STK_SIZE];
//任务函数
void ublox_task(void *pdata);

//传感器数据获取任务
//设置任务优先级
#define SENSOR_TASK_PRIO       			3 
//设置任务堆栈大小
#define SENSOR_STK_SIZE  					128
//任务堆栈
OS_STK SENSOR_TASK_STK[SENSOR_STK_SIZE];
//任务函数
void sensor_task(void *pdata);

//log输入设置
//设置任务优先级
#define LOGREV_TASK_PRIO       			8 
//设置任务堆栈大小
#define LOGREV_STK_SIZE  					128
//任务堆栈
OS_STK LOGREV_TASK_STK[LOGREV_STK_SIZE];
//任务函数
void logrev_task(void *pdata);

//LED1任务
//设置任务优先级
#define LED1_TASK_PRIO       			6 
//设置任务堆栈大小
#define LED1_STK_SIZE  					128
//任务堆栈
OS_STK LED1_TASK_STK[LED1_STK_SIZE];
//任务函数
void led1_task(void *pdata);

//LED0任务
//设置任务优先级
#define LED0_TASK_PRIO       			7 
//设置任务堆栈大小
#define LED0_STK_SIZE  		    		128
//任务堆栈	
OS_STK LED0_TASK_STK[LED0_STK_SIZE];
//任务函数
void led0_task(void *pdata);

//START 任务
//设置任务优先级
#define START_TASK_PRIO      			10 //开始任务的优先级设置为最低
//设置任务堆栈大小
#define START_STK_SIZE  				64
//任务堆栈	
OS_STK START_TASK_STK[START_STK_SIZE];
//任务函数
void start_task(void *pdata);	

int main(void)
 {
	u32 res,temp,ret;
	u32 dtsize,dfsize;
	 
	delay_init();	    //延时函数初始化
  NVIC_Configuration();	 
	LED_Init();		  	//初始化与LED连接的硬件接口
	TIM5_Configuration();
	uart_init(9600);
	Simulate_Usart();
	W25QXX_Init();				//初始化W25Q64
	TFTLCD_Init();
	CAN_Mode_Init(CAN_SJW_1tq,CAN_BS2_8tq,CAN_BS1_9tq,4,CAN_Mode_LoopBack);
//  while (SD_Init())
//	{
//		printf("%d\n",SD_Init());
//		printf("SD Card Error!");
//		delay_ms(500);
//	}		
//	show_sdcard_info();	//打印SD卡相关信息
	exfuns_init();							//为fatfs相关变量申请内存				 
 	res=f_mount(fs[1],"1:",1); 				//挂载FLASH.
 	 do
	{
		temp++;
 		res=exf_getfree("1:",&dtsize,&dfsize);//得到FLASH剩余容量和总容量
		delay_ms(200);		   
	}while(res&&temp<20);//连续检测20次
	
	if(res==0X0D)////文件系统不存在 FLASH磁盘,FAT文件系统错误,重新格式化FLASH
	{
		printf("Flash Disk Formatting...\n");	//格式� 疐LASH
		res=f_mkfs("1:",1,4096);//格式化FLASH,1,盘符;1,不需要引导区,8个扇区为1个簇
		if(res==0)
		{
			f_setlabel((const TCHAR *)"1:data");	//设置Flash磁盘的名字为：ALIENTEK
			printf("Flash Disk Format Finish\n");	//格式化完成
			res=exf_getfree("1:",&dtsize,&dfsize);//重新获取容量
		}
		else 
		printf("Flash Disk Format Error \n");	//格式化失败
	}
	if(0 == res)
	{
		printf("Flash Disk:   %d  KB", dfsize);
		temp=dtsize;
	}
	else 
		printf("Flash Fat Error!\n");	//格式化失败
  f_unlink("1:log.txt");
	ret = f_open(data_log,"1:log.txt",FA_CREATE_ALWAYS | FA_WRITE| FA_READ);

	Is_Enter_facotry();
	//Ublox_init();
	OSInit();   
 	OSTaskCreate(start_task,(void *)0,(OS_STK *)&START_TASK_STK[START_STK_SIZE-1],START_TASK_PRIO );//创建起始任务
	OSStart();	  	 
}
	  
//开始任务
void start_task(void *pdata)
{
  OS_CPU_SR cpu_sr=0;
	pdata = pdata; 
  OS_ENTER_CRITICAL();			//进入临界区(无法被中断打断)
  OSTaskCreate(ublox_task,(void *)0,(OS_STK*)&UBLOX_TASK_STK[UBLOX_STK_SIZE-1],UBLOX_TASK_PRIO);
	OSTaskCreate(sensor_task,(void *)0,(OS_STK*)&SENSOR_TASK_STK[SENSOR_STK_SIZE-1],SENSOR_TASK_PRIO);
	OSTaskCreate(logrev_task,(void *)0,(OS_STK*)&LOGREV_TASK_STK[LOGREV_STK_SIZE-1],LOGREV_TASK_PRIO);	
 	OSTaskCreate(led0_task,(void *)0,(OS_STK*)&LED0_TASK_STK[LED0_STK_SIZE-1],LED0_TASK_PRIO);						   
 	OSTaskCreate(led1_task,(void *)0,(OS_STK*)&LED1_TASK_STK[LED1_STK_SIZE-1],LED1_TASK_PRIO);	
	OSTaskSuspend(START_TASK_PRIO);	//挂起起始任务.
	OS_EXIT_CRITICAL();				//退出临界区(可以被中断打断)
}

//UBLOX任务
void ublox_task(void *pdata)
{	  
	while(1)
	{
		//Ublox_Analasis();
		delay_ms(100);
	}
}

//LED0任务
void sensor_task(void *pdata)
{	 	
	while(1)
	{
		//Can_test();
		delay_ms(900);
	}
}


//LED1任务
void led1_task(void *pdata)
{	  
	while(1)
	{
	printf("led1\n");
		LED1=0;
		delay_ms(300);
		LED1=1;
		delay_ms(300);
	};
}


//LED0任务
void led0_task(void *pdata)
{	 	
	while(1)
	{
		printf("led0\n");
		LED0=0;
		delay_ms(80);
		LED0=1;
		delay_ms(920);
	};
}

void logrev_task(void *pdata)
{	 	
	while(1)
	{
		//printf("led0\n");
		LED0=0;
		delay_ms(80);
		LED0=1;
		delay_ms(920);
	};
}






