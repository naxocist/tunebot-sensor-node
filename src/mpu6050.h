// mpu6050.h - register-level MPU6050 driver on the I2C master bus (E8)
#pragma once
#include <stdbool.h>
#include "esp_err.h"

#define MPU_SDA_GPIO 21
#define MPU_SCL_GPIO 22
#define MPU_ADDR     0x68      /* AD0 unconnected / low */

typedef struct {
    float ax, ay, az;          /* g */
    float gx, gy, gz;          /* degrees per second */
    float temp_c;
} mpu6050_sample_t;

esp_err_t mpu6050_init(void);
esp_err_t mpu6050_read(mpu6050_sample_t *s);
