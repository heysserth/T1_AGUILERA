#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "led.h"
#include "pines.h"
#include "ejercicio1.h"

static const char *TAG = "led";

#define CONFIG_SEYTR_PERIODO_MS 1000

static led_t s_led;
static volatile bool s_parpadeo_activo = true; /* Si está en true, parpadea; si está en false, obedece a la consola */

static void tarea_led(void *argumento)
{
    (void)argumento;

    TickType_t referencia = xTaskGetTickCount();
    const TickType_t intervalo = pdMS_TO_TICKS(CONFIG_SEYTR_PERIODO_MS / 2);

    for (;;) {
        xTaskDelayUntil(&referencia, intervalo);

        /* Solo conmuta si el parpadeo no ha sido pausado por la consola */
        if (s_parpadeo_activo) {
            led_conmutar(&s_led);
        }
    }
}


void run_ejercicio1(void)
{
    ESP_ERROR_CHECK(led_iniciar(&s_led, PIN_LED, LED_NIVEL_ENCENDIDO));

    ESP_LOGI(TAG, "TALLER #1 6.1 P1A · Salida digital y parpadeo estable");
    ESP_LOGI(TAG, "#0 encendido t=0.000 ms, origen de la medida");

    if (xTaskCreate(tarea_led, "led", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE("led", "No se pudo crear la tarea");
    }
}

/* Permite a la consola pausar el parpadeo automático para tomar el control */
void ejercicio1_pausar_parpadeo(bool pausar)
{
    s_parpadeo_activo = !pausar;
}

/* Permite a la consola encender o apagar el LED directamente */
void ejercicio1_fijar_estado_led(bool encender)
{
    s_parpadeo_activo = false; /* Si la consola manda encender/apagar, pausa el parpadeo */
    if (encender) {
        led_encender(&s_led);
    } else {
        led_apagar(&s_led);
    }
}
