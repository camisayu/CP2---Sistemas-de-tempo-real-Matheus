#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

// ======================
// Configurações gerais
// ======================
#define WDT_TIMEOUT_S   6
#define QUEUE_LENGTH    10
#define QUEUE_ITEM_SIZE sizeof(int)
#define GENERATION_DELAY_MS 500
#define RECEPTION_TIMEOUT_MS 3000
#define SUPERVISION_DELAY_MS 2000

// ======================
// Variáveis globais
// ======================
static QueueHandle_t dataQueue = NULL;

// Flags de status (usadas pela supervisão)
volatile bool generation_ok = false;
volatile bool reception_ok = false;

// Tag para logs
static const char *TAG = "Matheus-RM:87560";

// ======================
// Inicialização do Watchdog
// ======================
void init_task_watchdog(void)
{
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT_S * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };

    esp_task_wdt_init(&twdt_config);
    printf("{Matheus-RM:87560} [WDT] Watchdog inicializado com timeout de %d segundos.\n", WDT_TIMEOUT_S);
}

// ======================
// Tarefa 1 - Geração de Dados
// ======================
void task_generate(void *pvParameters)
{
    esp_task_wdt_add(NULL);
    int counter = 0;

    while (1)
    {
        int value = counter++;
        BaseType_t sent = xQueueSend(dataQueue, &value, 0);

        if (sent == pdPASS)
        {
            printf("{Matheus-RM:87560} [FILA] Dado %d enviado com sucesso!\n", value);
        }
        else
        {
            printf("{Matheus-RM:87560} [FILA] Fila cheia, dado %d descartado!\n", value);
        }

        generation_ok = true;
        esp_task_wdt_reset(); // alimenta o watchdog
        vTaskDelay(pdMS_TO_TICKS(GENERATION_DELAY_MS));
    }
}

// ======================
// Tarefa 2 - Recepção de Dados
// ======================
void task_receive(void *pvParameters)
{
    esp_task_wdt_add(NULL);
    int receivedValue;

    while (1)
    {
        if (xQueueReceive(dataQueue, &receivedValue, pdMS_TO_TICKS(RECEPTION_TIMEOUT_MS)) == pdPASS)
        {
            int *ptr = (int *)malloc(sizeof(int));
            if (ptr != NULL)
            {
                *ptr = receivedValue;
                printf("{Matheus-RM:87560} [RECEPÇÃO] Valor recebido: %d\n", *ptr);
                free(ptr);
            }
            else
            {
                printf("{Matheus-RM:87560} [RECEPÇÃO] Erro ao alocar memória!\n");
            }

            reception_ok = true;
        }
        else
        {
            // Caso não receba nada no tempo limite
            printf("{Matheus-RM:87560} [RECEPÇÃO] Nenhum dado recebido no tempo limite!\n");
            printf("{Matheus-RM:87560} [RECEPÇÃO] Tentando recuperar...\n");

            vTaskDelay(pdMS_TO_TICKS(1000));

            // Tentativa de recuperação
            if (xQueueReceive(dataQueue, &receivedValue, pdMS_TO_TICKS(1000)) == pdPASS)
            {
                printf("{Matheus-RM:87560} [RECEPÇÃO] Recuperação bem-sucedida! Valor: %d\n", receivedValue);
                reception_ok = true;
            }
            else
            {
                printf("{Matheus-RM:87560} [RECEPÇÃO] Falha persistente. Encerrando tarefa...\n");
                reception_ok = false;
                vTaskDelete(NULL);
            }
        }

        esp_task_wdt_reset();
    }
}

// ======================
// Tarefa 3 - Supervisão
// ======================
void task_supervision(void *pvParameters)
{
    esp_task_wdt_add(NULL);

    while (1)
    {
        printf("\n{Matheus-RM:87560} [SUPERVISÃO] STATUS DO SISTEMA:\n");
        printf("  → Geração de Dados: %s\n", generation_ok ? "OK" : "FALHA");
        printf("  → Recepção de Dados: %s\n", reception_ok ? "OK" : "FALHA");
        printf("  → Watchdog ativo e monitorando\n\n");

        generation_ok = false;
        reception_ok = false;

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(SUPERVISION_DELAY_MS));
    }
}

// ======================
// Função principal (main)
// ======================
void app_main(void)
{
    printf("{Matheus-RM:87560} [SISTEMA] Inicializando sistema multitarefa...\n");

    init_task_watchdog();

    dataQueue = xQueueCreate(QUEUE_LENGTH, QUEUE_ITEM_SIZE);
    if (dataQueue == NULL)
    {
        printf("{Matheus-RM:87560} [ERRO] Falha ao criar fila!\n");
        return;
    }

    // Criação das tarefas
    xTaskCreate(task_generate, "TaskGenerate", 2048, NULL, 2, NULL);
    xTaskCreate(task_receive, "TaskReceive", 4096, NULL, 2, NULL);
    xTaskCreate(task_supervision, "TaskSupervision", 4096, NULL, 1, NULL);

    printf("{Matheus-RM:87560} [SISTEMA] Tarefas iniciadas com sucesso!\n");
}

