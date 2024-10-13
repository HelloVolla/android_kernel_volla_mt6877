/*
 * Sitronix Touchscreen Controller Driver
 *
 * Copyright (C) 2018 Sitronix Technology Co., Ltd.
 *	CT Chen <ct_chen@sitronix.com.tw>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
*/

#include "sitronix_ts.h"
/* #include "sitronix_ts_spi.h" */

#include <linux/spi/spi.h>

#define TS_SPI_READ			0x80
#define TS_SPI_WRITE			0x00
#define ST_SPI_FW_ADDRESS_LEN		2
#define ST_SPI_FW_DUMMY_LEN			1
#define ST_SPI_FW_MAX_TRANS_LEN_READ		0x100
#define ST_SPI_FW_MAX_TRANS_LEN_WRITE		32
#define ST_SPI_RAM_MODE_MAX_TRANS_LEN	0x1000
#define SPI_MAX_FREQ			8000000

#define ST_SPI_DELAY_AFTER_FIRST_WORD
#define ST_SPI_DELAY_US			6

/* struct spi_transfer xfers[ST_SPI_FW_MAX_TRANS_LEN + ST_SPI_FW_ADDRESS_LEN]; */
struct spi_transfer *xfers;
//uint8_t *spi_buf;
uint8_t spi_buf[ST_SPI_FW_ADDRESS_LEN + ST_SPI_FW_DUMMY_LEN + ST_SPI_FW_MAX_TRANS_LEN_READ]; 

#define SPI_RETRY_COUNT 1


static int sitronix_ts_spi_read(uint16_t addr, uint8_t *data, uint16_t len, void *if_data)
{
	int ret;
	uint8_t retry;
	unsigned int count = len + ST_SPI_FW_ADDRESS_LEN + ST_SPI_FW_DUMMY_LEN;
	uint8_t status;
	struct spi_message msg;
	struct sitronix_ts_spi_data *spi_data = (struct sitronix_ts_spi_data *)if_data;
	struct spi_device *spi = spi_data->spi_dev;

	spi_message_init(&msg);

	spi_buf[0] = (addr >> 8) | TS_SPI_READ;
	spi_buf[1] = addr & 0xFF;

	memset(spi_buf + 2, 0, ST_SPI_FW_DUMMY_LEN + ST_SPI_FW_MAX_TRANS_LEN_READ);

#ifndef ST_SPI_DELAY_AFTER_FIRST_WORD
	xfers[0].len = count;
	xfers[0].delay_usecs = 0;
	xfers[0].tx_buf = spi_buf;
	xfers[0].rx_buf = spi_buf;
	spi_message_add_tail(&xfers[0], &msg);
#else
	xfers[0].len = 2;
	xfers[0].delay_usecs = ST_SPI_DELAY_US;
	xfers[0].tx_buf = spi_buf;
	xfers[0].rx_buf = spi_buf;

	xfers[1].len = count - 2;
	xfers[1].delay_usecs = 0;
	xfers[1].tx_buf = spi_buf + 2;
	xfers[1].rx_buf = spi_buf + 2;
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);
#endif /*ST_SPI_DELAY_AFTER_FIRST_WORD*/
	

	for (retry = 1; retry <= SPI_RETRY_COUNT; retry++) {
		ret = spi_sync(spi, &msg);
		status = spi_buf[1];
		//if (ret == 0 && status != 0xFF) {
		if (ret == 0 && status != 0xEE) {
			ret = len;
			break;
		}

		sterr("%s: SPI retry %d\n", __func__, retry);
		msleep(1);
	}

	if (retry > SPI_RETRY_COUNT) {
		sterr("%s: Failed to complete SPI transfer, error = %d , IC status = %d\n", __func__, ret, status);
		ret = -EIO;
	}
	memcpy(data, spi_buf + ST_SPI_FW_ADDRESS_LEN + ST_SPI_FW_DUMMY_LEN, len);
	return ret;
}

