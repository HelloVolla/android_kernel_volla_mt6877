#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <accel.h>
#include "cust_acc.h"
#include "sensors_io.h"

#define STK_MTK

#define STK_HEADER_VERSION          "0.0.2"

/*****************************************************************************
 * Global variable
 *****************************************************************************/
//#define STK_INTERRUPT_MODE
#define STK_POLLING_MODE
//#define STK_AMD /* Turn ON any motion */
//#define STK_HW_STEP_COUNTER /* Turn on step counter */
//#define STK_CALI /* Turn on sensortek calibration feature */
//#define STK_CALI_FILE /* Write cali data to file */
#define STK_FIR /* low-pass mode */
//#define STK_ZG
    /* enable Zero-G simulation.
     * This feature only works when both of STK_FIR and STK_ZG are turn ON. */
//#define STK_AUTOK /* Auto cali */

#ifdef STK_QUALCOMM
    #include <linux/sensors.h>
    #undef STK_SPREADTRUM
#elif defined STK_MTK
    #undef STK_INTERRUPT_MODE
    #undef STK_POLLING_MODE
    #undef STK_AMD
    #define STK_CALI
#elif defined STK_SPREADTRUM
    #include <linux/limits.h>
    #include <linux/version.h>
    #undef STK_INTERRUPT_MODE
    #define STK_POLLING_MODE
#elif defined STK_ALLWINNER
    #undef STK_INTERRUPT_MODE
    #define STK_POLLING_MODE
    #undef STK_AMD
#endif /* STK_QUALCOMM, STK_MTK, STK_SPREADTRUM, or STK_ALLWINNER */

/* Any motion only works under either STK_INTERRUPT_MODE or STK_POLLING_MODE */
#ifdef STK_AMD
    #if !defined STK_INTERRUPT_MODE && !defined STK_POLLING_MODE
        #undef STK_AMD
    #endif /* !defined STK_INTERRUPT_MODE && !defined STK_POLLING_MODE*/
#endif /* STK_AMD */

#ifdef STK_AUTOK
    #define STK_CALI
    #define STK_CALI_FILE
#endif /* STK_AUTOK */

#ifdef STK_FIR
    #define STK_FIR_LEN         2
    #define STK_FIR_LEN_MAX     32
    struct data_fir
    {
        s16 xyz[STK_FIR_LEN_MAX][3];
        int sum[3];
        int idx;
        int count;
    };
#endif /* STK_FIR */

#if (defined STK_FIR && defined STK_ZG)
    #define ZG_FACTOR   0
#endif /* defined STK_FIR && defined STK_ZG */

#ifndef GRAVITY_EARTH_1000
#define GRAVITY_EARTH_1000 9.876
#endif // !GRAVITY_EARTH_1000

/*****************************************************************************
 * stk8xxx register, start
 *****************************************************************************/
#define STK8XXX_REG_CHIPID          0x00
#define STK8XXX_REG_XOUT1           0x02
#define STK8XXX_REG_XOUT2           0x03
#define STK8XXX_REG_YOUT1           0x04
#define STK8XXX_REG_YOUT2           0x05
#define STK8XXX_REG_ZOUT1           0x06
#define STK8XXX_REG_ZOUT2           0x07
#define STK8XXX_REG_INTSTS1         0x09
#define STK8XXX_REG_INTSTS2         0x0A
#define STK8XXX_REG_FIFOSTS         0x0C
#define STK8XXX_REG_STEPOUT1        0x0D
#define STK8XXX_REG_STEPOUT2        0x0E
#define STK8XXX_REG_RANGESEL        0x0F
#define STK8XXX_REG_BWSEL           0x10
#define STK8XXX_REG_POWMODE         0x11
#define STK8XXX_REG_ODRMODE         0x12
#define STK8XXX_REG_SWRST           0x14
#define STK8XXX_REG_INTEN1          0x16
#define STK8XXX_REG_INTEN2          0x17
#define STK8XXX_REG_INTMAP1         0x19
#define STK8XXX_REG_INTMAP2         0x1A
#define STK8XXX_REG_INTCFG1         0x20
#define STK8XXX_REG_INTCFG2         0x21
#define STK8XXX_REG_SLOPEDLY        0x27
#define STK8XXX_REG_SLOPETHD        0x28
#define STK8XXX_REG_SIGMOT1         0x29
#define STK8XXX_REG_SIGMOT2         0x2A
#define STK8XXX_REG_SIGMOT3         0x2B
#define STK8XXX_REG_STEPCNT1        0x2C
#define STK8XXX_REG_STEPCNT2        0x2D
#define STK8XXX_REG_STEPTHD         0x2E
#define STK8XXX_REG_STEPDEB         0x2F
#define STK8XXX_REG_STEPMAXTW       0x31
#define STK8XXX_REG_INTFCFG         0x34
#define STK8XXX_REG_OFSTCOMP1       0x36
#define STK8XXX_REG_OFSTX           0x38
#define STK8XXX_REG_OFSTY           0x39
#define STK8XXX_REG_OFSTZ           0x3A
#define STK8XXX_REG_CFG1            0x3D
#define STK8XXX_REG_CFG2            0x3E
#define STK8XXX_REG_FIFOOUT         0x3F

/* STK8XXX_REG_CHIPID */
#define STK8BA50_R_ID                       0x86
#define STK8BA53_ID                         0x87
#define STK8323_ID                          0x23 /* include for STK8321 */
#define STK8327_ID                          0x26
#define STK8329_ID                          0x25
static const u8 STK_ID[] = {STK8BA50_R_ID, STK8BA53_ID, STK8323_ID, STK8327_ID,STK8329_ID};

/* STK8XXX_REG_INTSTS1 */
#define STK8XXX_INTSTS1_SIG_MOT_STS         0x1
#define STK8XXX_INTSTS1_ANY_MOT_STS         0x4

/* STK8XXX_REG_INTSTS2 */
#define STK8XXX_INTSTS2_FWM_STS_MASK        0x40

/* STK8XXX_REG_FIFOSTS */
#define STK8XXX_FIFOSTS_FIFOOVER            0x80
#define STK8XXX_FIFOSTS_FIFO_FRAME_CNT_MASK 0x7F

/* STK8XXX_REG_RANGESEL */
#define STK8XXX_RANGESEL_2G                 0x3
#define STK8XXX_RANGESEL_4G                 0x5
#define STK8XXX_RANGESEL_8G                 0x8
#define STK8XXX_RANGESEL_16G                0xc
#define STK8XXX_RANGESEL_BW_MASK            0xF
#define STK8XXX_RANGESEL_DEF                STK8XXX_RANGESEL_2G
typedef enum
{
    STK_2G = STK8XXX_RANGESEL_2G,
    STK_4G = STK8XXX_RANGESEL_4G,
    STK_8G = STK8XXX_RANGESEL_8G,
    STK_16G = STK8XXX_RANGESEL_16G
} stk_rangesel;

/* STK8XXX_REG_BWSEL */
#define STK8XXX_BWSEL_BW_7_81               0x08    /* ODR = BW x 2 = 15.62Hz */
#define STK8XXX_BWSEL_BW_15_63              0x09    /* ODR = BW x 2 = 31.26Hz */
#define STK8XXX_BWSEL_BW_31_25              0x0A    /* ODR = BW x 2 = 62.5Hz */
#define STK8XXX_BWSEL_BW_62_5               0x0B    /* ODR = BW x 2 = 125Hz */
#define STK8XXX_BWSEL_BW_125                0x0C    /* ODR = BW x 2 = 250Hz */
#define STK8XXX_BWSEL_BW_250                0x0D    /* ODR = BW x 2 = 500Hz */
#define STK8XXX_BWSEL_BW_500                0x0E    /* ODR = BW x 2 = 1000Hz */

/* STK8XXX_REG_POWMODE */
#define STK8XXX_PWMD_SUSPEND                0x80
#define STK8XXX_PWMD_LOWPOWER               0x40
#define STK8XXX_PWMD_SLEEP_TIMER            0x20
#define STK8XXX_PWMD_NORMAL                 0x00
/* STK8XXX_REG_ODRMODE */
#define STK8XXX_ODR_NORMODE                 0x00
#define STK8XXX_ODR_ESMMODE                 0x08

/* STK8XXX_SLEEP_DUR */
#define STK8XXX_PWMD_1                      0x1E
#define STK8XXX_PWMD_2                      0x1C
#define STK8XXX_PWMD_10                     0x1A
#define STK8XXX_PWMD_20                     0x18
#define STK8XXX_PWMD_25                     0x16
#define STK8XXX_PWMD_50                     0x14
#define STK8XXX_PWMD_100                    0x12
#define STK8XXX_PWMD_163                    0x10
#define STK8XXX_PWMD_200                    0x0E
#define STK8XXX_PWMD_300                    0x0C
#define STK8XXX_PWMD_SLP_MASK               0x7E

/* STK8XXX_REG_SWRST */
#define STK8XXX_SWRST_VAL                   0xB6

/* STK8XXX_REG_INTEN1 */
#define STK8XXX_INTEN1_SLP_EN_XYZ           0x07

/* STK8XXX_REG_INTEN2 */
#define STK8XXX_INTEN2_DATA_EN              0x10
#define STK8XXX_INTEN2_FWM_EN               0x40

/* STK8XXX_REG_INTMAP1 */
#define STK8XXX_INTMAP1_SIGMOT2INT1         0x01
#define STK8XXX_INTMAP1_ANYMOT2INT1         0x04

/* STK8XXX_REG_INTMAP2 */
#define STK8XXX_INTMAP2_DATA2INT1           0x01
#define STK8XXX_INTMAP2_FWM2INT1            0x02
#define STK8XXX_INTMAP2_FWM2INT2            0x40

/* STK8XXX_REG_INTCFG1 */
#define STK8XXX_INTCFG1_INT1_ACTIVE_H       0x01
#define STK8XXX_INTCFG1_INT1_OD_PUSHPULL    0x00
#define STK8XXX_INTCFG1_INT2_ACTIVE_H       0x04
#define STK8XXX_INTCFG1_INT2_OD_PUSHPULL    0x00

/* STK8XXX_REG_INTCFG2 */
#define STK8XXX_INTCFG2_NOLATCHED           0x00
#define STK8XXX_INTCFG2_LATCHED             0x0F
#define STK8XXX_INTCFG2_INT_RST             0x80

/* STK8XXX_REG_SLOPETHD */
#define STK8XXX_SLOPETHD_DEF                0x14

/* STK8XXX_REG_SIGMOT1 */
#define STK8XXX_SIGMOT1_SKIP_TIME_3SEC      0x96    /* default value */

/* STK8XXX_REG_SIGMOT2 */
#define STK8XXX_SIGMOT2_SIG_MOT_EN          0x02
#define STK8XXX_SIGMOT2_ANY_MOT_EN          0x04

/* STK8XXX_REG_SIGMOT3 */
#define STK8XXX_SIGMOT3_PROOF_TIME_1SEC     0x32    /* default value */

/* STK8XXX_REG_STEPCNT2 */
#define STK8XXX_STEPCNT2_RST_CNT            0x04
#define STK8XXX_STEPCNT2_STEP_CNT_EN        0x08

/* STK8XXX_REG_INTFCFG */
#define STK8XXX_INTFCFG_I2C_WDT_EN          0x04

/* STK8XXX_REG_OFSTCOMP1 */
#define STK8XXX_OFSTCOMP1_OFST_RST          0x80

/* STK8XXX_REG_CFG1 */
/* the maximum space for FIFO is 32*3 bytes */
#define STK8XXX_CFG1_XYZ_FRAME_MAX          32

/* STK8XXX_REG_CFG2 */
#define STK8XXX_CFG2_FIFO_MODE_BYPASS       0x0
#define STK8XXX_CFG2_FIFO_MODE_FIFO         0x1
#define STK8XXX_CFG2_FIFO_MODE_SHIFT        5
#define STK8XXX_CFG2_FIFO_DATA_SEL_XYZ      0x0
#define STK8XXX_CFG2_FIFO_DATA_SEL_X        0x1
#define STK8XXX_CFG2_FIFO_DATA_SEL_Y        0x2
#define STK8XXX_CFG2_FIFO_DATA_SEL_Z        0x3
#define STK8XXX_CFG2_FIFO_DATA_SEL_MASK     0x3

/* STK8XXX_REG_OFSTx */
#define STK8XXX_OFST_LSB                    128     /* 8 bits for +-1G */
/*****************************************************************************
 * stk8xxx register, end
 *****************************************************************************/

#define STK8XXX_NAME    "stk8xxx"

struct stk_data {

    struct spi_device           *spi;
    struct i2c_client           *client;
    const struct stk_bus_ops    *bops;
    struct mutex                i2c_lock;           /* mutex lock for register R/W */
    u8                          *spi_buffer;        /* SPI buffer, used for SPI transfer. */
    atomic_t                    enabled;            /* chip is enabled or not */
    bool                        temp_enable;        /* record current power status. For Suspend/Resume used. */
    u8                          power_mode;
    u8                          pid;
    bool                        is_esm_mode;
	u8                          data_shift;
	u8                          odr_table_count;
    bool                        fifo;               /* Support FIFO or not */
    int                         direction;
    s16                         xyz[3];             /* The latest data of xyz */
    int                         sr_no;              /* Serial number of stkODRTable */
    int                         sensitivity;        /* sensitivity, bit number per G */
    u8                          recv;
    atomic_t                    selftest;           /* selftest result */
#ifdef STK_CALI
    s16                         cali_sw[3];
    atomic_t                    cali_status;        /* cali status */
#ifdef STK_AUTOK
    atomic_t                    first_enable;
    u8                          offset[3];          /* offset value for STK8XXX_REG_OFSTX~Z */
#endif /* STK_AUTOK */
#endif /* STK_CALI */
#ifdef STK_HW_STEP_COUNTER
    int                         steps;              /* The latest step counter value */
#endif /* STK_HW_STEP_COUNTER */
#ifdef STK_INTERRUPT_MODE
    struct work_struct          stk_work; 
    int                         int_pin;
    int                         irq;
#elif defined STK_POLLING_MODE
    struct work_struct          stk_work; 
    struct hrtimer              stk_timer;
    ktime_t                     poll_delay;
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */
#if defined STK_QUALCOMM || defined STK_SPREADTRUM
#ifdef STK_QUALCOMM
    struct sensors_classdev     accel_cdev;
    u64                         fifo_start_ns;
#endif /* STK_QUALCOMM */
    struct input_dev            *input_dev_accel;   /* accel data */
    ktime_t                     timestamp;
#ifdef STK_AMD
    struct input_dev            *input_dev_amd;     /* any motion data */
#endif /* STK_AMD */
#elif defined STK_MTK
#ifndef STK_MTK_2_0
    struct acc_hw               hw;
    struct hwmsen_convert       cvt;
#else //2.0
	struct hf_device            hf_dev;
#endif
#endif /* STK_QUALCOMM, STK_SPREADTRUM, STK_MTK */
#ifdef STK_FIR
    struct data_fir             fir;
    /*
     * fir_len
     * 0: turn OFF FIR operation
     * 1 ~ STK_FIR_LEN_MAX: turn ON FIR operation
     */
    atomic_t                    fir_len;
#endif /* STK_FIR */
};

#define STK_ACC_TAG                 "[stkAccel]"
#define STK_ACC_FUN(f)              printk(KERN_INFO STK_ACC_TAG" %s\n", __FUNCTION__)
#define STK_ACC_LOG(fmt, args...)   printk(KERN_INFO STK_ACC_TAG" %s/%d: "fmt"\n", __FUNCTION__, __LINE__, ##args)
#define STK_ACC_ERR(fmt, args...)   printk(KERN_ERR STK_ACC_TAG" %s/%d: "fmt"\n", __FUNCTION__, __LINE__, ##args)

