/******************************************************************************
* file  MT5728 15W wireless charge  driver
* Copyright (C) 2020 prize.
******************************************************************************/
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/timer.h>

#include <linux/delay.h>
#include <linux/kernel.h>

#include <linux/poll.h>

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
//#include "mtk_charger_intf.h" //add by sunshuai

//#include <linux/wakelock.h>
//#include <extcon_usb.h>

//#include "mtk_charger.h"
//#include "MT5728_mtp_array.h"
#include "mtk_charger_intf.h"
#include "mtk_switch_charging.h"
#include "mtk_intf.h"

#include "mt5728_wireless_15w.h"

#define DEVICE_NAME  "mt5728_iic"

#define DRIVER_FIRMWARE_VERSION "0.0.1"


//prize add by lipengpeng 20220511 start 
//#define TRUE 1
//#define FALSE 0
//prize add by lipengpeng 20220511 end 
//#define MT5728_WIRELESS_TRX_MODE_SWITCH 1
//int wls_work_online = 0;                 //wireless charge working
//int reserse_charge_online = 0;
//int usb_otg_online = 0;
//int usbchip_otg_detect = 0;
// int MT5728_rt_mode_n = 0;                //GPOD5,rxmode - 0
//volatile unsigned int AfcSendTimeout; //afc vout set time out
//volatile unsigned char AfcIntFlag;
static int mt5728_init_done = 0;
volatile int rx_vout_max = 5000;
volatile int rx_iout_max = 1000;
//volatile int rx_iout_limit = 100;
#ifdef MT5728_USE_WAKELOCK
// static struct wake_lock mt5728_wls_wake_lock;
//static struct wakeup_source mt5728_wls_wake_lock;
#endif
//static int cur_last = 0;
static int powergood_err_cnt = 0;
volatile int setup_iout_start_cnt = 0;
volatile int reverse_timeout_cnt = 0;
volatile int current_change_interval_cnt = 0;
// static int mt5728_charging_otgin_flag = 0;
static int input_current_limit = 0;
#ifndef MT5728_CHIP_AUTO_AFC9V
//static volatile int vout_change_timecnt = 0;
#endif
//static volatile int current_reduce_flag = 0;
static int mt5728_vout_old;
static int mt5728_vrect_old;
static int mt5728_mtp_write_flag;
static volatile int mt5728_epp_ctrl_vout_flag;
//static int mt5728_ldo_on_flag;
//static struct charger_device *primary_charger;
//static volatile int mt5728_epp_ptpower;

#define OTP_WRITE_FLAG_ADDR  0x3E00    //In the middle of code area and trim area

