#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "pines.h"

#define VENTANA_MS 20

static const char *TAG = "pulsador";
static QueueHandle_t cola;
static portMUX_TYPE mux_perdidos = portMUX_INITIALIZER_UNLOCKED;
static uint32_t eventos_perdidos;

/* La cola guarda cuándo ocurrió cada flanco. */
static void isr_pulsador(void *argumento)
{
    (void)argumento;

    int64_t instante = esp_timer_get_time();
    BaseType_t despertar = pdFALSE;

    /* La ISR solo toma el tiempo y lo manda a la tarea. */
    if (xQueueSendFromISR(cola, &instante, &despertar) != pdPASS) {
        portENTER_CRITICAL_ISR(&mux_perdidos);
        eventos_perdidos++;
        portEXIT_CRITICAL_ISR(&mux_perdidos);
    }

    if (despertar == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/* Espera por flancos y decide en la tarea si fue una pulsación estable. */
static void tarea_pulsador(void *argumento)
{
    (void)argumento;

    int nivel_estable = gpio_get_level(PIN_PULSADOR); /* Suelto = 1. */
    int64_t ultima_pulsacion_us = 0;
    uint32_t pulsaciones = 0;
    int64_t instante;

    for (;;) {
        /* Se bloquea aquí hasta que la ISR ponga un evento en la cola. */
        xQueueReceive(cola, &instante, portMAX_DELAY);

        /* Informa si se perdió un evento porque la cola estaba llena. */
        portENTER_CRITICAL(&mux_perdidos);
        uint32_t perdidos = eventos_perdidos;
        eventos_perdidos = 0;
        portEXIT_CRITICAL(&mux_perdidos);
        if (perdidos > 0) {
            ESP_LOGW(TAG, "Se perdieron %lu flancos", (unsigned long)perdidos);
        }

        /* Ignora flancos dentro de la guarda desde la última pulsación contada. */
        if (ultima_pulsacion_us != 0 &&
            instante - ultima_pulsacion_us < (int64_t)VENTANA_MS * 1000) {
            continue;
        }

        /*
         * Espera una ventana completa sin nuevos flancos.
         * Si llega otro, xQueueReceive lo saca y empieza otra espera.
         */
        while (xQueueReceive(cola, &instante, pdMS_TO_TICKS(VENTANA_MS)) == pdTRUE) {
        }

        /* Confirma el estado una vez que el contacto ha dejado de rebotar. */
        int nivel_actual = gpio_get_level(PIN_PULSADOR);

        /* Solo cuenta el cambio estable de suelto (1) a pulsado (0). */
        if (nivel_estable == 1 && nivel_actual == 0) {
            pulsaciones++;
            ultima_pulsacion_us = esp_timer_get_time();
            ESP_LOGI(TAG, "Pulsacion valida: %lu", (unsigned long)pulsaciones);
        }

        /* También recuerda la liberación para poder aceptar la próxima pulsación. */
        nivel_estable = nivel_actual;
    }
}

void run_ejercicio2(void)
{
    /* Cola para guardar instantes mientras la tarea los procesa. */
    cola = xQueueCreate(32, sizeof(int64_t));
    ESP_ERROR_CHECK(cola != NULL ? ESP_OK : ESP_ERR_NO_MEM);

    /* Entrada con interrupción en ambos flancos y pull-up externa. */
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << PIN_PULSADOR,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));

    /* Instala el servicio y crea la tarea antes de habilitar la interrupción. */
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    if (xTaskCreate(tarea_pulsador, "pulsador", 3072, NULL, 5, NULL) != pdPASS) {
        ESP_ERROR_CHECK(ESP_FAIL);
    }
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_PULSADOR, isr_pulsador, NULL));

    ESP_LOGI(TAG, "Listo; antirrebote: %d ms", VENTANA_MS);
}