struct stk_bus_ops
{
    int (*read)(struct stk_data *, unsigned char, unsigned char *);
    int (*read_block)(struct stk_data *, unsigned char, int, unsigned char *);
    int (*write)(struct stk_data *, unsigned char, unsigned char);
};

#define STK_REG_READ(stk, addr, val)            (stk->bops->read(stk, addr, val))
#define STK_REG_READ_BLOCK(stk, addr, len, val) (stk->bops->read_block(stk, addr, len, val))
#define STK_REG_WRITE(stk, addr, val)           (stk->bops->write(stk, addr, val))

typedef struct {
    uint8_t     regBwsel;
    uint8_t     regPwsel;
    int         sample_rate_us;
	int         drop;
} _stkOdrMap;

_stkOdrMap stk_odr_map[20];

/* SENSOR_HZ(_hz) = ((uint32_t)((_hz)*1024.0f)) */
static const _stkOdrMap stkODRTable[] =
{
	/* 0: ODR = 1Hz */
	{
		.sample_rate_us = 1000000,
		.regBwsel = STK8XXX_BWSEL_BW_7_81,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 15
	},
	/* 1: ODR = 5Hz */
	{
		.sample_rate_us = 200000,
		.regBwsel = STK8XXX_BWSEL_BW_7_81,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0
	},
	/* 2: ODR = 15.62Hz */
	{
		.sample_rate_us = 62500,
		.regBwsel = STK8XXX_BWSEL_BW_7_81,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0
	},
	/* 3: ODR = 31.26Hz */
	{
		.sample_rate_us = 32000,
		.regBwsel = STK8XXX_BWSEL_BW_15_63,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0
	},
	/* 4: ODR = 50Hz */
	{
		.sample_rate_us = 20000,
		.regBwsel = STK8XXX_BWSEL_BW_125,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 5
	},
	/* 5: ODR = 125Hz */
	{
		.sample_rate_us = 8000,
		.regBwsel = STK8XXX_BWSEL_BW_62_5,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0
	},
	/* 6: 200Hz */
	{
		.sample_rate_us = 5000,
		.regBwsel = STK8XXX_BWSEL_BW_500,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 5
	},
	/* 7: 250Hz */
	{
		.sample_rate_us = 4000,
		.regBwsel = STK8XXX_BWSEL_BW_125,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0
	},
	/* 7: ODR = 400Hz */
	{
		.regBwsel = STK8XXX_BWSEL_BW_250,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0
	},
};

/* ESM mode only for stk8327 & stk8329 */
static const _stkOdrMap stkESMTable[] =
{
	/* 0: ODR = 1Hz */
	{
		.sample_rate_us = 1000000,
		.regBwsel = STK8XXX_BWSEL_BW_7_81,
		.regPwsel = STK8XXX_PWMD_LOWPOWER | STK8XXX_PWMD_SLEEP_TIMER | STK8XXX_PWMD_1,
		.drop = 0
	},
	/* 1: ODR = 2Hz */
	{
		.sample_rate_us = 500000,
		.regBwsel = STK8XXX_BWSEL_BW_7_81,
		.regPwsel = STK8XXX_PWMD_LOWPOWER | STK8XXX_PWMD_SLEEP_TIMER | STK8XXX_PWMD_2,
		.drop = 0
	},
	/* 2: ODR = 5Hz (normal mode + FIFO) */
	{
		.sample_rate_us = 200000,
		.regBwsel = STK8XXX_BWSEL_BW_7_81,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0

	},
	/* 3: ODR = 10Hz */
	{
		.sample_rate_us = 100000,
		.regBwsel = STK8XXX_BWSEL_BW_31_25,
		.regPwsel = STK8XXX_PWMD_LOWPOWER | STK8XXX_PWMD_SLEEP_TIMER | STK8XXX_PWMD_10,
		.drop = 0
	},
	/* 4: ODR = 16Hz (normal mode)*/
	{
		.sample_rate_us = 62500,
		.regBwsel = STK8XXX_BWSEL_BW_7_81,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0
	},
	/* 5: ODR = 20Hz */
	{
		.sample_rate_us = 50000,
		.regBwsel = STK8XXX_BWSEL_BW_62_5,
		.regPwsel = STK8XXX_PWMD_LOWPOWER | STK8XXX_PWMD_SLEEP_TIMER | STK8XXX_PWMD_20,
		.drop = 0
	},
	/* 6: ODR = 25Hz */
	{
		.sample_rate_us = 40000,
		.regBwsel = STK8XXX_BWSEL_BW_62_5,
		.regPwsel = STK8XXX_PWMD_LOWPOWER | STK8XXX_PWMD_SLEEP_TIMER | STK8XXX_PWMD_25,
		.drop = 0
	},
	/* 7: ODR = 31Hz (normal mode)*/
	{
		.sample_rate_us = 32000,
		.regBwsel = STK8XXX_BWSEL_BW_15_63,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0
	},
	/* 8: ODR = 50Hz */
	{
		.sample_rate_us = 20000,
		.regBwsel = STK8XXX_BWSEL_BW_62_5,
		.regPwsel = STK8XXX_PWMD_LOWPOWER | STK8XXX_PWMD_SLEEP_TIMER | STK8XXX_PWMD_50,
		.drop = 0
	},
	/* 9: ODR = 62.5Hz (normal mode)*/
	{
		.sample_rate_us = 16000,
		.regBwsel = STK8XXX_BWSEL_BW_31_25,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0
	},
	/* 10: ODR = =100Hz */
	{
		.sample_rate_us = 10000,
		.regBwsel = STK8XXX_BWSEL_BW_250,
		.regPwsel = STK8XXX_PWMD_LOWPOWER | STK8XXX_PWMD_SLEEP_TIMER | STK8XXX_PWMD_100,
		.drop = 0
	},
	/* 11: ODR = 125Hz (normal mode)*/
	{
		.sample_rate_us = 8000,
		.regBwsel = STK8XXX_BWSEL_BW_62_5,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0
	},
	/* 12: ODR = =200Hz */
	{
		.sample_rate_us = 5000,
		.regBwsel = STK8XXX_BWSEL_BW_500,
		.regPwsel = STK8XXX_PWMD_LOWPOWER | STK8XXX_PWMD_SLEEP_TIMER | STK8XXX_PWMD_200,
		.drop = 0
	},
	/* 13: ODR = 250Hz (normal mode)*/
	{
		.sample_rate_us = 4000,
		.regBwsel = STK8XXX_BWSEL_BW_125,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0
	},
	/* 14: ODR = =300Hz */
	{
		.sample_rate_us = 3333,
		.regBwsel = STK8XXX_BWSEL_BW_500,
		.regPwsel = STK8XXX_PWMD_LOWPOWER | STK8XXX_PWMD_SLEEP_TIMER | STK8XXX_PWMD_300,
		.drop = 0
	},
	/* 15: ODR = 400Hz (normal mode + FIFO)*/
	{
		.sample_rate_us = 2500,
		.regBwsel = STK8XXX_BWSEL_BW_250,
		.regPwsel = STK8XXX_PWMD_NORMAL,
		.drop = 0
	},
};

#define STK_MIN_DELAY_US 2000   /* stkODRTable[4].sample_rate_us */
#define STK_MAX_DELAY_US 32000  /* stkODRTable[0].sample_rate_us */

/* selftest usage */
#define STK_SELFTEST_SAMPLE_NUM             100
#define STK_SELFTEST_RESULT_NA              0
#define STK_SELFTEST_RESULT_RUNNING         (1 << 0)
#define STK_SELFTEST_RESULT_NO_ERROR        (1 << 1)
#define STK_SELFTEST_RESULT_DRIVER_ERROR    (1 << 2)
#define STK_SELFTEST_RESULT_FAIL_X          (1 << 3)
#define STK_SELFTEST_RESULT_FAIL_Y          (1 << 4)
#define STK_SELFTEST_RESULT_FAIL_Z          (1 << 5)
#define STK_SELFTEST_RESULT_NO_OUTPUT       (1 << 6)
static inline int stk_selftest_offset_factor(int sen)
{
    return sen * 3 / 10;
}
static inline int stk_selftest_noise_factor(int sen)
{
    return sen / 10;
}

#ifdef STK_CALI
/* calibration parameters */
#define STK_CALI_SAMPLE_NO          10
#ifdef STK_CALI_FILE
#define STK_CALI_VER0               0x18
#define STK_CALI_VER1               0x03
#define STK_CALI_END                '\0'
#define STK_CALI_FILE_PATH          "/data/misc/stkacccali.conf"
#define STK_CALI_FILE_SIZE          25
#endif /* STK_CALI_FILE */
/* parameter for cali_status/atomic_t and cali file */
#define STK_K_SUCCESS_FILE          0x01
/* parameter for cali_status/atomic_t */
#define STK_K_FAIL_WRITE_OFST       0xF2
#define STK_K_FAIL_I2C              0xF8
#define STK_K_FAIL_W_FILE           0xFB
#define STK_K_FAIL_VERIFY_CALI      0xFD
#define STK_K_RUNNING               0xFE
#define STK_K_NO_CALI               0xFF
#endif /* STK_CALI */

#define STK_MTK_VERSION "0.0.1"
void stk_read_accel_data(struct stk_data *stk);
int stk_i2c_probe(struct i2c_client *client, const struct stk_bus_ops *stk8xxx_bus_ops);
int stk_i2c_remove(struct i2c_client *client);
static struct stk_data *gStk = NULL;

#ifdef CONFIG_OF
static struct of_device_id stk8xxx_match_table[] =
{
    { .compatible = "mediatek,gsensor", },
	{ .compatible = "sensortek,stk8xxx"},
    {}
};
#endif /* CONFIG_OF */
#define STK_DRI_GET_DATA(ddri) \
    dev_get_drvdata((struct device *)container_of(ddri, struct device, driver))
#define STK_ATTR_VERSION "0.0.1"
#define STK_DRV_I2C_VERSION "0.0.2"
#define STK_C_VERSION       "0.0.1"
void stk_data_initialize(struct stk_data *stk);
int stk_get_pid(struct stk_data *stk);
int stk_show_all_reg(struct stk_data *stk, char *show_buffer);
int stk_range_selection(struct stk_data *stk, stk_rangesel range);
void stk_set_enable(struct stk_data *stk, char en);
int stk_get_delay(struct stk_data *stk);
int stk_set_delay(struct stk_data *stk, int delay_us);
int stk_get_offset(struct stk_data *stk, u8 offset[3]);
int stk_set_offset(struct stk_data *stk, u8 offset[3]);
void stk_fifo_reading(struct stk_data *stk, u8 fifo[], int len);
int stk_change_fifo_status(struct stk_data *stk, u8 wm);
int stk_read_accel_rawdata(struct stk_data *stk);
int stk_reg_init(struct stk_data *stk, stk_rangesel range, int sr_no);
void stk_selftest(struct stk_data *stk);
#ifndef STK_MTK_2_0
int stk8xxx_suspend(struct device *dev);
int stk8xxx_resume(struct device *dev);
#endif
#if defined STK_INTERRUPT_MODE || defined STK_POLLING_MODE
void stk_work_queue(struct work_struct *work);
#endif /* defined STK_INTERRUPT_MODE || defined STK_POLLING_MODE */
#ifdef STK_INTERRUPT_MODE
int stk_irq_setup(struct stk_data *stk);
void stk_exit_irq_setup(struct stk_data *stk);
#endif /* STK_INTERRUPT_MODE */
#ifdef STK_HW_STEP_COUNTER
void stk_read_step_data(struct stk_data *stk);
void stk_turn_step_counter(struct stk_data *stk, bool turn);
#endif /* STK_HW_STEP_COUNTER */
#ifdef STK_FIR
void stk_low_pass_fir(struct stk_data *stk, s16 *xyz);
#endif /* STK_FIR */
#ifdef STK_CALI
#ifdef STK_CALI_FILE
int stk_write_cali_to_file(struct stk_data *stk, u8 offset[3], u8 status);
void stk_get_cali(struct stk_data *stk);
#endif /* STK_CALI_FILE */
void stk_set_cali(struct stk_data *stk);
void stk_reset_cali(struct stk_data *stk);
#endif /* STK_CALI */

/*
 * @brief: Initialize some data in stk_data.
 *
 * @param[in/out] stk: struct stk_data *
 */
void stk_data_initialize(struct stk_data *stk)
{
    atomic_set(&stk->enabled, 0);
    stk->temp_enable = false;
    stk->sr_no = stk->odr_table_count - 1;
    atomic_set(&stk->selftest, STK_SELFTEST_RESULT_NA);
#ifdef STK_CALI
    memset(stk->cali_sw, 0x0, sizeof(stk->cali_sw));
    atomic_set(&stk->cali_status, STK_K_NO_CALI);
#ifdef STK_AUTOK
    atomic_set(&stk->first_enable, 1);
    stk->offset[0] = 0;
    stk->offset[1] = 0;
    stk->offset[2] = 0;
#endif /* STK_AUTOK */
#endif /* STK_CALI */
#ifdef STK_HW_STEP_COUNTER
    stk->steps = 0;
#endif /* STK_HW_STEP_COUNTER */
#ifdef STK_FIR
    memset(&stk->fir, 0, sizeof(struct data_fir));
    atomic_set(&stk->fir_len, STK_FIR_LEN);
#endif /* STK_FIR */
    STK_ACC_LOG("done");
}

/*
 * @brief: SW reset for stk8xxx
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return: Success or fail.
 *          0: Success
 *          others: Fail
 */
static int stk_sw_reset(struct stk_data *stk)
{
    int err = 0;

    err = STK_REG_WRITE(stk, STK8XXX_REG_SWRST, STK8XXX_SWRST_VAL);

    if (err)
        return err;

    usleep_range(1000, 2000);
    stk->power_mode = STK8XXX_PWMD_NORMAL;
    atomic_set(&stk->enabled, 1);
    return 0;
}

/*
 * @brief: Read PID and write to stk_data.pid.
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return: Success or fail.
 *          0: Success
 *          others: Fail
 */
int stk_get_pid(struct stk_data *stk)
{
    int err = 0;
    u8 val = 0;
    err = STK_REG_READ(stk, STK8XXX_REG_CHIPID, &val);

    if (err) {
        STK_ACC_ERR("failed to read PID");
        return -EIO;
    } else {
        stk->pid = val;
    }

    
    switch (stk->pid)
    {
    case STK8BA50_R_ID:
        stk->data_shift = 6;// 10bit
        stk->fifo = false;
        stk->is_esm_mode = false;
        memcpy(&stk_odr_map, &stkODRTable, sizeof(stkODRTable));
        stk->odr_table_count = sizeof(stkODRTable) / sizeof(_stkOdrMap);
        break;
    case STK8BA53_ID:
        stk->data_shift = 4;// 12bit
        stk->fifo = false;
        stk->is_esm_mode = false;
        memcpy(&stk_odr_map, &stkODRTable, sizeof(stkODRTable));
        stk->odr_table_count = sizeof(stkODRTable) / sizeof(_stkOdrMap);
        break;
    case STK8323_ID:
        stk->data_shift = 4;// 12bit
        stk->fifo = true;
        stk->is_esm_mode = false;
        memcpy(&stk_odr_map, &stkODRTable, sizeof(stkODRTable));
        stk->odr_table_count = sizeof(stkODRTable) / sizeof(_stkOdrMap);
        break;
    case STK8327_ID:
    case STK8329_ID:
        stk->data_shift = 0;// 16bit
        stk->fifo = true;
        stk->is_esm_mode = true;
        memcpy(&stk_odr_map, &stkESMTable, sizeof(stkESMTable));
        stk->odr_table_count = sizeof(stkESMTable) / sizeof(_stkOdrMap);
        break;
        default:
            STK_ACC_ERR("Cannot find correct PID: 0x%X", stk->pid);
            break;
    }

    return err;
}