static inline u16 crc_firmware(u16 poly1, u16 sed, u8 *buf, u32 n);
static ssize_t Mt5728_get_vout(void);
static ssize_t Mt5728_set_vout(struct device* cd, struct device_attribute *attr, const char* buf, size_t len);
static ssize_t get_adapter(struct device* cd, struct device_attribute* attr, char* buf);
static ssize_t brushfirmware(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t updata_txFW(struct device* cd, struct device_attribute* attr, char* buf);
static ssize_t Mt5728_get_vrect(void);
int set_rx_ocp(uint16_t ocp);
typedef struct
{
    unsigned char G;
    s8 Offs;
} FodType;

FodType mt5728fod[8]={{250,127},{250,127},{250,127},{250,127},{250,127},{250,127},{250,127},{250,127}};
FodType mt5728fod_epp[8]={{250,127},{250,127},{250,127},{250,127},{250,127},{250,127},{250,127},{250,127}};
typedef union {
    u16 value;
    u8 ptr[2];
} vuc;
typedef union {
    u32 value;
    u8 ptr[4];
} vuc32;

extern int wireless_charge_chage_current(void);
//prize add by lipengpeng 20220611 end
void fast_vfc(int vol);
//prize add by lipengpeng 20210831 start

//prize add by lipengpeng 20210416 start BPI_BUS4  GPIO90
int test_gpio(int en);
int get_MT5728_status(void);
//prize add by lipengpeng 20210416 start BPI_BUS4  GPIO90
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
int turn_off_5725(int en);
int turn_off_rever_5725(int en);
int turn_on_rever_5725(int en);
int turn_on_otg_charge_mode(int en);
int set_otg_gpio(int en);

extern void mt_vbus_on(void);
extern void mt_vbus_off(void);

extern void mt_vbus_revere_on(void);
extern void mt_vbus_revere_off(void);

extern void mt_vbus_reverse_on_limited_current(void);
extern void mt_vbus_reverse_off_limited_current(void);
#endif

struct MT5728_dev *mte, *chip;
//struct delayed_work MT5728_int_delayed_work;
typedef struct Pgm_Type {
    u16 status;
    u16 addr;
    u16 length;
    u16 cs;
    u8 data[MTP_BLOCK_SIZE];
} Pgm_Type_t;

struct pinctrl* mt5728_pinctrl;
struct pinctrl_state *mt5728_rsv0_low, *mt5728_rsv0_high;

struct MT5728_func {
    int (*read)(struct MT5728_dev* di, u16 reg, u8* val);
    int (*write)(struct MT5728_dev* di, u16 reg, u8 val);
    int (*read_buf)(struct MT5728_dev* di, u16 reg, u8* buf, u32 size);
    int (*write_buf)(struct MT5728_dev* di, u16 reg, u8* buf, u32 size);
};

struct otg_wireless_ctl {
	struct pinctrl *pinctrl_gpios;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *charger_otg_off, *charger_otg_on,*wireless_5728_off,*wireless_5728_on,*charger_otg_mode_on,*charger_otg_mode_off,*test_gpio_on,*test_gpio_off;
	bool gpio_otg_prepare;
};

enum wireless_charge_protocol {
	PROTOCOL_UNKNOWN = 0,
	BPP,
	EPP,
	AFC,
};

struct MT5728_dev {
    char  *name;
    struct i2c_client* client;
    struct device* dev;
    struct regmap* regmap;
    struct MT5728_func bus;
    struct device_node	*irq_nd;     /* node */
    struct delayed_work eint_work;
	struct delayed_work add_current_work;
    struct delayed_work charger_work;
	struct delayed_work reverse_charge_work;
	struct delayed_work fwcheck_work;
    int    irq_gpio;
	int    statu_gpio;
    int fsk_status;
	enum wireless_charge_protocol charge_protocol;
	int wireless_max_power;
	unsigned int input_current;
	unsigned int charge_current;
    int   tx_count;   //Enable TX function, write 0 for this value
    struct otg_wireless_ctl otg_5728_ctl;
	int otgen_gpio;
	int one_pin_ctl;
	int rx_power_cap;
	int rx_efficiency;
	int chipen_gpio;	//sgm2541 en pn, active low, low:auto high:slave mode
	int ldoctrl_gpio;
	int test_r_gpio;
	int (*select_charging_current)(void);
	atomic_t is_tx_mode;
    int mt5728_ldo_on_flag;
	int    rxdetect_flag;
    int    rxremove_flag;
};

#define REG_NONE_ACCESS 0
#define REG_RD_ACCESS (1 << 0)
#define REG_WR_ACCESS (1 << 1)
#define REG_BIT_ACCESS (1 << 2)

#define REG_MAX 0x0F

struct reg_attr {
    const char* name;
    u16 addr;
    u8 flag;
};

enum REG_INDEX {
    CHIPID = 0,
    FWVERSION,
    VOUT,
    INT_FLAG,
    INTCTLR,
    VOUTSET,
    VFC,
    CMD,
    INDEX_MAX,
};

static struct reg_attr reg_access[INDEX_MAX]={
    [CHIPID] = { "CHIPID", REG_CHIPID, REG_RD_ACCESS },
    [FWVERSION] = { "FWVERSION", REG_FW_VER, REG_RD_ACCESS },
    [VOUT] = { "VOUT", REG_VOUT, REG_RD_ACCESS },
    [INT_FLAG] = { "INT_FLAG", REG_INTFLAG, REG_RD_ACCESS },
    [INTCTLR] = { "INTCLR", REG_INTCLR, REG_WR_ACCESS },
    [VOUTSET] = { "VOUTSET", REG_VOUTSET, REG_RD_ACCESS | REG_WR_ACCESS },
    [VFC] = { "VFC", REG_VFC, REG_RD_ACCESS | REG_WR_ACCESS },
    [CMD] = { "CMD", REG_CMD, REG_RD_ACCESS | REG_WR_ACCESS | REG_BIT_ACCESS },
};

static u32 SizeofPkt(u8 hdr) {
    if (hdr < 0x20)
        return 1;

    if (hdr < 0x80)
        return (2 + ((hdr - 0x20) >> 4));

    if (hdr < 0xe0)
        return (8 + ((hdr - 0x80) >> 3));

    return (20 + ((hdr - 0xe0) >> 2));
}



static int MT5728_read(struct MT5728_dev* di, u16 reg, u8* val) {
    unsigned int temp;
    int rc;

    rc = regmap_read(di->regmap, reg, &temp);
    if (rc >= 0)
        *val = (u8)temp;

    return rc;
}

static int MT5728_write(struct MT5728_dev* di, u16 reg, u8 val) {
    int rc = 0;

    rc = regmap_write(di->regmap, reg, val);
    if (rc < 0)
        dev_err(di->dev, "MT5728 write error: %d\n", rc);

    return rc;
}

static int MT5728_read_buffer(struct MT5728_dev* di, u16 reg, u8* buf, u32 size) {

    return regmap_bulk_read(di->regmap, reg, buf, size);
}

static int MT5728_write_buffer(struct MT5728_dev* di, u16 reg, u8* buf, u32 size) {
    int rc = 0;
    rc = regmap_bulk_write(di->regmap, reg, buf, size);
    return rc;
}

static void mt5728_sram_write(u32 addr, u8 *data,u32 len) {
    u32 offset,length,size;
    offset = 0;
    length = 0;
    size = len;
    pr_info("[%s] Length to write:%d\n",__func__, len);
    while(size > 0) {
        if(size > SRAM_PAGE_SIZE) {
            length = SRAM_PAGE_SIZE;
        } else {
            length = size;
        }
        pr_info("[%s] Length of this write :%d\n",__func__,length);
		MT5728_write_buffer(mte, addr + offset ,data+offset, length);
        size -= length;
        offset += length;
        msleep(2);
    }
    pr_info("[%s] Write completion\n",__func__);
}

static void mt5728_run_pgm_fw(void) {
    vuc val;
    //wdg_disable
    val.value  = MT5728_WDG_DISABLE;
    MT5728_write_buffer(mte, MT5728_PMU_WDGEN_REG, val.ptr, 2);
    MT5728_write_buffer(mte, MT5728_PMU_WDGEN_REG, val.ptr, 2);
	MT5728_write_buffer(mte, MT5728_PMU_WDGEN_REG, val.ptr, 2);
    val.value  = MT5728_WDT_INTFALG;
    MT5728_write_buffer(mte, MT5728_PMU_FLAG_REG, val.ptr, 2);
    val.value = MT5728_KEY;
    MT5728_write_buffer(mte, MT5728_SYS_KEY_REG, val.ptr, 2);
	val.value = 0X08;
	MT5728_write_buffer(mte, MT5728_CODE_REMAP_REG, val.ptr, 2);
	val.value = 0x0FFF;
    MT5728_write_buffer(mte, MT5728_SRAM_REMAP_REG, val.ptr, 2);
    msleep(50);
    //sram_write
    mt5728_sram_write(0x1800,(u8 *)mt5728_pgm_bin,sizeof(mt5728_pgm_bin));
    //sys_run
    msleep(50);
    val.value = MT5728_KEY;
    MT5728_write_buffer(mte, MT5728_SYS_KEY_REG, val.ptr, 2);
    val.value = MT5728_M0_RESET;
    MT5728_write_buffer(mte, MT5728_M0_CTRL_REG, val.ptr, 2);
    msleep(50);
	pr_info("[%s]  finish  \n",__func__);
}

static u8 mt5728_mtp_read(u32 addr, u8 * buf , u32 size, u8 mode) {
    u32 length ,i,status,times;
    vuc val;
    Pgm_Type_t pgm;
    pr_info("[%s] parameter size :%d\n",__func__,size);
    length = (size+(MTP_BLOCK_SIZE-1))/MTP_BLOCK_SIZE*MTP_BLOCK_SIZE;
    mt5728_run_pgm_fw();
	pr_info("[%s] Calculate the length to read:%d\n",__func__,length);
    for (i = 0; i < length/MTP_BLOCK_SIZE; ++i) {
        pgm.length = MTP_BLOCK_SIZE;
        pgm.addr = addr+i*MTP_BLOCK_SIZE;
        pgm.status = PGM_STATUS_READY;
        val.value = pgm.status;
        MT5728_write_buffer(mte,PGM_STATUS_ADDR,val.ptr,2);
        val.value = pgm.addr;
        MT5728_write_buffer(mte,PGM_ADDR_ADDR,val.ptr,2);
        val.value = pgm.length;
        MT5728_write_buffer(mte,PGM_LENGTH_ADDR,val.ptr,2);

        val.value = PGM_STATUS_READ;
        MT5728_write_buffer(mte,PGM_STATUS_ADDR,val.ptr,2);
        msleep(50);
        MT5728_read_buffer(mte, PGM_STATUS_ADDR, val.ptr, 2);
        status = val.ptr[0];
        times = 0;
        while(status == PGM_STATUS_READ) {
            msleep(50);
            MT5728_read_buffer(mte, PGM_STATUS_ADDR, val.ptr, 2);
			pr_info("[%s] Program reading",__func__);
            status = val.ptr[0];
            times+=1;
            if (times>100) {
                pr_err("[%s] error! Read OTP TImeout\n",__func__);
                return FALSE;
            }
        }
        if (status == PGM_STATUS_PROGOK) {
            pr_info("[%s] PGM_STATUS_PROGOK\n",__func__);
			MT5728_read_buffer(mte, PGM_DATA_ADDR, &buf[MTP_BLOCK_SIZE*i], MTP_BLOCK_SIZE);
        } else {
		    pr_err("[%s] OtpRead error , status = 0x%02x\n",__func__,status);
            return FALSE;
        }
        if (mode == TRUE) {
            /* code */
        }
    }
	return TRUE;
}

static u8 mt5728_mtp_write(u32 addr, u8 * buf , u32 len) {
    u32 offset;
    u32 write_size ,status,times;
    u32 write_retrycnt;
    s32 size;
    int i;
    vuc val;
    Pgm_Type_t pgm;
    size = len;
    offset = 0;
    write_size = 0;
    pr_info("[%s] Size to write:%d\n",__func__,size);
    mt5728_run_pgm_fw();
    write_retrycnt = 8;
    while(size>0) {
        if (size>MTP_BLOCK_SIZE) {
            pgm.length = MTP_BLOCK_SIZE;
            write_size = MTP_BLOCK_SIZE;
        } else {
            pgm.length = size;
            write_size = size;
        }
        pgm.addr = addr+offset;
        pgm.cs = pgm.addr;
        pgm.status = PGM_STATUS_READY;
        for (i = 0; i < pgm.length; ++i) {
            pgm.data[i] = buf[offset + i];
            pgm.cs += pgm.data[i];
        }
        pgm.cs+=pgm.length;
        val.value = pgm.status;
        MT5728_write_buffer(mte,PGM_STATUS_ADDR,val.ptr,2);
        val.value = pgm.addr;
        MT5728_write_buffer(mte,PGM_ADDR_ADDR,val.ptr,2);
        val.value = pgm.length;
        MT5728_write_buffer(mte,PGM_LENGTH_ADDR,val.ptr,2);
        val.value = pgm.cs;
        MT5728_write_buffer(mte,PGM_CHECKSUM_ADDR,val.ptr,2);
        MT5728_write_buffer(mte, PGM_DATA_ADDR, pgm.data, pgm.length);
        val.value = PGM_STATUS_WMTP;
        MT5728_write_buffer(mte,PGM_STATUS_ADDR,val.ptr,2);
        msleep(50);
        MT5728_read_buffer(mte, PGM_STATUS_ADDR, val.ptr, 2);
        status = val.ptr[0];
        times = 0;
        while(status == PGM_STATUS_WMTP) {
            msleep(50);
            MT5728_read_buffer(mte, PGM_STATUS_ADDR, val.ptr, 2);
            status = val.ptr[0];
            pr_info("[%s] Program writing\n",__func__);
            times+=1;
            if (times > 100) {
                pr_err("[%s] Program write timeout\n",__func__);
                return FALSE;
            }

        }
        if (status == PGM_STATUS_PROGOK) {
            size-=write_size;
            offset+=write_size;
            pr_info("[%s] PGM_STATUS_PROGOK\n",__func__);
        } else if (status == PGM_STATUS_ERRCS) {
            if (write_retrycnt > 0) {
                write_retrycnt--;
                pr_err("[%s]  ERRCS write_retrycnt:%d\n",__func__,write_retrycnt);
                continue;
            } else {
            pr_err("[%s] PGM_STATUS_ERRCS\n",__func__);
            return FALSE;
            }
        } else if (status == PGM_STATUS_ERRPGM) {
            if (write_retrycnt > 0) {
                write_retrycnt--;
                pr_err("[%s] ERRPGM write_retrycnt:%d\n",__func__,write_retrycnt);
                continue;
            } else {
            pr_err("[%s] PGM_STATUS_ERRPGM\n",__func__);
            return FALSE;
            }
        } else {
            if (write_retrycnt > 0) {
                write_retrycnt--;
                pr_err("[%s] NUKNOWN write_retrycnt:%d\n",__func__,write_retrycnt);
                continue;
            } else {
                pr_err("[%s] PGM_STATUS_NUKNOWN \n",__func__);
                return FALSE;
            }
        }
    }
	return TRUE;
}

static u8 mt5728_mtp_write_check(u32 flagaddr,u16 crc) {
    u8 i;
    u8 otpwrite_flagdata[4];
    u8 *otpwrite_flagread;
	otpwrite_flagread = kmalloc(1064, GFP_KERNEL);
    otpwrite_flagdata[0] = crc % 256;
    otpwrite_flagdata[1] = crc / 256;
    otpwrite_flagdata[2] = crc % 256;
    otpwrite_flagdata[3] = crc / 256;
    mt5728_mtp_read(flagaddr, otpwrite_flagread,4,1);
    for (i = 0; i < 4; ++i) {
        if (otpwrite_flagread[i] != otpwrite_flagdata[i]) {
            pr_info("[%s] MT5728 MTP  Flag not written: %d\n",__func__,i);
            return FALSE;
        }
    }
    printk("[%s] MT5728 MTP Flag has been written",__func__);
	kfree(otpwrite_flagread);
    return TRUE;
}

void mt5728_write_mtpok_flag(u32 flagaddr,u16 crc) {

}

static u8 mt5728_mtp_verify(u32 addr,u8 * data,u32 len) {
    vuc val;
    int waitTimeOutCnt;
    int status;
    u16 crcvlaue;
    u16 crcvlaue_chip; 
    Pgm_Type_t pgm;
    // crcvlaue = crc_ccitt(0xFFFF,(u8 *)MT5728_mtp_bin,sizeof(MT5728_mtp_bin));
    crcvlaue = crc_firmware(0x1021, 0xFFFF, (u8*)MT5728_mtp_bin, sizeof(MT5728_mtp_bin)) & 0xffff;
    val.value = 0;
    MT5728_write_buffer(mte,PGM_ADDR_ADDR,val.ptr,2);
    val.value = sizeof(MT5728_mtp_bin);
    MT5728_write_buffer(mte,PGM_LENGTH_ADDR,val.ptr,2);
    pgm.cs = crcvlaue;
    val.value = pgm.cs;
    MT5728_write_buffer(mte,PGM_CHECKSUM_ADDR,val.ptr,2);
    val.value = (1 << 6);
    MT5728_write_buffer(mte,PGM_STATUS_ADDR,val.ptr,2);
    waitTimeOutCnt = 100;
    while(waitTimeOutCnt--) {
        MT5728_read_buffer(mte, PGM_STATUS_ADDR, val.ptr, 2);
        status = val.ptr[0] | (val.ptr[1] << 8);
        if (status & (1 << 7)) {
        	MT5728_read_buffer(mte, PGM_DATA_ADDR, val.ptr, 2);
        	crcvlaue_chip = val.value;
            printk(KERN_ALERT "[%s]  mt5728_mtp_verify error,crcvlaue_chip:%x,crcvlaue:%x",__func__,crcvlaue_chip,crcvlaue);
            return FALSE;
        }
        //VERIFYOK
        if (status & (1 << 8)) {
            MT5728_read_buffer(mte, PGM_DATA_ADDR, val.ptr, 2);
        	crcvlaue_chip = val.value;
            printk(KERN_ALERT "[%s]  mt5728_mtp_verify success,crcvlaue_chip:%x,crcvlaue:%x",__func__,crcvlaue_chip,crcvlaue);
            return TRUE;
        }
        msleep(60);
    }/**/
    printk(KERN_ALERT "[%s] TimeOut cal_crc :0x%04x\n",__func__,crcvlaue);
    return FALSE;
}

static inline u16 crc_ccitt_byte(u16 crc, const u8 c)
{
	return (crc >> 8) ^ mt5728_crc_ccitt_table[(crc ^ c) & 0xff];
}

static u16 crc_ccitt(u16 crc, u8 const *buffer, size_t len)
{
	while (len--)
		crc = crc_ccitt_byte(crc, *buffer++);
    return crc;
}

static inline u16 crc_firmware(u16 poly1, u16 sed, u8 *buf, u32 n) {
    u32 i = 0;
    u32 j = 0;
    u16 poly = 0x1021;
    u16 crc  = 0xffff;
    // u32 addr = 0x20000070;
    for (j = 0; j < n; j+=2) {
        crc ^= (buf[j+1] << 8);
        for (i = 0;i < 8; i++) {
            crc = (crc & 0x8000) ? (((crc << 1) & 0xffff) ^ poly) : (crc << 1);
        }
        /* *(u8*)addr = buf[j+1]; */
        /* addr ++; */
        crc ^= (buf[j] << 8);
        for (i = 0;i < 8; i++) {
            crc = (crc & 0x8000) ? (((crc << 1) & 0xffff) ^ poly) : (crc << 1);
        }
        /* *(u8*)addr = buf[j]; */
        /* addr ++; */
        /* *(u32*)0x200000a0 = addr; */
    }
    return crc;
}
/*
  Send proprietary packet to Tx
  Step 1: write data into REG_PPP
  Step 2: write REG_CMD
*/
void MT5728_send_ppp(PktType *pkt) {
    vuc val;
    MT5728_write_buffer(mte, REG_PPP, (u8 *)pkt, SizeofPkt(pkt->header)+1);
    mte->fsk_status = FSK_WAITTING;
    val.value = SEND_PPP;
    MT5728_write_buffer(mte, REG_CMD, val.ptr, 2);
}
EXPORT_SYMBOL(MT5728_send_ppp);

int Get_adaptertype(void){
    PktType eptpkt;
    int count = 0;
    u8 fsk_msg[10];
    eptpkt.header  = PP18;
    eptpkt.msg_pakt.cmd     = CMD_ADAPTER_TYPE;
    MT5728_send_ppp(&eptpkt);
    while(mte->fsk_status == FSK_WAITTING){
        msleep(20);
        if((count++) > 50 ){
            pr_err("[%s] AP system judgement:FSK receive timeout \n",__func__);
            return (-1);
        }
    }
    if(mte->fsk_status == FSK_FAILED){
        pr_err("[%s] Wireless charging system judgement:FSK receive timeout \n",__func__);
        return (-1);
    }
    if(mte->fsk_status == FSK_SUCCESS){
        MT5728_read_buffer(mte,REG_BC,fsk_msg,10);
        pr_info("[%s] Information received : 0x%02x 0x%02x 0x%02x \n",__func__,fsk_msg[0],fsk_msg[1],fsk_msg[2]);
    }
    return fsk_msg[2];
}

static ssize_t get_reg(struct device* cd, struct device_attribute* attr, char* buf) {
    vuc val;
    ssize_t len = 0;
    int i = 0;
    for (i = 0; i < INDEX_MAX; i++) {
        if (reg_access[i].flag & REG_RD_ACCESS) {
            MT5728_read_buffer(mte, reg_access[i].addr, val.ptr, 2);
            len += snprintf(buf + len, PAGE_SIZE - len, "reg:%s 0x%04x=0x%04x,%d\n", reg_access[i].name, reg_access[i].addr, val.value,val.value);
        }
    }
    return len;
}

static ssize_t set_reg(struct device* cd, struct device_attribute* attr, const char* buf, size_t len) {
    unsigned int databuf[2];
    vuc val;
    u8 tmp[2];
    u16 regdata;
    int i = 0;
    int ret = 0;

    ret = sscanf(buf, "%x %x", &databuf[0], &databuf[1]);

    if (2 == ret) {
        for (i = 0; i < INDEX_MAX; i++) {
            if (databuf[0] == reg_access[i].addr) {
                if (reg_access[i].flag & REG_WR_ACCESS) {
                    // val.ptr[0] = (databuf[1] & 0xff00) >> 8;
                    val.value = databuf[1];
                    // val.ptr[1] = databuf[1] & 0x00ff; //big endian
                    if (reg_access[i].flag & REG_BIT_ACCESS) {
                        MT5728_read_buffer(mte, databuf[0], tmp, 2);
                        regdata = tmp[0] << 8 | tmp[1];
                        val.value |= regdata;
                        pr_info("get reg: 0x%04x  set reg: 0x%04x \n", regdata, val.value);
                        MT5728_write_buffer(mte, databuf[0], val.ptr, 2);
                    } else {
                        pr_info("Set reg : [0x%04x]  0x%x \n", databuf[0], val.value);
                        MT5728_write_buffer(mte, databuf[0], val.ptr, 2);
                    }
                }
                break;
            }
        }
    }else{
        pr_info("Error \n");
    }
    return len;
}

static ssize_t fast_charging_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) {
    vuc val;
    int error;
    unsigned int a;
    error = kstrtouint(buf, 10, &a);
    val.value = (unsigned short)a;

    if (error)
        return error;
    if ((val.value < 0) || (val.value > 20000)) {
        pr_info("[%s] MT5728 Parameter error\n",__func__);
        return count;
    }
    fast_vfc(val.value);
    return count;
}


