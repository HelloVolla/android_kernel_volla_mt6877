// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef SPL07_BARO_H
#define SPL07_BARO_H

#include <linux/ioctl.h>

/* apply low pass filter on output */
/*#define CONFIG_SPL_LOWPASS*/
/*#define CONFIG_ID_TEMPERATURE*/
/*#define CONFIG_I2C_BASIC_FUNCTION*/

#define SPL_DRIVER_VERSION "V1.0"

#define SPL_DEV_NAME "spl07"

#define C_MAX_FIR_LENGTH (32)
#define MAX_SENSOR_NAME (32)
#define SPL_DATA_NUM 1
#define SPL_PRESSURE 0
#define SPL_BUFSIZE 128

/* common definition */
#define SPL_GET_BITSLICE(regvar, bitname)                                      \
	((regvar & bitname##__MSK) >> bitname##__POS)

#define SPL_SET_BITSLICE(regvar, bitname, val)                                 \
	((regvar & ~bitname##__MSK) |                                          \
	 ((val << bitname##__POS) & bitname##__MSK))

#define SPL_CHIP_ID_REG 0x0D

/*********************************[SPL07]*************************************/
/* data type */
#define SPL07_U16_t u16
#define SPL07_S16_t s16
#define SPL07_U32_t u32
#define SPL07_S32_t s32
#define SPL07_U64_t u64
#define SPL07_S64_t s64

/* chip id */
#define SPL07_CHIP_ID 0x11

/* i2c address */
/* 7-bit addr: 0x76 (SDO connected to GND); 0x77 (SDO connected to VDDIO) */
#define SPL07_I2C_ADDRESS 0x77

/* calibration data */
#define SPL07_CALIBRATION_DATA_START 0x10 /* SPL07_DIG_T1_LSB_REG */
#define SPL07_CALIBRATION_DATA_LENGTH 21

#define SHIFT_RIGHT_4_POSITION 4
#define SHIFT_LEFT_2_POSITION 2
#define SHIFT_LEFT_4_POSITION 4
#define SHIFT_LEFT_5_POSITION 5
#define SHIFT_LEFT_8_POSITION 8
#define SHIFT_LEFT_12_POSITION 12
#define SHIFT_LEFT_16_POSITION 16

/* power mode */
#define SPL07_SLEEP_MODE 0x00
#define SPL07_FORCED_MODE 0x01
#define SPL07_NORMAL_MODE 0x07

#define SPL07_PRS_CFG_REG 0x06
#define SPL07_TEM_CFG_REG 0x07
#define SPL07_POWER_MODE_REG 0x08
#define SPL07_T_P_SHIFT_CFG_REG 0x09
#define SPL07_soft_reset_REG 0x0C

/* oversampling */
#define SPL07_OVERSAMPLING_SINGLE 0x00
#define SPL07_OVERSAMPLING_2X 0x01
#define SPL07_OVERSAMPLING_4X 0x02
#define SPL07_OVERSAMPLING_8X 0x03
#define SPL07_OVERSAMPLING_16X 0x04
#define SPL07_OVERSAMPLING_32X 0x05
#define SPL07_OVERSAMPLING_64X 0x06

/* sampling */
#define SPL07_SAMPLING_1Hz 0x00
#define SPL07_SAMPLING_2Hz 0x01
#define SPL07_SAMPLING_4Hz 0x02
#define SPL07_SAMPLING_8Hz 0x03
#define SPL07_SAMPLING_16Hz 0x04
#define SPL07_SAMPLING_32Hz 0x05
#define SPL07_SAMPLING_64Hz 0x06
#define SPL07_SAMPLING_128Hz 0x07
#define SPL07_SAMPLING_25_16Hz 0x08
#define SPL07_SAMPLING_25_8Hz 0x09
#define SPL07_SAMPLING_25_4Hz 0x0A
#define SPL07_SAMPLING_25_2Hz 0x0B
#define SPL07_SAMPLING_25Hz 0x0C
#define SPL07_SAMPLING_50Hz 0x0D
#define SPL07_SAMPLING_100Hz 0x0E
#define SPL07_SAMPLING_200Hz 0x0F

/* sampling */
#define SPL07_SAMPLING_1 1
#define SPL07_SAMPLING_2 2
#define SPL07_SAMPLING_4 4
#define SPL07_SAMPLING_8 8
#define SPL07_SAMPLING_16 16
#define SPL07_SAMPLING_32 32
#define SPL07_SAMPLING_64 64
#define SPL07_SAMPLING_128 128
#define SPL07_SAMPLING_25_16 1
#define SPL07_SAMPLING_25_8 3
#define SPL07_SAMPLING_25_4 6
#define SPL07_SAMPLING_25_2 12
#define SPL07_SAMPLING_25 25
#define SPL07_SAMPLING_50 50
#define SPL07_SAMPLING_100 100
#define SPL07_SAMPLING_200 200

/* Scale Factor KP&KT  */
#define SPL07_OVERSAMPLING_SINGLE_CSF 524288
#define SPL07_OVERSAMPLING_2X_CSF 1572864
#define SPL07_OVERSAMPLING_4X_CSF 3670016
#define SPL07_OVERSAMPLING_8X_CSF 7864320
#define SPL07_OVERSAMPLING_16X_CSF 253952
#define SPL07_OVERSAMPLING_32X_CSF 516096
#define SPL07_OVERSAMPLING_64X_CSF 1040384

/* SPL07 POSR CFG */
#define SPL07_PRS_CFG_REG_OSRSP__POS 0
#define SPL07_PRS_CFG_REG_OSRSP__MSK 0x0F
#define SPL07_PRS_CFG_REG_OSRSP__LEN 4
#define SPL07_PRS_CFG_REG_OSRSP__REG SPL07_PRS_CFG_REG

/* SPL07 PODR CFG */
#define SPL07_PRS_CFG_REG_ODR__POS 4
#define SPL07_PRS_CFG_REG_ODR__MSK 0xF0
#define SPL07_PRS_CFG_REG_ODR__LEN 4
#define SPL07_PRS_CFG_REG_ODR__REG SPL07_PRS_CFG_REG

/* SPL07 TOSR CFG */
#define SPL07_TEM_CFG_REG_OSRSP__POS 0
#define SPL07_TEM_CFG_REG_OSRSP__MSK 0x0F
#define SPL07_TEM_CFG_REG_OSRSP__LEN 4
#define SPL07_TEM_CFG_REG_OSRSP__REG SPL07_TEM_CFG_REG

/* SPL07 TODR CFG */
#define SPL07_TEM_CFG_REG_ODR__POS 4
#define SPL07_TEM_CFG_REG_ODR__MSK 0xF0
#define SPL07_TEM_CFG_REG_ODR__LEN 4
#define SPL07_TEM_CFG_REG_ODR__REG SPL07_TEM_CFG_REG

/* SPL07 T SHIFT */
#define SPL07_T_SHIFT_CFG__POS 3
#define SPL07_T_SHIFT_CFG__MSK 0x08
#define SPL07_T_SHIFT_CFG__LEN 1
#define SPL07_T_SHIFT_CFG__REG SPL07_T_P_SHIFT_CFG_REG

/* SPL07 P SHIFT */
#define SPL07_P_SHIFT_CFG__POS 2
#define SPL07_P_SHIFT_CFG__MSK 0x04
#define SPL07_P_SHIFT_CFG__LEN 1
#define SPL07_P_SHIFT_CFG__REG SPL07_T_P_SHIFT_CFG_REG

/* data */
#define SPL07_PRESSURE_MSB_REG 0x00 /* Pressure MSB Register */
#define SPL07_TEMPERATURE_MSB_REG 0x03 /* Temperature MSB Reg */

/* i2c disable switch */
#define SPL07_I2C_DISABLE_SWITCH 0x25

#endif