#ifdef STK_AMD
/*
 * @brief: Trigger INT_RST for latched STS
 *
 * @param[in/out] stk: struct stk_data *
 */
static void stk_reset_latched_int(struct stk_data *stk)
{
    u8 data = 0;

    if (STK_REG_READ(stk, STK8XXX_REG_INTCFG2, &data))
        return;

    if (STK_REG_WRITE(stk, STK8XXX_REG_INTCFG2,
                          (data | STK8XXX_INTCFG2_INT_RST)))
        return;
}
#endif /* STK_AMD */

/*
 * @brief: Read all register (0x0 ~ 0x3F)
 *
 * @param[in/out] stk: struct stk_data *
 * @param[out] show_buffer: record all register value
 *
 * @return: buffer length or fail
 *          positive value: return buffer length
 *          -1: Fail
 */
int stk_show_all_reg(struct stk_data *stk, char *show_buffer)
{
    int reg;
    int len = 0;
    u8 data = 0;

    if (NULL == show_buffer)
        return -1;

    for (reg = 0; reg <= 0x3F; reg++)
    {
        if (STK_REG_READ(stk, reg, &data))
        {
            len = -1;
            goto exit;
        }

        if (0 >= (PAGE_SIZE - len))
        {
            STK_ACC_ERR("print string out of PAGE_SIZE");
            goto exit;
        }

        len += scnprintf(show_buffer + len, PAGE_SIZE - len,
                         "[0x%2X]=0x%2X, ", reg, data);

        if (4 == reg % 5) {
            len += scnprintf(show_buffer + len, PAGE_SIZE - len,
                    "\n");
        }
    }

    len += scnprintf(show_buffer + len, PAGE_SIZE - len, "\n");
exit:

    return len;
}

/*
 * @brief: Get sensitivity. Set result to stk_data.sensitivity.
 *          sensitivity = number bit per G (LSB/g)
 *          Example: RANGESEL=8g, 12 bits for STK8xxx full resolution
 *          Ans: number bit per G = 2^12 / (8x2) = 256 (LSB/g)
 *
 * @param[in/out] stk: struct stk_data *
 */
static void stk_get_sensitivity(struct stk_data *stk)
{
    u8 val = 0;
    stk->sensitivity = 0;

    if ( 0 == STK_REG_READ(stk, STK8XXX_REG_RANGESEL, &val))
    {
        val &= STK8XXX_RANGESEL_BW_MASK;

        switch (val)
        {
            case STK8XXX_RANGESEL_2G:
                if(stk->pid==STK8327_ID ||
				   stk->pid==STK8329_ID)
                    stk->sensitivity = 16384;
                else if(stk->pid==STK8BA50_R_ID)
                    stk->sensitivity = 256;
                else
                     stk->sensitivity = 1024;
                break;

            case STK8XXX_RANGESEL_4G:
                if(stk->pid==STK8327_ID||
				   stk->pid==STK8329_ID)
                    stk->sensitivity = 8192;
                else if(stk->pid==STK8BA50_R_ID)
                    stk->sensitivity = 128;
                else
                    stk->sensitivity = 512;
                break;

            case STK8XXX_RANGESEL_8G:
                if(stk->pid==STK8327_ID||
				   stk->pid==STK8329_ID)
                    stk->sensitivity = 4096;
                else if(stk->pid==STK8BA50_R_ID)
                    stk->sensitivity = 64;
                else
                    stk->sensitivity = 256;
                break;

            case STK8XXX_RANGESEL_16G:
                stk->sensitivity = 2048;
                break;


            default:
                STK_ACC_ERR("got wrong RANGE: 0x%X", val);
                break;
        }
    }
}

/*
 * @brief: Set range
 *          1. Setting STK8XXX_REG_RANGESEL
 *          2. Calculate sensitivity and store to stk_data.sensitivity
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] range: range for STK8XXX_REG_RANGESEL
 *              STK_2G
 *              STK_4G
 *              STK_8G
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
int stk_range_selection(struct stk_data *stk, stk_rangesel range)
{
    int err = 0;
    err = STK_REG_WRITE(stk, STK8XXX_REG_RANGESEL, range);

    if (err)
        return err;

    stk_get_sensitivity(stk);
    return 0;
}

/*
 * @brief: Change power mode
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] pwd_md: power mode for STK8XXX_REG_POWMODE
 *              STK8XXX_PWMD_SUSPEND
 *              STK8XXX_PWMD_LOWPOWER
 *              STK8XXX_PWMD_NORMAL
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static int stk_change_power_mode(struct stk_data *stk, u8 pwd_md)
{
    if (pwd_md != stk->power_mode)
    {
        int err = 0;
        u8 reg = STK8XXX_REG_POWMODE, val = 0;
        err = STK_REG_READ(stk, reg, &val);

        if (err)
            return err;

        val &= STK8XXX_PWMD_SLP_MASK;
        err = STK_REG_WRITE(stk, reg, (val | pwd_md));

        if (err)
            return err;

        stk->power_mode = pwd_md;
    }
    else
    {
        STK_ACC_ERR("Same as original power mode: 0x%X", stk->power_mode);
    }

    return 0;
}

/*
 * stk_set_enable
 * @brief: Turn ON/OFF the power state of stk8xxx.
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] en: turn ON/OFF
 *              0 for suspend mode;
 *              1 for normal mode.
 */
void stk_set_enable(struct stk_data *stk, char en)
{
#ifdef STK_AUTOK
    if (atomic_read(&stk->first_enable)) {
        STK_ACC_LOG("stk_autok");
        atomic_set(&stk->first_enable, 0);
        /* TODO. vivien */
        stk_autok(stk);
    }
#endif /* STK_AUTOK */
    if (en == atomic_read(&stk->enabled))
        return;

    if (en)
    {
        if (stk_change_power_mode(stk, STK8XXX_PWMD_NORMAL))
        {
            STK_ACC_ERR("failed to change power mode, en:%d", en);
            return;
        }

#ifdef STK_INTERRUPT_MODE
        /* do nothing */
#elif defined STK_POLLING_MODE
        hrtimer_start(&stk->stk_timer, stk->poll_delay, HRTIMER_MODE_REL);
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */
    }
    else
    {
        if (stk_change_power_mode(stk, STK8XXX_PWMD_SUSPEND))
        {
            STK_ACC_ERR("failed to change power mode, en:%d", en);
            return;
        }

#ifdef STK_INTERRUPT_MODE
        /* do nothing */
#elif defined STK_POLLING_MODE
        hrtimer_cancel(&stk->stk_timer);
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */
    }


#ifdef STK_AMD
    stk_report_ANYMOTION(stk, false);
#endif /* STK_AMD */
    atomic_set(&stk->enabled, en);
}

/*
 * @brief: Get delay
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return: delay in usec
 */
int stk_get_delay(struct stk_data *stk)
{
    return stk_odr_map[stk->sr_no].sample_rate_us;
}

/*
 * @brief: Set delay
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] delay_us: delay in usec
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
int stk_set_delay(struct stk_data *stk, int delay_us)
{
    int err = 0;
    bool enable = false;
    int sr_no;

    for (sr_no = 0; sr_no < stk->odr_table_count; sr_no++) {
        if (delay_us >= stk_odr_map[sr_no].sample_rate_us)
            break;
    }

    if (sr_no >= stk->odr_table_count)
    {
        sr_no = stk->odr_table_count - 1;
    }

    stk->sr_no = sr_no;

    if (atomic_read(&stk->enabled))
    {
        stk_set_enable(stk, 0);
        enable = true;
    }

    if (stk->is_esm_mode)
    {
        err = STK_REG_WRITE(stk, STK8XXX_REG_ODRMODE, STK8XXX_ODR_ESMMODE);
        if (err)
        {
            STK_ACC_ERR("failed to set ESM mode");
        }
    }

    err = STK_REG_WRITE(stk, STK8XXX_REG_POWMODE, stk_odr_map[sr_no].regPwsel);
    if (err)
    {
        STK_ACC_ERR("failed to set Powmode");
    }

    err = STK_REG_WRITE(stk, STK8XXX_REG_BWSEL, stk_odr_map[sr_no].regBwsel);
    if (err)
    {
        STK_ACC_ERR("failed to change ODR");
    }

#ifdef STK_INTERRUPT_MODE
        /* do nothing */
#elif defined STK_POLLING_MODE
    stk->poll_delay = ns_to_ktime(stk_odr_map[sr_no].sample_rate_us * NSEC_PER_USEC);
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */

    if (enable)
    {
        stk_set_enable(stk, 1);
    }

    return err;
}

/*
 * @brief: Get offset
 *
 * @param[in/out] stk: struct stk_data *
 * @param[out] offset: offset value read from register
 *                  STK8XXX_REG_OFSTX,  STK8XXX_REG_OFSTY, STK8XXX_REG_OFSTZ
 *
 * @return: Success or fail
 *          0: Success
 *          -1: Fail
 */
int stk_get_offset(struct stk_data *stk, u8 offset[3])
{
    if (STK_REG_READ(stk, STK8XXX_REG_OFSTX, &offset[0]))
    {
        return -1;
    }

    if (STK_REG_READ(stk, STK8XXX_REG_OFSTY, &offset[1]))
    {
        return -1;
    }

    if (STK_REG_READ(stk, STK8XXX_REG_OFSTZ, &offset[2]))
    {
        return -1;
    }

    return 0;
}

/*
 * @brief: Set offset
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] offset: offset value write to register
 *                  STK8XXX_REG_OFSTX,  STK8XXX_REG_OFSTY, STK8XXX_REG_OFSTZ
 *
 * @return: Success or fail
 *          0: Success
 *          -1: Fail
 */
int stk_set_offset(struct stk_data *stk, u8 offset[3])
{
    int err = 0;
    bool enable = false;

    if (!atomic_read(&stk->enabled))
        stk_set_enable(stk, 1);
    else
        enable = true;

    if (STK_REG_WRITE(stk, STK8XXX_REG_OFSTX, offset[0]))
    {
        err = -1;
        goto exit;
    }

    if (STK_REG_WRITE(stk, STK8XXX_REG_OFSTY, offset[1]))
    {
        err = -1;
        goto exit;
    }

    if (STK_REG_WRITE(stk, STK8XXX_REG_OFSTZ, offset[2]))
    {
        err = -1;
        goto exit;
    }

exit:

    if (!enable)
        stk_set_enable(stk, 0);

    return err;
}

/*
 * @brief: Read FIFO data
 *
 * @param[in/out] stk: struct stk_data *
 * @param[out] fifo: FIFO data
 * @param[in] len: FIFO size what you want to read
 */
void stk_fifo_reading(struct stk_data *stk, u8 fifo[], int len)
{
    if (!stk->fifo) {
        STK_ACC_ERR("No fifo data");
        return;
    }

    /* Reject all register R/W to protect FIFO data reading */
    STK_ACC_LOG("Start to read FIFO data");

    if (STK_REG_READ_BLOCK(stk, STK8XXX_REG_FIFOOUT, len, fifo))
    {
        STK_ACC_ERR("Break to read FIFO data");
    }

    STK_ACC_LOG("Done for reading FIFO data");
}


/*
 * @brief: Change FIFO status
 *          If wm = 0, change FIFO to bypass mode.
 *          STK8XXX_CFG1_XYZ_FRAME_MAX >= wm, change FIFO to FIFO mode +
 *                                          STK8XXX_CFG2_FIFO_DATA_SEL_XYZ.
 *          Do nothing if STK8XXX_CFG1_XYZ_FRAME_MAX < wm.
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] wm: water mark
 *
 * @return: Success or fail
 *          0: Success
 *          Others: Fail
 */
int stk_change_fifo_status(struct stk_data *stk, u8 wm)
{
    int err = 0;
#ifdef STK_INTERRUPT_MODE
    u8 regIntmap2 = 0, regInten2 = 0;
#endif
    if (!stk->fifo) {
        return -1;
    }

    if (STK8XXX_CFG1_XYZ_FRAME_MAX < wm)
    {
        STK_ACC_ERR("water mark out of range(%d)", wm);
        return -1;
    }

#ifdef STK_INTERRUPT_MODE
    err = STK_REG_READ(stk, STK8XXX_REG_INTMAP2, &regIntmap2);
    if (err)
        return err;

    err = STK_REG_READ(stk, STK8XXX_REG_INTEN2, &regInten2);
    if (err)
        return err;
#endif /* STK_INTERRUPT_MODE */

    if (wm)
    {
        /* FIFO settings: FIFO mode + XYZ per frame */
        err = STK_REG_WRITE(stk, STK8XXX_REG_CFG2,
                (STK8XXX_CFG2_FIFO_MODE_FIFO << STK8XXX_CFG2_FIFO_MODE_SHIFT)
                | STK8XXX_CFG2_FIFO_DATA_SEL_XYZ);
        if (err)
            return err;

#ifdef STK_INTERRUPT_MODE
        err = STK_REG_WRITE(stk, STK8XXX_REG_INTMAP2, regIntmap2 | STK8XXX_INTMAP2_FWM2INT1);
        if (err)
            return err;

        err = STK_REG_WRITE(stk, STK8XXX_REG_INTEN2, regInten2 | STK8XXX_INTEN2_FWM_EN);
        if (err)
            return err;
#endif /* STK_INTERRUPT_MODE */
    }
    else
    {
        /* FIFO settings: bypass mode */
        err = STK_REG_WRITE(stk, STK8XXX_REG_CFG2,
                STK8XXX_CFG2_FIFO_MODE_BYPASS << STK8XXX_CFG2_FIFO_MODE_SHIFT);
        if (err)
            return err;

#ifdef STK_INTERRUPT_MODE
        err = STK_REG_WRITE(stk, STK8XXX_REG_INTMAP2, regIntmap2 & (~STK8XXX_INTMAP2_FWM2INT1));
        if (err)
            return err;

        err = STK_REG_WRITE(stk, STK8XXX_REG_INTEN2, regInten2 & (~STK8XXX_INTEN2_FWM_EN));
        if (err)
            return err;
#endif /* STK_INTERRUPT_MODE */
    }

    err = STK_REG_WRITE(stk, STK8XXX_REG_CFG1, wm);

    if (err)
        return err;

    return 0;
}

/**
 * @brief: read accel raw data from register.
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
int stk_read_accel_rawdata(struct stk_data *stk)
{
    u8 data[6] = { 0 };
    int err = 0;

    err = STK_REG_READ_BLOCK(stk, STK8XXX_REG_XOUT1, 6, data);
    if (err)
        return err;

    stk->xyz[0] = (data[1] << 8) | data[0];
    stk->xyz[1] = (data[3] << 8) | data[2];
    stk->xyz[2] = (data[5] << 8) | data[4];

    if (stk->data_shift > 0) 
	{
        stk->xyz[0] >>= stk->data_shift;
        stk->xyz[1] >>= stk->data_shift;
        stk->xyz[2] >>= stk->data_shift;
    }
    return 0;
}

/*
 * @brief: stk8xxx register initialize
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] range: range for STK8XXX_REG_RANGESEL
 *              STK_2G
 *              STK_4G
 *              STK_8G
 * @param[in] sr_no: Serial number of stk_odr_map.
 *
 * @return: Success or fail.
 *          0: Success
 *          others: Fail
 */