static ssize_t get_adapter(struct device* cd, struct device_attribute* attr, char* buf) {
    ssize_t len = 0;
    int rc;
    rc = Get_adaptertype();
    if(rc == (-1)){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Failed to read adapter type\n", __func__);
    }else if(rc == ADAPTER_NONE ){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Adapter type : Unknown\n", __func__);
    }else if(rc == ADAPTER_SDP ){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Adapter type : SDP\n", __func__);
    }else if(rc == ADAPTER_CDP ){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Adapter type : CDP\n", __func__);
    }else if(rc == ADAPTER_DCP ){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Adapter type : DCP\n", __func__);
    }else if(rc == ADAPTER_QC20 ){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Adapter type : QC2.0\n", __func__);
    }else if(rc == ADAPTER_QC30 ){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Adapter type : QC3.0\n", __func__);
    }else if(rc == ADAPTER_PD ){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Adapter type : PD\n", __func__);
    }else if(rc == ADAPTER_FCP ){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Adapter type : FCP\n", __func__);
    }else if(rc == ADAPTER_SCP ){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Adapter type : SCP\n", __func__);
    }else if(rc == ADAPTER_DCS ){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Adapter type : DC source\n", __func__);
    }else if(rc == ADAPTER_AFC ){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Adapter type : AFC\n", __func__);
    }else if(rc == ADAPTER_PDPPS ){
        len += snprintf(buf + len, PAGE_SIZE - len, "[%s] Adapter type : PD PPS\n", __func__);
    }
    return len;
}


static ssize_t brushfirmware(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) {
    int error;
    int mt5728VoutTemp;
    unsigned int pter;
    error = kstrtouint(buf, 10, &pter);
    mt5728_mtp_write_flag = 1;
    mt5728VoutTemp = Mt5728_get_vout();
    if (mt5728VoutTemp < 0) {
        //mt5728_ap_open_otg_boost(true);
    }
    msleep(1000);        //Wait for Vout voltage to stabilize
    if(pter == 1){
        printk(KERN_ALERT "[%s]  brush MTP program\n",__func__);
        if(mt5728_mtp_write(0x0000,(u8 *)MT5728_mtp_bin,sizeof(MT5728_mtp_bin))){
            printk(KERN_ALERT "[%s] Write complete, start verification \n",__func__);
            if (mt5728_mtp_verify(0x0000,(u8 *)MT5728_mtp_bin,sizeof(MT5728_mtp_bin))) {
                printk(KERN_ALERT "[%s] mt5728_mtp_verify OK \n",__func__);
            } else {
                printk(KERN_ALERT "[%s] mt5728_mtp_verify check program failed \n",__func__);
            }
        }
    }else if(pter == 2){
        u16 crcvlaue;
        crcvlaue = crc_ccitt(0xFFFF,(u8 *)MT5728_mtp_bin,sizeof(MT5728_mtp_bin));
        printk(KERN_ALERT "[%s] cal_crc :0x%04x\n",__func__,crcvlaue);
        if(mt5728_mtp_write_check(MTP_WRITE_FLAG_ADDR,crcvlaue)==TRUE){
            printk(KERN_ALERT "[%s] mt5728_mtp_write exit \n",__func__);
        } else {
            if(mt5728_mtp_write(0x0000,(u8 *)MT5728_mtp_bin,sizeof(MT5728_mtp_bin))){
                printk(KERN_ALERT "[%s] Write complete, start verification \n",__func__);
                if (mt5728_mtp_verify(0x0000,(u8 *)MT5728_mtp_bin,sizeof (MT5728_mtp_bin))) {
                    printk(KERN_ALERT "[%s] mt5728_mtp_verify OK \n",__func__);
                    mt5728_write_mtpok_flag(MTP_WRITE_FLAG_ADDR,crcvlaue);
                    printk(KERN_ALERT "[%s] MT5728_write_mtp_flag \n",__func__);
                } else {
                    printk(KERN_ALERT "[%s] mt5728_mtp_verify check program failed \n",__func__);
                }
            }
        }
    } else if(pter == 3)
	{
		schedule_delayed_work(&mte->fwcheck_work, msecs_to_jiffies(100));
	}
    if (mt5728VoutTemp < 0) {
        //mt5728_ap_open_otg_boost(false);
    }
    printk(KERN_ALERT "[%s] Exit this operation \n",__func__);
    mt5728_mtp_write_flag = 0;
    return count;
}

static DEVICE_ATTR(fast_charging, S_IRUGO | S_IWUSR, NULL, fast_charging_store);
static DEVICE_ATTR(reg, S_IRUGO | S_IWUSR, get_reg, set_reg);
static DEVICE_ATTR(adapter_type,S_IRUGO,get_adapter,NULL);
static DEVICE_ATTR(brushFW,S_IRUGO | S_IWUSR,NULL,brushfirmware);
static DEVICE_ATTR(TxFirmware,S_IRUGO | S_IWUSR,updata_txFW,NULL);
static DEVICE_ATTR(epp_set_vout, S_IRUGO | S_IWUSR, NULL,  Mt5728_set_vout);

void fast_vfc(int vol) {
    vuc val;
    val.value = vol;
    MT5728_write_buffer(mte, REG_VFC, val.ptr, 2);
    val.value = FAST_CHARGE;
    MT5728_write_buffer(mte, REG_CMD, val.ptr, 2);
    pr_info("%s,write reg_cmd : 0x%04x,\n", __func__, val.value);
}

static void download_txSRAM_code(void) {
    vuc val;
    val.value  = MT5728_WDG_DISABLE;
    MT5728_write_buffer(mte, MT5728_PMU_WDGEN_REG, val.ptr, 2);
    MT5728_write_buffer(mte, MT5728_PMU_WDGEN_REG, val.ptr, 2);
	MT5728_write_buffer(mte, MT5728_PMU_WDGEN_REG, val.ptr, 2);
    val.value = MT5728_WDT_INTFALG;
    MT5728_write_buffer(mte,MT5728_PMU_FLAG_REG,val.ptr,2);
    val.value = MT5728_KEY;
    MT5728_write_buffer(mte,MT5728_SYS_KEY_REG,val.ptr,2);
    val.value = MT5728_M0_HOLD | LBIT(9);
    MT5728_write_buffer(mte,MT5728_M0_CTRL_REG,val.ptr,2);
    msleep(50);
    mt5728_sram_write(0x800,(u8 *)MT572x_TxSRAM_bin,sizeof(MT572x_TxSRAM_bin));
    val.value = 0x02;
    MT5728_write_buffer(mte,MT5728_CODE_REMAP_REG,val.ptr,2);
    val.value = 0xff0f;
    MT5728_write_buffer(mte,MT5728_SRAM_REMAP_REG,val.ptr,2);
    val.value = MT5728_KEY;
    MT5728_write_buffer(mte,MT5728_SYS_KEY_REG,val.ptr,2);
    val.value = MT5728_M0_RESET ;
    MT5728_write_buffer(mte,MT5728_M0_CTRL_REG,val.ptr,2);
    msleep(50);
    printk(KERN_ALERT "[%s] finish\n",__func__);
}

static ssize_t updata_txFW(struct device* cd, struct device_attribute* attr, char* buf) {
    ssize_t len = 0;
    download_txSRAM_code();
    return len;
}
void fastcharge_afc(void) {
    vuc val;
    vuc temp, fclr, scmd;
    scmd.value = 0;

    MT5728_read_buffer(mte, REG_INTFLAG, val.ptr, 2);
    fclr.value = FAST_CHARGE;
    if (val.value & INT_AFC_SUPPORT) {
        pr_info("MT5728 %s ,version 0.1 Tx support samsung_afc\n", __func__);
        temp.value = 9000;
        MT5728_write_buffer(mte, REG_VFC, temp.ptr, 2);
        scmd.value |= FAST_CHARGE;
        scmd.value |= CLEAR_INT;
        MT5728_write_buffer(mte, REG_INTCLR, fclr.ptr, 2);
        pr_info("%s,version 0.1 write reg_clr : 0x%04x,\n", __func__, fclr.value);
        MT5728_write_buffer(mte, REG_CMD, scmd.ptr, 2);
        pr_info("%s,version 0.1 write reg_cmd : 0x%04x,\n", __func__, scmd.value);
    }
}
//prize  add by lipengpeng 20210308 start

#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
static ssize_t gettx_flag_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	//if(TXupon_coil==1){
       return sprintf(buf, "%d", atomic_read(&mte->is_tx_mode));
	//}else{
	//	return sprintf(buf, "%d", 0);
	//}
}
static DEVICE_ATTR(gettxflag, 0644, gettx_flag_show, NULL);

/*static ssize_t disable_tx_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct MT5728_dev *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->ops_mutex);
	gpio_direction_output(chip->trxset_gpio,0);
	//charger_dev_enable_otg(g_info->primary_charger, false);
	//enable_boost_polling(false);
	mt_vbus_off();
	gpio_direction_output(chip->otgen_gpio,0);
	atomic_set(&mte->is_tx_mode,0);
	mutex_unlock(&chip->ops_mutex);
	printk(KERN_INFO"mt5728 disable_tx\n");
	return sprintf(buf, "%d", 1);
}
static DEVICE_ATTR(disabletx, 0644, disable_tx_show, NULL);
*/

static ssize_t enable_tx_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d", atomic_read(&mte->is_tx_mode));//atomic_read(&mte->is_tx_mode);// prize add by lpp 20210308
}
int revere_mode=0;
static ssize_t enable_tx_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
    int error;
    unsigned int temp;

	//struct i2c_client *client = to_i2c_client(dev);
	//struct MT5728_dev *chip = i2c_get_clientdata(client);

    error = kstrtouint(buf, 10, &temp);
	printk("LPP---enable_tx_store temp=%d\n",temp);

    if (error)
        return error;
    if(temp==1) {
			//mutex_lock(&chip->ops_mutex);
			mt_vbus_revere_off();
			printk(KERN_INFO"mt5728 111 enable tx\n");
			mdelay(3);
			printk(KERN_INFO"mt5728 222 enable tx\n");
			turn_on_otg_charge_mode(0);  //GPIO109---->high  //prize add by lipengpeng 20210408  GPIO ---> low.
			printk(KERN_INFO"mt5728 333 enable tx\n");
			turn_on_rever_5725(0);//OD5-->high  //GPIO88
			printk(KERN_INFO"mt5728 444 enable tx\n");
			set_otg_gpio(0);//OD7--->low    2541-->OTG low  //GPIO87
			mte->tx_count=0;
			printk(KERN_INFO"mt5728 555 enable tx\n");
			atomic_set(&mte->is_tx_mode,1);
			printk(KERN_INFO"mt5728 666 enable tx\n");
			revere_mode=1;
			printk(KERN_INFO"mt5728 777 enable tx\n");
			mt_vbus_revere_on();// open vbus
//#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
			mt_vbus_reverse_on_limited_current();  // prize add by lipengpeng 20210322 set otg voltage  5.4V
//#endif
			//mutex_unlock(&chip->ops_mutex);
			printk(KERN_INFO"mt5728 888 enable tx\n");
    }else{
			turn_on_otg_charge_mode(0);//GPIO109--->low
			turn_off_rever_5725(0);//OD5-->low  //GPIO88
			set_otg_gpio(0);//OD7--->low  //GPIO87
			mte->tx_count=0;
//#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
			mt_vbus_reverse_off_limited_current();// prize add by lipengpeng 20210322 close otg voltage
//#endif
			mt_vbus_revere_off();
            atomic_set(&mte->is_tx_mode,0);
			revere_mode=0;
			printk(KERN_INFO"mt5728 disable_tx\n");
	}
    return count;
}