static int sitronix_ts_spi_read_len_check(uint16_t addr, uint8_t *data, uint16_t len, void *if_data)
{
	int nowlen = len;
	int nowoff = 0;
	int ret = 0;

	while (nowlen > 0) {
		if (nowlen > ST_SPI_FW_MAX_TRANS_LEN_READ) {
			ret += sitronix_ts_spi_read(addr+nowoff, data+nowoff, ST_SPI_FW_MAX_TRANS_LEN_READ, if_data);

			nowoff += ST_SPI_FW_MAX_TRANS_LEN_READ;
			nowlen -= ST_SPI_FW_MAX_TRANS_LEN_READ;
		} else {
			ret += sitronix_ts_spi_read(addr+nowoff, data+nowoff, nowlen, if_data);
			nowlen = 0;
		}
	}
	return ret;
}


static int sitronix_ts_spi_write(uint16_t addr, uint8_t *data, uint16_t len, void *if_data)
{
	int ret;
	uint8_t retry;
	unsigned int count = len + ST_SPI_FW_ADDRESS_LEN;
	uint8_t status = 0;
	struct spi_message msg;

	struct sitronix_ts_spi_data *spi_data = (struct sitronix_ts_spi_data *)if_data;
	struct spi_device *spi = spi_data->spi_dev;

	spi_message_init(&msg);

	spi_buf[0] = (addr >> 8) & ~TS_SPI_READ;
	spi_buf[1] = addr & 0xFF;
	memcpy(spi_buf + ST_SPI_FW_ADDRESS_LEN, data, len);

#ifndef ST_SPI_DELAY_AFTER_FIRST_WORD
	xfers[0].len = count;
	xfers[0].delay_usecs = 0;
	xfers[0].tx_buf = spi_buf;
	xfers[0].rx_buf = NULL;
	spi_message_add_tail(&xfers[0], &msg);
#else
	xfers[0].len = 2;
	xfers[0].delay_usecs = ST_SPI_DELAY_US;
	xfers[0].tx_buf = spi_buf;
	xfers[0].rx_buf = NULL;
	
	xfers[1].len = count - 2;
	xfers[1].delay_usecs = 0;
	xfers[1].tx_buf = spi_buf + 2;
	xfers[1].rx_buf = NULL;
	
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);

#endif /* ST_SPI_DELAY_AFTER_FIRST_WORD */

	for (retry = 1; retry <= SPI_RETRY_COUNT; retry++) {
		ret = spi_sync(spi, &msg);
		if (ret == 0) {
			ret = len;
			break;
		}

		sterr("%s: SPI retry %d\n", __func__, retry);
		msleep(1);
	}

	if (retry > SPI_RETRY_COUNT) {
		sterr("%s: Failed to complete SPI transfer, error = %d , IC status = %d\n", __func__, ret, status);
		ret = -EIO;
	}

	return ret;
}

static int sitronix_ts_spi_write_len_check(uint16_t addr, uint8_t *data, uint16_t len, void *if_data)
{
	int nowlen = len;
	int nowoff = 0;
	int ret = 0;

	while (nowlen > 0) {
		if (nowlen > ST_SPI_FW_MAX_TRANS_LEN_WRITE) {
			ret += sitronix_ts_spi_write(addr+nowoff, data+nowoff, ST_SPI_FW_MAX_TRANS_LEN_WRITE, if_data);

			nowoff += ST_SPI_FW_MAX_TRANS_LEN_WRITE;
			nowlen -= ST_SPI_FW_MAX_TRANS_LEN_WRITE;
		} else {
			ret += sitronix_ts_spi_write(addr+nowoff, data+nowoff, nowlen, if_data);
			nowlen = 0;
		}
	}
	return ret;
}
static int sitronix_ts_spi_dread(uint8_t *data, uint16_t length, void *if_data)
{
	int ret = 0;
	uint16_t addr;	
	if (gts->wr_len == 2) {
		addr = gts->cmdio_addr[0] << 8 | gts->cmdio_addr[1];
		ret = sitronix_ts_spi_read_len_check(addr, data, length, if_data);
	} else {
		ret = sitronix_ts_spi_read_len_check(gts->cmdio_addr[0], data, length, if_data);
	}
	return ret;
}

