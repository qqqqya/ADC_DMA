#include "adc_and_dma.h"
#include "FreeRTOS.h"
#include "adc.h"
#include "elog.h"
#include "event_groups.h"
#include "main.h"
#include "semphr.h"
#include "stdio.h"
#include "task.h"
#include <stdlib.h>
#include <string.h>

extern DMA_HandleTypeDef hdma_adc1;

static SemaphoreHandle_t g_xMutex;
static TaskHandle_t g_dma_handle, g_adc_handle;

#define BIT(x) (1 << (x))
#define PRODUCED(x) BIT(x)
#define CONSUMING(x) BIT(x)
#define BIT_DMA BIT(8)

uint32_t* pdata;
uint8_t w_ch = 0, r_ch = 0;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE, xResult;
    xResult = xTaskNotifyFromISR(g_dma_handle, BIT_DMA, eSetBits, &xHigherPriorityTaskWoken);
    //发给dma_handler--Task A--生产者---通知dma完成一次转换
    if (xResult == pdPASS) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void dma_handler(void* parm)
{//dma_handler--Task A--生产者--切换buffer
    uint32_t adc_evt_mask = 0, dma_evt_mask = 0;
    pdata = malloc(8);
    if (!pdata) {
        log_e("malloc err");
        return;
    }
    memset(pdata, 0, 8);

    /* 0. start first dma transfer */
    log_d("get mutex %d ok", w_ch);
    HAL_StatusTypeDef status = HAL_ADC_Start_DMA(&hadc1, &pdata[w_ch], 1);
    if (status != HAL_OK) {
        log_e("HAL_ADC_Start_DMA ng");
    }
    while (1) {

        /* 1. wait until dma transfer finished */
        do {
            xTaskNotifyWait(0x0, BIT_DMA, &dma_evt_mask, portMAX_DELAY);
        } while (0 == (dma_evt_mask & BIT_DMA));

        /* 2. notify adc thread data produced done */
        xTaskNotifyAndQuery(g_adc_handle, PRODUCED(w_ch), eSetBits, &adc_evt_mask);

        log_d("adc_evt_mask 0x%x", adc_evt_mask);
        /* 3. let dma start next transfer */
        w_ch = !w_ch; /* change next write ch ----------------*/
        if (adc_evt_mask & PRODUCED(w_ch)) {
            /* 3.1 buffer is full, wait adc app consuming data */
            while (0 == (dma_evt_mask & CONSUMING(w_ch))) {
                xTaskNotifyWait(0x0, CONSUMING(w_ch), &dma_evt_mask, portMAX_DELAY);
            }

            /* 3.2 wait adc app consumed data finish, take mutex */
            xSemaphoreTake(g_xMutex, portMAX_DELAY);
            xSemaphoreGive(g_xMutex); /* give mutex */
        } else {
            /* buffer may already consumed, clear consuming bit */
            ulTaskNotifyValueClear(NULL, CONSUMING(w_ch));
        }

        /* 3.3 start next transfer */
        status = HAL_ADC_Start_DMA(&hadc1, &pdata[w_ch], 1);
        if (status != HAL_OK) {
            log_e("HAL_ADC_Start_DMA ng");
        }
        elog_flush();
    }
}

void adc_app(void* parm)
{//--Task B--消费者-
    uint32_t adc_evt_mask = 0;
    while (1) {
        /* 1. wait any data finish, not clear PRODUCED(0) | PRODUCED(1) */
        xTaskNotifyWait(0x0, 0x0, &adc_evt_mask, portMAX_DELAY);

        /* 2. get mutex and log data */
        for (int i = 0; i < 2; i++) {
            if (adc_evt_mask & PRODUCED(i)) {
                /* 2.1 take mutex, and set consuming bit */
                xSemaphoreTake(g_xMutex, portMAX_DELAY);
                xTaskNotify(g_dma_handle, CONSUMING(i), eSetBits);
                /* 2.2 consuming data */
                log_d("buffer %d, adc code is %d", i, pdata[i]);

                /* 2.3 after consumed done, clear produced bit */
                ulTaskNotifyValueClear(NULL, PRODUCED(i));
                xSemaphoreGive(g_xMutex);
            }
        }

        elog_flush();
    }
}

void adc_and_dma_init()
{
    g_xMutex = xSemaphoreCreateMutex();
    if (g_xMutex == NULL) {
        log_e("create g_xMutex faild");
    }

    xTaskCreate(adc_app, "adc app", 0x200, NULL, 1, &g_adc_handle);
    if (!g_adc_handle) {
        log_e("create adc app task faild");
    }
    xTaskCreate(dma_handler, "dma app", 0x200, NULL, 2, &g_dma_handle);
    if (!g_dma_handle) {
        log_e("create dma_handler task faild");
    }
    elog_flush();
    return;
}