static DEVICE_ATTR(enabletx, 0664, enable_tx_show, enable_tx_store);

#endif
//prize  add by lipengpeng 20210308 end
//prize add by lipengpeng 20220623 start 
static ssize_t wireless_connect_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	if(get_MT5728_status()==0)
	{
		return sprintf(buf, "%d", 1);
	}else{
	    return sprintf(buf, "%d", 0);
	}
}
static DEVICE_ATTR(wireless_connect, 0444, wireless_connect_show, NULL);

//prize add by lipengpeng 20220623 end

static struct attribute* mt5728_sysfs_attrs[] = {
    &dev_attr_fast_charging.attr,
    &dev_attr_reg.attr,
    &dev_attr_adapter_type.attr,
    &dev_attr_brushFW.attr,
    &dev_attr_TxFirmware.attr,
   // &dev_attr_otg.attr,
    // &dev_attr_otp.attr,
   // &dev_attr_mt5728_en.attr,
   // &dev_attr_mt5728_flag.attr,
    &dev_attr_epp_set_vout.attr,
    NULL,
};

static const struct attribute_group mt5728_sysfs_group = {
    .name  = "mt5728group",
    .attrs = mt5728_sysfs_attrs,
};

static const struct regmap_config MT5728_regmap_config = {
    .reg_bits     = 16,
    .val_bits     = 8,
    .max_register = 0xFFFF,
};

/**
 * [MT5728_send_EPT End Power Transfer Packet]
 * @param endreson [end powr Reson]
 */
void MT5728_send_EPT(u8 endreson) {
    PktType eptpkt;
    eptpkt.header  = PP18;
    eptpkt.msg_pakt.cmd     = ENDPOWERXFERPACKET;
    eptpkt.msg_pakt.data[0] = endreson;
    MT5728_send_ppp(&eptpkt);
}

void Set_staystate_current(void)
{
	pr_info("[%s],call\n", __func__);
	wireless_charge_chage_current();
	return;
}
EXPORT_SYMBOL(Set_staystate_current);

int get_mt5728_voltage(void){
	vuc val;
	//if(gpio_get_value(mte->statu_gpio)){
		MT5728_read_buffer(mte,REG_VOUT,val.ptr,2);
		pr_err("%s: vol read  vol=%d\n", __func__,val.value);
	//}
	return val.value;
}
EXPORT_SYMBOL(get_mt5728_voltage);

int get_mt5728_Iout(void){
	vuc val;
	//if(gpio_get_value(mte->statu_gpio)){
		MT5728_read_buffer(mte,REG_IOUT,val.ptr,2);
		pr_err("%s: vol read  vol=%d\n", __func__,val.value);
	//}
	return val.value;
}

static ssize_t mt5728_mtp_crc_slef_check_success(void)
{
    int i;
    int ret;
    vuc val;
    // u8 status = 0;
     /* wait for 10ms*100=1000ms for status check, typically 300ms */
     for (i = 0; i < 100; i++) {
         msleep(10);
         ret = MT5728_read_buffer(mte, 0x0a, val.ptr, 1);
         printk("wait mtp_crc_slef_check:%x,%x\n",val.ptr[0],val.ptr[1]);
         if (ret) {
             printk("mtp_crc_slef_check: read failed\n");
             return ret;
         }
         if ((val.ptr[0] & MT5728_OP_MODE_EXT_FWCRC_OK) && 
             !(val.ptr[0] & MT5728_OP_MODE_EXT_FWCRC_ERR))
             return 0;
     }
    return -1;
}

static ssize_t mt5728_mtp_crc_fw_check_itself(u16 len, u16 crc)
{
    int ret;
    vuc val;

    val.value = crc;
    ret = MT5728_write_buffer(mte, REG_FW_CRC_VAL, val.ptr, 2);

    val.value = len;
    ret += MT5728_write_buffer(mte, REG_FW_CRC_LEN, val.ptr, 2);

    val.value = MT5728_RX_FWCRCCHECK;
    ret += MT5728_write_buffer(mte, 0x0006, val.ptr, 2);
    msleep(100);
    ret += mt5728_mtp_crc_slef_check_success();

    if (ret) {
     printk("crc_check: failed\n");
     return ret;
    }
    printk("[crc_check] succ\n");
    return 0;
}

static void mt5728_fwcheck_work_func(struct work_struct* work) {
   int mt5728VoutTemp;
   u8 fwver[2];
    //if(wls_work_online ==1 )
   //{
    //MT5728_read_buffer(mte, REG_FW_VER, fwver, 2);  
    //printk(KERN_ALERT "MT5728 fw_version_in_chip : 0x%x%x\n",fwver[0], fwver[1]);
    //return;
    //}
    mt5728VoutTemp = Mt5728_get_vout();
    if (mt5728VoutTemp < 0) {
        //reserse_charge_online = 1;
        //mt5728_ap_open_otg_boost(true);
   }
    msleep(200);        //Wait for Vout voltage to stabilize
    mt5728VoutTemp = Mt5728_get_vout();
    MT5728_read_buffer(mte, REG_FW_VER, fwver, 2);
    printk(KERN_ALERT "MT5728 fw_version_in_chip : 0x%x%x\n",fwver[0], fwver[1]);
    if(mt5728_mtp_crc_fw_check_itself(MT5728_MTP_CRC_LEN,MT5728_MTP_CRC_VAL))
        {
      mt5728_mtp_write_flag = 1;
        printk(KERN_ALERT "MT5728 fw_version not match : 0x%x\n",MT5728_FWVERSION);
        printk(KERN_ALERT "[%s]  brush MTP program\n",__func__);
        if(mt5728_mtp_write(0x0000,(u8 *)MT5728_mtp_bin,sizeof(MT5728_mtp_bin))){
            printk(KERN_ALERT "[%s] Write complete, start verification \n",__func__);
            if (mt5728_mtp_verify(0x0000,(u8 *)MT5728_mtp_bin,sizeof(MT5728_mtp_bin))) {
                printk(KERN_ALERT "[%s] mt5728_mtp_verify OK \n",__func__);
                    MT5728_read_buffer(mte, REG_FW_VER, fwver, 2);
    printk(KERN_ALERT "MT5728 fw_version_in_chip222 : 0x%x%x\n",fwver[1], fwver[0]);
            } else {
                printk(KERN_ALERT "[%s] mt5728_mtp_verify check program failed \n",__func__);
                    MT5728_read_buffer(mte, REG_FW_VER, fwver, 2);
    printk(KERN_ALERT "MT5728 fw_version_in_chip333 : 0x%x%x\n",fwver[1], fwver[0]);
            }
        }
        //if (mt5728VoutTemp < 0) {
          //mt5728_ap_open_otg_boost(false);
       // }
        mt5728_mtp_write_flag = 0;
    }
}
//prize add by lipengpeng 20220611 start 
int get_mt5728_charge_protocol(void)
{
	return mte->charge_protocol;
}
EXPORT_SYMBOL(get_mt5728_charge_protocol);
//prize add by lipengpeng 20220611 end 

void En_Dis_add_current(int i){
	vuc protocol;
	protocol.ptr[0] = i;
	protocol.ptr[1] = 6;//default value in Firmware IIC address REG_CURFUNC+1
	printk(" En_Dis_add_current i=%d  protocol.ptr[0]=%02x\n",i,protocol.ptr[0]);
	//MT5728_write_buffer(mte, REG_CURFUNC, protocol.ptr,1);
}
EXPORT_SYMBOL(En_Dis_add_current);
#if 1
extern int gezi_boot_mode;

static void MT5728_add_current(void){
	vuc protocol;
	vuc epp;
	long voltage = 9000;
	long tcurrent =0;
	long tvoltage =0;
	long maxpower =0;
	long powertemp =0;
	long maxchargecurrent = 1000000;
	epp.value =0;
    protocol.value =0;
	//mte->charge_current = 1000000;
	tvoltage = get_mt5728_voltage();
	pr_err("[%s] Rx Vout:%d\n", __func__,tvoltage);
	tcurrent = get_mt5728_Iout();
	pr_err("[%s] Rx Iout:%d\n", __func__,tcurrent);
	if(1){
		 MT5728_read_buffer(mte, 0x00a7, protocol.ptr,1);   // tx type
		 pr_err("[%s]: protocol read 1 : %02x%02x  protocol.value =0x%04x  protocol.value =%d\n",__func__,protocol.ptr[0],protocol.ptr[1],protocol.value,protocol.value);
		 if(protocol.ptr[0]== 1){
		 	mte->charge_protocol = BPP;
			mte->wireless_max_power = 5;
			voltage = 5000;
			maxchargecurrent = 1000000;
			pr_info("[%s]: BPP Load power is recommended to be less than %dW\n", __func__,mte->wireless_max_power);
		 } else if(protocol.ptr[0] == 2){
		    mte->charge_protocol = AFC;
			mte->wireless_max_power = 10;
			voltage = 9000;
			maxchargecurrent = 1100000;
			pr_info("[%s]: AFC Load power is recommended to be less than %dW\n", __func__,mte->wireless_max_power);
		 } else if(protocol.ptr[0] == 3){
		    mte->charge_protocol = EPP;
			MT5728_read_buffer(mte,REG_MAX_POWER,epp.ptr,1);
		    epp.ptr[0] = epp.ptr[0]/2;
			voltage = 9000;
		    pr_info("[%s]: EPP Load power is recommended to be less than %dW\n", __func__,epp.ptr[0]);
		    mte->wireless_max_power = epp.ptr[0];
			//if(mte->wireless_max_power > 15) mte->wireless_max_power = 15;
			if (mte->wireless_max_power > mte->rx_power_cap){
				dev_info(mte->dev,"%s: limit rx power to %d from %d\n",mte->rx_power_cap,mte->wireless_max_power);
				mte->wireless_max_power = mte->rx_power_cap;
			}
			maxchargecurrent = mte->wireless_max_power * 1000 / 9 * 92 *10;
		 } else {
			chr_err("[%s]: read 0x00a7 info no BPP EPP AFC protocol\n", __func__);
		 }
     }
	// if(mte->wireless_max_power ==  5)  maxpower = mte->wireless_max_power * 1000;
     //else                               maxpower = mte->wireless_max_power * 1000;
     maxpower = mte->wireless_max_power * 1000 * mte->rx_efficiency / 100;
	 pr_err("[%s]: wireless system support power  = %d W\n",__func__,mte->wireless_max_power);
	 pr_err("[%s]: Max charge maxchargecurrent = %d mA\n",__func__,(maxchargecurrent/1000));
	 pr_err("[%s]: Get wireless output voltage = %d mV\n",__func__,voltage);
	 tcurrent = mte->input_current / 1000;
     pr_err("[%s]: last time,Set input tcurrent = %d mA\n",__func__,tcurrent);
	 if(tcurrent == 0) tcurrent = 100;
	 powertemp = voltage * tcurrent / 1000 ;
	 pr_err("[%s]: powertemp = %d , maxpower = %d\n",__func__,powertemp,maxpower);
	 if(powertemp < maxpower ){
	 	powertemp = powertemp + 2000;
		if(powertemp > maxpower) {
			powertemp = maxpower;
			pr_info("[%s] DISABLE_ADD_CURRENT INT!\n", __func__);
			En_Dis_add_current(DISABLE_ADD_CURRENT);
		}
		pr_info("[%s] add power  = %d mW\n", __func__,powertemp);
	 	mte->input_current = powertemp * 1000 / voltage * 1000;
		if(mte->charge_protocol == BPP){
			if(mte->input_current > 1000000) mte->input_current = 1000000;
		}
		if(mte->charge_protocol == AFC){
			if(mte->input_current > 1100000) mte->input_current = 1100000;
		}

		if(mte->input_current > maxchargecurrent)  mte->input_current= maxchargecurrent;
		mte->charge_current = powertemp / 4 * 1000;

//#if defined(CONFIG_PRIZE_ONLY5W_DISPLAY_15W)
		if(mte->charge_protocol == BPP)
		{
			mte->input_current  = 1000000;
			mte->charge_current = 1000000;
		}
		

		
		if(mte->charge_protocol == EPP  && mte->input_current > 1000000){
			
			
			if(gezi_boot_mode == 8 || gezi_boot_mode == 9){
				
				if(mte->wireless_max_power > 10){
					mte->input_current  = 1000000;
					mte->charge_current = 2000000;
				}
				else if(mte->wireless_max_power == 5){
					mte->input_current  = 800000;
					mte->charge_current = 1000000;
				}
				else{
					mte->input_current  = 800000;
					mte->charge_current = 1000000;
				}
			}
			else{
			
				if(mte->wireless_max_power > 10){
					mte->input_current  = 1200000;
					mte->charge_current = 2650000;
				}
				else if(mte->wireless_max_power == 5){
					mte->input_current  = 1000000;
					mte->charge_current = 1000000;
				}
				else{
					mte->input_current  = 1100000;
					mte->charge_current = 1800000;
				}
			}
		}
//#endif
//prize add by lipengpeng 20210305 end
		pr_info("[%s] Set input_current =  %d, Set charge_current = %d\n", __func__,mte->input_current,mte->charge_current);
		Set_staystate_current();
		schedule_delayed_work(&mte->add_current_work,100);
	 }else{
	 	pr_info("[%s] return!\n", __func__);
		En_Dis_add_current(DISABLE_ADD_CURRENT);
	 	return;
	 }
}
#endif
//mike add 2023.1.4
static void mt5728_removed_form_tx(void) {
    mte->mt5728_ldo_on_flag = 0;
    printk(KERN_ALERT "mt5728_removed_form_tx\n");
}
/**
 * [mt5728_set_pmic_input_current_ichg description]
 * @param input_current [mA]
 */