static int sitronix_ts_spi_dwrite(uint8_t *data, uint16_t length, void *if_data)
{
	int ret = 0;
	uint16_t addr;
	addr = data[0] << 8 | data[1];
	ret = sitronix_ts_spi_write_len_check(addr, data+2, length-2, if_data);
	if (ret < 0)
		return ret;
	else
		return ret + 2;
}

static int st_spi_sync_split(uint8_t *tx_buf, uint8_t *rx_buf, int len, void *if_data)
{
	int error;	
	struct spi_message msg;
	struct sitronix_ts_spi_data *spi_data = (struct sitronix_ts_spi_data *)if_data;
	struct spi_device *spi = spi_data->spi_dev;

	spi_message_init(&msg);

	xfers[0].tx_buf	   = tx_buf;
	xfers[0].rx_buf	   = rx_buf;
	xfers[0].len	   = 3;
	xfers[0].delay_usecs = 1;
	
	xfers[1].tx_buf	   = tx_buf + 3;
	xfers[1].rx_buf	   = rx_buf + 3;
	xfers[1].len	   = len - 3;
	xfers[1].delay_usecs = 0;

	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);
	
	error = spi_sync(spi, &msg);
	if (error) {
		sterr("SPI error (%d)\n", error);
		return error;
	}
	return len;
}

static int sitronix_ts_spi_sread(uint8_t *tx_buf, uint16_t tx_len, uint8_t *rx_buf, uint16_t rx_len, void *if_data)
{
	int ret;

	ret = st_spi_sync_split(tx_buf + 1, rx_buf, tx_len + rx_len, if_data);

	if (ret < 0)
		return ret;
	else
		return tx_len;
}

static int st_spi_sync_normal(uint8_t *tx_buf, uint8_t *rx_buf, int len, void *if_data)
{
	int error;
	struct spi_message msg;
	struct sitronix_ts_spi_data *spi_data = (struct sitronix_ts_spi_data *)if_data;
	struct spi_device *spi = spi_data->spi_dev;
	
	xfers[0].tx_buf	   = tx_buf;
	xfers[0].rx_buf	   = rx_buf;
	xfers[0].len	   = len;
	xfers[0].delay_usecs = 0;
	
	spi_message_init(&msg);
	spi_message_add_tail(&xfers[0], &msg);

	error = spi_sync(spi, &msg);
	if (error) {
		sterr("SPI error (%d)\n", error);
		return error;
	}
	return len;
}