int stk_reg_init(struct stk_data *stk, stk_rangesel range, int sr_no)
{
    int err = 0;

    /* SW reset */
    err = stk_sw_reset(stk);

    if (err)
        return err;


    /* INT1, push-pull, active high. */
    err = STK_REG_WRITE(stk, STK8XXX_REG_INTCFG1,
            STK8XXX_INTCFG1_INT1_ACTIVE_H | STK8XXX_INTCFG1_INT1_OD_PUSHPULL);

    if (err)
        return err;

#ifdef STK_INTERRUPT_MODE
    /* map new accel data interrupt to int1 */
    err = STK_REG_WRITE(stk, STK8XXX_REG_INTMAP2, STK8XXX_INTMAP2_DATA2INT1);

    if (err)
        return err;

    /* enable new data interrupt for both new accel data */
    err = STK_REG_WRITE(stk, STK8XXX_REG_INTEN2, STK8XXX_INTEN2_DATA_EN);

    if (err)
        return err;
#endif /* STK_INTERRUPT_MODE */

#ifdef STK_AMD

    /* enable new data interrupt for any motion */
    err = STK_REG_WRITE(stk, STK8XXX_REG_INTEN1, STK8XXX_INTEN1_SLP_EN_XYZ);

    if (err)
        return err;

    /* map any motion interrupt to int1 */
    err = STK_REG_WRITE(stk, STK8XXX_REG_INTMAP1, STK8XXX_INTMAP1_ANYMOT2INT1);

    if (err)
        return err;

    /* SIGMOT2 */
    err = STK_REG_WRITE(stk, STK8XXX_REG_SIGMOT2, STK8XXX_SIGMOT2_ANY_MOT_EN);

    if (err)
        return err;

    /*
     * latch int
     * In interrupt mode + significant mode, both of them share the same INT.
     * Set latched to make sure we can get ANY data(ANY_MOT_STS) before signal fall down.
     * Read ANY flow:
     * Get INT --> check INTSTS1.ANY_MOT_STS status -> INTCFG2.INT_RST(relese all latched INT)
     * Read FIFO flow:
     * Get INT --> check INTSTS2.FWM_STS status -> INTCFG2.INT_RST(relese all latched INT)
     * In latch mode, echo interrupt(SIT_MOT_STS/FWM_STS) will cause all INT(INT1/INT2)
     * rising up.
     */
    err = STK_REG_WRITE(stk, STK8XXX_REG_INTCFG2, STK8XXX_INTCFG2_LATCHED);

    if (err)
        return err;
#else /* STK_AMD */

    /* non-latch int */
    err = STK_REG_WRITE(stk, STK8XXX_REG_INTCFG2, STK8XXX_INTCFG2_NOLATCHED);

    if (err)
        return err;

    /* SIGMOT2 */
    err = STK_REG_WRITE(stk, STK8XXX_REG_SIGMOT2, 0);

    if (err)
        return err;
#endif /* STK_AMD */

    /* SLOPE DELAY */
    err = STK_REG_WRITE(stk, STK8XXX_REG_SLOPEDLY, 0x00);

    if (err)
        return err;

    /* SLOPE THRESHOLD */
    err = STK_REG_WRITE(stk, STK8XXX_REG_SLOPETHD, STK8XXX_SLOPETHD_DEF);

    if (err)
        return err;

    /* SIGMOT1 */
    err = STK_REG_WRITE(stk, STK8XXX_REG_SIGMOT1, STK8XXX_SIGMOT1_SKIP_TIME_3SEC);

    if (err)
        return err;

    /* SIGMOT3 */
    err = STK_REG_WRITE(stk, STK8XXX_REG_SIGMOT3, STK8XXX_SIGMOT3_PROOF_TIME_1SEC);

    if (err)
        return err;

    /* According to STK_DEF_DYNAMIC_RANGE */
    err = stk_range_selection(stk, range);

    if (err)
        return err;

    /* ODR */
    if (stk->is_esm_mode)
    {
        err = STK_REG_WRITE(stk, STK8XXX_REG_ODRMODE, STK8XXX_ODR_ESMMODE);
        if (err)
        {
            STK_ACC_ERR("failed to set ESM mode");
        }
    }

    err = STK_REG_WRITE(stk, STK8XXX_REG_POWMODE, stk_odr_map[sr_no].regPwsel);
    if (err)
    {
        STK_ACC_ERR("failed to set Powmode");
    }
    err = STK_REG_WRITE(stk, STK8XXX_REG_BWSEL, stk_odr_map[sr_no].regBwsel);

    if (err)
        return err;
    stk->sr_no = sr_no;

    if (stk->fifo) {
        stk_change_fifo_status(stk, 0);
    }

    /* i2c watchdog enable */
    err = STK_REG_WRITE(stk, STK8XXX_REG_INTFCFG, STK8XXX_INTFCFG_I2C_WDT_EN);

    /* power to suspend mode */
    err = STK_REG_WRITE(stk, STK8XXX_REG_POWMODE, STK8XXX_PWMD_SUSPEND);
    if (err)
        return err;
    stk->power_mode = STK8XXX_PWMD_SUSPEND;
    atomic_set(&stk->enabled, 0);

    return 0;
}

/**
 * @brief: Selftest for XYZ offset and noise.
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static char stk_testOffsetNoise(struct stk_data *stk)
{
    int read_delay_ms = 8; /* 125Hz = 8ms */
    int acc_ave[3] = {0, 0, 0};
    int acc_min[3] = {INT_MAX, INT_MAX, INT_MAX};
    int acc_max[3] = {INT_MIN, INT_MIN, INT_MIN};
    int noise[3] = {0, 0, 0};
    int sn = 0, axis = 0;
    int thresholdOffset, thresholdNoise;
    u8 localResult = 0;

    if (stk_sw_reset(stk))
        return -1;

    if (STK_REG_WRITE(stk, STK8XXX_REG_BWSEL, 0x0B)) /* ODR = 125Hz */
        return -1;

    if (stk_range_selection(stk, STK8XXX_RANGESEL_2G))
        return -1;

    thresholdOffset = stk_selftest_offset_factor(stk->sensitivity);
    thresholdNoise = stk_selftest_noise_factor(stk->sensitivity);

    for (sn = 0; sn < STK_SELFTEST_SAMPLE_NUM; sn++)
    {
        msleep(read_delay_ms);
        stk_read_accel_rawdata(stk);
        STK_ACC_LOG("acc = %d, %d, %d", stk->xyz[0], stk->xyz[1], stk->xyz[2]);

        for (axis = 0; axis < 3; axis++)
        {
            acc_ave[axis] += stk->xyz[axis];

            if (stk->xyz[axis] > acc_max[axis])
                acc_max[axis] = stk->xyz[axis];

            if (stk->xyz[axis] < acc_min[axis])
                acc_min[axis] = stk->xyz[axis];
        }
    }

    for (axis = 0; axis < 3; axis++)
    {
        acc_ave[axis] /= STK_SELFTEST_SAMPLE_NUM;
        noise[axis] = acc_max[axis] - acc_min[axis];
    }

    STK_ACC_LOG("acc_ave=%d, %d, %d, noise=%d, %d, %d",
            acc_ave[0], acc_ave[1], acc_ave[2], noise[0], noise[1], noise[2]);
    STK_ACC_LOG("offset threshold=%d, noise threshold=%d",
            thresholdOffset, thresholdNoise);

    if (0 < acc_ave[2])
        acc_ave[2] -= stk->sensitivity;
    else
        acc_ave[2] += stk->sensitivity;

    if (0 == acc_ave[0] && 0 == acc_ave[1] && 0 == acc_ave[2])
        localResult |= STK_SELFTEST_RESULT_NO_OUTPUT;

    if (thresholdOffset <= abs(acc_ave[0])
            || 0 == noise[0] || thresholdNoise <= noise[0])
        localResult |= STK_SELFTEST_RESULT_FAIL_X;

    if (thresholdOffset <= abs(acc_ave[1])
            || 0 == noise[1] || thresholdNoise <= noise[1])
        localResult |= STK_SELFTEST_RESULT_FAIL_Y;

    if (thresholdOffset <= abs(acc_ave[2])
            || 0 == noise[2] || thresholdNoise <= noise[2])
        localResult |= STK_SELFTEST_RESULT_FAIL_Z;

    if (0 == localResult)
        atomic_set(&stk->selftest, STK_SELFTEST_RESULT_NO_ERROR);
    else
        atomic_set(&stk->selftest, localResult);
    return 0;
}

/**
 * @brief: SW selftest function.
 *
 * @param[in/out] stk: struct stk_data *
 */
void stk_selftest(struct stk_data *stk)
{
    int i = 0;
    u8 data = 0;

    STK_ACC_FUN();

    atomic_set(&stk->selftest, STK_SELFTEST_RESULT_RUNNING);

    /* Check PID */
    if (stk_get_pid(stk))
    {
        atomic_set(&stk->selftest, STK_SELFTEST_RESULT_DRIVER_ERROR);
        return;
    }

    STK_ACC_LOG("PID 0x%x", stk->pid);

    for (i = 0; i < (sizeof(STK_ID)/sizeof(STK_ID[0])); i++) {
        if (STK_ID[i] == stk->pid) {
            break;
        }
        if (sizeof(STK_ID)/sizeof(STK_ID[0])-1 == i) {
            atomic_set(&stk->selftest, STK_SELFTEST_RESULT_DRIVER_ERROR);
            return;
        }
    }

    /* Touch all register */
    for (i = 0; i <= 0x3F; i++)
    {
        if (STK_REG_READ(stk, i, &data))
        {
            atomic_set(&stk->selftest, STK_SELFTEST_RESULT_DRIVER_ERROR);
            return;
        }

        STK_ACC_LOG("[0x%2X]=0x%2X", i, data);
    }

    if (stk_testOffsetNoise(stk))
    {
        atomic_set(&stk->selftest, STK_SELFTEST_RESULT_DRIVER_ERROR);
    }

    stk_reg_init(stk, STK8XXX_RANGESEL_DEF, (stk->odr_table_count - 1));
}

/*
 * @brief: Suspend function for dev_pm_ops.
 *
 * @param[in] dev: struct device *
 *
 * @return: 0
 */
int stk8xxx_suspend(struct device *dev)
{
    struct i2c_client *client = container_of(dev, struct i2c_client, dev);
    struct stk_data *stk = i2c_get_clientdata(client);

    if (atomic_read(&stk->enabled))
    {
        stk_set_enable(stk, 0);
        stk->temp_enable = true;
    }
    else
        stk->temp_enable = false;

    return 0;
}

/*
 * @brief: Resume function for dev_pm_ops.
 *
 * @param[in] dev: struct device *
 *
 * @return: 0
 */
int stk8xxx_resume(struct device *dev)
{
    struct i2c_client *client = container_of(dev, struct i2c_client, dev);
    struct stk_data *stk = i2c_get_clientdata(client);

    if (stk->temp_enable)
        stk_set_enable(stk, 1);

    stk->temp_enable = false;
    return 0;
}

#if defined STK_INTERRUPT_MODE || defined STK_POLLING_MODE
/*
 * @brief: Queue work list.
 *          1. Read accel data, then report to userspace.
 *          2. Read FIFO data.
 *          3. Read ANY MOTION data.
 *          4. Reset latch status.
 *          5. Enable IRQ.
 *
 * @param[in] work: struct work_struct *
 */
void stk_work_queue(struct work_struct *work)
{
    struct stk_data *stk = container_of(work, struct stk_data, stk_work);

#ifdef STK_INTERRUPT_MODE
    STK_ACC_ERR("stk_work_queue:: Interrupt mode");
#elif defined STK_POLLING_MODE
    STK_ACC_ERR("stk_work_queue:: Polling mode");
#endif // STK_INTERRUPT_MODE

    stk_read_accel_data(stk);
    stk_report_accel_data(stk);

    if (stk->fifo) {
        u8 data = 0;

        if (STK_REG_READ(stk, STK8XXX_REG_INTSTS2, &data)) {
            STK_ACC_ERR("cannot read register INTSTS2");
        } else {
            if (STK8XXX_INTSTS2_FWM_STS_MASK & data)
            {
                stk_read_then_report_fifo_data(stk);
            }
        }
    }

#ifdef STK_AMD
    stk_read_anymotion_data(stk);
    stk_reset_latched_int(stk);
#endif /* STK_AMD */
#ifdef STK_INTERRUPT_MODE
    enable_irq(stk->irq);
#elif defined STK_POLLING_MODE 
    hrtimer_forward_now(&stk->stk_timer, stk->poll_delay);
#endif /* defined STK_INTERRUPT_MODE || defined STK_POLLING_MODE */
}
#endif /* defined STK_INTERRUPT_MODE || defined STK_POLLING_MODE */

#ifdef STK_INTERRUPT_MODE
/*
 * @brief: IRQ handler. This function will be trigger after receiving IRQ.
 *          1. Disable IRQ without waiting.
 *          2. Send work to quque.
 *
 * @param[in] irq: irq number
 * @param[in] data: void *
 *
 * @return: IRQ_HANDLED
 */
static irqreturn_t stk_irq_handler(int irq, void *data)
{
    struct stk_data *stk = data;
    disable_irq_nosync(irq);
    schedule_work(&stk->stk_work);
    return IRQ_HANDLED;
}

/*
 * @brief: IRQ setup
 *          1. Set GPIO as input direction.
 *          2. Allocate an interrupt resource, enable the interrupt, and IRQ
 *              handling.
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return:
 *          IRQC_IS_HARDIRQ or IRQC_IS_NESTED: Success
 *          Negative value: Fail
 */
int stk_irq_setup(struct stk_data *stk)
{
    int irq = 0;

    gpio_direction_input(stk->int_pin);
    irq = gpio_to_irq(stk->int_pin);

    if (0 > irq)
    {
        STK_ACC_ERR("gpio_to_irq(%d) failed", stk->int_pin);
        return -1;
    }

    stk->irq = irq;
    STK_ACC_LOG("irq #=%d, int pin=%d", stk->irq, stk->int_pin);

    irq = request_irq(stk->irq, stk_irq_handler, IRQF_TRIGGER_RISING, "stk_acc_irq", stk);

    if (0 > irq)
    {
        STK_ACC_ERR("request_irq(%d) failed for %d", stk->irq, irq);
        return -1;
    }

    return irq;
}

void stk_exit_irq_setup(struct stk_data *stk)
{
    free_irq(stk->irq,stk);
}

#elif defined STK_POLLING_MODE
/*
 * @brief: This function will send delayed_work to queue.
 *          This function will be called regularly with period:
 *          stk_data.poll_delay.
 *
 * @param[in] timer: struct hrtimer *
 *
 * @return: HRTIMER_RESTART.
 */
static enum hrtimer_restart stk_timer_func(struct hrtimer *timer)
{
    struct stk_data *stk = container_of(timer, struct stk_data, stk_timer);
    schedule_work(&stk->stk_work);
    hrtimer_forward_now(&stk->stk_timer, stk->poll_delay);
    return HRTIMER_RESTART;
}
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */

#ifdef STK_HW_STEP_COUNTER
/**
 * @brief: read step counter value from register.
 *          Store result to stk_data.steps.
 *
 * @param[in/out] stk: struct stk_data *
 */
void stk_read_step_data(struct stk_data *stk)
{
    u8 dataL = 0;
    u8 dataH = 0;

    if (STK8323_ID != stk->pid) {
        STK_ACC_ERR("not support step counter");
        return;
    }

    if (STK_REG_READ(stk, STK8XXX_REG_STEPOUT1, &dataL))
        return;

    if (STK_REG_READ(stk, STK8XXX_REG_STEPOUT2, &dataH))
        return;

    stk->steps = dataH << 8 | dataL;
}

/**
 * @brief: Turn ON/OFF step count.
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] turn: true to turn ON step count; false to turn OFF.
 */
void stk_turn_step_counter(struct stk_data *stk, bool turn)
{
    if (STK8323_ID != stk->pid) {
        STK_ACC_ERR("not support step counter");
        return;
    }

    if (turn)
    {
        if (STK_REG_WRITE(stk, STK8XXX_REG_STEPCNT2,
                              STK8XXX_STEPCNT2_RST_CNT | STK8XXX_STEPCNT2_STEP_CNT_EN))
            return;
    }
    else
    {
        if (STK_REG_WRITE(stk, STK8XXX_REG_STEPCNT2, 0))
            return;
    }

    stk->steps = 0;
}
#endif /* STK_HW_STEP_COUNTER */