static void mt5728_set_pmic_input_current_ichg(int input_current,int charge_current) {
    // charger_dev_set_input_current(primary_charger,input_current * 1000);
    mte->input_current = input_current * 1000;
	mte->charge_current = charge_current * 1000;
    pr_info("[%s] Set input_current =  %d, Set charge_current = %d\n", __func__,mte->input_current,mte->charge_current);
    Set_staystate_current();
}

static void mt5728_charger_work_func(struct work_struct* work) {
    int vbus_now = 0;
    int vbus_read_temp = 0;
	vuc val;
	printk(KERN_ALERT "%s\n",__func__);
    Mt5728_get_vrect();
    vbus_read_temp = Mt5728_get_vout();
    if ((vbus_read_temp >= 0) && (vbus_read_temp <= 20000)) {
        vbus_now = Mt5728_get_vout();
    }
    //check if rx put on tx surface,if 1 rx removed from tx
    if(((vbus_now < 1000) && (mte->mt5728_ldo_on_flag == 1))|| (vbus_read_temp == -1)) {
		printk(KERN_ALERT "%s %d 111\n",__func__,powergood_err_cnt);
        powergood_err_cnt++;
        if(powergood_err_cnt > 20) {                    //Cancel wireless charging icon display
            mt5728_removed_form_tx();
            return;
        } else {
			printk(KERN_ALERT "%s 222\n",__func__);
            schedule_delayed_work(&mte->charger_work, msecs_to_jiffies(100));
            return;
        }
    } else {
        powergood_err_cnt = 0;
    }
	printk(KERN_ALERT "%s  333 %d\n",__func__,powergood_err_cnt);

    //After waiting for 10 seconds, change the PMIC input current
    if(setup_iout_start_cnt < 100) {
		printk(KERN_ALERT "%s  444 %d\n",__func__,setup_iout_start_cnt);
        setup_iout_start_cnt++;
        if (setup_iout_start_cnt == 70) {
            if (mt5728_epp_ctrl_vout_flag) {
                val.value = rx_vout_max;
                printk(KERN_ALERT "mt5728 epp set Vout to:%d,ptpower%d\n",rx_vout_max,mte->wireless_max_power);
                MT5728_write_buffer(mte, REG_VOUTSET, val.ptr, 2);
                val.value = VOUT_CHANGE;
                MT5728_write_buffer(mte, REG_CMD, val.ptr, 2);
                mt5728_epp_ctrl_vout_flag = 0;
                msleep(100);
                set_rx_ocp(2600);
            }
        }
    } else {
		printk(KERN_ALERT "%s  555 %d\n",__func__,current_change_interval_cnt);
        //Change one step every 500ms
        if(current_change_interval_cnt++ > 5) {
            current_change_interval_cnt = 0;
            // EPP check vout > 8V,2023.2.20
            if((rx_vout_max == 9000) && (vbus_now > 8000)) {
                if((input_current_limit < rx_iout_max)) {
                   input_current_limit += 100;
                }
                mt5728_set_pmic_input_current_ichg(input_current_limit,3000);                
            } else {
                printk(KERN_ALERT "%s  mt5728 vout < 8000: %d\n",__func__,vbus_now);
            }
            // BPP check vout > 4V,2023.2.20
            if((rx_vout_max == 5000) && (vbus_now > 4000)) {
                if((input_current_limit < rx_iout_max)) {
                   input_current_limit += 100;
                }
                mt5728_set_pmic_input_current_ichg(input_current_limit,3000);                
            } else {
                printk(KERN_ALERT "%s  mt5728 vout < 4000: %d\n",__func__,vbus_now);
            }
        } 
    }
    schedule_delayed_work(&mte->charger_work, msecs_to_jiffies(100));
}

//reverse charge delay work
static void mt5728_reverse_work_func(struct work_struct* work) {
    if(chip->rxdetect_flag == 1) {
        if(chip->rxremove_flag == 1) {
            chip->rxdetect_flag = 0;
            chip->rxremove_flag = 0;
            reverse_timeout_cnt = 0;
            schedule_delayed_work(&mte->reverse_charge_work, msecs_to_jiffies(100));
            return;
        } else {
            //Check the remaining battery capacity
            //Check battery temperature
        }
    } else {
        reverse_timeout_cnt++;
        if(reverse_timeout_cnt > 1200) { //RX put on timeout, active shutdown
            //mt5728_reverse_charge(0);
            return;
        }
    }
    schedule_delayed_work(&chip->reverse_charge_work, msecs_to_jiffies(100));
}
void print_curfunc_info(void){
	vuc protocol;
    protocol.value =0;
    if(1){
		//MT5728_read_buffer(mte, REG_CURFUNC, protocol.ptr,1);
		pr_err("%s: protocol read 1 : %02x%02x  protocol.value =0x%04x  protocol.value =%d\n",__func__,protocol.ptr[0],protocol.ptr[1],protocol.value,protocol.value);
    }else{
        pr_err("%s: statu_gpio is low\n",__func__);
	}
}
static ssize_t Mt5728_set_vout(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
    vuc val;
    int error;
    unsigned int a;
    error = kstrtouint(buf, 10, &a);
    val.value = (unsigned short)a;
    
    printk(KERN_ALERT "%s,write reg_cmd : 0x%04x,\n", __func__, val.value);
    MT5728_write_buffer(mte, REG_VOUTSET, val.ptr, 2);
    val.value = VOUT_CHANGE;
    MT5728_write_buffer(mte, REG_CMD, val.ptr, 2);
    printk(KERN_ALERT "%s,write reg_cmd : 0x%04x,\n", __func__, val.value);
    return 0;
}

static ssize_t Mt5728_get_fsk_buf_0_1(void) {
    vuc fsk;
    if(MT5728_read_buffer(mte,REG_BC,fsk.ptr,2) < 0) {
       printk(KERN_ALERT "%s,Mt5728_get_fsk error!\n", __func__);
    } else {
       printk(KERN_ALERT "%s,Mt5728_get_fsk : 0x%x,0x%x KHz !\n", __func__, fsk.ptr[0],fsk.ptr[1]);
    }
    return fsk.value;
}

int set_rx_ocp(uint16_t ocp)
{
    vuc temp;
    temp.value = ocp;
    MT5728_write_buffer(mte, 0x0034, temp.ptr, 2); // reg ocp
    temp.value = OCP_CHANGE;
    MT5728_write_buffer(mte, REG_CMD, temp.ptr, 2);
    return 0;
}

static ssize_t Mt5728_get_TxPeriod(void) {
    vuc period;
    if(MT5728_read_buffer(mte,0x0020,period.ptr,2) < 0) {
       printk(KERN_ALERT "%s,Mt5728_get_period error!\n", __func__);
    } else {
       printk(KERN_ALERT "%s,Mt5728_get_period : %d,%d KHz !\n", __func__, period.value,(80000/(period.value + 1)));
    }
    return period.value;
}

static ssize_t Mt5728_get_iout(void) {
    vuc iout;
    if(MT5728_read_buffer(mte,REG_IOUT,iout.ptr,2) < 0) {
       printk(KERN_ALERT "%s,Mt5728_get_iout error!\n", __func__);
    } else {
       printk(KERN_ALERT "%s,Mt5728_get_iout : %dmA !\n", __func__, iout.value);
    }
    return iout.value;
}

static ssize_t Mt5728_get_vout(void) {
    vuc vout;
    if(MT5728_read_buffer(mte,REG_VOUT,vout.ptr,2) < 0) {
        printk(KERN_ALERT "%s,Mt5728_get_vout error!\n", __func__);
       return -1;
    }
    Mt5728_get_iout();
    Mt5728_get_TxPeriod();
	//printk(KERN_ALERT "%s,Mt5728_get_vout_old : %d !\n", __func__,mt5728_vout_old);
    //if (abs(vout.value - mt5728_vout_old) > 1000) {
       mt5728_vout_old = vout.value;
       printk(KERN_ALERT "%s,Mt5728_get_vout : %dmV !\n", __func__, vout.value);
    //}
    return vout.value;
}
#if 1
static ssize_t Mt5728_get_vrect(void) {
    vuc vout;
    if(MT5728_read_buffer(mte,REG_VRECT,vout.ptr,2) < 0) {
       return -1;
    }
	//printk(KERN_ALERT "%s,Mt5728_get_vrect_old : %d !\n", __func__,mt5728_vout_old);
    //if (abs(vout.value - mt5728_vrect_old) > 1000) {
       mt5728_vrect_old = vout.value;
       printk(KERN_ALERT "%s,Mt5728_get_vrect : %dmV !\n", __func__, vout.value);
    //}
    return vout.value;
}
#endif
//prize add by lipengpeng 20210419 start
/*
 * vout,rx vout,mv
 * return 0,OK,others failed
 */
int set_rx_vout(uint16_t vout)
{
	vuc temp;
	uint16_t pre_vout=0;
	MT5728_read_buffer(mte, REG_VOUTSET, temp.ptr, 2);
    pre_vout= temp.value;
	if (pre_vout == vout) {
		return -1; /*vout already set or vout set running*/
	}
	temp.value = vout;
	MT5728_write_buffer(mte, REG_VOUTSET, temp.ptr, 2);
	temp.value = VOUT_CHANGE;
	MT5728_write_buffer(mte, REG_CMD, temp.ptr, 2);
	msleep(10);
	return 0;
}

void mt5728_SetFodPara(void) {
    MT5728_write_buffer(mte, REG_FOD, (unsigned char *)mt5728fod, 16);
    //MT5728_write_buffer(mte, REG_FODREG_FOD_EPP, (unsigned char *)mt5728fod_epp, 16);
}

