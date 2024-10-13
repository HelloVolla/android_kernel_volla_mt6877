/*****************************************************************************
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * Accelerometer Sensor Driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *****************************************************************************/

#ifndef INV_BARO_H
#define INV_BARO_H

#include <linux/ioctl.h>


/* apply low pass filter on output */
/*#define CONFIG_ICP_LOWPASS*/
/*#define CONFIG_ID_TEMPERATURE*/
/*#define CONFIG_I2C_BASIC_FUNCTION*/

#define ICP_DRIVER_VERSION "V1.0"

#define ICP_DEV_NAME        "icp201xx"

#define C_MAX_FIR_LENGTH (32)
#define MAX_SENSOR_NAME  (32)
#define ICP_DATA_NUM 1
#define ICP_PRESSURE         0
#define ICP_BUFSIZE			128

#define RETRY_CNT 3
#define DEBUG_PHASE 0

#define EXPECTED_DEVICE_ID	 				0x63

/* Main/ OTP Registers */
#define MPUREG_TRIM1_MSB						0X05
#define MPUREG_TRIM2_LSB						0X06
#define MPUREG_TRIM2_MSB						0X07
#define MPUREG_A1_WHO_AM_I						0X0C
#define MPUREG_IIR_K_FACTOR_LSB					((icp201xx_dev_version == ICP201XX_CHIP_VERSION_A) ?	0x78 : 0x88 ) 
#define MPUREG_IIR_K_FACTOR_MSB					((icp201xx_dev_version == ICP201XX_CHIP_VERSION_A) ?	0x79 : 0x89 ) 
#define MPUREG_OTP_MTP_OTP_CFG1					0XAC
#define MPUREG_OTP_MTP_MR_LSB					0XAD
#define MPUREG_OTP_MTP_MR_MSB					0XAE
#define MPUREG_OTP_MTP_MRA_LSB					0XAF
#define MPUREG_OTP_MTP_MRA_MSB					0XB0
#define MPUREG_OTP_MTP_MRB_LSB					0XB1
#define MPUREG_OTP_MTP_MRB_MSB					0XB2
#define MPUREG_OTP_MTP_OTP_ADDR					0XB5
#define MPUREG_OTP_MTP_OTP_CMD					0XB6
#define MPUREG_OTP_MTP_RD_DATA					0XB8
#define MPUREG_OTP_MTP_OTP_STATUS				0xB9
#define MPUREG_OTP_DEBUG2						0XBC
#define MPUREG_MASTER_LOCK						0XBE
#define REG_OTP_MTP_OTP_STATUS2				0XBF

#define MPUREG_MODE_SELECT			0xC0
#define MPUREG_INTERRUPT_STATUS     0xC1
#define MPUREG_INTERRUPT_MASK		0xC2
#define MPUREG_FIFO_CONFIG			0xC3
#define MPUREG_FIFO_FILL			0xC4
#define MPUREG_SPI_MODE				0xC5
#define MPUREG_PRESS_ABS_LSB		0xC7
#define MPUREG_PRESS_ABS_MSB		0xC8
#define MPUREG_PRESS_DELTA_LSB      0xC9
#define MPUREG_PRESS_DELTA_MSB      0xCA
#define MPUREG_DEVICE_STATUS		0xCD
#define MPUREG_I3C_INFO				0xCE
#define REG_VERSION		   			0XD3

#define MPUREG_FIFO_BASE            0XFA



typedef enum icp201xx_chip_verion
{
	ICP201XX_CHIP_VERSION_A = 0,  /* Version A */
	ICP201XX_CHIP_VERSION_B = 1  /* Version B */
}icp201xx_chip_version_t;