#ifdef STK_FIR
/*
 * @brief: low-pass filter operation
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in/out] xyz: s16 *
 */
void stk_low_pass_fir(struct stk_data *stk, s16 *xyz)
{
    int firlength = atomic_read(&stk->fir_len);
#ifdef STK_ZG
    s16 avg;
    int jitter_boundary = stk->sensitivity / 128;
#endif /* STK_ZG */

    if (0 == firlength)
    {
        /* stk_data.fir_len == 0: turn OFF FIR operation */
        return;
    }

    if (firlength > stk->fir.count)
    {
        stk->fir.xyz[stk->fir.idx][0] = xyz[0];
        stk->fir.xyz[stk->fir.idx][1] = xyz[1];
        stk->fir.xyz[stk->fir.idx][2] = xyz[2];
        stk->fir.sum[0] += xyz[0];
        stk->fir.sum[1] += xyz[1];
        stk->fir.sum[2] += xyz[2];
        stk->fir.count++;
        stk->fir.idx++;
    }
    else
    {
        if (firlength <= stk->fir.idx)
            stk->fir.idx = 0;

        stk->fir.sum[0] -= stk->fir.xyz[stk->fir.idx][0];
        stk->fir.sum[1] -= stk->fir.xyz[stk->fir.idx][1];
        stk->fir.sum[2] -= stk->fir.xyz[stk->fir.idx][2];
        stk->fir.xyz[stk->fir.idx][0] = xyz[0];
        stk->fir.xyz[stk->fir.idx][1] = xyz[1];
        stk->fir.xyz[stk->fir.idx][2] = xyz[2];
        stk->fir.sum[0] += xyz[0];
        stk->fir.sum[1] += xyz[1];
        stk->fir.sum[2] += xyz[2];
        stk->fir.idx++;
#ifdef STK_ZG
        avg = stk->fir.sum[0] / firlength;

        if (abs(avg) <= jitter_boundary)
            xyz[0] = avg * ZG_FACTOR;
        else
            xyz[0] = avg;

        avg = stk->fir.sum[1] / firlength;

        if (abs(avg) <= jitter_boundary)
            xyz[1] = avg * ZG_FACTOR;
        else
            xyz[1] = avg;

        avg = stk->fir.sum[2] / firlength;

        if (abs(avg) <= jitter_boundary)
            xyz[2] = avg * ZG_FACTOR;
        else
            xyz[2] = avg;

#else /* STK_ZG */
        xyz[0] = stk->fir.sum[0] / firlength;
        xyz[1] = stk->fir.sum[1] / firlength;
        xyz[2] = stk->fir.sum[2] / firlength;
#endif /* STK_ZG */
    }
}
#endif /* STK_FIR */

#ifdef STK_CALI
/*
 * @brief: Get sample_no of samples then calculate average
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] delay_ms: delay in msec
 * @param[in] sample_no: amount of sample
 * @param[out] acc_ave: XYZ average
 */
static void stk_calculate_average(struct stk_data *stk,
                                  unsigned int delay_ms, int sample_no, int acc_ave[3])
{
    int i;

    for (i = 0; i < sample_no; i++)
    {
        msleep(delay_ms);
        stk_read_accel_data(stk);
        acc_ave[0] += stk->xyz[0];
        acc_ave[1] += stk->xyz[1];
        acc_ave[2] += stk->xyz[2];
    }

    /*
     * Take ceiling operation.
     * ave = (ave + SAMPLE_NO/2) / SAMPLE_NO
     *     = ave/SAMPLE_NO + 1/2
     * Example: ave=7, SAMPLE_NO=10
     * Ans: ave = 7/10 + 1/2 = (int)(1.2) = 1
     */
    for (i = 0; i < 3; i++)
    {
        if ( 0 <= acc_ave[i])
            acc_ave[i] = (acc_ave[i] + sample_no / 2) / sample_no;
        else
            acc_ave[i] = (acc_ave[i] - sample_no / 2) / sample_no;
    }

    /*
     * For Z-axis
     * Pre-condition: Sensor be put on a flat plane, with +z face up.
     */
    if (0 < acc_ave[2])
        acc_ave[2] -= stk->sensitivity;
    else
        acc_ave[2] += stk->sensitivity;
}

/*
 * @brief: Align STK8XXX_REG_OFSTx sensitivity with STK8XXX_REG_RANGESEL
 *  Description:
 *  Example:
 *      RANGESEL=0x3 -> +-2G / 12bits for STK8xxx full resolution
 *              number bit per G = 2^12 / (2x2) = 1024 (LSB/g)
 *              (2x2) / 2^12 = 0.97 mG/bit
 *      OFSTx: There are 8 bits to describe OFSTx for +-1G
 *              number bit per G = 2^8 / (1x2) = 128 (LSB/g)
 *              (1x2) / 2^8 = 7.8125mG/bit
 *      Align: acc_OFST = acc * 128 / 1024
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in/out] acc: accel data
 *
 */
static void stk_align_offset_sensitivity(struct stk_data *stk, int acc[3])
{
    int axis;

    /*
     * Take ceiling operation.
     * ave = (ave + SAMPLE_NO/2) / SAMPLE_NO
     *     = ave/SAMPLE_NO + 1/2
     * Example: ave=7, SAMPLE_NO=10
     * Ans: ave = 7/10 + 1/2 = (int)(1.2) = 1
     */
    for (axis = 0; axis < 3; axis++)
    {
        if (acc[axis] > 0)
        {
            acc[axis] = (acc[axis] * STK8XXX_OFST_LSB + stk->sensitivity / 2)
                        / stk->sensitivity;
        }
        else
        {
            acc[axis] = (acc[axis] * STK8XXX_OFST_LSB - stk->sensitivity / 2)
                        / stk->sensitivity;
        }
    }
}

/*
 * @brief: Verify offset.
 *          Read register of STK8XXX_REG_OFSTx, then check data are the same as
 *          what we wrote or not.
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] offset: offset value to compare with the value in register
 *
 * @return: Success or fail
 *          0: Success
 *          STK_K_FAIL_I2C: I2C error
 *          STK_K_FAIL_WRITE_OFSET: offset value not the same as the value in
 *                                  register
 */
static int stk_verify_offset(struct stk_data *stk, u8 offset[3])
{
    int axis;
    u8 offset_from_reg[3] = {0, 0, 0};

    if (stk_get_offset(stk, offset_from_reg))
        return STK_K_FAIL_I2C;

    for (axis = 0; axis < 3; axis++)
    {
        if (offset_from_reg[axis] != offset[axis])
        {
            STK_ACC_ERR("set OFST failed! offset[%d]=%d, read from reg[%d]=%d",
                    axis, offset[axis], axis, offset_from_reg[axis]);
            atomic_set(&stk->cali_status, STK_K_FAIL_WRITE_OFST);
            return STK_K_FAIL_WRITE_OFST;
        }
    }

    return 0;
}

#ifdef STK_CALI_FILE
/*
 * @brief: Write calibration config file to STK_CALI_FILE_PATH.
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] w_buf: cali data what want to write to STK_CALI_FILE_PATH.
 * @param[in] buf_size: size of w_buf.
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static int stk_write_to_file(struct stk_data *stk,
                             char *w_buf, int8_t buf_size)
{
    struct file *cali_file;
    char r_buf[buf_size];
    mm_segment_t fs;
    ssize_t ret;
    int i;

    cali_file = filp_open(STK_CALI_FILE_PATH, O_CREAT | O_RDWR, 0666);

    if (IS_ERR(cali_file))
    {
        STK_ACC_ERR("err=%ld, failed to open %s", PTR_ERR(cali_file), STK_CALI_FILE_PATH);
        return -ENOENT;
    }
    else
    {
        loff_t pos = 0;
        fs = get_fs();
        set_fs(get_ds());
        ret = vfs_write(cali_file, w_buf, buf_size, &pos);

        if (0 > ret)
        {
            STK_ACC_ERR("write error, ret=%d", (int)ret);
            filp_close(cali_file, NULL);
            return -EIO;
        }

        pos = 0x00;
        ret = vfs_read(cali_file, r_buf, buf_size, &pos);

        if (0 > ret)
        {
            STK_ACC_ERR("read error, ret=%d", (int)ret);
            filp_close(cali_file, NULL);
            return -EIO;
        }

        set_fs(fs);

        for (i = 0; i < buf_size; i++)
        {
            if (r_buf[i] != w_buf[i])
            {
                STK_ACC_ERR("read back error! r_buf[%d]=0x%X, w_buf[%d]=0x%X",
                        i, r_buf[i], i, w_buf[i]);
                filp_close(cali_file, NULL);
                return -1;
            }
        }
    }

    filp_close(cali_file, NULL);
    return 0;
}

/*
 * @brief: Write calibration data to config file
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] offset: offset value
 * @param[in] status: status
 *                  STK_K_SUCCESS_FILE
 *
 * @return: Success or fail
 *          0: Success
 *          -1: Fail
 */
int stk_write_cali_to_file(struct stk_data *stk,
                                  u8 offset[3], u8 status)
{
    char file_buf[STK_CALI_FILE_SIZE];
    memset(file_buf, 0, sizeof(file_buf));
    file_buf[0] = STK_CALI_VER0;
    file_buf[1] = STK_CALI_VER1;
    file_buf[3] = offset[0];
    file_buf[5] = offset[1];
    file_buf[7] = offset[2];
    file_buf[8] = status;
    file_buf[STK_CALI_FILE_SIZE - 2] = '\0';
    file_buf[STK_CALI_FILE_SIZE - 1] = STK_CALI_END;

    if (stk_write_to_file(stk, file_buf, STK_CALI_FILE_SIZE))
        return -1;

    return 0;
}

/*
 * @brief: Get calibration config file from STK_CALI_FILE_PATH.
 *
 * @param[in/out] stk: struct stk_data *
 * @param[out] r_buf: cali data what want to read from STK_CALI_FILE_PATH.
 * @param[in] buf_size: size of r_buf.
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static int stk_get_from_file(struct stk_data *stk,
                             char *r_buf, int8_t buf_size)
{
    struct file *cali_file;
    mm_segment_t fs;
    ssize_t ret;

    cali_file = filp_open(STK_CALI_FILE_PATH, O_RDONLY, 0);

    if (IS_ERR(cali_file))
    {
        STK_ACC_ERR("err=%ld, failed to open %s", PTR_ERR(cali_file), STK_CALI_FILE_PATH);
        return -ENOENT;
    }
    else
    {
        loff_t pos = 0;
        fs = get_fs();
        set_fs(get_ds());
        ret = vfs_read(cali_file, r_buf, buf_size, &pos);
        set_fs(fs);

        if (0 > ret)
        {
            STK_ACC_ERR("read error, ret=%d", (int)ret);
            filp_close(cali_file, NULL);
            return -EIO;
        }
    }

    filp_close(cali_file, NULL);
    return 0;
}

/*
 * @brief: Get calibration data and status.
 *          Set cali status to stk_data.cali_status.
 *
 * @param[in/out] stk: struct stk_data *
 * @param[out] r_buf: cali data what want to read from STK_CALI_FILE_PATH.
 * @param[in] buf_size: size of r_buf.
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
void stk_get_cali(struct stk_data *stk)
{
    char stk_file[STK_CALI_FILE_SIZE];

    if (stk_get_from_file(stk, stk_file, STK_CALI_FILE_SIZE) == 0)
    {
        if (STK_CALI_VER0 == stk_file[0]
            && STK_CALI_VER1 == stk_file[1]
            && STK_CALI_END == stk_file[STK_CALI_FILE_SIZE - 1])
        {
            atomic_set(&stk->cali_status, (int)stk_file[8]);
            stk->offset[0] = (u8)stk_file[3];
            stk->offset[1] = (u8)stk_file[5];
            stk->offset[2] = (u8)stk_file[7];
            STK_ACC_LOG("offset:%d,%d,%d, mode=0x%X",
                    stk_file[3], stk_file[5], stk_file[7], stk_file[8]);
            STK_ACC_LOG("variance=%u,%u,%u",
                     (stk_file[9] << 24 | stk_file[10] << 16 | stk_file[11] << 8 | stk_file[12]),
                     (stk_file[13] << 24 | stk_file[14] << 16 | stk_file[15] << 8 | stk_file[16]),
                     (stk_file[17] << 24 | stk_file[18] << 16 | stk_file[19] << 8 | stk_file[20]));
        }
        else
        {
            int i;
            STK_ACC_ERR("wrong cali version number");

            for (i = 0; i < STK_CALI_FILE_SIZE; i++)
            {
                STK_ACC_LOG("cali_file[%d]=0x%X", i, stk_file[i]);
            }
        }
    }
}
#endif /* STK_CALI_FILE */

/*
 * @brief: Calibration action
 *          1. Calculate calibration data
 *          2. Write data to STK8XXX_REG_OFSTx
 *          3. Check calibration well-done with chip register
 *          4. Write calibration data to file
 *          Pre-condition: Sensor be put on a flat plane, with +z face up.
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] delay_us: delay in usec
 *
 * @return: Success or fail
 *          0: Success
 *          STK_K_FAIL_I2C: I2C error
 *          STK_K_FAIL_WRITE_OFSET: offset value not the same as the value in
 *                                  register
 *          STK_K_FAIL_W_FILE: fail during writing cali to file
 */
static int stk_cali_do(struct stk_data *stk, int delay_us)
{
    int err = 0;
    int acc_ave[3] = {0, 0, 0};
    unsigned int delay_ms = delay_us / 1000;
    u8 offset[3] = {0, 0, 0};
    int acc_verify[3] = {0, 0, 0};
    const unsigned char verify_diff = stk->sensitivity / 10;
    int axis;

    stk_calculate_average(stk, delay_ms, STK_CALI_SAMPLE_NO, acc_ave);
    stk_align_offset_sensitivity(stk, acc_ave);

#ifdef STK_AUTOK
    STK_ACC_LOG("acc_ave:%d,%d,%d", acc_ave[0], acc_ave[1], acc_ave[2]);
    for (axis = 0; axis < 3; axis++) {
        int threshold = STK8XXX_OFST_LSB * 3 / 10; // 300mG
        if (abs(acc_ave[axis] > threshold)) {
            STK_ACC_ERR("%s: abs(%d) > %d(300mG)", acc_ave[axis], threshold);
            return STK_K_FAIL_VERIFY_CALI;
        }
    }
#endif /* STK_AUTOK */

    stk->cali_sw[0] = (s16)acc_ave[0];
    stk->cali_sw[1] = (s16)acc_ave[1];
    stk->cali_sw[2] = (s16)acc_ave[2];

    for (axis = 0; axis < 3; axis++)
        offset[axis] = -acc_ave[axis];

    STK_ACC_LOG("New offset for XYZ: %d, %d, %d", acc_ave[0], acc_ave[1], acc_ave[2]);
    err = stk_set_offset(stk, offset);

    if (err)
        return STK_K_FAIL_I2C;

    /* Read register, then check OFSTx are the same as we wrote or not */
    err = stk_verify_offset(stk, offset);

    if (err)
        return err;

    /* verify cali */
    stk_calculate_average(stk, delay_ms, 3, acc_verify);

    if (verify_diff < abs(acc_verify[0]) || verify_diff < abs(acc_verify[1])
        || verify_diff < abs(acc_verify[2]))
    {
        STK_ACC_ERR("Check data x:%d, y:%d, z:%d. Check failed!",
                acc_verify[0], acc_verify[1], acc_verify[2]);
        return STK_K_FAIL_VERIFY_CALI;
    }

#ifdef STK_CALI_FILE
    /* write cali to file */
    err = stk_write_cali_to_file(stk, offset, STK_K_SUCCESS_FILE);

    if (err)
    {
        STK_ACC_ERR("failed to stk_write_cali_to_file, err=%d", err);
        return STK_K_FAIL_W_FILE;
    }
#endif /* STK_CALI_FILE */

    atomic_set(&stk->cali_status, STK_K_SUCCESS_FILE);
    return 0;
}