void MT5728_irq_handle(void) {
    vuc val;
	vuc temp,scmd,fclr;
    int iic_rf;
    scmd.value = 0;
    pr_info("----------------MT5728_delayed_work-----------------------\n");
    temp.value = MT5728ID;
    iic_rf = MT5728_write_buffer(mte,REG_CHIPID,temp.ptr,2);
    if(iic_rf < 0){
        pr_err("[%s] Chip may not be working\n",__func__);
        return;
    }
    MT5728_read_buffer(mte,REG_FW_VER,temp.ptr,2);
	printk("%s: fw_ver 0x%2x%2x\n",__func__,temp.ptr[0],temp.ptr[1]);

	MT5728_read_buffer(mte,0x04,temp.ptr,2);
	printk("%s: System mode %x\n",__func__,temp.ptr[1]);
	
    if(temp.ptr[1] & RXMODE){
        pr_info("[%s] The chip works in Rx mode\n",__func__);
        MT5728_read_buffer(mte, REG_INTFLAG, val.ptr, 2);
        fclr.value = val.value;
        pr_info("[%s] REG_INTFLAG value:0x%04x\n", __func__, val.value);
        if(val.value == 0){
            pr_info("[%s] There's no interruption here\n", __func__);
            return;
        }
        print_curfunc_info();
        if (val.value & INT_POWER_ON) {
			pr_info("[%s] Interrupt signal: PowerON 01\n", __func__);
			mte->wireless_max_power = 0;
			mte->charge_protocol = PROTOCOL_UNKNOWN;
			mte->input_current = 500000;
	        mte->charge_current = 500000;
            rx_vout_max = 5000;
            rx_iout_max = 1000;
            setup_iout_start_cnt = 0;
            input_current_limit = 100;
            pr_info("[%s] Interrupt signal: PowerON 02\n", __func__);
            //mike 2023.1.4
            mt5728_set_pmic_input_current_ichg(100,100);
            // schedule_delayed_work(&mte->charger_work, msecs_to_jiffies(100));
        }
        if (val.value & INT_LDO_ON) {
            pr_info("[%s] Interrupt signal:LDO ON\n", __func__);
        }
        if (val.value & INT_RX_READY) {
            pr_info("[%s] Interrupt signal:MT5728 is Ready\n", __func__);
        }
        if (val.value & INT_LDO_OFF) {
            pr_info("[%s] Interrupt signal:MT5728 LDO_OFF\n", __func__);
			if(val.value == 0x80){
                pr_info("[%s] There's no interruption here\n", __func__);
                return;
            }
        }
        if (val.value & INT_FSK_RECV) {
            pr_info("[%s] Interrupt signal:FSK received successfully\n", __func__);
            mte->fsk_status = FSK_SUCCESS;
            Mt5728_get_fsk_buf_0_1();
            //read REG_BC
        }
        if (val.value & INT_FSK_SUCCESS) {
            pr_info("[%s] Interrupt signal:FSK received successfully\n", __func__);
            mte->fsk_status = FSK_SUCCESS;
            //read REG_BC
        }
        if (val.value & INT_FSK_TIMEOUT) {
            pr_info("[%s] Interrupt signal:Failed to receive FSK\n", __func__);
            mte->fsk_status = FSK_FAILED;
            //read REG_BC
        }
       if(val.value & INT_BPP){
           pr_info("[%s] Interrupt signal:Tx BPP\n", __func__);
           pr_info("[%s] Load power is recommended to be less than 5W\n", __func__);
			mte->wireless_max_power = 5;
			mte->charge_protocol = BPP;
			mte->input_current = 500000;
	       mte->charge_current = 500000;
			Set_staystate_current();
			schedule_delayed_work(&mte->add_current_work,400);
       }
        if(val.value & INT_EPP){
            vuc epp;
            pr_info("[%s] Interrupt signal:Tx EPP\n", __func__);
            MT5728_read_buffer(mte,REG_MAX_POWER,epp.ptr,1);
            epp.ptr[0] = epp.ptr[0]/2;
            pr_info("[%s] Load power is recommended to be less than %dW\n", __func__,epp.ptr[0]);
			mte->wireless_max_power = epp.ptr[0];
			if (mte->wireless_max_power > mte->rx_power_cap){
				dev_info(mte->dev,"%s: limit rx power to %d from %d\n", __func__, mte->rx_power_cap,mte->wireless_max_power);
				mte->wireless_max_power = mte->rx_power_cap;
			}
			mte->charge_protocol = EPP;
			mte->input_current = 500000;
	        mte->charge_current = 500000;
            mt5728_epp_ctrl_vout_flag = 1;
            if (mte->wireless_max_power < 15) {
                rx_vout_max = 9000;
                rx_iout_max = 1500;
            } else {
                rx_vout_max = 9000;
                rx_iout_max = 1500;
            }
			//Set_staystate_current();
		    /*set vout to 9000mv*/
		     set_rx_vout(9000);
			 msleep(30);
//prize add by lipenpeng 20210419 end
        }
        if (val.value & INT_AFC_SUPPORT) {
            pr_info("[%s] Interrupt signal:Tx support samsung_afc\n", __func__);
            pr_info("[%s] Load power recommended to 9W\n", __func__);
			mte->wireless_max_power = 10;
			mte->charge_protocol = AFC;
			mte->input_current = 500000;
	        mte->charge_current = 500000;
			//Set_staystate_current();
//prize add by lipengpeng 20210419 start     Samsung agreement  9V
		    fast_vfc(9000);
//prize add by lipengpeng 20210419 end
        }
		if(val.value & INT_ADDCURRENT){
			 pr_info("[%s] Add current\n", __func__);
			 MT5728_add_current();
		}
		pr_info("[%s] Get power wireless charging chip %dW\n", __func__,mte->wireless_max_power);
    }
    if(temp.ptr[1] & TXMODE){
        pr_info("[%s] The chip works in Tx mode\n",__func__);

        MT5728_read_buffer(mte, REG_INTFLAG, val.ptr, 2);
        fclr.value = val.value;
        pr_info("[%s] REG_INTFLAG value:0x%04x\n", __func__, val.value);
//prize add by lipengpeng 20210408 if usb charger close otg start
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
		if((val.value & INT_CHIP_DISABLE)&&(revere_mode==1)){
           printk(KERN_INFO"lpp----disable otg charge111\n");
		   	mt_vbus_revere_off();
		   printk(KERN_INFO"lpp----disable otg charge222\n");
            turn_on_otg_charge_mode(0);//GPIO109--->low
			turn_off_rever_5725(0);//OD5-->low
			set_otg_gpio(0);//OD7--->low
			mte->tx_count=0;
			mt_vbus_reverse_off_limited_current();// prize add by lipengpeng 20210322 close otg voltage
            atomic_set(&mte->is_tx_mode,0);
			revere_mode=0;
        }
#endif
//prize add by lipengpeng 20210408 if usb charger close otg end
        if(val.value == 0){
            pr_info("[%s] There's no interruption here\n", __func__);
            return;
        }
        if(val.value & INT_DETECT_RX){
            pr_info("[%s] Found RX close\n", __func__);
        }
        if(val.value & INT_OCPFRX){
//prize add by lipengpeng 20210507 start
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
           atomic_set(&mte->is_tx_mode,3);	//Reverse charging device abnormal, stop reverse charging
#endif
//prize add by lipengpeng 20210507 end
            pr_info("[%s] TX OCP protection\n", __func__);
        }

     //   if(val.value & INT_FODDE){
     //       vuc fod;
     //       pr_info("[%s] TX FOD\n", __func__);
     //       MT5728_read_buffer(mte,REG_F_TXPOWER,fod.ptr,2);
     //       pr_info("[%s] TX_power:%d mW\n", __func__,fod.value);
     //       MT5728_read_buffer(mte,REG_F_RXPOWER,fod.ptr,2);
     //       pr_info("[%s] RX_power:%d mW\n", __func__,fod.value);
      //      MT5728_read_buffer(mte,REG_F_ISTAY,fod.ptr,2);
      //      pr_info("[%s] Stay current:%d mA\n", __func__,fod.value);
     //       pr_info("[%s] Clear this flag and the system will ping again\n", __func__,fod.value);
     //       pr_info("[%s] In case of FOD, there may be metal in the middle, and TX function needs to be turned off\n", __func__,fod.value);
//prize add by lipengpeng 20210507 start
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
    //       atomic_set(&mte->is_tx_mode,3);	//Reverse charging device abnormal, stop reverse charging
#endif
//prize add by lipengpeng 20210507 end
    //    }
        if(val.value & INT_POWER_TRANS){
//prize add by lipengpeng 20210507 start
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
           atomic_set(&mte->is_tx_mode,2);	//The device is approaching and reverse charging is started
#endif
//prize add by lipengpeng 20210507 end
            pr_info("[%s] Charge RX normally\n", __func__);
        }
   //     if(val.value & INT_REMOVE_POEWR){
//prize add by lipengpeng 20210507 start
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
   //        atomic_set(&mte->is_tx_mode,3);	//Keep the device away and stop reverse charging
#endif
//prize add by lipengpeng 20210507 end
  //          pr_info("[%s] Off charge, RX may be removed, or for other reasons\n", __func__);
   //     }
   #if 0
        if(val.value & INT_CHARGE_STATUS){
            vuc eleq;
            pr_info("[%s] RX reported the current power\n", __func__);
            MT5728_read_buffer(mte,REG_RXCHARGESTATUS,eleq.ptr,2);
            if(eleq.value >= 100){
                pr_info("[%s] Off charge,Disconnect wireless charging,now!!!\n", __func__);
            }
        }
        if(val.value & INT_TXINIT){
            mte->tx_count++;
            if(mte->tx_count == 1){
                vuc txinit;
                MT5728_read_buffer(mte,REG_STABILITY,txinit.ptr,2);
                if(txinit.value == 0x5555){
                    txinit.value = 0x6666;
                    MT5728_write_buffer(mte,REG_STABILITY,txinit.ptr,2);
                    pr_info("[%s] TX starts working\n", __func__);
                }
            }
            if(mte->tx_count == 2){
                pr_err("[%s] The chip is reset. It may be put on the TX of another home when it is charged reversely\n",__func__);
                pr_err("[%s] Turn off TX function\n",__func__);
            }
        }
	#endif
    }
    scmd.value |= CLEAR_INT;
    //---clrintflag
    //MT5728_write_buffer(mte, REG_INTCLR, fclr.ptr, 2);
    MT5728_write_buffer(mte, REG_INTCLR, fclr.ptr, 2);
    pr_info("[%s] write REG_INTCLR : 0x%04x,\n", __func__, fclr.value);

    MT5728_write_buffer(mte, REG_CMD, scmd.ptr, 2);
    pr_info("[%s] write REG_CMD : 0x%04x,\n", __func__, scmd.value);
//prize add by lipengpeng 20210409 start  Modify the problem that the time of closing and recharging exceeds 400ms when USB is inserted
    //schedule_delayed_work(&mte->eint_work,100);  //Callback check if the interrupt is cleared
	schedule_delayed_work(&mte->eint_work,100);  //Callback check if the interrupt is cleared
//prize add by lipengpeng 20210409 end
}
EXPORT_SYMBOL(MT5728_irq_handle);

static void MT5728_eint_work(struct work_struct* work) {
     MT5728_irq_handle();
}
static void MT5728_add_current_work(struct work_struct* work) {
	 vuc tcur;
     vuc temp;
	// MT5728_read_buffer(mte, REG_CURFUNC, temp.ptr,1);
	 pr_err("[%s] Call back\n",__func__);
	 if(temp.ptr[0] == 0){
	 	MT5728_read_buffer(mte, REG_IOUT, tcur.ptr,2);
		pr_err("[%s] Rx:Iout:%d\n",__func__,tcur.value);
		pr_err("[%s] wireless max power :%d\n",__func__,mte->wireless_max_power);
		if(mte->wireless_max_power >=10){
			int ifit =  mte->wireless_max_power *1000 / 9 * 98 / 100;
			if(ifit < tcur.value) {
				En_Dis_add_current(0xFF);
				pr_err("[%s] En_Dis_add_current : DISABLE\n",__func__);
			}
		}
	 }else{
	 	pr_err("[%s]  It has been disabled\n",__func__);
	 }
	 return;
}


static irqreturn_t MT5728_irq(int irq,void * data){
    struct  MT5728_dev * mt5728 = data;
//prize add by lipengpeng 20210409 start  Modify the problem that the time of closing and recharging exceeds 400ms when USB is inserted
	schedule_delayed_work(&mt5728->eint_work,100);
//prize add by lipengpeng 20210409 end
    return IRQ_HANDLED;
}

static const struct of_device_id match_table[] = {
    {.compatible = "maxictech,mt5728-30w",},
    { },
};


int get_MT5728_status(void){
   vuc chipid;
   u8 state = 0;
   chipid.value = 0;
   
   if(!mt5728_init_done){
	   return -1;
   }
   
   if(MT5728_read_buffer(mte, MT5728_STATE_ADDR, &state,1) == 0){
	   pr_err("gezi:%s 0x%x %d\n",__func__,state,mte->charge_protocol);
	   if((state & 0x01) && (mte->charge_protocol != PROTOCOL_UNKNOWN)){
		   return 0;
	   }
	   else{
		   return -1;
	   }
   }
   else{
	   pr_err("gezi:err!!! %s 0x%x\n",__func__,state);
	   return -1;
   }
   
   
   
 /* 
   if((mte->charge_protocol == PROTOCOL_UNKNOWN) || (mte->wireless_max_power <= 0)){
		print_curfunc_info();
		pr_err("gezi:wireless_charge not exist,return...\n");
		return -1;
	}
*/	
	 if(MT5728_read_buffer(mte, REG_CHIPID, chipid.ptr,2) == 0){
	 if(chipid.value == MT5728ID){
		pr_err("%s: chipID : %02x%02x  chipid.value =0x%04x\n",__func__,chipid.ptr[0],chipid.ptr[1],chipid.value);
		return 0;
	 } else {
		pr_err("ID error :%d\n ", chipid.value);
		return -3;
	 }
   }
   else
   {
	   return -1;
   }
}
EXPORT_SYMBOL(get_MT5728_status);

enum wireless_charge_protocol check_wireless_charge_status (void){
	if(get_MT5728_status() != 0)
		return PROTOCOL_UNKNOWN;
	return mte->charge_protocol;

}
EXPORT_SYMBOL(check_wireless_charge_status);

int reset_mt5728_info(void)
{
	if(!mt5728_init_done){
		return 0;
	}
	mte->wireless_max_power =0;
	mte->charge_protocol = PROTOCOL_UNKNOWN;
	mte->input_current = 0;
	mte->charge_current = 0;
	pr_err("%s: \n",__func__);
	return 0;
}
EXPORT_SYMBOL(reset_mt5728_info);