/* ICP 201XX Operation Mode */
typedef enum icp201xx_op_mode{  
	ICP201XX_OP_MODE0 = 0 ,  /* Mode 0: Bw:6.25 Hz ODR: 25Hz */
	ICP201XX_OP_MODE1     ,  /* Mode 1: Bw:30 Hz ODR: 120Hz */
	ICP201XX_OP_MODE2     ,  /* Mode 2: Bw:10 Hz ODR: 40Hz */
	ICP201XX_OP_MODE3     ,  /* Mode 3: Bw:0.5 Hz ODR: 2Hz */ 
	ICP201XX_OP_MODE4     ,  /* Mode 4: User configurable Mode */ 
	ICP201XX_OP_MODE_MAX
}icp201xx_op_mode_t;

typedef enum icp201xx_forced_meas_trigger {
	ICP201XX_FORCE_MEAS_STANDBY = 0,			/* Stay in Stand by */
	ICP201XX_FORCE_MEAS_TRIGGER_FORCE_MEAS = 1	/* Trigger for forced measurements */
}icp201xx_forced_meas_trigger_t;

typedef enum icp201xx_meas_mode
{
	ICP201XX_MEAS_MODE_FORCED_TRIGGER = 0, /* Force trigger mode based on icp201xx_forced_meas_trigger_t **/
	ICP201XX_MEAS_MODE_CONTINUOUS = 1   /* Continuous measurements based on selected mode ODR settings*/
}icp201xx_meas_mode_t;

typedef enum icp201xx_power_mode
{
	ICP201XX_POWER_MODE_NORMAL = 0,  /* Normal Mode: Device is in standby and goes to active mode during the execution of a measurement */
	ICP201XX_POWER_MODE_ACTIVE = 1   /* Active Mode: Power on DVDD and enable the high frequency clock */
}icp201xx_power_mode_t;


typedef enum icp201xx_FIFO_readout_mode
{
	ICP201XX_FIFO_READOUT_MODE_PRES_TEMP = 0,   /* Pressure and temperature as pair and address wraps to the start address of the Pressure value ( pressure first ) */
	ICP201XX_FIFO_READOUT_MODE_TEMP_ONLY = 1,   /* Temperature only reporting */
	ICP201XX_FIFO_READOUT_MODE_TEMP_PRES = 2,   /* Pressure and temperature as pair and address wraps to the start address of the Temperature value ( Temperature first ) */
	ICP201XX_FIFO_READOUT_MODE_PRES_ONLY = 3    /* Pressure only reporting */
}icp201xx_FIFO_readout_mode_t;


#define ICP201XX_INT_MASK_PRESS_DELTA      (0X01 << 6)
#define ICP201XX_INT_MASK_PRESS_ABS        (0X01 << 5)
#define ICP201XX_INT_MASK_FIFO_WMK_LOW     (0X01 << 3)
#define ICP201XX_INT_MASK_FIFO_WMK_HIGH    (0X01 << 2)
#define ICP201XX_INT_MASK_FIFO_UNDER_FLOW  (0X01 << 1)
#define ICP201XX_INT_MASK_FIFO_OVER_FLOW   (0X01 << 0)	



#define ICP201XX_INT_STATUS_PRESS_DELTA      (0X01 << 6)
#define ICP201XX_INT_STATUS_PRESS_ABS        (0X01 << 5)
#define ICP201XX_INT_STATUS_FIFO_WMK_LOW     (0X01 << 3)
#define ICP201XX_INT_STATUS_FIFO_WMK_HIGH    (0X01 << 2)
#define ICP201XX_INT_STATUS_FIFO_UNDER_FLOW  (0X01 << 1)
#define ICP201XX_INT_STATUS_FIFO_OVER_FLOW   (0X01 << 0)

/********************************************
Register Name:
Bank         : Main registers
Register Type: READ/WRITE
Register Address: 5(Decimal); 5 (Hex)
********************************************/
#define BIT_PEFE_OFFSET_TRIM_MASK			0x3F

/********************************************
Register Name:
Bank         : Main registers
Register Type: READ/WRITE
Register Address: 6(Decimal); 6 (Hex)
********************************************/
#define BIT_HFOSC_OFFSET_TRIM_MASK			0x7F

