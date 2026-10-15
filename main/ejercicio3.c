#include "bme280.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pines.h"

static const char *TAG = "bme280";

typedef struct {
    i2c_master_dev_handle_t handle;
} bme280_i2c_context_t;

static BME280_INTF_RET_TYPE leer_registro(uint8_t registro, uint8_t *datos,
                                          uint32_t cantidad, void *contexto)
{
    bme280_i2c_context_t *i2c = contexto;
    const esp_err_t error = i2c_master_transmit_receive(
        i2c->handle, &registro, sizeof(registro), datos, cantidad, 1000);

    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Error al leer el registro 0x%02X: %s",
                 registro, esp_err_to_name(error));
        return -1;
    }

    return 0;
}

static BME280_INTF_RET_TYPE escribir_registro(uint8_t registro,
                                              const uint8_t *datos,
                                              uint32_t cantidad,
                                              void *contexto)
{
    bme280_i2c_context_t *i2c = contexto;
    uint8_t mensaje[16];

    if (cantidad + 1 > sizeof(mensaje)) {
        ESP_LOGE(TAG, "Bloque demasiado grande al escribir el sensor");
        return -1;
    }

    mensaje[0] = registro;
    for (uint32_t i = 0; i < cantidad; i++) {
        mensaje[i + 1] = datos[i];
    }

    const esp_err_t error = i2c_master_transmit(
        i2c->handle, mensaje, cantidad + 1, 1000);

    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Error al escribir el registro 0x%02X: %s",
                 registro, esp_err_to_name(error));
        return -1;
    }

    return 0;
}

static void esperar_microsegundos(uint32_t tiempo, void *contexto)
{
    (void)contexto;
    esp_rom_delay_us(tiempo);
}

void run_ejercicio3(void)
{
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&config, &bus));

    int direccion_sensor_encontrada = 0;
    for (uint8_t direccion = 0x08; direccion <= 0x77; direccion++) {
        const esp_err_t error = i2c_master_probe(bus, direccion, 20);

        if (error == ESP_OK) {
            ESP_LOGI(TAG, "Dispositivo encontrado en 0x%02X", direccion);
            if (direccion == BME280_I2C_ADDR) {
                direccion_sensor_encontrada = 1;
            }
        } else if (error != ESP_ERR_NOT_FOUND) {
            ESP_ERROR_CHECK(error);
        }
    }

    if (direccion_sensor_encontrada) {
        const i2c_device_config_t sensor_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = BME280_I2C_ADDR,
            .scl_speed_hz = 100000,
        };

        i2c_master_dev_handle_t sensor;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &sensor_config, &sensor));

        bme280_i2c_context_t contexto = {
            .handle = sensor,
        };
        struct bme280_dev sensor_bme = {
            .intf = BME280_I2C_INTF,
            .intf_ptr = &contexto,
            .read = leer_registro,
            .write = escribir_registro,
            .delay_us = esperar_microsegundos,
        };

        const int8_t resultado = bme280_init(&sensor_bme);
        if (resultado < BME280_OK) {
            ESP_LOGE(TAG, "La API Bosch no pudo inicializar el sensor (error %d)",
                     resultado);
        } else if (sensor_bme.chip_id == BME280_CHIP_ID) {
            if (resultado > BME280_OK) {
                ESP_LOGW(TAG, "La API Bosch inicializo el sensor con aviso %d",
                         resultado);
            }
            ESP_LOGI(TAG, "BME280 confirmado por la API Bosch (ID 0x%02X)",
                     sensor_bme.chip_id);
            ESP_LOGI(TAG, "Calibracion interna leida correctamente");

            const struct bme280_settings ajustes = {
                .osr_t = BME280_OVERSAMPLING_1X,
                .osr_p = BME280_OVERSAMPLING_1X,
                .osr_h = BME280_OVERSAMPLING_1X,
                .filter = BME280_FILTER_COEFF_OFF,
                .standby_time = BME280_STANDBY_TIME_0_5_MS,
            };

            int8_t estado = bme280_set_sensor_settings(
                BME280_SEL_OSR_TEMP | BME280_SEL_OSR_PRESS | BME280_SEL_OSR_HUM,
                &ajustes, &sensor_bme);
            if (estado < BME280_OK) {
                ESP_LOGE(TAG, "No se pudieron configurar las medidas (error %d)",
                         estado);
            } else {
                uint32_t espera_us = 0;
                estado = bme280_cal_meas_delay(&espera_us, &ajustes);
                if (estado < BME280_OK) {
                    ESP_LOGE(TAG, "No se pudo calcular el tiempo de medida (error %d)",
                             estado);
                } else {
                    while (1) {
                        estado = bme280_set_sensor_mode(BME280_POWERMODE_FORCED,
                                                        &sensor_bme);
                        if (estado < BME280_OK) {
                            ESP_LOGE(TAG, "No se pudo iniciar la medida (error %d)",
                                     estado);
                            vTaskDelay(pdMS_TO_TICKS(2000));
                            continue;
                        }

                        vTaskDelay(pdMS_TO_TICKS(espera_us / 1000 + 1));
                        struct bme280_data medida;
                        estado = bme280_get_sensor_data(BME280_ALL, &medida,
                                                        &sensor_bme);
                        if (estado < BME280_OK) {
                            ESP_LOGE(TAG, "No se pudieron leer las medidas (error %d)",
                                     estado);
                        } else {
                            const int32_t temperatura =
                                (int32_t)(medida.temperature * 100.0 +
                                          (medida.temperature < 0 ? -0.5 : 0.5));
                            const uint32_t temperatura_abs =
                                temperatura < 0 ? (uint32_t)-temperatura :
                                                   (uint32_t)temperatura;

                            const uint32_t presion_centesimas =
                                (uint32_t)(medida.pressure * 100.0 + 0.5);
                            const uint32_t presion_pa = presion_centesimas / 100;
                            const uint32_t presion_decimal =
                                presion_centesimas % 100;
                            const uint32_t humedad_x100 =
                                (uint32_t)(medida.humidity * 100.0 + 0.5);

                            ESP_LOGI(TAG, "Temperatura: %s%lu.%02lu C",
                                     temperatura < 0 ? "-" : "",
                                     (unsigned long)(temperatura_abs / 100),
                                     (unsigned long)(temperatura_abs % 100));
                            ESP_LOGI(TAG, "Presion: %lu.%02lu Pa",
                                     (unsigned long)presion_pa,
                                     (unsigned long)presion_decimal);
                            ESP_LOGI(TAG, "Humedad: %lu.%02lu %%",
                                     (unsigned long)(humedad_x100 / 100),
                                     (unsigned long)(humedad_x100 % 100));
                        }

                        vTaskDelay(pdMS_TO_TICKS(2000));
                    }
                }
            }
        } else {
            ESP_LOGE(TAG, "ID inesperado: 0x%02X", sensor_bme.chip_id);
        }

        ESP_ERROR_CHECK(i2c_master_bus_rm_device(sensor));
    } else {
        ESP_LOGW(TAG, "El dispositivo esperado no respondio en 0x%02X",
                 BME280_I2C_ADDR);
    }

    ESP_ERROR_CHECK(i2c_del_master_bus(bus));
}
