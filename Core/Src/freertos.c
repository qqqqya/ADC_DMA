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
#define buffer_size 4

// ADC_HandleTypeDef hadc1;
// DMA_HandleTypeDef hdma_adc1;
uint8_t g_buf_flag=which_buf1;
  uint32_t *p_g_buf1 = NULL;
  uint32_t *p_g_buf2 = NULL;

  QueueHandle_t xMailbox;
void myadc_dma_init(void){

// 注意：ADC通常�?12位或16位，建议�? uint16_t 节省内存并匹�?
  p_g_buf1 = (uint32_t *)malloc(sizeof(uint32_t)*buffer_size);
  p_g_buf2 = (uint32_t *)malloc(sizeof(uint32_t)*buffer_size);
  if(p_g_buf1&&p_g_buf2){
    // 初始buf
//    *p_g_buf1 = 0xff;
//    *p_g_buf2 = 0xff;
    memset(p_g_buf1,0xff,sizeof(uint32_t)*buffer_size);
     memset(p_g_buf2,0xff,sizeof(uint32_t)*buffer_size);  
  }

  xMailbox=xQueueCreate(1, sizeof(uint8_t));//创建邮箱 大小uint8_t g_buf_flag
}
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
};  //用邮箱的时候会出错--难道是用队列就要修改优先级？
////优先�?---




 /**dma 传输完成回调 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  log_i("HAL_ADC_ConvCpltCallback running---------------");
  /**adc 传输完成回调 */
  if(hadc->Instance == ADC1){
    // 切换buf
    if(g_buf_flag == which_buf1){
      g_buf_flag=which_buf2;
    }
    else{
      g_buf_flag=which_buf1;
    }
    
    xTaskNotifyFromISR( Output_logTaskHandle, g_buf_flag ,eSetValueWithOverwrite,0 );
    ///覆写 通知�?=uvalue
    /**
     *     vTaskNotifyGiveFromISR( Output_logTaskHandle, NULL );
     * 高级通知函数
     * BaseType_t xTaskNotifyFromISR( TaskHandle_t xTaskToNotify,
                               uint32_t ulValue, 
                               eNotifyAction eAction, 
                               BaseType_t *pxHigherPriorityTaskWoken );
     *ulValue	怎么使用ulValue，由eAction参数决定   eAction	见下�?   返回�?	pdPASS：成功，大部分调用都会成�? 
     */
    /**
     * 用邮�?  读取不删�?  只能覆盖 */
    // xQueueOverwriteFromISR(xMailbox, &g_buf_flag, 0);

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
   * �?个默认线程，初始�?
   * �?个切换buffer线程�?
   * �?个处理数�?--convert voltage 线程 
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
{
	osDelay(500);
  /* USER CODE BEGIN StartDefaultTask */
  log_i("StartDefaultTask runninggggggggggggggg");
  myadc_dma_init();

  /** adc cfile中
   *   hadc1.Init.ContinuousConvMode = ENABLE;//连续转换模式，开启后，转换完成后会自动启动下一个转换
   * HAL_ADC_Start_DMA(&hadc1, (uint32_t*)p_g_buf1, buffer_size);
   */

  /* Infinite loop */
  for(;;)
  {
    ////循环切换buffer
    if(g_buf_flag==which_buf1){
    // 选择buf1
    log_i("buf111111111111"); // 启动ADC+DMA双缓冲传  ?
     HAL_ADC_Start_DMA(&hadc1, (uint32_t*)p_g_buf1, buffer_size);
    }
  else{
    // 选择buf2
    log_i("buf222222222222"); // 启动ADC+DMA双缓冲传  ?
     HAL_ADC_Start_DMA(&hadc1, (uint32_t*)p_g_buf2, buffer_size);
    }
//    HAL_GPIO_TogglePin(LED_Test_GPIO_Port,LED_Test_Pin);
  //  osDelay(500);
    osDelay(10);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void Output_logTask(void *arg){
  
  /**切换buffer线程 */
  log_i("Output_logTask running---------------");
  uint32_t recvValue = 0;
  BaseType_t Notifyret = pdPASS;    
  while(1){
    // Notifyret = xQueuePeek( xMailbox, &recvValue,  0);//portMAX_DELAY
  Notifyret = xTaskNotifyWait(0,0,&recvValue,portMAX_DELAY);//发过来的通知�? �? buf_flag
                                                    //接收的�?�知�?
  if(Notifyret==pdPASS){
    log_i("Notifyret=pdPASS!!!");
    ////打印buffer中数�?
    if(recvValue==which_buf1){

       log_d("buffer1===========");
       for(int i=0;i<buffer_size;i++){
        log_d("buf1[%d]=%d",i+1,p_g_buf1[i]);        
      }
        log_d("buf2[%d]=%d",77,p_g_buf1[buffer_size-1]);

    }

    else{
       log_d("buffer2------------");

       for(int i=0;i<buffer_size;i++){
        log_d("buf2[%d]=%d",i+1,p_g_buf2[i]);
      }
        log_d("buf2[%d]=%d",77,p_g_buf2[buffer_size-1]);
    }
  }
  else{
    log_e("Notifyret!=pdPASS");
  }
}
}
/* USER CODE END Application */