/********************************************
Register Name:
Bank         : Main registers
Register Type: READ/WRITE
Register Address: 7(Decimal); 7 (Hex)
********************************************/
#define BIT_PEFE_GAIN_TRIM_MASK			0x70

#define BIT_PEFE_GAIN_TRIM_POS			4

/********************************************
Register Name:
Bank         : Main registers
Register Type: READ/WRITE
Register Address: 12(Decimal); 0X0C (Hex)
********************************************/

/********************************************
Register Name: IIR_K_FACTOR_LSB
Register Type: READ/WRITE
Register Address: 120 (Decimal); 78 (Hex)
********************************************/

/********************************************
Register Name: IIR_K_FACTOR_MSB
Register Type: READ/WRITE
Register Address: 121 (Decimal); 79 (Hex)
********************************************/
/********************************************
Register Name: OTP Config 1
Bank         : otp registers
Register Type: READ/WRITE
Register Address: 172 (Decimal); AC (Hex)
********************************************/
#define BIT_OTP_CONFIG1_WRITE_SWITCH_MASK			0x02
#define BIT_OTP_CONFIG1_OTP_ENABLE_MASK				0x01
#define BIT_OTP_CONFIG1_WRITE_SWITCH_POS			1

/********************************************
Register Name: OTP MR LSB
Bank         : otp registers
Register Type: READ/WRITE
Register Address: 173 (Decimal); AD (Hex)
********************************************/

/********************************************
Register Name: OTP MR MSB
Bank         : otp registers
Register Type: READ/WRITE
Register Address: 174 (Decimal); AE (Hex)
********************************************/
/********************************************
Register Name: OTP MRA LSB
Bank         : otp registers
Register Type: READ/WRITE
Register Address: 175 (Decimal); AF (Hex)
********************************************/

/********************************************
Register Name: OTP MRA MSB
Bank         : otp registers
Register Type: READ/WRITE
Register Address: 176 (Decimal); B0 (Hex)
********************************************/

/********************************************
Register Name: OTP MRB LSB
Bank         : otp registers
Register Type: READ/WRITE
Register Address: 177 (Decimal); B1 (Hex)
********************************************/


/********************************************
Register Name: OTP MRB MSB
Bank         : otp registers
Register Type: READ/WRITE
Register Address: 178 (Decimal); B2 (Hex)
********************************************/
/********************************************
Register Name: OTP ADDR
Bank         : otp registers
Register Type: READ/WRITE
Register Address: 181 (Decimal); B5 (Hex)
********************************************/

/********************************************
Register Name: OTP CMD
Bank         : otp register
Register Type: READ/WRITE
Register Address: 182 (Decimal); B6 (Hex)
********************************************/

/********************************************
Register Name: OTP Read Reg
Bank         : otp registers
Register Type: READ/WRITE
Register Address: 184 (Decimal); B8 (Hex)
********************************************/

/********************************************
Register Name: OTP status
Bank         : otp registers
Register Type: READ/WRITE
Register Address: 185 (Decimal); B9 (Hex)
********************************************/


/********************************************
Register Name: OTP Debug2
Bank         : OTP registers
Register Type: READ/WRITE
Register Address:  180(Decimal); BC (Hex)
********************************************/
#define BIT_OTP_DBG2_RESET_MASK   0x80
#define BIT_OTP_DBG2_RESET_POS	  7


/********************************************
Register Name: Master lock
Bank         : OTP registers
Register Type: READ/WRITE
Register Address:  190(Decimal); BE (Hex)
********************************************/

/********************************************
Register Name: MODE_SELECT
Register Type: READ/WRITE
Register Address: 192 (Decimal); C0 (Hex)
********************************************/
#define BIT_MEAS_CONFIG_MASK          0xE0
#define BIT_FORCED_MEAS_TRIGGER_MASK  0x10
#define BIT_MEAS_MODE_MASK            0x08
#define BIT_POWER_MODE_MASK           0x04
#define BIT_FIFO_READOUT_MODE_MASK    0x03

