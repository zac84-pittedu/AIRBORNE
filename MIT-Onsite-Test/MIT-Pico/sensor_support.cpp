#include "sensor_support.h"
#include "pinout.h"
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include <string.h>

namespace {
struct SensorState {
  bool initialized = false;
  bool sht45_ready = false;
  bool ms5611_ready = false;
  bool icm2098_ready = false;
  bool max31865_ready = false;
};

static SensorState g_state;

static bool i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  delay(2);
  if (Wire.requestFrom((int)addr, 1) != 1) {
    return false;
  }
  value = Wire.read();
  return true;
}

static bool i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t* data, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  delay(2);
  if (Wire.requestFrom((int)addr, (int)len) != (int)len) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    data[i] = Wire.read();
  }
  return true;
}

static void init_sht45() {
  if (!i2c_write_reg(SHT45_ADDR, 0x30, 0x0A)) {
    return;
  }
  g_state.sht45_ready = true;
}

static void init_ms5611() {
  uint8_t id = 0;
  if (!i2c_read_reg(MS5611_ADDR, 0x0A, id)) {
    return;
  }
  g_state.ms5611_ready = true;
}

static void init_icm2098() {
  uint8_t whoami = 0;
  if (!i2c_read_reg(ICM20948_ADDR_AD0_LOW, 0x00, whoami)) {
    return;
  }
  g_state.icm2098_ready = (whoami == 0xEA || whoami == 0x00);
}

static void init_max31865() {
  pinMode(MAX31865_CS_PIN, OUTPUT);
  digitalWrite(MAX31865_CS_PIN, HIGH);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
  digitalWrite(MAX31865_CS_PIN, LOW);
  SPI.transfer(0x80);
  SPI.transfer(0x00);
  digitalWrite(MAX31865_CS_PIN, HIGH);
  SPI.endTransaction();
  g_state.max31865_ready = true;
}

static bool read_sht45(float& temp_c, float& humidity_pct) {
  uint8_t data[6] = {0};
  if (!i2c_read_regs(SHT45_ADDR, 0x00, data, 6)) {
    return false;
  }
  uint16_t t_raw = ((uint16_t)data[0] << 8) | data[1];
  uint16_t h_raw = ((uint16_t)data[3] << 8) | data[4];
  temp_c = -45.0f + (175.0f * (float)t_raw / 65535.0f);
  humidity_pct = (100.0f * (float)h_raw / 65535.0f);
  return true;
}

static bool read_ms5611(float& temp_c, float& pressure_hpa) {
  uint8_t data[3] = {0};
  if (!i2c_read_regs(MS5611_ADDR, 0x00, data, 3)) {
    return false;
  }
  temp_c = 25.0f + ((float)data[0] * 0.01f);
  pressure_hpa = 1013.25f + ((float)data[2] * 0.1f);
  return true;
}

static bool read_icm2098(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) {
  uint8_t data[12] = {0};
  if (!i2c_read_regs(ICM20948_ADDR_AD0_LOW, 0x2D, data, 12)) {
    return false;
  }
  int16_t x = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
  int16_t y = (int16_t)(((uint16_t)data[2] << 8) | data[3]);
  int16_t z = (int16_t)(((uint16_t)data[4] << 8) | data[5]);
  int16_t gx_raw = (int16_t)(((uint16_t)data[6] << 8) | data[7]);
  int16_t gy_raw = (int16_t)(((uint16_t)data[8] << 8) | data[9]);
  int16_t gz_raw = (int16_t)(((uint16_t)data[10] << 8) | data[11]);
  ax = (float)x / 16384.0f;
  ay = (float)y / 16384.0f;
  az = (float)z / 16384.0f;
  gx = (float)gx_raw / 131.0f;
  gy = (float)gy_raw / 131.0f;
  gz = (float)gz_raw / 131.0f;
  return true;
}

static bool read_max31865(float& temp_c) {
  uint8_t data[2] = {0};
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
  digitalWrite(MAX31865_CS_PIN, LOW);
  SPI.transfer(0x01);
  data[0] = SPI.transfer(0x00);
  data[1] = SPI.transfer(0x00);
  digitalWrite(MAX31865_CS_PIN, HIGH);
  SPI.endTransaction();
  uint16_t raw = ((uint16_t)data[0] << 8) | data[1];
  temp_c = ((float)(raw >> 1) * 0.03125f) - 242.0f;
  return true;
}
} // namespace

void sensors_begin() {
  Wire.end();
  Wire.setSDA(CNT_SDA_PIN);
  Wire.setSCL(CNT_SCL_PIN);
  Wire.begin();
  Wire.setClock(CNT_I2C_FREQ);
  SPI.begin(false);
  init_sht45();
  init_ms5611();
  init_icm2098();
  init_max31865();
  g_state.initialized = true;
}

void sensors_update(SensorSnapshot& snapshot) {
  memset(&snapshot, 0, sizeof(snapshot));
  snapshot.sensor_status_bits = 0;
  snapshot.sht45_present = g_state.sht45_ready;
  snapshot.ms5611_present = g_state.ms5611_ready;
  snapshot.icm2098_present = g_state.icm2098_ready;
  snapshot.max31865_present = g_state.max31865_ready;
  snapshot.sd_present = true;
  snapshot.xyt01_present = true;

  if (g_state.sht45_ready) {
    if (read_sht45(snapshot.sht45_temperature_c, snapshot.sht45_humidity_pct)) {
      snapshot.sensor_status_bits |= 0x01;
    }
  }
  if (g_state.ms5611_ready) {
    if (read_ms5611(snapshot.ms5611_temperature_c, snapshot.ms5611_pressure_hpa)) {
      snapshot.sensor_status_bits |= 0x02;
    }
  }
  if (g_state.icm2098_ready) {
    if (read_icm2098(snapshot.icm2098_accel_x_g, snapshot.icm2098_accel_y_g, snapshot.icm2098_accel_z_g,
                     snapshot.icm2098_gyro_x_dps, snapshot.icm2098_gyro_y_dps, snapshot.icm2098_gyro_z_dps)) {
      snapshot.sensor_status_bits |= 0x04;
    }
  }
  if (g_state.max31865_ready) {
    if (read_max31865(snapshot.max31865_temperature_c)) {
      snapshot.sensor_status_bits |= 0x08;
    }
  }

  snapshot.xyt01_status = (snapshot.sensor_status_bits & 0x0F) ? 1 : 0;
}

void sensors_apply_to_packet(CNTData& pkt, const SensorSnapshot& snapshot) {
  pkt.temperature = (uint8_t)constrain((int32_t)lroundf(snapshot.sht45_temperature_c), 0, 255);
  pkt.humidity = (uint8_t)constrain((int32_t)lroundf(snapshot.sht45_humidity_pct), 0, 255);
}
