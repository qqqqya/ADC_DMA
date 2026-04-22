/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "task.h"    // 任务通知函数
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

uint32_t *p_g_buf1 = NULL;
uint32_t *p_g_buf2 = NULL;

SemaphoreHandle_t xSemaphore; // 用于保护数据，防止生产者覆写消费者还没读完的数据

void myadc_dma_init(void){
  // 分配内存
  p_g_buf1 = (uint32_t *)malloc(sizeof(uint32_t)*buffer_size);
  p_g_buf2 = (uint32_t *)malloc(sizeof(uint32_t)*buffer_size);
  
  if(p_g_buf1 && p_g_buf2){
    memset(p_g_buf1, 0, sizeof(uint32_t)*buffer_size);
    memset(p_g_buf2, 0, sizeof(uint32_t)*buffer_size);  
  }

  // 创建二值信号量，初始状态给出一个信号量，表示消费者当前"空闲"
  xSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(xSemaphore);
}
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
  .priority = (osPriority_t) osPriorityBelowNormal,
};

/** dma 传输完成回调 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if(hadc->Instance == ADC1){
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // 仅仅通知生产者 (defaultTask) DMA 已经完成一次传输
    // 这里不需要传 buf_flag，因为生产者自己知道当前在用哪个 buf
    vTaskNotifyGiveFromISR(defaultTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void MX_FREERTOS_Init(void); 

void MX_FREERTOS_Init(void) {
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  Output_logTaskHandle = osThreadNew(Output_logTask, NULL, &Output_logTask_attributes);
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread. (生产者)
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  log_i("StartDefaultTask running...");
  myadc_dma_init();

  uint8_t current_buf_index = which_buf1; 
  uint32_t *active_buf = NULL;

  /* Infinite loop */
  for(;;)
  {
    // 1. 确定当前要使用的 buffer
    active_buf = (current_buf_index == which_buf1) ? p_g_buf1 : p_g_buf2;
    
    log_i("启动 DMA 传输到 buf%d", current_buf_index + 1);
    
    // 2. 启动ADC+DMA
    HAL_ADC_Start_DMA(&hadc1, active_buf, buffer_size);

    // 3. 等待 DMA 中断回调通知我们传输完成
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // 4. DMA完成了！把刚刚填满的 buffer 索引告诉消费者任务
    // 使用 eSetValueWithOverwrite 将索引值 (0 或 1) 直接发过去
    xTaskNotify(Output_logTaskHandle, current_buf_index, eSetValueWithOverwrite);

    // 5. 切换到另一个 buffer，为下一次传输做准备
    current_buf_index = (current_buf_index == which_buf1) ? which_buf2 : which_buf1;

    // 6. 【关键同步】在启动下一次传输前，确保消费者已经处理完了前一次的数据。
    // 如果消费者还在处理，获取信号量会阻塞在这里，防止 DMA 跑得太快覆写数据。
    xSemaphoreTake(xSemaphore, portMAX_DELAY);
    // 拿到信号量说明消费者处理完了，立刻归还信号量，让下一次循环可以继续
    xSemaphoreGive(xSemaphore); 
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void Output_logTask(void *arg){
  // 处理数-convert voltage 线程 ---output log-- consume (消费者)
  log_i("Output_logTask running...");
  
  uint32_t ready_buf_index = 0; // 用于接收生产者发来的 buffer 索引

  while(1){
    // 1. 等待生产者发送通知，并把发来的数据（准备好的buf索引）存入 ready_buf_index
    if(xTaskNotifyWait(0x00, 0xFFFFFFFF, &ready_buf_index, portMAX_DELAY) == pdPASS) {
      
      // 2. 获取信号量，上锁，表示"我正在消费数据，生产者请不要覆写"
      xSemaphoreTake(xSemaphore, portMAX_DELAY);

      // 3. 消费数据：根据收到的索引，只读取准备好的那一个 buffer
      // 【修复的致命错误】：必须使用 *p_g_buf1 或者 p_g_buf1[0] 来打印数据，而不是打印指针本身！
      if(ready_buf_index == which_buf1) {
        log_d("消费: buf1 = %lu", *p_g_buf1); 
      } else if (ready_buf_index == which_buf2) {
        log_d("消费: buf2 = %lu", *p_g_buf2);
      }

      // 4. 数据处理完毕，释放信号量，允许生产者继续
      xSemaphoreGive(xSemaphore);  
    }
  }
}
/* USER CODE END Application */