#define BIT_MEAS_CONFIG_POS				5
#define BIT_FORCED_MEAS_TRIGGER_POS	    4
#define BIT_FORCED_MEAS_MODE_POS	    3
#define BIT_FORCED_POW_MODE_POS			2	


/********************************************
Register Name: INTERRUPT_STATUS
Register Type: READ/WRITE
Register Address: 193 (Decimal); C1(Hex)
********************************************/


/********************************************
Register Name: INTERRUPT_MASK
Register Type: READ/WRITE
Register Address: 194 (Decimal); C2(Hex)
********************************************/


/********************************************
Register Name: FIFO_CONFIG
Register Type: READ/WRITE
Register Address: 195 (Decimal); C3(Hex)
********************************************/
#define BIT_FIFO_WM_HIGH_MASK    0xF0
#define BIT_FIFO_WM_LOW_MASK     0x0F

#define BIT_FIFO_WM_HIGH_POS     3


/********************************************
Register Name: FIFO_FILL
Register Type: READ/WRITE
Register Address: 196 (Decimal); C4 (Hex)
********************************************/
#define BIT_FIFO_FLUSH_MASK				0x80
#define BIT_FIFO_EMPTY_STATUS_MASK		0x40
#define BIT_FIFO_FULL_STATUS_MASK		0x20
#define BIT_FIFO_LEVEL_MASK				0x1F

#define BIT_FIFO_EMPTY_POS     6
#define BIT_FIFO_FULL_POS      5


/********************************************
Register Name: SPI_MODE
Register Type: READ/WRITE
Register Address: 197 (Decimal); C5 (Hex)
********************************************/
typedef enum icp201xx_spi_mode {
	ICP201XX_SPI_MODE_4_WIRE = 0,
	ICP201XX_SPI_MODE_3_WIRE,
}icp201xx_spi_mode_t;

#define BIT_FIFO_SPI_MODE_MASK    0x01  
                                 // 0: SPI 4-WIRE
								 // 1: SPI 3-WIRE

/********************************************
Register Name: PRESS_ABS_LSB
Register Type: READ/WRITE
Register Address: 199 (Decimal); C7 (Hex)
********************************************/

/********************************************
Register Name: PRESS_ABS_MSB
Register Type: READ/WRITE
Register Address: 200 (Decimal); C8 (Hex)
********************************************/

/********************************************
Register Name: PRESS_DELTA_LSB
Register Type: READ/WRITE
Register Address: 201 (Decimal); C9 (Hex)
********************************************/

/********************************************
Register Name: PRESS_DELTA_MSB
Register Type: READ/WRITE
Register Address: 202 (Decimal); CA (Hex)
********************************************/

/********************************************
Register Name: DEVICE_STATUS
Register Type: READ
Register Address: 205 (Decimal); CD (Hex)
********************************************/
#define BIT_DEVICE_STATUS_LP_SEQ_STATE_MASK			0X06

#define BIT_OTP_STATUS2_BOOTUP_STATUS_MASK   		0x01
#define BIT_DEVICE_STATUS_MODE_SYNC_STATUS_MASK     0x01
                                      // 0 : Mode sync is going on, MODE_SELECT Reg is NOT accessible.
									  // 1 : MODE_SELECT Reg is accessible.
									  
#define BIT_DEVICE_STATUS_LP_SEQ_STATE_POS			1

/**********************************************************************************************************************/
/*****                               additionnal define for mode4                **************************************/
/**********************************************************************************************************************/

