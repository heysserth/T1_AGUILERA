idf_component_register(SRCS "main.c" "main_copy.c" "ejercicio1.c" "ejercicio2.c" "ejercicio3.c"
                    INCLUDE_DIRS "."
                    REQUIRES led esp_timer pulsador esp_driver_i2c bme280)