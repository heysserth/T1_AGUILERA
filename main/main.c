#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "pulsador.h"
#include "pines.h"

static QueueHandle_t cola_flancos;
static pulsador_t pulsador;

/* ISR mínima: captura el instante y lo copia a la cola. */
static void isr_pulsador(void *arg)
{
    const int64_t instante_us = esp_timer_get_time();
    BaseType_t despertar_tarea = pdFALSE;

    xQueueSendFromISR(cola_flancos, &instante_us, &despertar_tarea);

    if (despertar_tarea == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void tarea_pulsador(void *arg)
{
    int64_t instante_us;

    while (true) {
        if (xQueueReceive(cola_flancos, &instante_us, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI("pulsador", "Flanco recibido en %lld us", instante_us);
            /* El resto del trabajo se hace aquí, no en la ISR. */
        }
    }
}

void app_main(void)
{
    cola_flancos = xQueueCreate(10, sizeof(int64_t));
    ESP_ERROR_CHECK(cola_flancos != NULL ? ESP_OK : ESP_ERR_NO_MEM);

    // Configuramos el pin definido como PIN_PULSADOR (cero indica boton pulsado, esto es que esta )
    ESP_ERROR_CHECK(pulsador_iniciar(&pulsador, PIN_PULSADOR, 0));
    
    // Instalamos el manejador de interrupciones
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    // Creamos la tarea
    if (xTaskCreate(tarea_pulsador, "tarea_pulsador", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    /* Añadir el manejador registra la ISR y habilita la interrupción del pin. */
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_PULSADOR, isr_pulsador, NULL));
}