#define MPUREG_MODE4_OSR_PRESS			((icp201xx_dev_version == ICP201XX_CHIP_VERSION_A) ?	0x2C : 0x3C )
#define MPUREG_MODE4_CONFIG1			((icp201xx_dev_version == ICP201XX_CHIP_VERSION_A) ?	0x2D : 0x3D )	
#define MPUREG_MODE4_ODR_LSB			((icp201xx_dev_version == ICP201XX_CHIP_VERSION_A) ?	0x2E : 0x3E )		
#define MPUREG_MODE4_CONFIG2			((icp201xx_dev_version == ICP201XX_CHIP_VERSION_A) ?	0x2F : 0x3F )		
#define MPUREG_MODE4_BS_VALUE			((icp201xx_dev_version == ICP201XX_CHIP_VERSION_A) ?	0x30 : 0x40 )		

#define MPUREG_MODE4_PRESS_GAIN_FACTOR_LSB		     ((icp201xx_dev_version == ICP201XX_CHIP_VERSION_A) ?	0x82 : 0x92 ) 
#define MPUREG_MODE4_PRESS_GAIN_FACTOR_MSB		 	 ((icp201xx_dev_version == ICP201XX_CHIP_VERSION_A) ?	0x83 : 0x93 ) 


/********************************************
Register Name: MODE4_OSR_PRESS
Register Type: READ/WRITE
Register Address: 44 (Decimal); 2C (Hex)
********************************************/


/********************************************
Register Name: MODE4_CONFIG1
Register Type: READ/WRITE
Register Address: 45 (Decimal); 2D (Hex)
********************************************/
#define BIT_MODE4_CONFIG1_OSR_TEMP_MASK         0x1F
#define BIT_MODE4_CONFIG1_FIR_EN_MASK			0x20
#define BIT_MODE4_CONFIG1_IIR_EN_MASK			0x40

#define BIT_MODE4_CONFIG1_FIR_EN_POS			5
#define BIT_MODE4_CONFIG1_IIR_EN_POS			6



/********************************************
Register Name: MODE4_ODR_LSB
Register Type: READ/WRITE
Register Address: 46 (Decimal); 2E (Hex)
********************************************/

/********************************************
Register Name: MODE4_CONFIG2
Register Type: READ/WRITE
Register Address: 47 (Decimal); 2F (Hex)
********************************************/
#define BIT_MODE4_CONFIG2_ODR_MSB_MASK           0x1F
#define BIT_MODE4_CONFIG2_DVDD_ON_MASK			 0x20
#define BIT_MODE4_CONFIG2_HFOSC_ON_MASK			 0x40

#define BIT_MODE4_CONFIG2_DVDD_ON_POS			5
#define BIT_MODE4_CONFIG2_HFOSC_ON_POS			6


/********************************************
Register Name:
Bank         : Main registers
Register Type: READ/WRITE
Register Address: 48(Decimal); 30(Hex)
********************************************/
#define BIT_MODE4_BS_VALUE_PRESS		0x0F
#define BIT_MODE4_BS_VALUE_TEMP		    0XF0                                                                        

/** User configurable MODE4 parameters **/
/* osr_pressure_settings = ( OSR / 2 ^ 5 ) -1 **/
#define ICP201XX_MODE4_CONFIG_PRESS_OSR         0xb1
#define ICP201XX_MODE4_CONFIG_TEMP_OSR          0X0F
/* odr_setting = ( 8000 / ODR in Hz ) -1  : 25 Hz => ODR setting = 320(0x140) **/
#define ICP201XX_MODE4_CONFIG_ODR_SETTING       0x140 
#define ICP201XX_MODE4_CONFIG_HFOSC_EN          0
#define ICP201XX_MODE4_CONFIG_DVDD_EN           0
#define ICP201XX_MODE4_CONFIG_IIR_EN            0
#define ICP201XX_MODE4_CONFIG_FIR_EN            0
#define ICP201XX_MODE4_CONFIG_IIR_K_FACTOR      1

#ifdef __cplusplus
}
#endif

#endif/* TDK_BARO_H */