static int sitronix_ts_spi_aread(uint8_t *tx_buf, uint16_t tx_len, uint8_t *rx_buf, uint16_t rx_len, void *if_data)
{
	int ret;

	ret = st_spi_sync_normal(tx_buf + 1, rx_buf, tx_len + rx_len, if_data);

	if (ret < 0)
		return ret;
	else
		return tx_len;
}
static int sitronix_ts_spi_awrite(uint8_t *tx_buf, uint16_t tx_len, void *if_data)
{
	int ret;

	ret = st_spi_sync_normal(tx_buf + 1, NULL, tx_len, if_data);
	return ret;
}
static int sitronix_ts_spi_ram_mode_rw(bool isread, uint16_t cmd, uint16_t addr, uint8_t *txbuf, uint8_t *rxbuf, int len, void *if_data)
{
	uint8_t *wfbuf;
	uint8_t *rfbuf;
	int nowlen = len;
	uint16_t nowoff = 0;
	uint16_t ramoff = addr;
	int ret = 0;

	wfbuf = kmalloc(ST_SPI_RAM_MODE_MAX_TRANS_LEN+6, GFP_KERNEL);
	rfbuf = kmalloc(ST_SPI_RAM_MODE_MAX_TRANS_LEN+6, GFP_KERNEL);
	memset(wfbuf, 0, ST_SPI_RAM_MODE_MAX_TRANS_LEN+6);
	memset(rfbuf, 0, ST_SPI_RAM_MODE_MAX_TRANS_LEN+6);

	while (nowlen > 0) {
		if (nowlen > ST_SPI_RAM_MODE_MAX_TRANS_LEN) {

			wfbuf[0] = (cmd >> 8) & 0xFF;
			wfbuf[1] = cmd & 0xFF;

			ramoff = addr + (nowoff);
			wfbuf[2] = (ramoff >> 8) & 0xFF;
			wfbuf[3] = ramoff & 0xFF;

			if (isread) {
				ret += st_spi_sync_normal(wfbuf, rfbuf, ST_SPI_RAM_MODE_MAX_TRANS_LEN+6, if_data) - 6;
				memcpy(rxbuf+nowoff, rfbuf+6, ST_SPI_RAM_MODE_MAX_TRANS_LEN);
			} else {
				memcpy(wfbuf+4, txbuf+nowoff, ST_SPI_RAM_MODE_MAX_TRANS_LEN);
				ret += st_spi_sync_normal(wfbuf, rfbuf, ST_SPI_RAM_MODE_MAX_TRANS_LEN+4, if_data) - 4;
			}

			/* stmsg("ramoff %x , nowoff %x, len %x , data %x %x %x %x\n",ramoff,nowoff ,nowlen , wfbuf[3] , wfbuf[4] , wfbuf[5] ,wfbuf[6]); */

			nowoff += ST_SPI_RAM_MODE_MAX_TRANS_LEN;
			nowlen -= ST_SPI_RAM_MODE_MAX_TRANS_LEN;
		} else {

			wfbuf[0] = (cmd >> 8) & 0xFF;
			wfbuf[1] = cmd & 0xFF;

			ramoff = addr + (nowoff);
			wfbuf[2] = (ramoff >> 8) & 0xFF;
			wfbuf[3] = ramoff & 0xFF;

			/* stmsg("addr %x ,nowoff %x, len %x , data %x %x %x %x\n",ramoff,nowoff ,nowlen , wfbuf[3] , wfbuf[4] , wfbuf[5] ,wfbuf[6]); */
			if (isread) {
				/* stmsg("read b %02X %02X %02X %02X\n",rfbuf[6],rfbuf[7],rfbuf[8],rfbuf[9]); */
				ret += st_spi_sync_normal(wfbuf, rfbuf, nowlen+6, if_data) - 6;
				/* stmsg("read a %02X %02X %02X %02X\n",rfbuf[17],rfbuf[18],rfbuf[19],rfbuf[20]); */
				memcpy(rxbuf+nowoff, rfbuf+6, nowlen);
			} else {
				memcpy(wfbuf+4, txbuf+nowoff, nowlen+4);
				ret += st_spi_sync_normal(wfbuf, rfbuf, nowlen+4, if_data) - 4;
			}

			nowlen = 0;
		}

	}

	kfree(wfbuf);
	kfree(rfbuf);
	if (ret != len)
		return -EIO;
	else
		return 0;
}

static int sitronix_ts_spi_ram_mode_cmd(uint16_t cmd, void *if_data)
{
	int ret = 0;
	uint8_t *wfbuf;
	uint8_t *rfbuf;


	wfbuf = kmalloc(2, GFP_KERNEL);
	rfbuf = kmalloc(2, GFP_KERNEL);
	memset(wfbuf, 0, 2);
	memset(rfbuf, 0, 2);

	wfbuf[0] = (cmd >> 8) & 0xFF;
	wfbuf[1] = cmd & 0xFF;

	ret += st_spi_sync_normal(wfbuf, rfbuf, 2, if_data);

	kfree(wfbuf);
	kfree(rfbuf);

	msleep(1);

	if (ret != 2)
		return -EIO;
	else
		return 0;
}



