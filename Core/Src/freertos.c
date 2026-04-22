/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "elog.h"
#include "adc.h"

#include <string.h>  // memset
// #include <stdlib.h>
#include "task.h"   // 任务通知函数  xTaskNotifyFromISR  MAX_DELAY
#include "queue.h"   // 队列
#include "semphr.h"  // 信号量
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
void Output_logTask(void *arg);
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
#define which_buf1 0
#define which_buf2 1
#define buffer_size 1
uint32_t produce_evt = 0;
uint32_t consume_evt = 0;

// ADC_HandleTypeDef hadc1;
// DMA_HandleTypeDef hdma_adc1;
#if 1
uint8_t g_buf_flag=which_buf1;
  uint32_t *p_g_buf1 = NULL;
  uint32_t *p_g_buf2 = NULL;

  QueueHandle_t xMailbox;
  SemaphoreHandle_t xSemaphore;//aka  QueueHandle_t
void myadc_dma_init(void){

// 注意：ADC通常�??12位或16位，建议�?? uint16_t 节省内存并匹�??
  p_g_buf1 = (uint32_t *)malloc(sizeof(uint32_t)*buffer_size);
  p_g_buf2 = (uint32_t *)malloc(sizeof(uint32_t)*buffer_size);
  if(p_g_buf1&&p_g_buf2){
    // 初始buf
//    *p_g_buf1 = 0xff;
//    *p_g_buf2 = 0xff;
    memset(p_g_buf1,0xff,sizeof(uint32_t)*buffer_size);
     memset(p_g_buf2,0xff,sizeof(uint32_t)*buffer_size);  
  }
  /**创建信号量 */
   // xSemaphore=xSemaphoreCreateMutex();

   xSemaphore=xSemaphoreCreateBinary();
	xSemaphoreGive(xSemaphore);
  ///////-----------
  xMailbox=xQueueCreate(1, sizeof(uint8_t));//创建邮箱 大小uint8_t g_buf_flag
}
#endif

#if 0
/**新方法�?�过�?个数组切换buf 
 * �?个数组两�?32位buf 半中断触�?--全中段也触发
*/
uint32_t buf[buffer_size*2];

#endif

/***
 *   * @brief  Starts the multi_buffer DMA Transfer.
 * HAL_DMAEx_MultiBufferStart
 */
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

osThreadId_t Output_logTaskHandle;
const osThreadAttr_t Output_logTask_attributes = {
  .name = "Output_logTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,//osPriorityBelowNormal
};  //用邮箱的时�?�会出错--难道是用队列就要修改优先级？
////优先�??---




 /**dma 传输完成回调 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  log_i("HAL_ADC_ConvCpltCallback running---------------");
  /**adc 传输完成回调 */
  if(hadc->Instance == ADC1){
    //转存数据--
    uint8_t completed_flag=g_buf_flag;
    // 切换buf
    if(g_buf_flag == which_buf1)      g_buf_flag=which_buf2;
    else      g_buf_flag=which_buf1;
    //send notify to Output_logTask--consume---实际上应该发给produce--告诉发送完毕
    // xTaskNotifyFromISR( Output_logTaskHandle, completed_flag ,eSetValueWithOverwrite,0 );
    xTaskNotifyFromISR( defaultTaskHandle, completed_flag ,eSetValueWithOverwrite,0 );
    
    ///覆写 通知�??=uvalue
    /**
     *     vTaskNotifyGiveFromISR( Output_logTaskHandle, NULL );
     * 高级通知函数
     * BaseType_t xTaskNotifyFromISR( TaskHandle_t xTaskToNotify,
                               uint32_t ulValue, 
                               eNotifyAction eAction, 
                               BaseType_t *pxHigherPriorityTaskWoken );
     *ulValue	怎么使用ulValue，由eAction参数决定   eAction	见下�??   返回�??	pdPASS：成功，大部分调用都会成�?? 
     */
    /**
     * 用邮�??  读取不删�??  只能覆盖 */
    // xQueueOverwriteFromISR(xMailbox, &completed_flag, 0);

    // 发信号量完成  解锁------不在这里解锁
    // xSemaphoreGiveFromISR(xSemaphore,NULL);
    
  }
}

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  
  /**应当有三个线程，
   * �??个默认线程，初始�??
   * �??个切换buffer线程�??
   * �??个处理数�??--convert voltage 线程 
   * */
  Output_logTaskHandle = osThreadNew(Output_logTask, NULL, &Output_logTask_attributes);
  // ConvertVoltageTaskHandle = osThreadNew(ConvertVoltageTask, NULL, &ConvertVoltageTask_attributes);

  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{//切换buffer线程--produce
  /* USER CODE BEGIN StartDefaultTask */
  log_i("StartDefaultTask runninggggggggggggggg");
  myadc_dma_init();

  
  /** adc cfile�?
   *   hadc1.Init.ContinuousConvMode = ENABLE;//连续转换模式，开启后，转换完成后会自动启动下�?个转�?
   * HAL_ADC_Start_DMA(&hadc1, (uint32_t*)p_g_buf1, buffer_size);
   */

  /* Infinite loop */
  for(;;)
  {
    
    log_i("buf111111111111"); // 启动ADC+DMA双缓冲传  ?
     HAL_ADC_Start_DMA(&hadc1, (uint32_t*)p_g_buf1, buffer_size);
    /***等待通知---中断中发给produce 任务的handler 
     * --通知DMA传输完成，
     * 如果consumedd--bufer为空--切换buff */
    if(pdPASS==xTaskNotifyWait(0,0,&produce_evt,portMAX_DELAY)){
      xTaskNotifyGive(Output_logTaskHandle);//给哪个任务发通知
      // xTaskNotifyGive(Output_logTaskHandle);
      //notice consumer produced done
    }
    //wait 等消费者消耗结束
    if(consume_evt){
    // wait for consume---消耗结束
        xSemaphoreTake(xSemaphore, portMAX_DELAY);
        xSemaphoreGive(xSemaphore);
    }
    
    ////循环切换buffer
    log_i("buf222222222222"); // 启动ADC+DMA双缓冲传  ?
     HAL_ADC_Start_DMA(&hadc1, (uint32_t*)p_g_buf2, buffer_size);

//    HAL_GPIO_TogglePin(LED_Test_GPIO_Port,LED_Test_Pin);
  //  osDelay(500);
    osDelay(100);
    
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void Output_logTask(void *arg){
// 处理数-convert voltage 线程 ---output log-- consume
  log_i("Output_logTask running---------------");
  BaseType_t Notifyret = pdPASS;    
  while(1){

    // 发信号量完成  加锁
  // if(xSemaphoreTake(xSemaphore, portMAX_DELAY)==pdTRUE){
    // Notifyret = xQueuePeek( xMailbox, &recvValue,  portMAX_DELAY);//portMAX_DELAY
  Notifyret = xTaskNotifyWait(0,0,&consume_evt,portMAX_DELAY);//produce send notify
      if(Notifyret==pdPASS){
        
        xSemaphoreTake(xSemaphore, portMAX_DELAY);
        //cosume buffer
        for(int i=0;i<2;i++){
          if(i==0)        log_d("buf%d=%d",i+1,p_g_buf1[0]);           
          else            log_d("buf%d=%d",i+1,p_g_buf2[0]);           
        }
        ulTaskNotifyValueClear(NULL, 0xffffffff);//清除通知
        // 发信号量完成  解锁
        xSemaphoreGive(xSemaphore);  
      }
           
      }

}

/* USER CODE END Application */