/*
 * @brief: Set calibration
 *          1. Change delay to 8000usec
 *          2. Reset offset value by trigger OFST_RST
 *          3. Calibration action
 *          4. Change delay value back
 *
 * @param[in/out] stk: struct stk_data *
 */
void stk_set_cali(struct stk_data *stk)
{
    int err = 0;
    bool enable;
    int org_delay_us, real_delay_us;

    atomic_set(&stk->cali_status, STK_K_RUNNING);
    org_delay_us = stk_get_delay(stk);
    /* Use several samples (with ODR:125) for calibration data base */
    err = stk_set_delay(stk, 8000);

    if (err)
    {
        STK_ACC_ERR("failed to stk_set_delay, err=%d", err);
        atomic_set(&stk->cali_status, STK_K_FAIL_I2C);
        goto err_for_set_delay;
    }

    real_delay_us = stk_get_delay(stk);

    /* SW reset before getting calibration data base */
    if (atomic_read(&stk->enabled))
    {
        enable = true;
        stk_set_enable(stk, 0);
    }
    else {
        enable = false;
    }

    stk_set_enable(stk, 1);
    err = STK_REG_WRITE(stk, STK8XXX_REG_OFSTCOMP1,
                              STK8XXX_OFSTCOMP1_OFST_RST);

    if (err)
    {
        atomic_set(&stk->cali_status, STK_K_FAIL_I2C);
        goto exit_for_OFST_RST;
    }

    /* Action for calibration */
    err = stk_cali_do(stk, real_delay_us);

    if (err)
    {
        STK_ACC_ERR("failed to stk_cali_do, err=%d", err);
        atomic_set(&stk->cali_status, err);
        goto exit_for_OFST_RST;
    }

    STK_ACC_LOG("successful calibration");
exit_for_OFST_RST:

    if (!enable) {
        stk_set_enable(stk, 0);
    }

err_for_set_delay:
    stk_set_delay(stk, org_delay_us);
}

/**
 * @brief: Reset calibration
 *          1. Reset offset value by trigger OFST_RST
 *          2. Calibration action
 */
void stk_reset_cali(struct stk_data *stk)
{
    STK_REG_WRITE(stk, STK8XXX_REG_OFSTCOMP1,
                       STK8XXX_OFSTCOMP1_OFST_RST);
    atomic_set(&stk->cali_status, STK_K_NO_CALI);
    memset(stk->cali_sw, 0x0, sizeof(stk->cali_sw));
}
#endif /* STK_CALI */

#ifdef STK_AUTOK
/*
 * @brief: Auto cali if there is no any cali files.
 *
 * @param[in/out] stk: struct stk_data *
 */
static void stk_autok(struct stk_data *stk)
{
    STK_ACC_FUN();
#ifdef STK_CALI_FILE
    stk_get_cali(stk);
#endif /* STK_CALI_FILE */
    if (STK_K_SUCCESS_FILE == atomic_read(&stk->cali_status))
    {
        STK_ACC_LOG("get offset from cali: %d %d %d",
                stk->offset[0], stk->offset[1], stk->offset[2]);
        atomic_set(&stk->first_enable, 0);
        stk_set_offset(stk, stk->offset);
    } else {
        stk_set_cali(stk);
    }
}
#endif /* STK_AUTOK */

/*
 * stk8xxx register write
 * @brief: Register writing via I2C
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] reg: Register address
 * @param[in] val: Data, what you want to write.
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static int stk_reg_write(struct stk_data *stk, u8 reg, u8 val)
{
    int error = 0;
    mutex_lock(&stk->i2c_lock);
    error = i2c_smbus_write_byte_data(stk->client, reg, val);
    mutex_unlock(&stk->i2c_lock);

    if (error) {
        STK_ACC_ERR("failed to write reg:0x%x with val:0x%x", reg, val);
        return -EIO;
    }

    return error;
}

/*
 * stk8xxx register read
 * @brief: Register reading via I2C
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] reg: Register address
 * @param[in] len: 0, for normal usage. Others, read length (FIFO used).
 * @param[out] val: Data, the register what you want to read.
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static int stk_reg_read(struct stk_data *stk, u8 reg, int len, u8 *val)
{
    int error = 0;
    struct i2c_msg msgs[2] = {
        {
            .addr = stk->client->addr,
            .flags = 0,
            .len = 1,
            .buf = &reg
        },
        {
            .addr = stk->client->addr,
            .flags = I2C_M_RD,
            .len = (0 >= len) ? 1 : len,
            .buf = val
        }
    };

    mutex_lock(&stk->i2c_lock);
    error = i2c_transfer(stk->client->adapter, msgs, 2);
    mutex_unlock(&stk->i2c_lock);

    if (2 == error)
        error = 0;
    else if (0 > error)
    {
        STK_ACC_ERR("transfer failed to read reg:0x%x with len:%d, error=%d", reg, len, error);
        return -EIO;
    }
    else
    {
        STK_ACC_ERR("size error in reading reg:0x%x with len:%d, error=%d", reg, len, error);
        return -EIO;
    }

    return error;
}

static int stk_read(struct stk_data *stk, unsigned char addr, unsigned char *val)
{
    return stk_reg_read(stk, addr, 0, val);
}

static int stk_read_block(struct stk_data *stk, unsigned char addr, int len, unsigned char *val)
{
    return stk_reg_read(stk, addr, len, val);
}

static int stk_write(struct stk_data *stk, unsigned char addr, unsigned char val)
{
    return stk_reg_write(stk, addr, val);
}

/* Bus operations */
static const struct stk_bus_ops stk8xxx_bus_ops =
{
    .read = stk_read,
    .read_block = stk_read_block,
    .write = stk_write,
};

/*
 * @brief: Proble function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 * @param[in] id: struct i2c_device_id *
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static int stk8xxx_i2c_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    return stk_i2c_probe(client, &stk8xxx_bus_ops);
}

/*
 * @brief: Remove function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 *
 * @return: 0
 */
static int stk8xxx_i2c_remove(struct i2c_client *client)
{
    return stk_i2c_remove(client);
}

/**
 * @brief:
 */
static int stk8xxx_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    strcpy(info->type, STK8XXX_NAME);
    return 0;
}

#ifdef CONFIG_PM_SLEEP
/*
 * @brief: Suspend function for dev_pm_ops.
 *
 * @param[in] dev: struct device *
 *
 * @return: 0
 */
static int stk8xxx_i2c_suspend(struct device *dev)
{
    return stk8xxx_suspend(dev);
}

/*
 * @brief: Resume function for dev_pm_ops.
 *
 * @param[in] dev: struct device *
 *
 * @return: 0
 */
static int stk8xxx_i2c_resume(struct device *dev)
{
    return stk8xxx_resume(dev);
}

static const struct dev_pm_ops stk8xxx_pm_ops =
{
    .suspend = stk8xxx_i2c_suspend,
    .resume = stk8xxx_i2c_resume,
};
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_ACPI
static const struct acpi_device_id stk8xxx_acpi_id[] =
{
    {"STK8XXX", 0},
    {}
};
MODULE_DEVICE_TABLE(acpi, stk8xxx_acpi_id);
#endif /* CONFIG_ACPI */

static const struct i2c_device_id stk8xxx_i2c_id[] =
{
    {STK8XXX_NAME, 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, stk8xxx_i2c_id);

static struct i2c_driver stk8xxx_i2c_driver =
{
    .probe      = stk8xxx_i2c_probe,
    .remove     = stk8xxx_i2c_remove,
    .detect     = stk8xxx_i2c_detect,
    .id_table   = stk8xxx_i2c_id,
    .class      = I2C_CLASS_HWMON,
    .driver = {
        .owner  = THIS_MODULE,
        .name   = STK8XXX_NAME,
#ifdef CONFIG_PM_SLEEP
        .pm     = &stk8xxx_pm_ops,
#endif
#ifdef CONFIG_ACPI
        .acpi_match_table = ACPI_PTR(stk8xxx_acpi_id),
#endif /* CONFIG_ACPI */
#ifdef CONFIG_OF
        .of_match_table = stk8xxx_match_table,
#endif /* CONFIG_OF */
    }
};

/**
 * @brief: Get power status
 *          Send 0 or 1 to userspace.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t enable_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;
    char en;

    en = atomic_read(&stk->enabled);
    return scnprintf(buf, PAGE_SIZE, "%d\n", en);
}

/**
 * @brief: Set power status
 *          Get 0 or 1 from userspace, then set stk832x power status.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t enable_store(struct device_driver *ddri, const char *buf, size_t count)
{
    struct stk_data *stk = gStk;
    unsigned int data;
    int error;
    error = kstrtouint(buf, 10, &data);

    if (error)
    {
        STK_ACC_ERR("kstrtoul failed, error=%d", error);
        return error;
    }

    if ((1 == data) || (0 == data))
        stk_set_enable(stk, data);
    else
        STK_ACC_ERR("invalid argument, en=%d", data);

    return count;
}

/**
 * @brief: Get accel data
 *          Send accel data to userspce.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t value_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;
    bool enable = true;

    if (!atomic_read(&stk->enabled))
    {
        stk_set_enable(stk, 1);
        enable = false;
    }

    stk_read_accel_data(stk);

    if (!enable)
        stk_set_enable(stk, 0);

    return scnprintf(buf, PAGE_SIZE, "%hd %hd %hd\n",
                     stk->xyz[0], stk->xyz[1], stk->xyz[2]);
}

/**
 * @brief: Get delay value in usec
 *          Send delay in usec to userspce.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t delay_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;

    return scnprintf(buf, PAGE_SIZE, "%lld\n", (long long)stk_get_delay(stk) * 1000);
}

/**
 * @brief: Set delay value in usec
 *          Get delay value in usec from userspace, then write to register.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t delay_store(struct device_driver *ddri, const char *buf, size_t count)
{
    struct stk_data *stk = gStk;
    long long data;
    int err;
    err = kstrtoll(buf, 10, &data);

    if (err)
    {
        STK_ACC_ERR("kstrtoul failed, error=%d", err);
        return err;
    }

    stk_set_delay(stk, (int)((int)data / 1000));
    return count;
}

/**
 * @brief: Get offset value
 *          Send X/Y/Z offset value to userspace.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t offset_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;
    u8 offset[3] = {0, 0, 0};

    stk_get_offset(stk, offset);
    return scnprintf(buf, PAGE_SIZE, "0x%X 0x%X 0x%X\n",
                     offset[0], offset[1], offset[2]);
}

/**
 * @brief: Set offset value
 *          Get X/Y/Z offset value from userspace, then write to register.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t offset_store(struct device_driver *ddri, const char *buf, size_t count)
{
    struct stk_data *stk = gStk;
    char *token[10];
    u8 r_offset[3];
    int err, data, i;

    for (i = 0; i < 3; i++)
        token[i] = strsep((char **)&buf, " ");

    err = kstrtoint(token[0], 16, &data);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, error=%d", err);
        return err;
    }

    r_offset[0] = (u8)data;
    err = kstrtoint(token[1], 16, &data);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, error=%d", err);
        return err;
    }

    r_offset[1] = (u8)data;
    err = kstrtoint(token[2], 16, &data);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, error=%d", err);
        return err;
    }

    r_offset[2] = (u8)data;
    STK_ACC_LOG("offset=0x%X, 0x%X, 0x%X", r_offset[0], r_offset[1], r_offset[2]);
    stk_set_offset(stk, r_offset);
    return count;
}

/**
 * @brief: Register writting
 *          Get address and content from userspace, then write to register.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t send_store(struct device_driver *ddri, const char *buf, size_t count)
{
    struct stk_data *stk = gStk;
    char *token[10];
    int addr, cmd, err, i;
    bool enable = false;

    for (i = 0; i < 2; i++)
        token[i] = strsep((char **)&buf, " ");

    err = kstrtoint(token[0], 16, &addr);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, err=%d", err);
        return err;
    }

    err = kstrtoint(token[1], 16, &cmd);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, err=%d", err);
        return err;
    }

    STK_ACC_LOG("write reg[0x%X]=0x%X", addr, cmd);

    if (!atomic_read(&stk->enabled))
        stk_set_enable(stk, 1);
    else
        enable = true;

    if (STK_REG_WRITE(stk, (u8)addr, (u8)cmd))
    {
        err = -1;
        goto exit;
    }

exit:

    if (!enable)
        stk_set_enable(stk, 0);

    if (err)
        return -1;

    return count;
}

/**
 * @brief: Read stk_data.recv(from stk_recv_store), then send to userspace.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t recv_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;

    return scnprintf(buf, PAGE_SIZE, "0x%X\n", stk->recv);
}

/**
 * @brief: Get the read address from userspace, then store the result to
 *          stk_data.recv.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t recv_store(struct device_driver *ddri, const char *buf, size_t count)
{
    struct stk_data *stk = gStk;
    int addr, err;
    bool enable = false;
    err = kstrtoint(buf, 16, &addr);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, error=%d", err);
        return err;
    }

    if (!atomic_read(&stk->enabled))
        stk_set_enable(stk, 1);
    else
        enable = true;

    if (STK_REG_READ(stk, (u8)addr, &stk->recv))
    {
        err = -1;
        goto exit;
    }

    STK_ACC_LOG("read reg[0x%X]=0x%X", addr, stk->recv);
exit:

    if (!enable)
        stk_set_enable(stk, 0);

    if (err)
        return -1;

    return count;
}

/**
 * @brief: Read all register value, then send result to userspace.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t allreg_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;
    int result;
    result = stk_show_all_reg(stk, buf);

    if (0 > result)
        return result;

    return (ssize_t)result;
}

/**
 * @brief: Check PID, then send chip number to userspace.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t chipinfo_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;

    switch (stk->pid) {
        case STK8BA50_R_ID:
            return scnprintf(buf, PAGE_SIZE, "stk8ba50-r\n");
        case STK8BA53_ID:
            return scnprintf(buf, PAGE_SIZE, "stk8ba53\n");
        case STK8323_ID:
            return scnprintf(buf, PAGE_SIZE, "stk8321/8323\n");        
        case STK8327_ID:
            return scnprintf(buf, PAGE_SIZE, "stk8327\n");
		case STK8329_ID:
            return scnprintf(buf, PAGE_SIZE, "stk8329\n");
        default:
            return scnprintf(buf, PAGE_SIZE, "unknown\n");
    }

    return scnprintf(buf, PAGE_SIZE, "unknown\n");
}

/**
 * @brief: Read FIFO data, then send to userspace.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t fifo_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;
    u8 fifo_wm = 0;
    u8 frame_unit = 0;
    int fifo_len, len = 0;

    if (!stk->fifo) {
        return scnprintf(buf, PAGE_SIZE , "No fifo data\n");
    }

    if (STK_REG_READ(stk, STK8XXX_REG_FIFOSTS, &fifo_wm))
        return scnprintf(buf, PAGE_SIZE , "fail to read FIFO cnt\n");

    fifo_wm &= STK8XXX_FIFOSTS_FIFO_FRAME_CNT_MASK;
    if (0 == fifo_wm)
        return scnprintf(buf, PAGE_SIZE , "no fifo data yet\n");

    if (STK_REG_READ(stk, STK8XXX_REG_CFG2, &frame_unit))
        return scnprintf(buf, PAGE_SIZE , "fail to read FIFO\n");

    frame_unit &= STK8XXX_CFG2_FIFO_DATA_SEL_MASK;

    if (0 == frame_unit)
        fifo_len = fifo_wm * 6; /* xyz * 2 bytes/axis */
    else
        fifo_len = fifo_wm * 2; /* single axis * 2 bytes/axis */

    {
        u8 *fifo = NULL;
        int i;
        /* vzalloc: allocate memory and set to zero. */
        fifo = vzalloc(sizeof(u8) * fifo_len);

        if (!fifo)
        {
            STK_ACC_ERR("memory allocation error");
            return scnprintf(buf, PAGE_SIZE , "fail to read FIFO\n");
        }

        stk_fifo_reading(stk, fifo, fifo_len);

        for (i = 0; i < fifo_wm; i++)
        {
            if (0 == frame_unit)
            {
                s16 x, y, z;
                x = fifo[i * 6 + 1] << 8 | fifo[i * 6];
                x >>= 4;
                y = fifo[i * 6 + 3] << 8 | fifo[i * 6 + 2];
                y >>= 4;
                z = fifo[i * 6 + 5] << 8 | fifo[i * 6 + 4];
                z >>= 4;
                len += scnprintf(buf + len, PAGE_SIZE - len,
                                 "%dth x:%d, y:%d, z:%d\n", i, x, y, z);
            }
            else
            {
                s16 xyz;
                xyz = fifo[i * 2 + 1] << 8 | fifo[i * 2];
                xyz >>= 4;
                len += scnprintf(buf + len, PAGE_SIZE - len,
                                 "%dth fifo:%d\n", i, xyz);
            }

            if ( 0 >= (PAGE_SIZE - len))
            {
                STK_ACC_ERR("print string out of PAGE_SIZE");
                break;
            }
        }

        vfree(fifo);
    }
    return len;
}