static int sitronix_ts_spi_parse_dt(struct device *dev, struct sitronix_ts_host_interface *host_if)
{
	int rc;
	struct device_node *np = dev->of_node;
	struct sitronix_ts_spi_data *spi_data = (struct sitronix_ts_spi_data *)host_if->if_data;

	rc = of_property_read_u32(np, "spi-max-frequency", &spi_data->spi_max_freq);
	if (rc) {
		sterr("%s: Failed to read SPI max frequency!(%d) Set to default frequency %u.\n",
				__func__, rc, SPI_MAX_FREQ);
		spi_data->spi_max_freq = SPI_MAX_FREQ;
	} else {
		stmsg("%s: SPI max frequency = %d.\n", __func__, spi_data->spi_max_freq);
	}

	host_if->irq_gpio = of_get_named_gpio_flags(np, "irq-gpio", 0, &host_if->irq_gpio_flags);
	if (host_if->irq_gpio < 0)
		sterr("%s: Failed to read interrupt GPIO!(%d)\n", __func__, host_if->irq_gpio);
	else
		stmsg("%s: Interrupt GPIO = %d. (flag=%d)\n", __func__, host_if->irq_gpio, host_if->irq_gpio_flags);

	host_if->rst_gpio = of_get_named_gpio_flags(np, "rst-gpio", 0, &host_if->rst_gpio_flags);
	if (host_if->rst_gpio < 0)
		sterr("%s: Failed to read Reset GPIO!(%d)\n", __func__, host_if->rst_gpio);
	else
		stmsg("%s: Reset GPIO = %d. (flag=%d)\n", __func__, host_if->rst_gpio, host_if->rst_gpio_flags);

	spi_data->spi_mode = 0;
	rc = of_property_read_bool(np, "spi-cpol");
	if (rc)
		spi_data->spi_mode |= SPI_CPOL;

	rc = of_property_read_bool(np, "spi-cpha");
	if (rc)
		spi_data->spi_mode |= SPI_CPHA;
	stmsg("%s: SPI Mode = %u.\n", __func__, spi_data->spi_mode);
	
	return 0;
}

struct sitronix_ts_spi_data spi_data = {
	.byte_delay_us	= 1,
};

static struct sitronix_ts_host_interface spi_host_if = {
	.bus_type = BUS_SPI,
#ifdef	SITRONIX_TP_WITH_FLASH
	.is_use_flash = 1,
#else
	.is_use_flash = 0,
#endif
	.if_data = &spi_data,
	.read = sitronix_ts_spi_read_len_check,
	.write = sitronix_ts_spi_write_len_check,
	.dread = sitronix_ts_spi_dread,
	.dwrite = sitronix_ts_spi_dwrite,
	.aread = sitronix_ts_spi_aread,
	.awrite = sitronix_ts_spi_awrite,
	.sread = sitronix_ts_spi_sread,
	.ram_mode_rw = sitronix_ts_spi_ram_mode_rw,
	.ram_mode_cmd = sitronix_ts_spi_ram_mode_cmd,
};

static void sitronix_ts_spi_dev_release(struct device *dev)
{
	return;
}

static struct platform_device sitronix_ts_spi_device = {
	.name = SITRONIX_TS_SPI_DRIVER_NAME,
	.id = 0,
	.num_resources = 0,
	.dev = {
		.release = sitronix_ts_spi_dev_release,
	},
};

