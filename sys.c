/*********************** (C) COPYRIGHT 2013 Libraworks *************************
* File Name	    : RTOS.c
* Author		: 卢晓铭 
* Version		: V1.0
* Date			: 01/26/2013
* Description	: LXM-RTOS 任务管理
*******************************************************************************/
 
#include"RTOS.h"
 
TaskCtrBlock TCB[OS_TASKS - 1];	/*任务控制块定义*/
TaskCtrBlock *p_OSTCBCur;		/*指向当前任务控制块的指针*/
TaskCtrBlock *p_OSTCBHighRdy;	/*指向最高优先级就绪任务控制块的指针*/
u8 OSPrioCur;					/*当前执行任务*/
u8 OSPrioHighRdy;				/*最高优先级*/
u8 OSRunning;					/*多任务运行标志0:为运行，1:运行*/
 
u32 OSInterruptSum;				/*进入中断次数*/
 
u32 OSTime;						/*系统时间(进入时钟中断次数)*/
 
u32 OSRdyTbl;					/*任务就绪表,0表示挂起,1表示就绪*/
 
u32 OSIntNesting;				/*任务嵌套数*/
 
/*在就绪表中登记任务*/
void OSSetPrioRdy(u8 prio)
{
	OSRdyTbl |= 1 << prio;
}
 
/*在就绪表中删除任务*/
void OSDelPrioRdy(u8 prio)
{
	OSRdyTbl &= ~(1<<prio);
}
 
/*在就绪表中查找更高级的就绪任务*/
void OSGetHighRdy(void)
{
	u8 OSNextTaskPrio = 0;		/*任务优先级*/
    //从左到右 优先级依次递增
	for (OSNextTaskPrio = 0; (OSNextTaskPrio < OS_TASKS) && (!(OSRdyTbl&(0x01<<OSNextTaskPrio))); OSNextTaskPrio++ );
	OSPrioHighRdy = OSNextTaskPrio;	
}
 
/*设置任务延时时间*/
void OSTimeDly(u32 ticks)
{
	if(ticks > 0)
	{
		OS_ENTER_CRITICAL();				//进入临界区
		OSDelPrioRdy(OSPrioCur);			//将任务挂起
		TCB[OSPrioCur].OSTCBDly = ticks;	//设置TCB中任务延时节拍数
		OS_EXIT_CRITICAL();					//退出临界区
		OSSched();
	}
}
 
/*定时器中断对任务延时处理函数*/
void TicksInterrupt(void)
{
	static u8 i;
 
	OSTime++;
	for(i=0;i<OS_TASKS;i++)
	{
		if(TCB[i].OSTCBDly)
		{
			TCB[i].OSTCBDly--;
			if(TCB[i].OSTCBDly==0)	//延时时钟到达
			{
				OSSetPrioRdy(i);	//任务重新就绪
			}	
		}
	}
}
 
//系统时钟中断服务函数
void SysTick_Handler(void)     //？？？
{
   OS_ENTER_CRITICAL();                             //进入临界区

   OSIntNesting++;                         //任务前套数

   OS_EXIT_CRITICAL();                  //退出临界区

   TicksInterrupt();          //任务延时时钟,延时到了重新就绪

   OSIntExit();                                   //在中断中处理任务调度
}
 
