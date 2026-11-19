// mpu6050.c - MPU6050 written at register level. The ESP-IDF I2C master driver
// only moves bytes; every register address and bit setting is ours.
#include "mpu6050.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

/* registers (MPU-6000/6050 Register Map, rev 4.2) */
#define REG_SMPLRT_DIV    0x19
#define REG_CONFIG        0x1A
#define REG_GYRO_CONFIG   0x1B
#define REG_ACCEL_CONFIG  0x1C
#define REG_ACCEL_XOUT_H  0x3B   /* 14 bytes: accel XYZ, temp, gyro XYZ */
#define REG_PWR_MGMT_1    0x6B
#define REG_WHO_AM_I      0x75

#define ACCEL_LSB_PER_G   16384.0f   /* +-2 g */
#define GYRO_LSB_PER_DPS  131.0f     /* +-250 deg/s */

static const char *TAG = "mpu6050";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dev, buf, 2, 100);
}

static esp_err_t read_regs(uint8_t reg, uint8_t *out, size_t n)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, out, n, 100);
}

esp_err_t mpu6050_init(void)
{
    i2c_master_bus_config_t bus = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = MPU_SDA_GPIO,
        .scl_io_num = MPU_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,   /* GY-521 also has its own pull-ups */
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus, &s_bus));
    i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus, &dev, &s_dev));

    uint8_t who = 0;
    esp_err_t err = read_regs(REG_WHO_AM_I, &who, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no answer at 0x%02x (%s): check SDA=GPIO%d, SCL=GPIO%d, VCC, GND",
                 MPU_ADDR, esp_err_to_name(err), MPU_SDA_GPIO, MPU_SCL_GPIO);
        return err;
    }
    if (who != 0x68) ESP_LOGW(TAG, "WHO_AM_I = 0x%02x (genuine chip says 0x68; clones differ, continuing)", who);
    else ESP_LOGI(TAG, "found MPU6050 (WHO_AM_I 0x68)");

    ESP_ERROR_CHECK(write_reg(REG_PWR_MGMT_1, 0x01));   /* wake, clock = gyro X PLL */
    ESP_ERROR_CHECK(write_reg(REG_CONFIG, 0x03));       /* DLPF ~44 Hz */
    ESP_ERROR_CHECK(write_reg(REG_SMPLRT_DIV, 19));     /* 1 kHz / (1+19) = 50 Hz */
    ESP_ERROR_CHECK(write_reg(REG_GYRO_CONFIG, 0x00));  /* +-250 deg/s */
    ESP_ERROR_CHECK(write_reg(REG_ACCEL_CONFIG, 0x00)); /* +-2 g */
    return ESP_OK;
}

esp_err_t mpu6050_read(mpu6050_sample_t *s)
{
    uint8_t b[14];
    esp_err_t err = read_regs(REG_ACCEL_XOUT_H, b, sizeof b);
    if (err != ESP_OK) return err;
    #define W(i) ((int16_t)((b[i] << 8) | b[(i) + 1]))
    s->ax = W(0) / ACCEL_LSB_PER_G;
    s->ay = W(2) / ACCEL_LSB_PER_G;
    s->az = W(4) / ACCEL_LSB_PER_G;
    s->temp_c = W(6) / 340.0f + 36.53f;
    s->gx = W(8) / GYRO_LSB_PER_DPS;
    s->gy = W(10) / GYRO_LSB_PER_DPS;
    s->gz = W(12) / GYRO_LSB_PER_DPS;
    #undef W
    return ESP_OK;
}