static int sitronix_ts_spi_probe(struct spi_device *spi)
{
	int ret;
	uint8_t retry;

	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
		sterr("%s: Full duplex not supported by host!\n", __func__);
		return -EIO;
	}

	stmsg("%s:%d:\n",__func__,__LINE__);

	if (spi->dev.of_node)
		ret = sitronix_ts_spi_parse_dt(&spi->dev, &spi_host_if);

	stmsg("%s:%d:\n",__func__,__LINE__);
	spi->bits_per_word = 8;
	spi->mode = spi_data.spi_mode;	/* SPI_MODE_3 */
	spi->max_speed_hz = spi_data.spi_max_freq;
	/* stmsg(" spi_data.spi_max_freq = %d\n", spi_data.spi_max_freq); */
	/* spi->max_speed_hz = 9600000; */

	ret = spi_setup(spi);
	if (ret < 0) {
		sterr("%s: Failed to perform SPI setup\n", __func__);
		return ret;
	}
	stmsg("%s:%d:\n",__func__,__LINE__);
	
	spi_data.spi_dev = spi;
	sitronix_ts_spi_device.id = 0;
	sitronix_ts_spi_device.num_resources = 0;
	sitronix_ts_spi_device.dev.parent = &spi->dev;
	sitronix_ts_spi_device.dev.platform_data = &spi_host_if;
	sitronix_ts_spi_device.dev.release = sitronix_ts_spi_dev_release;

	
	/* xfers = kcalloc(ST_SPI_FW_MAX_TRANS_LEN + ST_SPI_FW_ADDRESS_LEN, sizeof(struct spi_transfer), GFP_KERNEL); */
	for(retry = 0; retry < 3; retry++){
		xfers = kcalloc(2, sizeof(struct spi_transfer), GFP_KERNEL);
		if(xfers != NULL){
			break;
		}
	}
	if(retry >= 3){
		sterr("%s: Failed to kcalloc xfers\n", __func__);
		return -ENOMEM	;	
		
	}
	//spi_buf = kcalloc(ST_SPI_FW_ADDRESS_LEN + ST_SPI_FW_DUMMY_LEN + ST_SPI_FW_MAX_TRANS_LEN_READ, sizeof(uint8_t), GFP_KERNEL);

	ret = platform_device_register(&sitronix_ts_spi_device);
	if (ret) {
		sterr("%s: Failed to register platform device\n", __func__);
		return -ENODEV;
	}

	return 0;
}
#ifdef SITRONIX_INTERFACE_SPI
static int sitronix_ts_spi_remove(struct spi_device *spi)
{
	kfree(xfers);
	//kfree(spi_buf);
	platform_device_unregister(&sitronix_ts_spi_device);

	return 0;
}
#endif

static const struct spi_device_id spi_id_table[] = {
	{ SITRONIX_TS_SPI_DRIVER_NAME, 0, },
	{ },
};
/* MODULE_DEVICE_TABLE(spi, spi_id_table); */

#ifdef CONFIG_OF
static struct of_device_id spi_match_table[] = {
	{ .compatible = "sitronix_ts", },
	{ },
};
/* MODULE_DEVICE_TABLE(of, spi_match_table); */
#else /* CONFIG_OF */
#define spi_match_table		NULL
#endif /* CONFIG_OF */

static struct spi_driver sitronix_ts_spi_driver = {
	.driver = {
		.name = SITRONIX_TS_SPI_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = spi_match_table,
	},
	.probe = sitronix_ts_spi_probe,
#ifdef SITRONIX_INTERFACE_SPI	
	/*.remove = __exit_p(sitronix_ts_spi_remove),*/
	.remove = sitronix_ts_spi_remove,
#endif
	.id_table = spi_id_table,
};


int sitronix_ts_spi_init(void)
{
	int rc;

	rc = spi_register_driver(&sitronix_ts_spi_driver);

	return rc;
}
EXPORT_SYMBOL(sitronix_ts_spi_init);

void sitronix_ts_spi_exit(void)
{
	spi_unregister_driver(&sitronix_ts_spi_driver);

	return;
}
EXPORT_SYMBOL(sitronix_ts_spi_exit);

MODULE_AUTHOR("Sitronix Technology Co., Ltd.");
MODULE_DESCRIPTION("Sitronix Touchscreen Controller Driver");
MODULE_LICENSE("GPL");