/**
 * @brief: Read water mark from userspace, then send to register.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t fifo_store(struct device_driver *ddri, const char *buf, size_t count)
{
    struct stk_data *stk = gStk;
    int wm, err;

    if (!stk->fifo) {
        STK_ACC_ERR("not support fifo");
        return count;
    }

    err = kstrtoint(buf, 10, &wm);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, error=%d", err);
        return err;
    }

    if (stk_change_fifo_status(stk, (u8)wm))
    {
        return -1;
    }

    return count;
}

/**
 * @brief: Show self-test result.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t selftest_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;
    u8 result = atomic_read(&stk->selftest);

    if (STK_SELFTEST_RESULT_NA == result)
        return scnprintf(buf, PAGE_SIZE, "No result\n");
    if (STK_SELFTEST_RESULT_RUNNING == result)
        return scnprintf(buf, PAGE_SIZE, "selftest is running\n");
    else if (STK_SELFTEST_RESULT_NO_ERROR == result)
        return scnprintf(buf, PAGE_SIZE, "No error\n");
    else
        return scnprintf(buf, PAGE_SIZE, "Error code:0x%2X\n", result);
}

/**
 * @brief: Do self-test.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t selftest_store(struct device_driver *ddri, const char *buf, size_t count)
{
    struct stk_data *stk = gStk;

    stk_selftest(stk);

    return count;
}

/**
 * @brief: Get range value
 *          Send range to userspce.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t range_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;
    switch(stk->sensitivity)
    {
        //2G , for 16it , 12 bit
        // 2^16 / 2x2 , 2^12/2x2
        case 16384:
        case 1024:
            return scnprintf(buf, PAGE_SIZE, "2\n");

        //4G , for 16it , 12 bit,10bit
        //2^16 / 2x4 , 2^12/2x4, 2^10/2x4
        case 8192:
        case 512:
        case 128:
            return scnprintf(buf, PAGE_SIZE, "4\n");

        //8G , for 16it ,10bit
        //2^16 / 2x8 , 2^10/2x8
        case 4096:
        case 64:
            return scnprintf(buf, PAGE_SIZE, "8\n");

        //2G, 10bit  ,8G,12bit
        //2^10/2x2 , 2^12/2x8
        case 256:
            if(stk->pid==STK8BA50_R_ID)
                return scnprintf(buf, PAGE_SIZE, "2\n");
            else
                return scnprintf(buf, PAGE_SIZE, "8\n");

        //16G , for 16it
        //2^16 / 2x16
        case 2048:
            return scnprintf(buf, PAGE_SIZE, "16\n");

        default:
            return scnprintf(buf, PAGE_SIZE, "unkown\n" );

    }
    return scnprintf(buf, PAGE_SIZE, "unkown\n");
}

/**
 * @brief: Set range value
 *         Get range value from userspace, then write to register.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t range_store(struct device_driver *ddri, const char *buf, size_t count)
{
    struct stk_data *stk = gStk;
    long long data;
    int err;
    stk_rangesel range;
    err = kstrtoll(buf, 10, &data);
    if (err)
    {
        STK_ACC_ERR("kstrtoul failed, error=%d", err);
        return err;
    }

    if(stk->pid!=STK8327_ID)
    {
        if(data==16)
        {
            STK_ACC_LOG(" This chip not support 16G,auto switch to 8G");
            data=8;
        }
    }

    switch(data)
    {
        case 2:
        default:
            range=STK8XXX_RANGESEL_2G;
            break;

        case 4:
            range=STK8XXX_RANGESEL_4G;
            break;

        case 8:
            range=STK8XXX_RANGESEL_8G;
            break;

        case 16:
            range=STK8XXX_RANGESEL_16G;
            break;
    }
    stk_range_selection(stk, range);

    return count;
}

#ifdef STK_CALI
/**
 * @brief: Get calibration status
 *          Send calibration status to userspace.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t cali_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;
    int result = atomic_read(&stk->cali_status);

#ifdef STK_CALI_FILE
    if (STK_K_RUNNING != result)
    {
        stk_get_cali(stk);
    }
#endif /* STK_CALI_FILE */

    return scnprintf(buf, PAGE_SIZE, "0x%X\n", result);
}

/**
 * @brief: Trigger to calculate calibration data
 *          Get 1 from userspace, then start to calculate calibration data.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t cali_store(struct device_driver *ddri, const char *buf, size_t count)
{
    struct stk_data *stk = gStk;
    if (sysfs_streq(buf, "1"))
    {
        stk_set_cali(stk);
    }
    else
    {
        STK_ACC_ERR("invalid value %d", *buf);
        return -EINVAL;
    }

    return count;
}
#endif /* STK_CALI */

#ifdef STK_HW_STEP_COUNTER
/**
 * @brief: Read step counter data, then send to userspace.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stepcount_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;

    if (STK8323_ID != stk->pid) {
        STK_ACC_ERR("not support step counter");
        return scnprintf(buf, PAGE_SIZE, "Not support\n");
    }

    stk_read_step_data(stk);

    return scnprintf(buf, PAGE_SIZE, "%d\n", stk->steps);
}

/**
 * @brief: Read step counter settins from userspace, then send to register.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stepcount_store(struct device_driver *ddri, const char *buf, size_t count)
{
    struct stk_data *stk = gStk;
    int step, err;

    if (STK8323_ID != stk->pid) {
        STK_ACC_ERR("not support step counter");
        return count;
    }

    err = kstrtoint(buf, 10, &step);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, err=%d", err);
        return err;
    }

    if (step)
        stk_turn_step_counter(stk, true);
    else
        stk_turn_step_counter(stk, false);

    return count;
}
#endif /* STK_HW_STEP_COUNTER */

#ifdef STK_FIR
/**
 * @brief: Get FIR parameter, then send to userspace.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t firlen_show(struct device_driver *ddri, char *buf)
{
    struct stk_data *stk = gStk;
    int len = atomic_read(&stk->fir_len);

    if (len)
    {
        STK_ACC_LOG("FIR count=%2d, idx=%2d", stk->fir.count, stk->fir.idx);
        STK_ACC_LOG("sum = [\t%d \t%d \t%d]",
                stk->fir.sum[0], stk->fir.sum[1], stk->fir.sum[2]);
        STK_ACC_LOG("avg = [\t%d \t%d \t%d]",
                 stk->fir.sum[0] / len, stk->fir.sum[1] / len, stk->fir.sum[2] / len);
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", len);
}

/**
 * @brief: Get FIR length from userspace, then write to stk_data.fir_len.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t firlen_store(struct device_driver *ddri, const char *buf, size_t count)
{
    struct stk_data *stk = gStk;
    int firlen, err;
    err = kstrtoint(buf, 10, &firlen);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, error=%d", err);
        return err;
    }

    if (STK_FIR_LEN_MAX < firlen)
    {
        STK_ACC_ERR("maximum FIR length is %d", STK_FIR_LEN_MAX);
    }
    else
    {
        memset(&stk->fir, 0, sizeof(struct data_fir));
        atomic_set(&stk->fir_len, firlen);
    }

    return count;
}
#endif /* STK_FIR */

static DRIVER_ATTR_RW(enable);//, 0664, mtk_enable_show, mtk_enable_store);
static DRIVER_ATTR_RO(value);//, 0444, mtk_value_show, NULL);
static DRIVER_ATTR_RW(delay);//, 0664, mtk_delay_show, mtk_delay_store);
static DRIVER_ATTR_RW(offset);//, 0664, mtk_offset_show, mtk_offset_store);
static DRIVER_ATTR_WO(send);//, 0220, NULL, mtk_send_store);
static DRIVER_ATTR_RW(recv);//, 0664, mtk_recv_show, mtk_recv_store);
static DRIVER_ATTR_RO(allreg);//, 0444, mtk_allreg_show, NULL);
static DRIVER_ATTR_RO(chipinfo);//, 0444, mtk_chipinfo_show, NULL);
static DRIVER_ATTR_RW(fifo);//, 0664, mtk_fifo_show, mtk_fifo_store);
static DRIVER_ATTR_RW(selftest);//, 0664, mtk_selftest_show, mtk_selftest_store);
static DRIVER_ATTR_RW(range);//, 0664, mtk_range_show, mtk_range_store);
#ifdef STK_CALI
    static DRIVER_ATTR_RW(cali);//, 0664, mtk_cali_show, mtk_cali_store);
#endif /* STK_CALI */
#ifdef STK_HW_STEP_COUNTER
    static DRIVER_ATTR_RW(stepcount);//, 0664, mtk_stepcount_show, mtk_stepcount_store);
#endif /* STK_HW_STEP_COUNTER */
#ifdef STK_FIR
    static DRIVER_ATTR_RW(firlen);//, 0664, mtk_firlen_show, mtk_firlen_store);
#endif /* STK_FIR */

static struct driver_attribute *stk_attr_list[] =
{
    &driver_attr_enable,
    &driver_attr_value,
    &driver_attr_delay,
    &driver_attr_offset,
    &driver_attr_send,
    &driver_attr_recv,
    &driver_attr_allreg,
    &driver_attr_chipinfo,
    &driver_attr_fifo,
    &driver_attr_selftest,
    &driver_attr_range,
#ifdef STK_CALI
    &driver_attr_cali,
#endif /* STK_CALI */
#ifdef STK_HW_STEP_COUNTER
    &driver_attr_stepcount,
#endif /* STK_HW_STEP_COUNTER */
#ifdef STK_FIR
    &driver_attr_firlen,
#endif /* STK_FIR */
};


static int stk_init_flag = 0;
/**
 * @brief: read accel data from register.
 *          Store result to stk_data.xyz[].
 *
 * @param[in/out] stk: struct stk_data *
 */
void stk_read_accel_data(struct stk_data *stk)
{
    stk_read_accel_rawdata(stk);
#ifdef STK_FIR
    stk_low_pass_fir(stk, stk->xyz);
#endif /* STK_FIR */
}


static int gsensor_get_data(int *x, int *y, int *z, int *status)
{
    struct stk_data *stk = gStk;
    int x_data, y_data, z_data, err;
    int acc[3] = {0};
    char buff[256];
    stk_read_accel_data(stk);
    sprintf(buff, "%04x %04x %04x", stk->xyz[0], stk->xyz[1], stk->xyz[2]);
    /* remap coordinate */
#ifndef STK_MTK_2_0
    acc[stk->cvt.map[0]] = stk->cvt.sign[0] * stk->xyz[0];
    acc[stk->cvt.map[1]] = stk->cvt.sign[1] * stk->xyz[1];
    acc[stk->cvt.map[2]] = stk->cvt.sign[2] * stk->xyz[2];
#else
    acc[0] = stk->xyz[0];
    acc[1] = stk->xyz[1];
    acc[2] = stk->xyz[2];
#endif // STK_MTK_2_0
    /* Output for mg */
    x_data =(acc[0] * GRAVITY_EARTH_1000 )/stk->sensitivity;
    y_data =(acc[1] * GRAVITY_EARTH_1000 )/stk->sensitivity;
    z_data =(acc[2] * GRAVITY_EARTH_1000 )/stk->sensitivity;
    STK_ACC_LOG("report x = %d, y = %d, z = %d\n", acc[0], acc[1], acc[2]);
    sprintf(buff, "%04x %04x %04x", x_data, y_data, z_data);
    err = sscanf(buff, "%x %x %x", x, y, z);

    if (3 != err)
    {
        STK_ACC_ERR("Invalid argument");
        return -EINVAL;
    }
#ifndef STK_MTK_2_0
    *status = SENSOR_STATUS_ACCURACY_MEDIUM;
#else
    *status = SENSOR_ACCURANCY_HIGH;	
#endif // STK_MTK_2_0
    return 0;
}

#ifndef STK_MTK_2_0
static int stk_acc_init(void);
static int stk_acc_uninit(void);

static struct acc_init_info stk_acc_init_info =
{
    .name = STK8XXX_NAME,
    .init = stk_acc_init,
    .uninit = stk_acc_uninit,
};

/**
 * @brief:
 */
static int stk_create_attr(struct device_driver *driver)
{
    int err = 0;
    int i, num = (int)(ARRAY_SIZE(stk_attr_list));

    if (NULL == driver)
    {
        STK_ACC_ERR("Cannot find driver");
        return -EINVAL;
    }

    for (i = 0; i < num; i++)
    {
        err = driver_create_file(driver, stk_attr_list[i]);

        if (err)
        {
            STK_ACC_ERR("driver_create_file (%s) = %d",
                    stk_attr_list[i]->attr.name, err);
            break;
        }
    }

    return err;
}

static void stk_remove_attr(struct device_driver *driver)
{
    int i, num = (int)(ARRAY_SIZE(stk_attr_list));

    if (NULL == driver)
    {
        STK_ACC_ERR("Cannot find driver");
        return;
    }

    for (i = 0; i < num; i++)
    {
        driver_remove_file(driver, stk_attr_list[i]);
    }
}

static int stk_readCalibration(int *dat)
{
    struct stk_data *stk = gStk;

    STK_ACC_LOG("ori x:%d, y:%d, z:%d", stk->cali_sw[0], stk->cali_sw[1], stk->cali_sw[2]);
    dat[0] = stk->cali_sw[0];
    dat[1] = stk->cali_sw[1];
    dat[2] = stk->cali_sw[2];

    return 0;
}

static int stk_writeCalibration(int *dat)
{
    struct stk_data *stk = gStk;
    int err = 0;
    int cali[3];

    err = stk_readCalibration(cali);

    STK_ACC_LOG("raw cali_sw[%d][%d][%d] dat[%d][%d][%d]",
            cali[0], cali[1], cali[2], dat[0], dat[1], dat[2]);

    cali[0] += dat[0];
    cali[1] += dat[1];
    cali[2] += dat[2];

    stk->cali_sw[0] = cali[0];
    stk->cali_sw[1] = cali[1];
    stk->cali_sw[2] = cali[2];

    STK_ACC_LOG("new cali_sw[%d][%d][%d]",
            stk->cali_sw[0], stk->cali_sw[1], stk->cali_sw[2]);

    mdelay(1);

    return err;
}

/**
 * @brief: Open data rerport to HAL.
 *      refer: drivers/misc/mediatek/accelerometer/inc/accel.h
 */
static int gsensor_open_report_data(int open)
{
    /* TODO. should queuq work to report event if  is_report_input_direct=true */
    return 0;
}