static struct power_supply *bat_psy = NULL;

static unsigned int mt5728_get_input_charge_limit(unsigned int curr)
{
	union power_supply_propval prop;
	int ret = 0,temp = 0;
	unsigned int charging_current = 0;
	
	charging_current = curr;
	
	if (IS_ERR_OR_NULL(bat_psy)) {
		//pr_err("%s Couldn't get bms_psy\n", __func__);
		bat_psy = power_supply_get_by_name("battery");
		if (IS_ERR_OR_NULL(bat_psy)) {
			return charging_current;
		}
		else{
			goto to;
		}
	}
to:
	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_TEMP, &prop);
	temp = prop.intval / 10;
	
	pr_err("gezi:%s--in-charging_current:%d,temp:%d\n",__func__,charging_current,temp);
	
	if(temp < 0){
		if(charging_current > 500000){
			charging_current = 500000;
		}
	}
	else if(temp >= 0 && temp < 15){
		if(charging_current > 1000000){
			charging_current = 1000000;
		}
	}
	else if(temp >= 45){
		if(charging_current > 1500000){
			charging_current = 1500000;
		}
	}
	else{
		
	}
	pr_err("gezi:%s--out-charging_current:%d,temp:%d\n",__func__,charging_current,temp);
	
	return charging_current;
}

int get_wireless_charge_current(struct charger_data *pdata){
	
	if((mte->charge_protocol == PROTOCOL_UNKNOWN) || (mte->wireless_max_power <= 0)){
		print_curfunc_info();
		pr_err("gezi:wireless_charge not exist,return...\n");
		return 0;
	}
	else{
		pr_err("gezi:wireless_charge is exist,protocol:%d,max_powe:%d\n",mte->charge_protocol,mte->wireless_max_power);
	}
	
	if(mte->input_current > 0 && mte->charge_current > 0){
		pdata->input_current_limit = mt5728_get_input_charge_limit(mte->input_current);
		pdata->charging_current_limit = mte->charge_current;
	}
	else{
		pr_info("[%s] input_current =  %d,charge_current = %d\n", __func__,pdata->input_current_limit,pdata->charging_current_limit);
	}
	pr_info("[%s] input_current =  %d,charge_current = %d\n", __func__,mte->input_current,mte->charge_current);
	print_curfunc_info();
    return 0;
}
EXPORT_SYMBOL(get_wireless_charge_current);

static int MT5728_parse_dt(struct i2c_client *client, struct MT5728_dev *mt5728)
{
    int ret =0;
	mt5728->statu_gpio = of_get_named_gpio(client->dev.of_node, "statu_gpio", 0);
	if (mt5728->statu_gpio < 0) {
		pr_err("%s: no dc gpio provided\n", __func__);
		return -1;
	} else {
		pr_info("%s: dc gpio provided ok. mt5715->statu_gpio = %d\n", __func__, mt5728->statu_gpio);
		devm_gpio_request_one(&client->dev, mt5728->statu_gpio,GPIOF_DIR_IN, "mt5728_statu");
	}

	mt5728->irq_gpio = of_get_named_gpio(client->dev.of_node, "irq-gpio", 0);
	if (mt5728->irq_gpio < 0) {
		pr_err("%s: no irq gpio provided.\n", __func__);
		return -1;
	} else {
		pr_info("%s: irq gpio provided ok. mt5715->irq_gpio = %d\n", __func__, mt5728->irq_gpio);
	}

	ret = of_property_read_u32(client->dev.of_node,"rx_power_capability",&mt5728->rx_power_cap);
	if (ret < 0){
		mt5728->rx_power_cap = 15;
		//return ret;
	}
	dev_info(&client->dev,"%s: Set rx_power_capability:%d\n",__func__,mt5728->rx_power_cap);

	ret = of_property_read_u32(client->dev.of_node,"rx_efficiency",&mt5728->rx_efficiency);
	if (ret < 0){
		mt5728->rx_efficiency = 98;
		//return ret;
	}
	dev_info(&client->dev,"%s: Set rx_efficiency:%d\n",__func__,mt5728->rx_efficiency);

	ret = of_property_read_u32(client->dev.of_node,"one_pin_ctl",&mt5728->one_pin_ctl);
	if (ret >= 0){
		if (mt5728->one_pin_ctl){
			mt5728->otgen_gpio = of_get_named_gpio(client->dev.of_node, "otgen_gpio", 0);
			if (gpio_is_valid(mt5728->otgen_gpio)) {
				dev_info(&client->dev,"%s: otgen_gpio:%d\n", __func__, mt5728->otgen_gpio);
				ret = devm_gpio_request(&client->dev, mt5728->otgen_gpio, "mt5728_otgen");
				if (ret < 0) {
					dev_err(&client->dev, "%s: otgen_gpio request fail(%d)\n",__func__, ret);
					return ret;
				}
			} else {
				dev_err(&client->dev,"get otgen_gpio fail %d\n", mt5728->otgen_gpio);
				return -EINVAL;
			}
			return 0;
		}
	}else{
		mt5728->one_pin_ctl = 0;
	}

	mt5728->chipen_gpio = of_get_named_gpio(client->dev.of_node, "chipen_gpio", 0);
	if (gpio_is_valid(mt5728->chipen_gpio)) {
		dev_info(&client->dev,"%s: chipen_gpio:%d\n", __func__, mt5728->chipen_gpio);
		ret = devm_gpio_request(&client->dev, mt5728->chipen_gpio, "sgm2541_en");
		if (ret < 0) {
			dev_err(&client->dev, "%s: otgen_gpio request fail(%d)\n",__func__, ret);
		}
	} else {
		dev_err(&client->dev,"get otgen_gpio fail %d\n", mt5728->otgen_gpio);
	}
	
	mt5728->test_r_gpio = of_get_named_gpio(client->dev.of_node, "test_r_gpio", 0);
	if (gpio_is_valid(mt5728->test_r_gpio)) {
		dev_info(&client->dev,"%s: test_r_gpio:%d\n", __func__, mt5728->test_r_gpio);
		ret = devm_gpio_request(&client->dev, mt5728->test_r_gpio, "sgm2541_test_r");
		if (ret < 0) {
			dev_err(&client->dev, "%s: test_r_gpio request fail(%d)\n",__func__, ret);
		}
	} else {
		dev_err(&client->dev,"get test_r_gpio fail %d\n", mt5728->test_r_gpio);
	}
	
	mt5728->ldoctrl_gpio = of_get_named_gpio(client->dev.of_node, "ldoctrl_gpio", 0);
	if (gpio_is_valid(mt5728->ldoctrl_gpio)) {
		dev_info(&client->dev,"%s: ldoctrl_gpio:%d\n", __func__, mt5728->ldoctrl_gpio);
		ret = devm_gpio_request(&client->dev, mt5728->ldoctrl_gpio, "sgm2541_ldo");
		if (ret < 0) {
			dev_err(&client->dev, "%s: ldoctrl_gpio request fail(%d)\n",__func__, ret);
		}
	} else {
		dev_err(&client->dev,"get ldoctrl_gpio fail %d\n", mt5728->ldoctrl_gpio);
	}

	mt5728->otg_5728_ctl.pinctrl_gpios = devm_pinctrl_get(&client->dev);
   if (IS_ERR(mt5728->otg_5728_ctl.pinctrl_gpios)) {
		ret = PTR_ERR(mt5728->otg_5728_ctl.pinctrl_gpios);
		pr_err("%s can't find chg_data pinctrl\n", __func__);
		return ret;
   }

	mt5728->otg_5728_ctl.pins_default = pinctrl_lookup_state(mt5728->otg_5728_ctl.pinctrl_gpios, "default");
	if (IS_ERR(mt5728->otg_5728_ctl.pins_default)) {
		ret = PTR_ERR(mt5728->otg_5728_ctl.pins_default);
		pr_err("%s can't find chg_data pinctrl default\n", __func__);
		/* return ret; */
	}

	mt5728->otg_5728_ctl.charger_otg_off = pinctrl_lookup_state(mt5728->otg_5728_ctl.pinctrl_gpios, "charger_otg_off");
	if (IS_ERR(mt5728->otg_5728_ctl.charger_otg_off)) {
		ret = PTR_ERR(mt5728->otg_5728_ctl.charger_otg_off);
		pr_err("%s  can't find chg_data pinctrl otg high\n", __func__);
		return ret;
	}

	mt5728->otg_5728_ctl.charger_otg_on = pinctrl_lookup_state(mt5728->otg_5728_ctl.pinctrl_gpios, "charger_otg_on");
	if (IS_ERR(mt5728->otg_5728_ctl.charger_otg_on)) {
		ret = PTR_ERR(mt5728->otg_5728_ctl.charger_otg_on);
		pr_err("%s  can't find chg_data pinctrl otg low\n", __func__);
		return ret;
	}

	mt5728->otg_5728_ctl.wireless_5728_off = pinctrl_lookup_state(mt5728->otg_5728_ctl.pinctrl_gpios, "wireless_5728_off");
	if (IS_ERR(mt5728->otg_5728_ctl.wireless_5728_off)) {
		ret = PTR_ERR(mt5728->otg_5728_ctl.wireless_5728_off);
		pr_err("%s  can't find chg_data pinctrl wireless_5728_off\n", __func__);
		return ret;
	}

	mt5728->otg_5728_ctl.wireless_5728_on = pinctrl_lookup_state(mt5728->otg_5728_ctl.pinctrl_gpios, "wireless_5728_on");
	if (IS_ERR(mt5728->otg_5728_ctl.wireless_5728_on)) {
		ret = PTR_ERR(mt5728->otg_5728_ctl.wireless_5728_on);
		pr_err("%s  can't find chg_data pinctrl wireless_5728_on\n", __func__);
		return ret;
	}

//prize add by lipengpeng 20210308 start
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
	mt5728->otg_5728_ctl.charger_otg_mode_on = pinctrl_lookup_state(mt5728->otg_5728_ctl.pinctrl_gpios, "charger_otg_mode_on");
	if (IS_ERR(mt5728->otg_5728_ctl.charger_otg_mode_on)) {
		ret = PTR_ERR(mt5728->otg_5728_ctl.charger_otg_mode_on);
		pr_err("%s  can't find chg_data pinctrl charger_otg_mode_on\n", __func__);
		return ret;
	}

	mt5728->otg_5728_ctl.charger_otg_mode_off = pinctrl_lookup_state(mt5728->otg_5728_ctl.pinctrl_gpios, "charger_otg_mode_off");
	if (IS_ERR(mt5728->otg_5728_ctl.charger_otg_mode_off)) {
		ret = PTR_ERR(mt5728->otg_5728_ctl.charger_otg_mode_off);
		pr_err("%s  can't find chg_data pinctrl charger_otg_mode_off\n", __func__);
		return ret;
	}
//prize add by lipengpeng 20210308 end

//prize add by lipengpeng 20210416 start BPI_BUS4  GPIO90
	mt5728->otg_5728_ctl.test_gpio_on = pinctrl_lookup_state(mt5728->otg_5728_ctl.pinctrl_gpios, "test_gpio");
	if (IS_ERR(mt5728->otg_5728_ctl.test_gpio_on)) {
		ret = PTR_ERR(mt5728->otg_5728_ctl.test_gpio_on);
		pr_err("%s  can't find chg_data pinctrl test_gpio_on\n", __func__);
		//return ret;
	}

	mt5728->otg_5728_ctl.test_gpio_off = pinctrl_lookup_state(mt5728->otg_5728_ctl.pinctrl_gpios, "test_off");
	if (IS_ERR(mt5728->otg_5728_ctl.test_gpio_off)) {
		ret = PTR_ERR(mt5728->otg_5728_ctl.test_gpio_off);
		pr_err("%s  can't find chg_data pinctrl test_gpio_off\n", __func__);
		//return ret;
	}
#endif
//prize add by lipengpeng 20210416 start BPI_BUS4  GPIO90
	mt5728->otg_5728_ctl.gpio_otg_prepare = true;
	return 0;
}

int turn_off_5728(int en){
	int ret =0;

	if (mte->one_pin_ctl){
		if (gpio_is_valid(mte->otgen_gpio)){
			if (en){
				gpio_direction_output(mte->otgen_gpio,1);
			}else{
				gpio_direction_output(mte->otgen_gpio,0);
			}
		}
		return ret;
	}

    if (mte->otg_5728_ctl.gpio_otg_prepare) {
		if (en) {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.wireless_5728_off); //high
			printk("%s: set W_OTG_EN2 to hight %d\n", __func__,gpio_get_value(mte->test_r_gpio));
			ret =0;
		}
		else {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.wireless_5728_on);//low
			printk("%s: set W_OTG_EN2 to low %d\n", __func__,gpio_get_value(mte->test_r_gpio));
			ret =0;
		}
	}
	else {
		printk("%s:, error, gpio otg not prepared\n", __func__);
		ret =-1;
	}
	return ret;
}
EXPORT_SYMBOL(turn_off_5728);