/*任务切换*/
void OSSched(void)                      //调度
{
	OSGetHighRdy();					//找出任务就绪表中优先级最高的任务
	if(OSPrioHighRdy!=OSPrioCur)	//如果不是当前运行任务，进行任务调度
	{
		p_OSTCBCur = &TCB[OSPrioCur];				//汇编中引用			
		p_OSTCBHighRdy = &TCB[OSPrioHighRdy];		//汇编中引用
		OSPrioCur = OSPrioHighRdy;	//更新OSPrio
		OSCtxSw();					//调度任务
	}
}
/*任务创建*/
void OSTaskCreate(void  (*Task)(void  *parg), void *parg, u32 *p_Stack, u8 TaskID)
{
	if(TaskID <= OS_TASKS)
	{
		*(p_Stack) = (u32)0x01000000L;					/*  xPSR                        */ 
	    *(--p_Stack) = (u32)Task;						/*  Entry Point of the task  任务入口地址   */
	    *(--p_Stack) = (u32)0xFFFFFFFEL;				/*  R14 (LR)  (init value will  */
	                                                                           
	    *(--p_Stack) = (u32)0x12121212L;				/*  R12                         */
	    *(--p_Stack) = (u32)0x03030303L;				/*  R3                          */
	    *(--p_Stack) = (u32)0x02020202L;				/*  R2                          */
	    *(--p_Stack) = (u32)0x01010101L;				/*  R1                          */
		*(--p_Stack) = (u32)parg;						/*  R0 : argument  输入参数     */
 
		*(--p_Stack) = (u32)0x11111111L;				/*  R11                         */
	    *(--p_Stack) = (u32)0x10101010L;				/*  R10                         */
	    *(--p_Stack) = (u32)0x09090909L;				/*  R9                          */
	    *(--p_Stack) = (u32)0x08080808L;				/*  R8                          */
	    *(--p_Stack) = (u32)0x07070707L;				/*  R7                          */
	    *(--p_Stack) = (u32)0x06060606L;				/*  R6                          */
	    *(--p_Stack) = (u32)0x05050505L;				/*  R5                          */
	    *(--p_Stack) = (u32)0x04040404L;				/*  R4                          */
 
		TCB[TaskID].OSTCBStkPtr = (u32)p_Stack;			/*保存堆栈地址*/
		TCB[TaskID].OSTCBDly = 0;						/*初始化任务延时*/
		OSSetPrioRdy(TaskID);							/*在任务就绪表中登记*/
	}
}
 
//挂起   可以是正在执行的任务 也可以是不在执行的任务但是加在   就绪表中                                挂起代表后面不执行 
void OSTaskSuspend(u8 prio)          
{
	OS_ENTER_CRITICAL();		/*进入临界区*/
	TCB[prio].OSTCBDly = 0;
	OSDelPrioRdy(prio);			/*挂起任务*/
	OS_EXIT_CRITICAL();			/*退出临界区*/
 
	if(OSPrioCur == prio)		/*挂起的任务为当前运行的任务*/            //越小优先级越高 OSRdyTbl低位优先级高
	{
		OSSched();				/*重新调度*/        //把当前任务调度成就绪队列中优先级最高得任务
	}
}

//就绪
void OSTaskResume(u8 prio)            
{
	OS_ENTER_CRITICAL();
	TCB[prio].OSTCBDly = 0;		/*设置任务延时时间为0*/
	OSSetPrioRdy(prio);			/*就绪任务*/
	OS_EXIT_CRITICAL();
 
	if(OSPrioCur > prio)		/*当前任务优先级小于恢复的任务优先级*/
	{
		OSSched();
	}
}
 
u32 IDELTASK_STK[32];
 
void OSStart(void)
{
	if(OSRunning == 0)    //只能执行一次
	{
		OSRunning = 1;
		
		OSTaskCreate(IdleTask, (void *)0, (u32 *)&IDELTASK_STK[31], IdelTask_Prio);	//创建空闲任务
 
		OSGetHighRdy();					/*获得最高级的就绪任务*/
		OSPrioCur = OSPrioHighRdy;		/*获得最高优先级就绪任务ID*/
		p_OSTCBCur = &TCB[OSPrioCur];
		p_OSTCBHighRdy = &TCB[OSPrioHighRdy];
		OSStartHighRdy();
	}
}
 
void OSIntExit(void)
{
	OS_ENTER_CRITICAL();
	
	if(OSIntNesting > 0)
		OSIntNesting--;
	if(OSIntNesting == 0)
	{
		OSGetHighRdy();					/*找出任务优先级最高的就绪任务*/
		if(OSPrioHighRdy!=OSPrioCur)	/*当前任务并非优先级最高的就绪任务*/
		{
			p_OSTCBCur = &TCB[OSPrioCur];
			p_OSTCBHighRdy = &TCB[OSPrioHighRdy];
			OSPrioCur = OSPrioHighRdy;
			OSIntCtxSw();				/*中断级任务调度*/
		}
	}
 
	OS_EXIT_CRITICAL();
}
 
/*系统空闲任务*/
void IdleTask(void *pdata)
{
	u32 IdleCount = 0;
	while(1)
	{
		IdleCount++;
	}
}
 
void OSTaskSwHook(void)
{
}