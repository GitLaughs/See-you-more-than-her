#include "timx_callback.h"

/**************************************************************************
Function: Model aircraft remote control receiving interrupt
Input   : none
Output  : none
函数功能：航模遥控接收中断（定时器X捕获中断）
入口参数：无
返 回 值：无
**************************************************************************/
void REMOTE_TIM_IRQHandler(void)
{
	//Input the capture flag for channel,
	//the capture flag for the higher bits, and the overflow flag for the lower 6 bits
	//通道输入捕获标志，高两位做捕获标志，低6位做溢出标志
	static u8 RemoteCH1_CAPTURE_STA = 0;
	static u8 RemoteCH2_CAPTURE_STA = 0;
	static u8 RemoteCH3_CAPTURE_STA = 0;
	static u8 RemoteCH4_CAPTURE_STA = 0;

	static u16 RemoteCH1_CAPTURE_UPVAL;
	static u16 RemoteCH1_CAPTURE_DOWNVAL;

	static u16 RemoteCH2_CAPTURE_UPVAL;
	static u16 RemoteCH2_CAPTURE_DOWNVAL;

	static u16 RemoteCH3_CAPTURE_UPVAL;
	static u16 RemoteCH3_CAPTURE_DOWNVAL;

	static u16 RemoteCH4_CAPTURE_UPVAL;
	static u16 RemoteCH4_CAPTURE_DOWNVAL;
	
	//遥控数值的历史记录
	static REMOTER_t last_remoter;
	
	//偏差值,辅助计算捕获数值
	u32 CH1_Diff;
	u32 CH2_Diff;
	u32 CH3_Diff;
	u32 CH4_Diff;

	//通道突变滤波
    static u8 ch1_filter_times=0,ch2_filter_times=0,ch3_filter_times=0,ch4_filter_times=0;

    //连接航模遥遥控器后，需要推下前进杆，才可以正式航模控制小车
    //After connecting the remote controller of the model aircraft,
    //you need to push down the forward lever to officially control the car of the model aircraft
	static uint16_t state_filter=0;//状态滤波,防止航模控制模式误判
    if(remoter.ch2>1600&& remoter.check_count< Filter_Threshold &&Get_Control_Mode(_RC_Control)==0&&SysVal.Time_count>=CONTROL_DELAY)
    {
		state_filter++;
		if( state_filter > 250 ) //满足开启航模遥控超过一定的时间再开启航模控制,防止出现干扰信号.250没有具体单位,跟进入定时器中断频率有关.
		{
			Set_Control_Mode(_RC_Control);//设置为航模遥控模式 
		}     
    }
	else
	{
		state_filter = 0;//非航模遥控的判断条件,复位滤波值
	}
	
    //Channel 1 //通道一
    if ((RemoteCH1_CAPTURE_STA & 0X80) == 0)
    {
        if ( Get_CH1_State != RESET) //A capture event occurred on channel 1 //通道1发生捕获事件
        {
            Clear_CH1_State; //Clear the interrupt flag bit //清除中断标志位
            if (RemoteCH1_CAPTURE_STA & 0X40)	//A falling edge is caught //捕获到一个下降沿
            {
                RemoteCH1_CAPTURE_DOWNVAL = Get_CH1_CNT ; //Record the timer value at this point //记录下此时的定时器计数值
                if (RemoteCH1_CAPTURE_DOWNVAL < RemoteCH1_CAPTURE_UPVAL)
                {
                    CH1_Diff = 9999;
                }
                else
                    CH1_Diff = 0;
                remoter.ch1 = RemoteCH1_CAPTURE_DOWNVAL - RemoteCH1_CAPTURE_UPVAL + CH1_Diff;	//Time to get the total high level //得到总的高电平的时间
				
                if(abs(remoter.ch1-last_remoter.ch1)>500)
                {
                    ch1_filter_times++;
                    if( ch1_filter_times<=5 ) remoter.ch1=last_remoter.ch1; //Filter //滤波
                    else ch1_filter_times=0;
                }
                else
                {
                    ch1_filter_times=0;
                }
                last_remoter.ch1 = remoter.ch1;

                RemoteCH1_CAPTURE_STA = 0; //Capture flag bit to zero	//捕获标志位清零
				Set_CH1_Rising; //Set to rising edge capture //设置为上升沿捕获
            }
            else
            {
                //When the capture time occurs but not the falling edge, the first time the rising edge is captured, record the timer value at this time
                //发生捕获时间但不是下降沿，第一次捕获到上升沿，记录此时的定时器计数值
                RemoteCH1_CAPTURE_UPVAL = Get_CH1_CNT; //Obtain rising edge data //获取上升沿数据
                RemoteCH1_CAPTURE_STA |= 0X40; //The flag has been caught on the rising edge //标记已捕获到上升沿
                Set_CH1_Falling; //Set to Falling Edge Capture //设置为下降沿捕获
            }
        }
    }
    //Channel 2 //通道二
    if ((RemoteCH2_CAPTURE_STA & 0X80) == 0)
    {
        if ( Get_CH2_State != RESET)	//A capture event occurred on channel 2 //通道2发生捕获事件
        {
            Clear_CH2_State; //Clear the interrupt flag bit //清除中断标志位
            if (RemoteCH2_CAPTURE_STA & 0X40)	//A falling edge is caught //捕获到一个下降沿
            {
                RemoteCH2_CAPTURE_DOWNVAL = Get_CH2_CNT; //Record the timer value at this point //记录下此时的定时器计数值
                if (RemoteCH2_CAPTURE_DOWNVAL < RemoteCH2_CAPTURE_UPVAL)
                {
                    CH2_Diff = 9999;
                }
                else
                    CH2_Diff = 0;
                remoter.ch2 = RemoteCH2_CAPTURE_DOWNVAL - RemoteCH2_CAPTURE_UPVAL + CH2_Diff; //Time to get the total high level //得到总的高电平的时间
                if(abs(remoter.ch2-last_remoter.ch2)>500)
                {
                    ch2_filter_times++;
                    if( ch2_filter_times<=5 ) remoter.ch2=last_remoter.ch2; //Filter //滤波
                    else ch2_filter_times=0;
                }
                else
                {
                    ch2_filter_times=0;
                }
                last_remoter.ch2=remoter.ch2;

                RemoteCH2_CAPTURE_STA = 0; //Capture flag bit to zero	//捕获标志位清零
                Set_CH2_Rising; //Set to rising edge capture //设置为上升沿捕获
            }
            else
            {
                //When the capture time occurs but not the falling edge, the first time the rising edge is captured, record the timer value at this time
                //发生捕获时间但不是下降沿，第一次捕获到上升沿，记录此时的定时器计数值
                RemoteCH2_CAPTURE_UPVAL = Get_CH2_CNT; //Obtain rising edge data //获取上升沿数据
                RemoteCH2_CAPTURE_STA |= 0X40; //The flag has been caught on the rising edge //标记已捕获到上升沿
                Set_CH2_Falling; //Set to Falling Edge Capture //设置为下降沿捕获
            }
        }
    }
    //Channel 3 //通道三
    if ((RemoteCH3_CAPTURE_STA & 0X80) == 0)
    {
        if (Get_CH3_State != RESET)	//A capture event occurred on channel 3 //通道3发生捕获事件
        {
			remoter.check_count = 0;//航模遥控检查复位,如果该位不是频繁复位,说明是干扰信号.
			
            Clear_CH3_State; //Clear the interrupt flag bit //清除中断标志位
            if (RemoteCH3_CAPTURE_STA & 0X40)	//A falling edge is caught //捕获到一个下降沿
            {
                RemoteCH3_CAPTURE_DOWNVAL = Get_CH3_CNT; //Record the timer value at this point //记录下此时的定时器计数值
                if (RemoteCH3_CAPTURE_DOWNVAL < RemoteCH3_CAPTURE_UPVAL)
                {
                    CH3_Diff = 9999;
                }
                else
                    CH3_Diff = 0;
                remoter.ch3 = RemoteCH3_CAPTURE_DOWNVAL - RemoteCH3_CAPTURE_UPVAL + CH3_Diff; //Time to get the total high level //得到总的高电平的时间
                if(abs(remoter.ch3-last_remoter.ch3)>500)
                {
                    ch3_filter_times++;
                    if( ch3_filter_times<=5 ) remoter.ch3=last_remoter.ch3; //Filter //滤波
                    else ch3_filter_times=0;
                }
                else
                {
                    ch3_filter_times=0;
                }
                last_remoter.ch3=remoter.ch3;
                RemoteCH3_CAPTURE_STA = 0; //Capture flag bit to zero	//捕获标志位清零
                Set_CH3_Rising; //Set to rising edge capture //设置为上升沿捕获
            }
            else
            {
                //When the capture time occurs but not the falling edge, the first time the rising edge is captured, record the timer value at this time
                //发生捕获时间但不是下降沿，第一次捕获到上升沿，记录此时的定时器计数值
                RemoteCH3_CAPTURE_UPVAL = Get_CH3_CNT; //Obtain rising edge data //获取上升沿数据
                RemoteCH3_CAPTURE_STA |= 0X40; //The flag has been caught on the rising edge //标记已捕获到上升沿
                Set_CH3_Falling; //Set to Falling Edge Capture //设置为下降沿捕获
            }
        }
    }
	
    //Channel 4 //通道四
    if ((RemoteCH4_CAPTURE_STA & 0X80) == 0)
    {
        if (Get_CH4_State != RESET)	//A capture event occurred on channel 4 //通道4发生捕获事件
        {
            Clear_CH4_State; //Clear the interrupt flag bit //清除中断标志位
            if (RemoteCH4_CAPTURE_STA & 0X40)	//A falling edge is caught //捕获到一个下降沿
            {
                RemoteCH4_CAPTURE_DOWNVAL = Get_CH4_CNT; //Record the timer value at this point //记录下此时的定时器计数值
                if (RemoteCH4_CAPTURE_DOWNVAL < RemoteCH4_CAPTURE_UPVAL)
                {
                    CH4_Diff = 9999;
                }
                else
                    CH4_Diff = 0;
                remoter.ch4 = RemoteCH4_CAPTURE_DOWNVAL - RemoteCH4_CAPTURE_UPVAL + CH4_Diff; //Time to get the total high level //得到总的高电平的时间
                if(abs(remoter.ch4-last_remoter.ch4)>500)
                {
                    ch4_filter_times++;
                    if( ch4_filter_times<=5 ) remoter.ch4=last_remoter.ch4; //Filter //滤波
                    else ch4_filter_times=0;
                }
                else
                {
                    ch4_filter_times=0;
                }
                last_remoter.ch4=remoter.ch4;
                RemoteCH4_CAPTURE_STA = 0; //Capture flag bit to zero	//捕获标志位清零
                Set_CH4_Rising; //Set to rising edge capture //设置为上升沿捕获
            }
            else
            {
                //When the capture time occurs but not the falling edge, the first time the rising edge is captured, record the timer value at this time
                //发生捕获时间但不是下降沿，第一次捕获到上升沿，记录此时的定时器计数值
                RemoteCH4_CAPTURE_UPVAL = Get_CH4_CNT; //Obtain rising edge data //获取上升沿数据
                RemoteCH4_CAPTURE_STA |= 0X40; //The flag has been caught on the rising edge //标记已捕获到上升沿
                Set_CH4_Falling; //Set to Falling Edge Capture //设置为下降沿捕获
            }
        }
    }
	
}