/**
 * @brief: Only enable not report event to HAL.
 *      refer: drivers/misc/mediatek/accelerometer/inc/accel.h
 */
static int gsensor_enable_nodata(int en)
{
    struct stk_data *stk = gStk;

    if (en)
    {
        stk_set_enable(stk, 1);
        atomic_set(&stk->enabled, 1);
    }
    else
    {
        stk_set_enable(stk, 0);
        atomic_set(&stk->enabled, 0);
    }

    STK_ACC_LOG("enabled is %d", en);
    return 0;
}

/**
 * @brief:
 */
static int gsensor_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
    STK_ACC_FUN();
    return 0;
}

/**
 * @brief:
 */
static int gsensor_flush(void)
{
    STK_ACC_FUN();
//    return -1; /* error */
    return acc_flush_report();
}

/**
 * @brief:
 */
static int gsensor_set_delay(u64 delay_ns)
{
    struct stk_data *stk = gStk;
    STK_ACC_LOG("delay= %d ms", (int)((int)delay_ns / 1000));
    stk_set_delay(stk, (int)((int)delay_ns / 1000));
    return 0;
}

/* struct accel_factory_fops */
static int stk_factory_enable_sensor(bool enable, int64_t sample_ms)
{
    int en = (true == enable ? 1 : 0);
    if (gsensor_enable_nodata(en))
    {
        STK_ACC_ERR("enable sensor failed");
        return -1;
    }

    return 0;
}


/* struct accel_factory_fops */
static int stk_factory_get_data(int32_t data[3], int *status)
{
    return gsensor_get_data(&data[0], &data[1], &data[2], status);
}

/* struct accel_factory_fops */
static int stk_factory_get_raw_data(int32_t data[3])
{
    struct stk_data *stk = gStk;
    stk_read_accel_rawdata(stk);
    data[0] = (int32_t)stk->xyz[0];
    data[1] = (int32_t)stk->xyz[1];
    data[2] = (int32_t)stk->xyz[2];
    return 0;
}

/* struct accel_factory_fops */
static int stk_factory_enable_cali(void)
{
#ifdef STK_CALI
    struct stk_data *stk = gStk;
    stk_set_cali(stk);
#endif /* STK_CALI */
    return 0;
}

/* struct accel_factory_fops */
static int stk_factory_clear_cali(void)
{
    struct stk_data *stk = gStk;
#ifdef STK_CALI
    stk_reset_cali(stk);
#endif /* STK_CALI */
    memset(stk->cali_sw, 0x0, sizeof(stk->cali_sw));

    return 0;
}

/* struct accel_factory_fops */
static int stk_factory_set_cali(int32_t data[3])
{
    int err = 0;
    struct stk_data *stk = gStk;
    int cali[3] = {0, 0, 0};

#ifdef STK_CALI_FILE
    u8 xyz[3] = {0, 0, 0};
    atomic_set(&stk->cali_status, STK_K_RUNNING);
#endif /* STK_CALI_FILE */
    cali[0] = data[0] * stk->sensitivity / GRAVITY_EARTH_1000;
    cali[1] = data[1] * stk->sensitivity / GRAVITY_EARTH_1000;
    cali[2] = data[2] * stk->sensitivity / GRAVITY_EARTH_1000;

    STK_ACC_LOG("new x:%d, y:%d, z:%d", cali[0], cali[1], cali[2]);
#ifdef STK_CALI_FILE
    xyz[0] = (u8)cali[0];
    xyz[1] = (u8)cali[1];
    xyz[2] = (u8)cali[2];
    /* write cali to file */
    err = stk_write_cali_to_file(stk, xyz, STK_K_SUCCESS_FILE);

    if (err)
    {
        STK_ACC_ERR("failed to stk_write_cali_to_file, err=%d", err);
        return -1;
    }
#endif /* STK_CALI_FILE */

    err = stk_writeCalibration(cali);
    if (err)
    {
        STK_ACC_ERR("stk_writeCalibration failed!");
        return -1;
    }
#ifdef STK_CALI
    atomic_set(&stk->cali_status, STK_K_SUCCESS_FILE);
#endif /* STK_CALI */
    return 0;
}

/* struct accel_factory_fops */
static int stk_factory_get_cali(int32_t data[3])
{
    struct stk_data *stk = gStk;

    data[0] = (int32_t)(stk->cali_sw[0] * GRAVITY_EARTH_1000 / stk->sensitivity);
    data[1] = (int32_t)(stk->cali_sw[1] * GRAVITY_EARTH_1000 / stk->sensitivity);
    data[2] = (int32_t)(stk->cali_sw[2] * GRAVITY_EARTH_1000 / stk->sensitivity);
    STK_ACC_LOG("x:%d, y:%d, z:%d", data[0], data[1], data[2]);

    return 0;
}

/* struct accel_factory_fops */
static int stk_factory_do_self_test(void)
{
    struct stk_data *stk = gStk;

    stk_selftest(stk);

    if (STK_SELFTEST_RESULT_NO_ERROR == atomic_read(&stk->selftest))
        return 0;
    else
        return -1;
}

static struct accel_factory_fops stk_factory_fops =
{
    .enable_sensor = stk_factory_enable_sensor,
    .get_data = stk_factory_get_data,
    .get_raw_data = stk_factory_get_raw_data,
    .enable_calibration = stk_factory_enable_cali,
    .clear_cali = stk_factory_clear_cali,
    .set_cali = stk_factory_set_cali,
    .get_cali = stk_factory_get_cali,
    .do_self_test = stk_factory_do_self_test,
};

static struct accel_factory_public stk_factory_device =
{
    .gain = 1,
    .sensitivity = 1,
    .fops = &stk_factory_fops,
};

#else //def STK_MTK_2_0
static int stk8xxx_hf_enable(struct hf_device* hfdev, int sensor_type, int en)
{
    struct i2c_client* i2c_dev = hf_device_get_private_data(hfdev);
    struct stk_data* stk = i2c_get_drvdata(i2c_dev);

    if (sensor_type == SENSOR_TYPE_ACCELEROMETER)
        stk_set_enable(stk, en);
    return 0;
}

/**
 * @brief:
 */
static int stk8xxx_hf_batch(struct hf_device* hfdev, int sensor_type,
                            int64_t delay, int64_t latency)
{
    pr_debug("%s id:%d delay:%lld latency:%lld\n", __func__, sensor_type,
             delay, latency);
    return 0;
}

static int stk8xxx_hf_get_data(struct hf_device* hfdev)
{
    struct i2c_client* i2c_dev = hf_device_get_private_data(hfdev);
    struct stk_data* stk = i2c_get_drvdata(i2c_dev);
    struct hf_manager* manager = stk->hf_dev.manager;
    struct hf_manager_event event;

    int x_data, y_data, z_data, err;
    if (EINVAL == gsensor_get_data(&x_data, &y_data, &z_data, &err))
    {
        STK_ACC_ERR("stk8xxx_hf_get_data: get data fail");
        return -EINVAL;
    }

    event.timestamp = ktime_get_boot_ns();
    event.sensor_type = SENSOR_TYPE_ACCELEROMETER;
    event.accurancy = SENSOR_ACCURANCY_HIGH;
    event.action = DATA_ACTION;
    event.word[0] = x_data;
    event.word[1] = y_data;
    event.word[2] = z_data;
    manager->report(manager, &event);
    manager->complete(manager);
    return 0;
}
#endif // STK_MTK_2_0

/*
 * @brief:
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return:
 *      0: Success
 *      others: Fail
 */
static int stk_init_mtk(struct stk_data *stk)
{
    int err = 0;
#ifndef STK_MTK_2_0
    struct acc_control_path stk_acc_control_path = { 0 };
    struct acc_data_path stk_acc_data_path = { 0 };

    err = stk_create_attr(&stk_acc_init_info.platform_diver_addr->driver);
    if (err) {
        STK_ACC_ERR("Fail in stk_create_attr, err=%d", err);
        return -1;
    }

    stk_acc_control_path.is_use_common_factory = false;
    err = accel_factory_device_register(&stk_factory_device);
    if (err) {
        STK_ACC_ERR("Fail in accel_factory_device_register, err=%d", err);
        accel_factory_device_deregister(&stk_factory_device);
		return -1;
    }

    stk_acc_control_path.open_report_data = gsensor_open_report_data;
    stk_acc_control_path.enable_nodata = gsensor_enable_nodata;
    stk_acc_control_path.is_support_batch = false;
    stk_acc_control_path.batch = gsensor_batch;
    stk_acc_control_path.flush = gsensor_flush;
    stk_acc_control_path.set_delay = gsensor_set_delay;
    stk_acc_control_path.is_report_input_direct = false;
    err = acc_register_control_path(&stk_acc_control_path);

    if (err)
    {
        STK_ACC_ERR("acc_register_control_path fail");
        accel_factory_device_deregister(&stk_factory_device);
		stk_remove_attr(&stk_acc_init_info.platform_diver_addr->driver);
		return -1;
    }

    stk_acc_data_path.get_data = gsensor_get_data;
    stk_acc_data_path.vender_div = 1000;
    err = acc_register_data_path(&stk_acc_data_path);

    if (err)
    {
        STK_ACC_ERR("acc_register_data_path fail");
        accel_factory_device_deregister(&stk_factory_device);
		stk_remove_attr(&stk_acc_init_info.platform_diver_addr->driver);
		return -1;
    }
#else
    struct sensor_info list;
    memset(&stk->hf_dev, 0, sizeof(struct hf_device));
    stk->hf_dev.dev_name = STK8XXX_NAME;
    stk->hf_dev.device_poll = HF_DEVICE_IO_POLLING;
    stk->hf_dev.device_bus = HF_DEVICE_IO_SYNC;//HF_DEVICE_IO_ASYNC;

    list.sensor_type = SENSOR_TYPE_ACCELEROMETER;
    list.gain = 1;
    strcpy(list.name, STK8XXX_NAME);
    strcpy(list.vendor, "sensortek");
    stk->hf_dev.support_list = &list;

    stk->hf_dev.support_size = 1;
    stk->hf_dev.enable = stk8xxx_hf_enable;
    stk->hf_dev.batch = stk8xxx_hf_batch;
    stk->hf_dev.sample = stk8xxx_hf_get_data;
    err = hf_manager_create(&stk->hf_dev);

    if (err < 0)
    {
        STK_ACC_ERR("stk_init_mtk 2.0: hf_manager_create fail err=%d", err);
        return -1;
    }
    hf_device_set_private_data(&stk->hf_dev, stk->client);
    STK_ACC_ERR("stk_init_mtk: 2.0 done");
    
    /* sysfs: create file system */
    err = sysfs_create_group(&stk->client->dev.kobj,
                             &stk_attribute_accel_group);
    if (err) {
        STK_ACC_ERR("Fail in sysfs_create_group, err=%d", err);
        sysfs_remove_group(&stk->client->dev.kobj, &stk_attribute_accel_group);
        return -1;
    }
#endif // STK_MTK_2_0
    

    return 0;
}

/*
 * @brief: Exit mtk related settings safely.
 *
 * @param[in/out] stk: struct stk_data *
 */
static void stk_exit_mtk(struct stk_data *stk)
{
#ifndef STK_MTK_2_0
    accel_factory_device_deregister(&stk_factory_device);
    stk_remove_attr(&stk_acc_init_info.platform_diver_addr->driver);
#else
    sysfs_remove_group(&stk->client->dev.kobj,
                       &stk_attribute_accel_group);
#endif // STK_MTK_2_0
}

/**
 * @brief: Probe function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 * @param[in] stk_bus_ops: const struct stk_bus_ops *
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
int stk_i2c_probe(struct i2c_client *client, const struct stk_bus_ops *stk8xxx_bus_ops)
{
    int err = 0;
    struct stk_data *stk;

    STK_ACC_LOG("STK_HEADER_VERSION: %s ", STK_HEADER_VERSION);
    STK_ACC_LOG("STK_C_VERSION: %s ", STK_C_VERSION);
    STK_ACC_LOG("STK_DRV_I2C_VERSION: %s ", STK_DRV_I2C_VERSION);
    STK_ACC_LOG("STK_MTK_VERSION: %s ", STK_MTK_VERSION);

    if (NULL == client)
    {
        return -ENOMEM;
    }
    else if (NULL == stk8xxx_bus_ops)
    {
        STK_ACC_ERR("cannot get stk_bus_ops. EXIT");
        return -EIO;
    }

    /* kzalloc: allocate memory and set to zero. */
    stk = kzalloc(sizeof(struct stk_data), GFP_KERNEL);

    if (!stk)
    {
        STK_ACC_ERR("memory allocation error");
        return -ENOMEM;
    }

    gStk = stk;
    stk->client = client;
    stk->bops = stk8xxx_bus_ops;
    i2c_set_clientdata(client, stk);
    mutex_init(&stk->i2c_lock);

#ifndef STK_MTK_2_0
    err = get_accel_dts_func(client->dev.of_node, &stk->hw);
    if (err)
    {
        STK_ACC_ERR("DTS info fail");
        goto err_free_mem;
    }

    /* direction */
    err = hwmsen_get_convert(stk->hw.direction, &stk->cvt);
    if (err)
    {
        STK_ACC_ERR("Invalid direction: %d", stk->hw.direction);
        goto err_free_mem;
    }
#endif // STK_MTK_2_0

    err = stk_get_pid(stk);
    if (err)
        goto err_free_mem;

    STK_ACC_LOG("PID 0x%x", stk->pid);
    stk_data_initialize(stk);

    if (stk_reg_init(stk, STK8XXX_RANGESEL_DEF, stk->sr_no))
    {
        STK_ACC_ERR("stk8xxx initialization failed");
        goto err_free_mem;
    }

    if (stk_init_mtk(stk)) {
        STK_ACC_ERR("stk_init_mtk failed");
        goto err_free_mem;
    }

    STK_ACC_LOG("Success");
    stk_init_flag = 0;
    return 0;

    //stk_exit_mtk(stk);
err_free_mem:
    mutex_destroy(&stk->i2c_lock);
    kfree(stk);
    stk_init_flag = -1;
    return err;
}

/*
 * @brief: Remove function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 *
 * @return: 0
 */
int stk_i2c_remove(struct i2c_client *client)
{
    struct stk_data *stk = i2c_get_clientdata(client);

    stk_exit_mtk(stk);
    mutex_destroy(&stk->i2c_lock);
    kfree(stk);
    stk_init_flag = -1;
    return 0;
}

#ifndef STK_MTK_2_0
/**
 * @brief:
 *
 * @return: Success or fail.
 *          0: Success
 *          others: Fail
 */
static int stk_acc_init(void)
{
    STK_ACC_FUN();

    if (i2c_add_driver(&stk8xxx_i2c_driver))
    {
        STK_ACC_ERR("i2c_add_driver fail");
        return -1;
    }

    if (0 != stk_init_flag)
    {
        STK_ACC_ERR("stk8xxx init error");
        return -1;
    }

    return 0;
}

/**
 * @brief:
 *
 * @return: Success or fail.
 *          0: Success
 *          others: Fail
 */
static int stk_acc_uninit(void)
{
    i2c_del_driver(&stk8xxx_i2c_driver);
    return 0;
}

/**
 * @brief:
 *
 * @return: Success or fail.
 *          0: Success
 *          others: Fail
 */
static int __init stk8xxx_init(void)
{
    STK_ACC_FUN();
    acc_driver_add(&stk_acc_init_info);
    return 0;
}

static void __exit stk8xxx_exit(void)
{
    STK_ACC_FUN();
}

module_init(stk8xxx_init);
module_exit(stk8xxx_exit);
#else //2.0
module_i2c_driver(stk8xxx_i2c_driver);
#endif // STK_MTK_2_0

MODULE_AUTHOR("Sensortek");
MODULE_DESCRIPTION("stk8xxx 3-Axis accelerometer driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(STK_MTK_VERSION);