#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)

/**退出TX模式 OD7拉低，OD5拉低  进入tx模式：OD7拉高，OD5拉低*/
int turn_on_otg_charge_mode(int en){
	int ret =0;

    if (mte->otg_5728_ctl.charger_otg_mode_on) {
		if (en) {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.charger_otg_mode_on);//
			printk("%s: set W_OTG_EN2 to hight\n", __func__);
			ret =0;
		}
		else {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.charger_otg_mode_off);//
			printk("%s: set W_OTG_EN2 to low\n", __func__);
			ret =0;
		}
	}
	else {
		printk("%s:, error, gpio otg not prepared\n", __func__);
		ret =-1;
	}
	return ret;
}
EXPORT_SYMBOL(turn_on_otg_charge_mode);

int turn_on_rever_5725(int en){
	int ret =0;
	if (mte->one_pin_ctl){
		if (gpio_is_valid(mte->otgen_gpio)){
			if (en){
				gpio_direction_output(mte->otgen_gpio,1); //GPOD7  high
			}else{
				gpio_direction_output(mte->otgen_gpio,0);//GPOD7  low
			}
		}
		return ret;
	}

    if (mte->otg_5728_ctl.gpio_otg_prepare) {
		if (en) {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.wireless_5728_on);//GPOD5  low
			printk("%s: set W_OTG_EN2 to hight\n", __func__,);
			ret =0;
		}
		else {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.wireless_5728_off);//GPOD5  high
			printk("%s: set W_OTG_EN2 to low\n", __func__);
			ret =0;
		}
	}
	else {
		printk("%s:, error, gpio otg not prepared\n", __func__);
		ret =-1;
	}
	return ret;
}
EXPORT_SYMBOL(turn_on_rever_5725);

/**退出TX模式 OD7拉低，OD5拉低  进入tx模式：OD7拉高，OD5拉低*/
int turn_off_rever_5725(int en){
	int ret =0;
	if (mte->one_pin_ctl){
		if (gpio_is_valid(mte->otgen_gpio)){
			if (en){
				gpio_direction_output(mte->otgen_gpio,1); //GPOD7  high
			}else{
				gpio_direction_output(mte->otgen_gpio,0);//GPOD7  low
			}
		}
		return ret;
	}

    if (mte->otg_5728_ctl.gpio_otg_prepare) {
		if (en) {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.wireless_5728_off);//GPOD5  high
			printk("%s: set W_OTG_EN2 to hight %d\n", __func__,gpio_get_value(mte->statu_gpio));
			ret =0;
		}
		else {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.wireless_5728_on);//GPOD5  low
			printk("%s: set W_OTG_EN2 to low %d\n", __func__,gpio_get_value(mte->statu_gpio));
			ret =0;
		}
	}
	else {
		printk("%s:, error, gpio otg not prepared\n", __func__);
		ret =-1;
	}
	return ret;
}
EXPORT_SYMBOL(turn_off_rever_5725);
#endif

int set_otg_gpio(int en){
	int ret =0;
	if (mte->one_pin_ctl){
		if (gpio_is_valid(mte->otgen_gpio)){
			if (en){
				gpio_direction_output(mte->otgen_gpio,1);
			}else{
				gpio_direction_output(mte->otgen_gpio,0);
			}
		}
		return ret;
	}

    if (mte->otg_5728_ctl.gpio_otg_prepare) {
		if (en) {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.charger_otg_on);
			printk("%s: set w_otg_en PIN to high %d \n", __func__,gpio_get_value(mte->otgen_gpio));
			ret =0;
		}
		else {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.charger_otg_off);
			printk("%s: set w_otg_en PIN to low %d \n", __func__,gpio_get_value(mte->otgen_gpio));
			ret =0;
		}
	}
	else {
		printk("%s:, error, gpio otg not prepared\n", __func__);
		ret =-1;
	}
	return ret;
}
EXPORT_SYMBOL(set_otg_gpio);


int set_test_r_gpio(int en)
{
	int ret =0;
    if (mte->otg_5728_ctl.gpio_otg_prepare) {
		if (en) {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.wireless_5728_off);//GPOD5  high
			printk("%s: set set_test_r_gpio to hight\n", __func__);
			ret = 0;
		}
		else {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.wireless_5728_on);//GPOD5  low
			printk("%s: set set_test_r_gpio to low\n", __func__);
			ret = 0;
		}
	}
	return ret;
}
EXPORT_SYMBOL(set_test_r_gpio);
//prize add by lipengpeng 20210408 start
//prize add by lipengpeng 20210416 start BPI_BUS4  GPIO90
//#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
int test_gpio(int en){
	int ret =0;
    if (mte->otg_5728_ctl.gpio_otg_prepare) {
		if (en) {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.test_gpio_on);
			printk("%s: set test gpio  to hight\n", __func__);
			ret =0;
		}
		else {
			pinctrl_select_state(mte->otg_5728_ctl.pinctrl_gpios, mte->otg_5728_ctl.test_gpio_off);
			printk("%s: set test gpio  to low\n", __func__);
			ret =0;
		}
	}
	return ret;
}
EXPORT_SYMBOL(test_gpio);
//#endif


static int MT5728_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    struct MT5728_dev *chip;
    int irq_flags = 0;
    int rc = 0;
	vuc protocol;

    pr_err("MT5728 probe.\n");
    chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
    if (!chip){
        return -ENOMEM;
	}
	chip->client = client;
	chip->dev = &client->dev;

    pr_err("MT5728 chip.\n");
//prize add by lipengpeng 20220716 start 
    chip->regmap = devm_regmap_init_i2c(client, &MT5728_regmap_config);
//prize add by lipengpeng 20220716 start 	
    if (!chip->regmap) {
        pr_err("parent regmap is missing\n");
        return -EINVAL;
    }
    pr_err("MT5728 regmap.\n");
    chip->bus.read = MT5728_read;
    chip->bus.write = MT5728_write;
    chip->bus.read_buf = MT5728_read_buffer;
    chip->bus.write_buf = MT5728_write_buffer;

    device_init_wakeup(chip->dev, true);

    sysfs_create_group(&client->dev.kobj, &mt5728_sysfs_group);

    pr_err("MT5728 probed successfully\n");

    mte = chip;

	mte->wireless_max_power = 0;
	mte->otg_5728_ctl.gpio_otg_prepare = false;
	mte->charge_protocol = PROTOCOL_UNKNOWN;
	mte->input_current = 0;
	mte->charge_current = 0;

    INIT_DELAYED_WORK(&chip->eint_work, MT5728_eint_work);
	INIT_DELAYED_WORK(&chip->add_current_work, MT5728_add_current_work);
    INIT_DELAYED_WORK(&chip->charger_work, mt5728_charger_work_func);
	INIT_DELAYED_WORK(&chip->reverse_charge_work, mt5728_reverse_work_func);
	INIT_DELAYED_WORK(&chip->fwcheck_work, mt5728_fwcheck_work_func);
    rc = MT5728_parse_dt(client, chip);
    if (rc ) {
    	pr_err("%s: failed to parse device tree node\n", __func__);
        chip->statu_gpio = -1;
        chip->irq_gpio = -1;
    }

    protocol.value =0;
    //if(gpio_get_value(mte->statu_gpio)){
		//MT5728_read_buffer(mte, REG_CURFUNC, protocol.ptr,1);
		pr_err("%s: protocol read 1 : %02x%02x  protocol.value =0x%04x  protocol.value =%d\n",__func__,protocol.ptr[0],protocol.ptr[1],protocol.value,protocol.value);
    //}

    if (gpio_is_valid(chip->irq_gpio)) {
        rc = devm_gpio_request_one(&client->dev, chip->irq_gpio,
			GPIOF_DIR_IN, "mt5728_int");
        if (rc) {
			pr_err("%s: irq_gpio request failed\n", __func__);
			goto err;
        }

        irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
        rc = devm_request_threaded_irq(&client->dev, gpio_to_irq(chip->irq_gpio),
						NULL, MT5728_irq, irq_flags, "mt5728", chip);
        if (rc != 0) {
			pr_err("failed to request IRQ %d: %d\n", gpio_to_irq(chip->irq_gpio), rc);
			goto err;
        }
		pr_err("sucess to request IRQ %d: %d\n", gpio_to_irq(chip->irq_gpio), rc);

		//start add by sunshuai
		//if(!(gpio_get_value(mte->irq_gpio))){
			pr_err("%s The interruption has come \n", __func__);
			MT5728_irq_handle();
		//}
		//end   add by sunshuai
    } else {
		pr_info("%s skipping IRQ registration\n", __func__);
    }
 //prize add by lpp 20210308 start Get whether the device is close when the mobile phone is in a backcharging state
//#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
	rc = sysfs_create_link(kernel_kobj,&client->dev.kobj,"wirelessrx");
	if (rc){
		pr_err(KERN_ERR"mt5728 sysfs_create_link fail\n");
	}

//	rc = device_create_file(&client->dev, &dev_attr_enabletx);
//	if (rc){
//		pr_err(KERN_ERR"mt5728 failed device_create_file(dev_attr_enabletx)\n");
//	}
//prize add by lipengpeng 20220623 start 
	rc = device_create_file(&client->dev, &dev_attr_wireless_connect);
	if (rc){
		pr_err(KERN_ERR"mt5728 failed device_create_file(dev_attr_wireless_connect)\n");
	}
//prize add by lipengpeng 20220623 end 
//	rc = device_create_file(&client->dev, &dev_attr_gettxflag);
//	if (rc){
//		pr_err(KERN_ERR"mt5728 failed device_create_file(dev_attr_gettxflag)\n");
//	}
//#endif
//prize add by lpp 20210308 end Get whether the device is close when the mobile phone is in a backcharging state

//prize add by lipengpeng 20210408 start  chipen_gpio  status
	if (gpio_is_valid(mte->chipen_gpio)) {
			gpio_direction_output(mte->chipen_gpio, 0); //sgm2541 auto mode
	}
//prize add by lipengpeng 20210408 start  chipen_gpio  status
	i2c_set_clientdata(client,chip);
	
	mt5728_init_done = 1;
	
	pr_info("%s probe done\n", __func__);

	return rc;

err:
	devm_kfree(&client->dev, chip);
	return rc;
}

static int MT5728_remove(struct i2c_client *client) {
    sysfs_remove_group(&client->dev.kobj, &mt5728_sysfs_group);
 //prize add by lpp 20190821 start

//#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
	sysfs_remove_link(kernel_kobj,"wirelessrx");
  //prize add by lpp 20190821 start
	device_remove_file(&client->dev, &dev_attr_wireless_connect);
 //prize add by lpp 20190821 end
//	device_remove_file(&client->dev, &dev_attr_enabletx);
	//device_remove_file(&client->dev, &dev_attr_disabletx);
//	device_remove_file(&client->dev, &dev_attr_gettxflag);
//#endif
 //prize add by lpp 20190821 end
    if (gpio_is_valid(mte->irq_gpio)){
        devm_gpio_free(&client->dev, mte->irq_gpio);
	}

	if (gpio_is_valid(mte->statu_gpio)){
        devm_gpio_free(&client->dev, mte->statu_gpio);
	}

	if (mte->one_pin_ctl){
		if (gpio_is_valid(mte->otgen_gpio)){
			devm_gpio_free(&client->dev,mte->otgen_gpio);
		}
	}
    return 0;
}

static const struct i2c_device_id MT5728_dev_id[] = {
    {"MT5728_receiver", 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, MT5728_dev_id);

static struct i2c_driver MT5728_driver = {
    .driver   = {
        .name           = DEVICE_NAME,
        .owner          = THIS_MODULE,
        .of_match_table = match_table,
    },
    .probe    = MT5728_probe,
    .remove   = MT5728_remove,
    .id_table = MT5728_dev_id,
};

//prize add by lipengpeng 20220609 start 
static int __init mt5728_wireless_init(void)
{
    int ret = 0;

    printk("mt5728 wireless driver init start");
    ret = i2c_add_driver(&MT5728_driver);
    if ( ret != 0 ) {
        printk("mt5728 wireless driver init failed!");
    }
    printk("mt5728 wireless driver init end");
    return ret;
}

static void __exit mt5728_wireless_exit(void)
{
    i2c_del_driver(&MT5728_driver);
}

module_init(mt5728_wireless_init);
module_exit(mt5728_wireless_exit);


MODULE_AUTHOR("sunshuai@szprize.com");
MODULE_DESCRIPTION("MT5728 Wireless Power Receiver");
MODULE_LICENSE("GPL v2");
