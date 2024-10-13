#include "sitronix_ts.h"
#include "sitronix_st7123.h"
#include <linux/platform_device.h>



int sitronix_copystring_without_line(const char *buf, char *path)
{
	int len = strlen(buf);

	strlcpy(path, buf, len);
	/* strcpy(path,buf); */
	if (buf[len-1] == 10)
		path[len-1] = 0;
	return 0;
}

int sitronix_copycmd_without_line(const char *buf, char *cmd, int size)
{
	int ret = 0;

	if (buf != NULL) {
		ret = copy_from_user(cmd, buf, size - 1);
		if (ret < 0) {
			sterr("copy data from user space, failed\n");
			return -EIO;
		}
	}
	stmsg("size = %d, cmd = %s\n", (int)size, cmd);

	return ret;
}

int st_formatinput(char *tmp, int slen, int index)
{
/* int retval; */
/* long unsigned int vlong; */
	unsigned long vlong;
	int ret;
	/* if(index == 0 || index == 1) { */
	/* stmsg("tmp %s , len = %d\n",tmp,slen); */
	if (slen == 1 || slen == 2) {
		/* retval = strict_strtoul(tmp, 16, &vlong); */
		/*vlong = simple_strtoul(tmp, NULL, 16);*/
		ret = kstrtoul(tmp, 16 , &vlong);
		/* if(retval != 0) { */
		/* sterr("input error for not hex\n"); */
		/* return -1; */
		/* } */
	} else {
		sterr("input len error for arg index:%d\n", index);
		return -EIO;
	}
	/* } */

	return (int) vlong;
}

static ssize_t sitronix_icinfo_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	ssize_t sret;

	mutex_lock(&gts->mutex);
	sitronix_ts_get_device_info(gts);
	mutex_unlock(&gts->mutex);
	sret = snprintf(buf, 1000,
		"Sitronix touch\n"
		"Chip ID = %02X\n"
		"FW Verison = %02X\n"
		"FW Revision = %02X %02X %02X %02X\n"
		"Customer Info = %02X %02X %02X %02X\n"
		"Resolution = %d x %d\n"
		"Channels = %d x %d\n"
		"Max touches = %d\n"
		"Misc. Info = 0x%X\n",
		gts->ts_dev_info.chip_id,
		gts->ts_dev_info.fw_version,
		gts->ts_dev_info.fw_revision[0], gts->ts_dev_info.fw_revision[1], gts->ts_dev_info.fw_revision[2], gts->ts_dev_info.fw_revision[3],
		gts->ts_dev_info.customer_info[0], gts->ts_dev_info.customer_info[1], gts->ts_dev_info.customer_info[2], gts->ts_dev_info.customer_info[3],
		gts->ts_dev_info.x_res, gts->ts_dev_info.y_res,
		gts->ts_dev_info.x_chs, gts->ts_dev_info.y_chs,
		gts->ts_dev_info.max_touches,
		gts->ts_dev_info.misc_info);
	return sret;
}

static DEVICE_ATTR(sticinfo, S_IRUGO, sitronix_icinfo_show, NULL);

static ssize_t sitronix_stmt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	stmsg("%s with %x\n", __func__, buf[0]);
	if (buf[0] == '1')
		sitronix_mt_restore();
	if (buf[0] == '0')
		sitronix_mt_pause();

	return count;
}

static ssize_t sitronix_stmt_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	return snprintf(buf, 200, "is_pause_mt = %s\n", gts->is_pause_mt ? "true" : "false");
}

static DEVICE_ATTR(stmt, (S_IRUGO | (S_IWUSR|S_IWGRP|S_IROTH)), sitronix_stmt_show, sitronix_stmt_store);

static ssize_t sitronix_stcmdio_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int s, e, i;
	char tmp[10] = {0};
	int index = 0;
	int rwflag = 0;
	int iotype = 0;
	int addr_h = 0;
	int addr_l = 0;
	int data = 0;
	int dlen = 0;
	int ret = 0;

	gts->cmdio_result = 0;
	stmsg("input: %s", buf);
	s = 0;
	for (i = 0; i < count; i++) {
		if (buf[i] == ' ') {
			e = i;
			memset(tmp, 0, sizeof(tmp));
			strlcpy(tmp, buf + s, e-s+1);

			if (index == 0) {
				rwflag = st_formatinput(tmp, strlen(tmp), index);
				stdbg("write or read %d\n", rwflag);
				if (rwflag == 0 || rwflag == 1) {
					index++;
				} else {
					sterr("write or read must be 0 or 1\n");
					strlcpy(gts->cmdio_msg, "write or read must be 0 or 1", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else if (index == 1) {
				iotype = st_formatinput(tmp, strlen(tmp), index);
				stdbg("io type %d\n", iotype);
				if (iotype >= 0 && iotype <= 6) {
					index++;
				} else {
					sterr("io type must be 0 to 6\n");
					strlcpy(gts->cmdio_msg, "io type must be 0 to 6", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else if (index == 2) {
				addr_h = st_formatinput(tmp, strlen(tmp), index);
				stdbg("addr_h 0x%x\n", addr_h);
				if (addr_h >= 0)
					index++;
				else {
					strlcpy(gts->cmdio_msg, "parser addr_h fail", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else if (index == 3) {
				addr_l = st_formatinput(tmp, strlen(tmp), index);
				stdbg("addr_l 0x%x\n", addr_l);
				if (addr_l >= 0)
					index++;
				else {
					strlcpy(gts->cmdio_msg, "parser addr_l fail", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else {
				/* data or read len */
				data = st_formatinput(tmp, strlen(tmp), index);
				stdbg("data 0x%x\n", data);
				if (data >= 0) {
					index++;
					wbuf[dlen++] = data;
				} else {
					strlcpy(gts->cmdio_msg, "parser data fail", sizeof(gts->cmdio_msg));
					return -EIO;
				}

			}


			/* stmsg("%d,%s\n" , i, tmp); */
			s = i+1;
		}
	}
	memset(tmp, 0, sizeof(tmp));
	strlcpy(tmp, buf + s, count-1-s+1);

	data = st_formatinput(tmp, strlen(tmp), index);
	stdbg("data 0x%x\n", data);
	if (data >= 0) {
		index++;
		wbuf[dlen++] = data;
	}

	if (rwflag == 1)
		dlen = (wbuf[0] << 8) + wbuf[1];

	stmsg("rwflag = %d , io type = %d , addr = 0x%02X%02X , data len = 0x%04X , data = %x\n", rwflag, iotype, addr_h, addr_l, dlen, wbuf[0]);
	if (rwflag == 0 && (iotype == 3 || iotype == 4) && (dlen%2) == 1) {
		sterr("io type 3 and 4 must write even number of data\n");
		strlcpy(gts->cmdio_msg, "io type 3 and 4 must write even number of data", sizeof(gts->cmdio_msg));
		return -EIO;
	}

	gts->cmdio_addr[0] = (u16)((addr_h<<8) + addr_l);
	gts->cmdio_rwflag = rwflag;
	gts->cmdio_len = dlen;
	gts->cmdio_iotype = iotype;

	mutex_lock(&gts->mutex);

	if (rwflag == 1 && iotype == 6)
		ret = sitronix_ts_reg_read(gts, gts->cmdio_addr[0], rbuf, gts->cmdio_len);
	else if (rwflag == 0 && iotype == 6)
		ret = sitronix_ts_reg_write(gts, gts->cmdio_addr[0], wbuf, gts->cmdio_len);
	else if (rwflag == 1 && iotype != 6)
		ret = TDU_CmdioRead(gts->cmdio_iotype, gts->cmdio_addr[0], rbuf, gts->cmdio_len);
	else if (rwflag == 0 && iotype != 6)
		ret = TDU_CmdioWrite(gts->cmdio_iotype, gts->cmdio_addr[0], wbuf, gts->cmdio_len);

	mutex_unlock(&gts->mutex);
	if (ret < 0) {
		strlcpy(gts->cmdio_msg, "I2C/SPI error" , sizeof(gts->cmdio_msg));
		return ret;
	}

	gts->cmdio_result = 1;
	return count;
}

static ssize_t sitronix_stcmdio_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	ssize_t sret = 0;
	int i;

	if (gts->cmdio_result == 0) {
		sret = snprintf(buf, 100, "cmdio fail\nmesage = %s\n", gts->cmdio_msg);
	} else {
		if (gts->cmdio_rwflag == 0)
			sret += snprintf(buf, 200, "Write , io type = %d , addr = 0x%04X , data len = 0x%04X OK\n", gts->cmdio_iotype, gts->cmdio_addr[0], gts->cmdio_len);
		else {
			sret += snprintf(buf, 20, "Data :");
			if ((sret + gts->cmdio_len*3) >= PAGE_SIZE)
				sret = snprintf(buf, 200, "The total string length > PAGE_SIZE (%ld), please split many times to read\n", PAGE_SIZE);
			else {
				for (i = 0; i < gts->cmdio_len; i++)
					sret += snprintf(buf+sret, 10, " %02X", rbuf[i]);

				sret += snprintf(buf+sret, 10, "\n");
			}
		}
	}

	return sret;
}

static DEVICE_ATTR(stcmdio, (S_IRUGO | (S_IWUSR|S_IWGRP|S_IROTH)), sitronix_stcmdio_show, sitronix_stcmdio_store);

static ssize_t sitronix_stfwinfo_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int s, e, i;
	char tmp[10] = {0};
	int index = 0;
	int rwflag = 0;
	int iotype = 0;
	int data = 0;
	int dlen = 0;
	int ret = 0;

	gts->cmdio_result = 0;
	stmsg("input: %s", buf);
	s = 0;
	for (i = 0; i < count; i++) {
		if (buf[i] == ' ') {
			e = i;
			memset(tmp, 0, sizeof(tmp));
			strlcpy(tmp, buf + s, e-s+1);

			if (index == 0) {
				rwflag = st_formatinput(tmp, strlen(tmp), index);
				stdbg("write or read %d\n", rwflag);
				if (rwflag == 0 || rwflag == 1) {
					index++;
				} else {
					sterr("write or read must be 0 or 1\n");
					strlcpy(gts->cmdio_msg, "write or read must be 0 or 1", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else if (index == 1) {
				iotype = st_formatinput(tmp, strlen(tmp), index);
				stdbg("info ID %d\n", iotype);
				index++;
			} else {
				/* data or read len */
				data = st_formatinput(tmp, strlen(tmp), index);
				stdbg("data 0x%x\n", data);
				if (data >= 0) {
					index++;
					wbuf[dlen++] = data;
				} else {
					strlcpy(gts->cmdio_msg, "parser data fail", sizeof(gts->cmdio_msg));
					return -EIO;
				}

			}


			/* stmsg("%d,%s\n" , i, tmp); */
			s = i+1;
		}
	}
	memset(tmp, 0, sizeof(tmp));
	strlcpy(tmp, buf + s, count-1-s+1);

	data = st_formatinput(tmp, strlen(tmp), index);
	stdbg("data 0x%x\n", data);
	if (data >= 0) {
		index++;
		wbuf[dlen++] = data;
	}

	if (rwflag == 1)
		dlen = (wbuf[0] << 8) + wbuf[1];

	stmsg("rwflag = %d , info ID = %d , data len = 0x%04X , data = %x\n", rwflag, iotype, dlen, wbuf[0]);	
	
	gts->cmdio_rwflag = rwflag;
	gts->cmdio_len = dlen;
	gts->cmdio_iotype = iotype;

	mutex_lock(&gts->mutex);

	ret = TDU_FWInfoRead(gts->cmdio_iotype, rbuf, gts->cmdio_len);

	mutex_unlock(&gts->mutex);
	if (ret < 0) {
		strlcpy(gts->cmdio_msg, "I2C/SPI error" , sizeof(gts->cmdio_msg));
		return ret;
	}

	gts->cmdio_result = 1;
	return count;
}

static ssize_t sitronix_stfwinfo_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	ssize_t sret = 0;
	int i;

	if (gts->cmdio_result == 0) {
		sret = snprintf(buf, 100, "fwinfo fail\nmesage = %s\n", gts->cmdio_msg);
	} else {
		if (gts->cmdio_rwflag == 0)
			sret += snprintf(buf, 200, "Write , info ID = %d , data len = 0x%04X OK\n", gts->cmdio_iotype, gts->cmdio_len);
		else {
			sret += snprintf(buf, 20, "Data :");
			if ((sret + gts->cmdio_len*3) >= PAGE_SIZE)
				sret = snprintf(buf, 200, "The total string length > PAGE_SIZE (%ld), please split many times to read\n", PAGE_SIZE);
			else {
				for (i = 0; i < gts->cmdio_len; i++)
					sret += snprintf(buf+sret, 10, " %02X", rbuf[i]);

				sret += snprintf(buf+sret, 10, "\n");
			}
		}
	}

	return sret;
}

static DEVICE_ATTR(stfwinfo, (S_IRUGO | (S_IWUSR|S_IWGRP|S_IROTH)), sitronix_stfwinfo_show, sitronix_stfwinfo_store);

static ssize_t sitronix_stdrivercmd_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int s, e, i;
	char tmp[10] = {0};
	int index = 0;
	int rwflag = 0;
	int addr = 0;
	int data = 0;
	int dlen = 0;
	int ret = 0;

	gts->cmdio_result = 0;
	stmsg("input: %s", buf);
	s = 0;
	for (i = 0; i < count; i++) {
		if (buf[i] == ' ') {
			e = i;
			memset(tmp, 0, sizeof(tmp));
			strlcpy(tmp, buf + s, e-s+1);

			if (index == 0) {
				rwflag = st_formatinput(tmp, strlen(tmp), index);
				stdbg("write or read %d\n", rwflag);
				if (rwflag == 0 || rwflag == 1) {
					index++;
				} else {
					sterr("write or read must be 0 or 1\n");
					strlcpy(gts->cmdio_msg, "write or read must be 0 or 1", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else if (index == 1) {
				addr = st_formatinput(tmp, strlen(tmp), index);
				stdbg("addr 0x%x\n", addr);
				if (addr >= 0)
					index++;
				else {
					strlcpy(gts->cmdio_msg, "parser addr fail", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else {
				/* data or read len */
				data = st_formatinput(tmp, strlen(tmp), index);
				stdbg("data 0x%x\n", data);
				if (data >= 0) {
					index++;
					wbuf[dlen++] = data;
				} else {
					strlcpy(gts->cmdio_msg, "parser data fail", sizeof(gts->cmdio_msg));
					return -EIO;
				}

			}


			/* stmsg("%d,%s\n" , i, tmp); */
			s = i+1;
		}
	}
	memset(tmp, 0, sizeof(tmp));
	strlcpy(tmp, buf + s, count-1-s+1);

	data = st_formatinput(tmp, strlen(tmp), index);
	stdbg("data 0x%x\n", data);
	if (data >= 0) {
		index++;
		wbuf[dlen++] = data;
	}

	if (rwflag == 1)
		dlen = wbuf[0];

	stmsg("rwflag = %d , addr = 0x%02X , data len = 0x%04X , data = %x\n", rwflag, addr, dlen, wbuf[0]);
	if (rwflag == 0 && (dlen%2) == 1) {
		sterr("driver cmd must write even number of data\n");
		strlcpy(gts->cmdio_msg, "driver cmd must write even number of data", sizeof(gts->cmdio_msg));
		return -EIO;
	}

	gts->cmdio_addr[0] = (u16) addr;
	gts->cmdio_rwflag = rwflag;
	gts->cmdio_len = dlen;	

	mutex_lock(&gts->mutex);

	if (rwflag == 1 )
		ret = sitronix_read_driver_cmd(gts->cmdio_addr[0], rbuf, gts->cmdio_len);
	else
		ret = sitronix_write_driver_cmd(gts->cmdio_addr[0], wbuf, gts->cmdio_len);

	mutex_unlock(&gts->mutex);
	if (ret < 0) {
		strlcpy(gts->cmdio_msg, "I2C/SPI error" , sizeof(gts->cmdio_msg));
		return ret;
	}

	gts->cmdio_result = 1;
	return count;
}

static ssize_t sitronix_stdrivercmd_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	ssize_t sret = 0;
	int i;

	if (gts->cmdio_result == 0) {
		sret = snprintf(buf, 100, "driver cmd fail\nmesage = %s\n", gts->cmdio_msg);
	} else {
		if (gts->cmdio_rwflag == 0)
			sret += snprintf(buf, 200, "Write , addr = 0x%04X , data len = 0x%04X OK\n", gts->cmdio_addr[0], gts->cmdio_len);
		else {
			sret += snprintf(buf, 20, "Data :");
			if ((sret + gts->cmdio_len*3) >= PAGE_SIZE)
				sret = snprintf(buf, 200, "The total string length > PAGE_SIZE (%ld), please split many times to read\n", PAGE_SIZE);
			else {
				for (i = 0; i < gts->cmdio_len; i++)
					sret += snprintf(buf+sret, 10, " %02X", rbuf[i]);

				sret += snprintf(buf+sret, 10, "\n");
			}
		}
	}

	return sret;
}

static DEVICE_ATTR(stdrivercmd, (S_IRUGO | (S_IWUSR|S_IWGRP|S_IROTH)), sitronix_stdrivercmd_show, sitronix_stdrivercmd_store);

static ssize_t sitronix_stupgrade_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef SITRONIX_TP_WITH_FLASH
	if (buf[0] == '1') {
		stmsg("flash_powerful_upgrade!\n");
		gts->flash_powerful_upgrade = 1;
	}
#endif /* SITRONIX_TP_WITH_FLASH */
	mutex_lock(&gts->mutex);
	gts->upgrade_result = sitronix_do_upgrade();
	mutex_unlock(&gts->mutex);
#ifdef SITRONIX_TP_WITH_FLASH
	gts->flash_powerful_upgrade = 0;
#endif /* SITRONIX_TP_WITH_FLASH */
	if (gts->upgrade_result < 0)
		return gts->upgrade_result;
	return count;
}

static ssize_t sitronix_stupgrade_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	return snprintf(buf, 1000, "upgrade result = %s\nmessage: %s\n", gts->upgrade_result >= 0 ? "success" : "fail", gts->upgrade_msg);
}

static DEVICE_ATTR(stupgrade, (S_IRUGO | (S_IWUSR|S_IWGRP|S_IROTH)), sitronix_stupgrade_show, sitronix_stupgrade_store);


static ssize_t sitronix_strequestfw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	int ret = 0;
	const struct firmware *fw = NULL;
	char path[200];
	int fwOff;
	int fwSize;
	int fwInfoOff;
	int cfgFlashOff;
	int cfgDramOff;
	int cfgSize;

	sitronix_copystring_without_line(buf, path);

	stmsg("input: %s\n", buf);
	stmsg("after: %s\n", path);
	ret = request_firmware(&fw, path, &gts->pdev->dev);
	stmsg("request_firmware ret = %d\n", ret);
	if (ret)
		return -ENOENT;

	stmsg("fw size 0x%X\n", (unsigned int)fw->size);
	if ( fw->size > 0x1000)
		ret =  st_check_fw(fw->data, &fwOff, &fwSize, &fwInfoOff, &cfgFlashOff, &cfgDramOff);
	else {
		ret = -1;
		
	}
	stmsg("sitronix_set_fw_buf ret = %d\n", ret);
	if (ret == 0) {
		st_set_dump(fw->data+fwOff, fwOff, fwSize);
		if (fw->size > cfgFlashOff) {
			ret = st_check_cfg(fw->data+cfgFlashOff, &cfgSize);
			stmsg("sitronix_set_cfg_buf ret = %d\n", ret);
			if (ret == 0)
				st_set_dump(fw->data+cfgFlashOff, cfgFlashOff, cfgSize);
		}
	} else {		
		ret = st_check_cfg(fw->data, &cfgSize);
		stmsg("sitronix_set_cfg_buf ret = %d\n", ret);
		if (ret == 0)
			st_set_dump(fw->data, ST_FW_LEN, cfgSize);
	}
	gts->fw_request_status = 1;
	release_firmware(fw);
	if (ret != 0)
		return ret;
	return count;

}

static ssize_t sitronix_strequestfw_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{

	return snprintf(buf, 200, "sitronix_strequestfw_show\n");

}
static DEVICE_ATTR(strequestfw, (S_IRUGO | (S_IWUSR|S_IWGRP|S_IROTH)), sitronix_strequestfw_show, sitronix_strequestfw_store);


static ssize_t sitronix_stselftest_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sitronix_ts_irq_enable(gts, false);
	
	gts->upgrade_doing = true;
	mutex_lock(&gts->mutex);
	gts->self_test_result = st_self_test();
	mutex_unlock(&gts->mutex);
	
	sitronix_ts_mt_reset_process();
	gts->upgrade_doing = false;
	
	//Enable IRQ
	sitronix_ts_irq_enable(gts, true);
	 
	return count;
}
static ssize_t sitronix_stselftest_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	ssize_t sret;
	sret = snprintf(buf, 200,
					"Sitronix Self Test Result = %d ( 0 == success, >0 failed sensor number, < 0 err) \n", gts->self_test_result);
	return sret;
}

static DEVICE_ATTR(stselftest, (S_IRUGO | (S_IWUSR|S_IWGRP|S_IROTH)), sitronix_stselftest_show, sitronix_stselftest_store);


static ssize_t sitronix_stswu_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	ssize_t sret;

	sitronix_ts_report_swu(gts->input_dev, ST_SWU_GESTURE_DOUBLECLICK);
	sret = snprintf(buf, 1000,
		"swu ok");
	return sret;
}

static DEVICE_ATTR(stswu, S_IRUGO, sitronix_stswu_show, NULL);

static ssize_t sitronix_stmodeswitch_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int modeID = 0;
	bool modeFlag = true;
	unsigned char modeValue = 0;
	char tmp[3] = {0};

	//strlcpy(tmp, buf, 3);
	strlcpy(tmp, buf, sizeof(tmp));
	modeID = st_formatinput(tmp, 2, 0);

	if (modeID == ST_MODE_GRIP) {
			
		if (buf[3] == '0')
			modeValue = ST_MODE_GRIP_ROTATE_0;
		else if(buf[3] == '1')
			modeValue = ST_MODE_GRIP_ROTATE_90;
		else if(buf[3] == '2')
			modeValue = ST_MODE_GRIP_ROTATE_180;
		else if(buf[3] == '3')
			modeValue = ST_MODE_GRIP_ROTATE_270;
		else
			modeFlag = false;			
			
		mutex_lock(&gts->mutex);
		sitronix_mode_switch_value(modeID, modeFlag, modeValue);
		mutex_unlock(&gts->mutex);
	} else {
		if (buf[3] == '1')
			modeFlag = true;
		if (buf[3] == '0')
			modeFlag = false;
		mutex_lock(&gts->mutex);
		sitronix_mode_switch(modeID, modeFlag);
		mutex_unlock(&gts->mutex);
	}

	

	return count;
}

static ssize_t sitronix_stmodeswitch_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int i;
	ssize_t sret = 0;

	for (i = 1; i < ST_MODE_SIZE; i++) {
		if( i == ST_MODE_GRIP)
			sret += snprintf(buf+sret, 100, "modeID = %d , flag = %s , value = %x\n", i, gts->mode_flag[i] ? "on" : "off", gts->mode_value[i]);
		else
			sret += snprintf(buf+sret, 100, "modeID = %d , flag = %s\n", i, gts->mode_flag[i] ? "on" : "off");
	}

	return sret;
}

static DEVICE_ATTR(stmodeswitch, (S_IRUGO | (S_IWUSR|S_IWGRP|S_IROTH)), sitronix_stmodeswitch_show, sitronix_stmodeswitch_store);

static ssize_t sitronix_stproximity_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (!gts->is_support_proximity) {
		sterr("Don't support proximity!\n");
		return count;
	}
	
	stmsg("%s with %x\n", __func__, buf[0]);
	mutex_lock(&gts->mutex);
	if (buf[0] == '1') {		
		sitronix_ts_proximity_enable(gts, true);

	}
	if (buf[0] == '0') {
		sitronix_ts_proximity_enable(gts, false);
	}
	
	if (buf[0] == '2' && gts->proximity_flag && gts->in_suspend) {
		stmsg("Enable Proximity Sensing in suspend\n");
		sitronix_ts_proximity_control_sensing(gts, true);
	}
	
	mutex_unlock(&gts->mutex);
	return count;
}

static ssize_t sitronix_stproximity_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	return snprintf(buf, 200, "proximity_flag = %s\n", gts->proximity_flag ? "true" : "false");
}

static DEVICE_ATTR(stproximity, (S_IRUGO | (S_IWUSR|S_IWGRP|S_IROTH)), sitronix_stproximity_show, sitronix_stproximity_store);

static struct attribute *sitronix_attrs[] = {
	&dev_attr_sticinfo.attr,
	&dev_attr_stmt.attr,
	&dev_attr_stcmdio.attr,
	&dev_attr_stfwinfo.attr,
	&dev_attr_stupgrade.attr,
	&dev_attr_strequestfw.attr,
	&dev_attr_stselftest.attr,	
	&dev_attr_stswu.attr,
	&dev_attr_stmodeswitch.attr,
	&dev_attr_stproximity.attr,
	&dev_attr_stdrivercmd.attr,
	NULL
};

static const struct attribute_group sitronix_attr_group = {
	/* .name		= "sitronix_ts_attrs", */
	.attrs		= sitronix_attrs,
};


int sitronix_create_sysfs(struct sitronix_ts_data *ts)
{
	int error;

	ts->sitronix_kobj = kobject_create_and_add("sitronix_ts_attrs", NULL);

	/* error = sysfs_create_group(&ts->pdev->dev.kobj, &sitronix_attr_group); */
	error = sysfs_create_group(ts->sitronix_kobj, &sitronix_attr_group);
	return error;
}

void sitronix_remove_sysfs(struct sitronix_ts_data *ts)
{
	/* sysfs_remove_group(&ts->pdev->dev.kobj, &sitronix_attr_group); */
	sysfs_remove_group(ts->sitronix_kobj, &sitronix_attr_group);
	kobject_put(ts->sitronix_kobj);
}



static int sitronix_major;
static struct cdev sitronix_cdev;
static struct class *sitronix_class;
#define SITRONIX_I2C_TOUCH_DEV_NAME "sitronixDev"


int  st_dev_open(struct inode *inode, struct file *filp)
{
	sitronix_mt_pause();
	return 0;
}
EXPORT_SYMBOL(st_dev_open);
int  st_dev_release(struct inode *inode, struct file *filp)
{
	sitronix_mt_restore();
	return 0;
}
EXPORT_SYMBOL(st_dev_release);
ssize_t  st_dev_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	int off;
	int len;
	unsigned char *temp_buf;

	if (count == 0)
		return 0;
	if (count > SITRONIX_RW_BUF_LEN  + 8) {
		count = SITRONIX_RW_BUF_LEN + 8;
		sterr("SITRONIX_RW_BUF_LEN = %d\n", SITRONIX_RW_BUF_LEN);
	}

	if (copy_from_user(wbuf, buf, count)) {
		/* kfree(tmp); */
		return -EFAULT;
	}
	mutex_lock(&gts->mutex);

	if (gts->stdev_mode != 0) { 
		switch (gts->stdev_mode) {
		case STDEV_WRITE_DUMP:

			if(gts->host_if->is_use_flash){
				st_bakcup_flash_dump_buf();
				ret = copy_from_user(wbuf, buf, count);	//restore command
			}

			off = wbuf[0] << 24 | wbuf[1] << 16 | wbuf[2] << 8 | wbuf[3];
			len = wbuf[4] << 24 | wbuf[5] << 16 | wbuf[6] << 8 | wbuf[7];

			stmsg("write dump off = 0x%X , len = 0x%X\n", off, len);
			if (off + len > st_get_dump_size()) {
				stmsg("off + len > dump size( 0x%X )\n", st_get_dump_size());
				ret = -1;
			} else {
				st_set_dump(wbuf + 8, off, len);
				ret = count;
			}
			gts->stdev_mode = 0;
			break;
		case STDEV_READ_DUMP:
			
			off = wbuf[0] << 24 | wbuf[1] << 16 | wbuf[2] << 8 | wbuf[3];
			len = wbuf[4] << 24 | wbuf[5] << 16 | wbuf[6] << 8 | wbuf[7];
			
			if(gts->host_if->is_use_flash){
				st_bakcup_flash_dump_buf();
			}
			
			stmsg("read dump off = 0x%X , len = 0x%X\n", off, len);
			if (off + len > st_get_dump_size()) {
				stmsg("off + len > dump size( 0x%X )\n", st_get_dump_size());
				ret = -1;
			} else {
				temp_buf = kzalloc(len, GFP_KERNEL);
				st_get_dump(temp_buf, off, len);
				memcpy(rbuf, temp_buf, len);
				kfree(temp_buf);
				ret = count;
			}
			gts->stdev_mode = STDEV_RETURN_MODE;
			break;
		case STDEV_ADDR_MODE_ON:
			if (gts->wr_len == count) {
				for (ret = 0; ret < count; ret++)
					gts->cmdio_addr[ret] = wbuf[ret];
				gts->wr_mode = true;
				ret = count;
				stmsg("w_r len = %d, write: %x %x %x %x\n", ret, wbuf[0], wbuf[1], wbuf[2], wbuf[3]);
			} else {
				/* call address mode write */
				memcpy(rbuf + 1, wbuf, count);
				ret = sitronix_ts_addrmode_write(gts, rbuf, count);
				stmsg(" write len %d , %x %x %x %x\n", count, rbuf[1], rbuf[2], rbuf[3], rbuf[4]);
			}


			break;
		default:
			break;
		}
	} else if (gts->host_if->bus_type == BUS_SPI && gts->wr_len == count) {
		for (ret = 0; ret < count; ret++)
			gts->cmdio_addr[ret] = wbuf[ret];
		gts->wr_mode = true;
		ret = count;
	} else {
		if (gts->host_if->bus_type == BUS_I2C && gts->wr_len == count)
			gts->wr_mode = true;
		ret = sitronix_ts_reg_dwrite(gts, wbuf, count);
	}
	mutex_unlock(&gts->mutex);

	if (gts->cr_mode == 1)
		gts->cr_mode = 2;
	else if (gts->cr_mode == 2)
	{
		gts->cr_mode = 0;
		stmsg("cr_mode = 0\n");
	}
	return ret;
}
EXPORT_SYMBOL(st_dev_write);

int st_dev_function(int cmd)
{
	int ret = 0;

	if (cmd < 8) {
		stmsg("Set WR_LEN = %d\n", cmd);
		gts->wr_len = cmd;
		memset(rbuf, 0, cmd);
		return cmd;
	}

	switch (cmd) {
	case STDEV_GET_DRIVER_INFO:
		rbuf[0] = 'S';
		rbuf[1] = 'T';
		rbuf[2] = gts->ts_dev_info.chip_id;
		if (gts->host_if->bus_type == BUS_I2C && gts->host_if->is_use_flash)
			rbuf[3] = 1;
		else if (gts->host_if->bus_type == BUS_SPI && gts->host_if->is_use_flash)
			rbuf[3] = 2;
		else
			rbuf[3] = 3;

		ret = st_get_dump_size();
		rbuf[4] = (ret >> 24) & 0xFF;
		rbuf[5] = (ret >> 16) & 0xFF;
		rbuf[6] = (ret >> 8) & 0xFF;
		rbuf[7] = (ret) & 0xFF;

		rbuf[8] = (ST_PLATFORM_WRITE_LEN_MAX >> 24) & 0xFF;
		rbuf[9] = (ST_PLATFORM_WRITE_LEN_MAX >> 16) & 0xFF;
		rbuf[10] = (ST_PLATFORM_WRITE_LEN_MAX >> 8) & 0xFF;
		rbuf[11] = (ST_PLATFORM_WRITE_LEN_MAX) & 0xFF;

		rbuf[12] = (ST_PLATFORM_READ_LEN_MAX >> 24) & 0xFF;
		rbuf[13] = (ST_PLATFORM_READ_LEN_MAX >> 16) & 0xFF;
		rbuf[14] = (ST_PLATFORM_READ_LEN_MAX >> 8) & 0xFF;
		rbuf[15] = (ST_PLATFORM_READ_LEN_MAX) & 0xFF;
		ret = 16;
		break;
	case STDEV_ADDR_MODE_ON:
		stmsg("Address mode on\n");
		gts->wr_len = 8;
		gts->stdev_mode = STDEV_ADDR_MODE_ON;
		memset(rbuf, 0, STDEV_ADDR_MODE_ON);
		ret = 0;
		break;
	case STDEV_ADDR_MODE_OFF:
		stmsg("Address mode off\n");
		gts->wr_len = STDEV_WR_DF;
		gts->stdev_mode = 0;
		memset(rbuf, 0, STDEV_ADDR_MODE_OFF);
		ret = 0;
		break;
	case STDEV_WRITE_DUMP:
		stmsg("Write dump CMD\n");
		gts->stdev_mode = STDEV_WRITE_DUMP;
		memset(rbuf, 0, STDEV_WRITE_DUMP);
		ret = 0;
		break;
	case STDEV_READ_DUMP:
		stmsg("Read dump CMD\n");
		gts->stdev_mode = STDEV_READ_DUMP;
		memset(rbuf, 0, STDEV_READ_DUMP);
		ret = 0;
		break;
	case STDEV_EXECUTE_UPGRADE:
		stmsg("Execute upgrade CMD\n");
		gts->flash_powerful_upgrade = 1;
		ret = sitronix_do_upgrade();
		gts->flash_powerful_upgrade = 0;
		if (ret >= 0) {
			memset(rbuf, 0, STDEV_EXECUTE_UPGRADE);
			ret = STDEV_EXECUTE_UPGRADE;
		}
		break;
	case STDEV_ENABLE_IRQ:
		stmsg("enable IRQ\n");
		sitronix_ts_irq_enable(gts, true);
		ret = 0;
		break;
	case STDEV_DISABLE_IRQ:
		stmsg("disable IRQ\n");
		sitronix_ts_irq_enable(gts, false);
		ret = 0;
		break;
	case STDEV_HW_RESET:
		stmsg("HW reset\n");
		sitronix_ts_reset_device(gts);
		ret = 0;
		break;
	case STDEV_GET_COORD_BUF:		
		memcpy(rbuf, gts->coord_buf, STDEV_GET_COORD_BUF);
		ret = STDEV_GET_COORD_BUF;
		break;
	case STDEV_IRQ_WAITMODE_COORD:
	case STDEV_IRQ_WAITMODE_RAW:
		//stmsg("STDEV_IRQ_WAITMODE_RAW ON\n");
		gts->irq_wait_mode = cmd;
		rbuf[0] = 'I';
		rbuf[1] = 'R';
		rbuf[2] = 'Q';
		ret = 3;
		break;
	case STDEV_IRQ_WAITMODE_OFF:
		//stmsg("STDEV_IRQ_WAITMODE_RAW OFF\n");
		gts->irq_wait_mode = 0;
		ret = 0;
		break;
	case STDEV_IRQ_WAITMODE_DO:		
		ret = wait_event_interruptible_timeout(gts->irq_wait_queue , gts->irq_wait_flag != 0 , 500);
		//stmsg("STDEV_IRQ_WAITMODE_DO ret = %d\n", ret);
		rbuf[0] = (ret >> 8) & 0xFF;
		rbuf[1] = ret & 0xFF;
		ret = 2;		
		gts->irq_wait_flag = 0;
		break;
	case STDEV_CONTINUE_READ:
		//stmsg("STDEV_IRQ_WAITMODE_RAW OFF\n");
		gts->cr_mode = 1;
		ret = 0;
		break;
	case STDEV_PROXIMITY_STATUS:
		if (gts->proximity_flag)
			rbuf[0] = gts->proximity_status;
		else {
			sitronix_ts_reg_read(gts, TOUCH_INFO, rbuf, 1);
			rbuf[0] = (rbuf[0] >> 4 ) & 0x07;
		}
		ret = 1;
		break;
	case STDEV_PROXIMITY_ON:
		sitronix_ts_proximity_enable(gts, true);
		ret = 0;
		break;
	case STDEV_PROXIMITY_OFF:
		sitronix_ts_proximity_enable(gts, false);
		ret = 0;
		break;
	case STDEV_RESET_INPUT_DEV:
		sitronix_ts_reset_input_dev();
		ret = 0;
		break;
	default:
		break;
	}

	return ret;
}
ssize_t st_dev_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{	
	int ret = 0;

	if (count > SITRONIX_RW_BUF_LEN + 8) {
		count = SITRONIX_RW_BUF_LEN + 8;
		sterr("SITRONIX_RW_BUF_LEN = %d\n", SITRONIX_RW_BUF_LEN);
	}

	if (gts->stdev_mode == STDEV_RETURN_MODE) {
		ret = copy_to_user(buf, rbuf, count) ?  -EFAULT : count;
		gts->stdev_mode = 0;
		return ret;
	}
	
	//stmsg("gts->wr_mode = %d , st_dev_read %d , gts->irq_wait_mode = %d\n", gts->wr_mode, count, gts->irq_wait_mode);
	/* IRQ wait mode*/
	if (!gts->wr_mode && count == STDEV_IRQ_WAITMODE_DO && gts->irq_wait_mode != 0 ) {
		ret = st_dev_function(count);
		if (ret > 0)
			ret = copy_to_user(buf, rbuf, ret) ?  -EFAULT : ret;
		return count;
	}

	mutex_lock(&gts->mutex);

	if (!gts->wr_mode && gts->cr_mode != 2) {
		ret = st_dev_function(count);
		if (ret > 0)
			ret = copy_to_user(buf, rbuf, ret) ?  -EFAULT : ret;

	} else if (gts->stdev_mode == STDEV_ADDR_MODE_ON) {
		for (ret = 0; ret < gts->wr_len; ret++)
			wbuf[ret+1] = gts->cmdio_addr[ret];

		ret = sitronix_ts_addrmode_read(gts, wbuf, gts->wr_len, rbuf, count);
		if (ret >= 0)
			ret = copy_to_user(buf, rbuf + ret, count) ?  -EFAULT : count;
		gts->wr_len = 8;
		stmsg("read len = %d, result: %x %x %x %x\n", ret, buf[0], buf[1], buf[2], buf[3]);
	} else {
		ret = sitronix_ts_reg_dread(gts, rbuf, count);
		if (ret > 0)
			ret = copy_to_user(buf, rbuf, ret) ?  -EFAULT : ret;

	}

	gts->wr_mode = false;

	mutex_unlock(&gts->mutex);
	if (ret < 0)
		return ret;
	else
		return count;
}
EXPORT_SYMBOL(st_dev_read);

long st_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	int subcmd;

	if (_IOC_TYPE(cmd) != STDEV_IOC_MAGIC)
		return -ENOTTY;

	stmsg("st_dev_ioctl cmd: 0x%X\n", cmd);
	subcmd = cmd & 0xFF;

	retval = st_dev_function(subcmd);

	if (retval < 0)
		return retval;
	else
		return 0;
}
EXPORT_SYMBOL(st_dev_ioctl);
static struct file_operations st_dev_fops = {
	.owner =        THIS_MODULE,
	.write		= st_dev_write,
	.read		= st_dev_read,
	.open		= st_dev_open,
	.unlocked_ioctl = st_dev_ioctl,
	.release	= st_dev_release,
};

int sitronix_create_st_dev(struct sitronix_ts_data *ts)
{
	int result;
	dev_t devno = MKDEV(sitronix_major, 0);

	result  = alloc_chrdev_region(&devno, 0, 1, SITRONIX_I2C_TOUCH_DEV_NAME);
	if (result < 0) {
		sterr("fail to allocate chrdev (%d)\n", result);
		return result;
	}

	sitronix_major = MAJOR(devno);
	cdev_init(&sitronix_cdev, &st_dev_fops);
	sitronix_cdev.owner = THIS_MODULE;
	sitronix_cdev.ops = &st_dev_fops;

	result =  cdev_add(&sitronix_cdev, devno, 1);
	if (result) {
		sterr("fail to add cdev (%d)\n", result);
		return result;
	}

	sitronix_class = class_create(THIS_MODULE, SITRONIX_I2C_TOUCH_DEV_NAME);
	if (IS_ERR(sitronix_class)) {
		result = PTR_ERR(sitronix_class);
		unregister_chrdev(sitronix_major, SITRONIX_I2C_TOUCH_DEV_NAME);
		sterr("fail to create class (%d)\n", result);
		return result;
	}
	device_create(sitronix_class, NULL, MKDEV(sitronix_major, 0), NULL, SITRONIX_I2C_TOUCH_DEV_NAME);
	
	init_waitqueue_head(&ts->irq_wait_queue); 
	ts->irq_wait_flag = 0;
	return 0;
}

void sitronix_remove_st_dev(struct sitronix_ts_data *ts)
{
	dev_t dev_id = MKDEV(sitronix_major, 0);

	cdev_del(&sitronix_cdev);
	device_destroy(sitronix_class, dev_id); /* delete device node under /dev */
	class_destroy(sitronix_class); /* delete class created by us */
	unregister_chrdev_region(dev_id, 1);
}

/* ///////////////////// proc */
typedef struct {
	char *name;
	struct proc_dir_entry *node;
	struct file_operations *fops;
	bool isCreated;
	char *on_cmd;
	char *off_cmd;
	char *option0;
	char *option1;
	char *option2;
	char *option3;
} sitronix_proc_node_t;


extern sitronix_proc_node_t sitronix_proc_table[];



ssize_t sitronix_copy_to_user(char *name, bool flag, char __user *buff)
{
	int ret = 0;
	uint32_t len = 0;

	memset(rbuf, 0, sizeof(rbuf) * sizeof(unsigned char));

	//len = snprintf(rbuf, sizeof(rbuf), "%s = %d\n", name, flag);
	len = snprintf(rbuf, (SITRONIX_RW_BUF_LEN+8), "%s = %d\n", name, flag);

	stmsg("%s = %d\n", name, flag);

	ret = copy_to_user((uint32_t *) buff, rbuf, len);
	if (ret < 0) {
		sterr("Failed to copy data to user space\n");
		len = 0;
	}

	return len;
}
static ssize_t sitronix_proc_swu_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{
	uint32_t len = 0;

	len = sitronix_copy_to_user("ST_MODE_SWU", gts->mode_flag[ST_MODE_SWU], buff);
	*pPos = len;

	return 0;
}

static ssize_t sitronix_proc_swu_write(struct file *filp, const char *buff, size_t size, loff_t *pPos)
{
	char cmd[10] = { 0 };

	if (0 < sitronix_copycmd_without_line(buff, cmd, size)) {
		sterr("copy data from user space, failed\n");
		return -EINVAL;
	}

	if (strcmp(cmd, sitronix_proc_table[0].on_cmd) == 0)
		sitronix_mode_switch(ST_MODE_SWU, true);
	else if (strcmp(cmd, sitronix_proc_table[0].off_cmd) == 0)
		sitronix_mode_switch(ST_MODE_SWU, false);
	else
		return -EIO;

	return size;
}

static ssize_t sitronix_proc_glove_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{

	uint32_t len = 0;

	len = sitronix_copy_to_user("ST_MODE_GLOVE", gts->mode_flag[ST_MODE_GLOVE], buff);
	*pPos = len;

	return 0;
}

static ssize_t sitronix_proc_glove_write(struct file *filp, const char *buff, size_t size, loff_t *pPos)
{
	char cmd[10] = { 0 };

	if (0 < sitronix_copycmd_without_line(buff, cmd, size)) {
		sterr("copy data from user space, failed\n");
		return -EINVAL;
	}

	if (strcmp(cmd, sitronix_proc_table[1].on_cmd) == 0)
		sitronix_mode_switch(ST_MODE_GLOVE, true);
	else if (strcmp(cmd, sitronix_proc_table[1].off_cmd) == 0)
		sitronix_mode_switch(ST_MODE_GLOVE, false);
	else
		return -EIO;

	return size;
}

static ssize_t sitronix_proc_charge_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{

	uint32_t len = 0;

	len = sitronix_copy_to_user("ST_MODE_CHARGE", gts->mode_flag[ST_MODE_CHARGE], buff);
	*pPos = len;

	return 0;
}

static ssize_t sitronix_proc_charge_write(struct file *filp, const char *buff, size_t size, loff_t *pPos)
{
	char cmd[10] = { 0 };

	if (0 < sitronix_copycmd_without_line(buff, cmd, size)) {
		sterr("copy data from user space, failed\n");
		return -EINVAL;
	}

	if (strcmp(cmd, sitronix_proc_table[2].on_cmd) == 0)
		sitronix_mode_switch(ST_MODE_CHARGE, true);
	else if (strcmp(cmd, sitronix_proc_table[2].off_cmd) == 0)
		sitronix_mode_switch(ST_MODE_CHARGE, false);
	else
		return -EIO;

	return size;
}

static ssize_t sitronix_proc_jittersuppress_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{

	uint32_t len = 0;

	len = sitronix_copy_to_user("ST_MODE_JITTERSUPPRESS", gts->mode_flag[ST_MODE_JITTERSUPPRESS], buff);
	*pPos = len;

	return 0;
}

static ssize_t sitronix_proc_jittersuppress_write(struct file *filp, const char *buff, size_t size, loff_t *pPos)
{
	char cmd[10] = { 0 };

	if (0 < sitronix_copycmd_without_line(buff, cmd, size)) {
		sterr("copy data from user space, failed\n");
		return -EINVAL;
	}

	if (strcmp(cmd, sitronix_proc_table[3].on_cmd) == 0)
		sitronix_mode_switch(ST_MODE_JITTERSUPPRESS, true);
	else if (strcmp(cmd, sitronix_proc_table[3].off_cmd) == 0)
		sitronix_mode_switch(ST_MODE_JITTERSUPPRESS, false);
	else
		return -EIO;

	return size;
}

static ssize_t sitronix_proc_palm_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{

	uint32_t len = 0;

	len = sitronix_copy_to_user("ST_MODE_PALM", gts->mode_flag[ST_MODE_PALM], buff);
	*pPos = len;

	return 0;
}

static ssize_t sitronix_proc_palm_write(struct file *filp, const char *buff, size_t size, loff_t *pPos)
{
	char cmd[10] = { 0 };

	if (0 < sitronix_copycmd_without_line(buff, cmd, size)) {
		sterr("copy data from user space, failed\n");
		return -EINVAL;
	}

	if (strcmp(cmd, sitronix_proc_table[4].on_cmd) == 0)
		sitronix_mode_switch(ST_MODE_PALM, true);
	else if (strcmp(cmd, sitronix_proc_table[4].off_cmd) == 0)
		sitronix_mode_switch(ST_MODE_PALM, false);
	else
		return -EIO;

	return size;
}

static ssize_t sitronix_proc_headphone_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{

	uint32_t len = 0;

	len = sitronix_copy_to_user("ST_MODE_HEADPHONE", gts->mode_flag[ST_MODE_HEADPHONE], buff);
	*pPos = len;

	return 0;
}

static ssize_t sitronix_proc_headphone_write(struct file *filp, const char *buff, size_t size, loff_t *pPos)
{
	char cmd[10] = { 0 };

	if (0 < sitronix_copycmd_without_line(buff, cmd, size)) {
		sterr("copy data from user space, failed\n");
		return -EINVAL;
	}

	if (strcmp(cmd, sitronix_proc_table[5].on_cmd) == 0)
		sitronix_mode_switch(ST_MODE_HEADPHONE, true);
	else if (strcmp(cmd, sitronix_proc_table[5].off_cmd) == 0)
		sitronix_mode_switch(ST_MODE_HEADPHONE, false);
	else
		return -EIO;

	return size;
}

static ssize_t sitronix_proc_grip_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{

	uint32_t len = 0;

	len = sitronix_copy_to_user("ST_MODE_GRIP", gts->mode_flag[ST_MODE_GRIP], buff);
	*pPos = len;

	return 0;
}

static ssize_t sitronix_proc_grip_write(struct file *filp, const char *buff, size_t size, loff_t *pPos)
{
	char cmd[10] = { 0 };
	unsigned char value;

	if (0 < sitronix_copycmd_without_line(buff, cmd, size)) {
		sterr("copy data from user space, failed\n");
		return -EINVAL;
	}
	
	value = 0;
	if(strcmp(cmd, sitronix_proc_table[6].option0) == 0){
		mutex_lock(&gts->mutex);
		sitronix_mode_switch_value(ST_MODE_GRIP, true, ST_MODE_GRIP_ROTATE_0);
		mutex_unlock(&gts->mutex);
	}
	else if(strcmp(cmd, sitronix_proc_table[6].option1) == 0){
		mutex_lock(&gts->mutex);
		sitronix_mode_switch_value(ST_MODE_GRIP, true, ST_MODE_GRIP_ROTATE_90);
		mutex_unlock(&gts->mutex);
	}
	else if(strcmp(cmd, sitronix_proc_table[6].option2) == 0){
		mutex_lock(&gts->mutex);
		sitronix_mode_switch_value(ST_MODE_GRIP, true, ST_MODE_GRIP_ROTATE_180);
		mutex_unlock(&gts->mutex);
	}
	else if(strcmp(cmd, sitronix_proc_table[6].option3) == 0){
		mutex_lock(&gts->mutex);
		sitronix_mode_switch_value(ST_MODE_GRIP, true, ST_MODE_GRIP_ROTATE_270);
		mutex_unlock(&gts->mutex);
	}
	else if (strcmp(cmd, sitronix_proc_table[6].on_cmd) == 0){
		mutex_lock(&gts->mutex);
		sitronix_mode_switch_value(ST_MODE_GRIP, true, gts->mode_value[ST_MODE_GRIP]);
		mutex_unlock(&gts->mutex);
	}
	else if (strcmp(cmd, sitronix_proc_table[6].off_cmd) == 0){
		mutex_lock(&gts->mutex);
		sitronix_mode_switch_value(ST_MODE_GRIP, false, gts->mode_value[ST_MODE_GRIP]);
		mutex_unlock(&gts->mutex);
	}
	else
		return -EIO;

	return size;
}


int st_proc_sticinfo_show(struct seq_file *m, void *v) {	
	mutex_lock(&gts->mutex);
	sitronix_ts_get_device_info(gts);
	mutex_unlock(&gts->mutex);
	seq_printf(m,
		"Sitronix touch\n"
		"Chip ID = %02X\n"
		"FW Verison = %02X\n"
		"FW Revision = %02X %02X %02X %02X\n"
		"Customer Info = %02X %02X %02X %02X\n"
		"Resolution = %d x %d\n"
		"Channels = %d x %d\n"
		"Max touches = %d\n"
		"Misc. Info = 0x%X\n",
		gts->ts_dev_info.chip_id,
		gts->ts_dev_info.fw_version,
		gts->ts_dev_info.fw_revision[0], gts->ts_dev_info.fw_revision[1], gts->ts_dev_info.fw_revision[2], gts->ts_dev_info.fw_revision[3],
		gts->ts_dev_info.customer_info[0], gts->ts_dev_info.customer_info[1], gts->ts_dev_info.customer_info[2], gts->ts_dev_info.customer_info[3],
		gts->ts_dev_info.x_res, gts->ts_dev_info.y_res,
		gts->ts_dev_info.x_chs, gts->ts_dev_info.y_chs,
		gts->ts_dev_info.max_touches,
		gts->ts_dev_info.misc_info);
	return 0;
}

int  st_proc_sticinfo_open(struct inode *inode, struct file *filp)
{		
	return single_open(filp, st_proc_sticinfo_show, NULL);
}

int st_proc_stfwver_show(struct seq_file *m, void *v) {	
	mutex_lock(&gts->mutex);
	sitronix_ts_get_device_info(gts);
	mutex_unlock(&gts->mutex);
	seq_printf(m,
		"%02X.%02X%02X_%02X%02X\n", gts->ts_dev_info.fw_version, 
		gts->ts_dev_info.fw_revision[0], gts->ts_dev_info.fw_revision[1], gts->ts_dev_info.fw_revision[2], gts->ts_dev_info.fw_revision[3]);
	return 0;
}

int  st_proc_stfwver_open(struct inode *inode, struct file *filp)
{		
	return single_open(filp, st_proc_stfwver_show, NULL);
}

static ssize_t st_proc_stmt_write(struct file *file, const char *buff, size_t size, loff_t *ppos)
{
	char cmd[10] = { 0 };	

	if (0 < sitronix_copycmd_without_line(buff, cmd, size)) {
		sterr("copy data from user space, failed\n");
		return -EINVAL;
	}
	
	if(strcmp(cmd, "on") == 0)
		sitronix_mt_restore();
	if(strcmp(cmd, "off") == 0)
		sitronix_mt_pause();

	return size;
}

int st_proc_stmt_show(struct seq_file *m, void *v) {	
	
	seq_printf(m,"is_pause_mt = %s\n", gts->is_pause_mt ? "true" : "false");
	return 0;
}

int  st_proc_stmt_open(struct inode *inode, struct file *filp)
{		
	return single_open(filp, st_proc_stmt_show, NULL);
}

static ssize_t st_proc_strequestfw_write(struct file *file, const char *buff, size_t size, loff_t *ppos)
{
	int ret = 0;
	char cmd[100] = { 0 };
	const struct firmware *fw = NULL;
	int fwOff;
	int fwSize;
	int fwInfoOff;
	int cfgFlashOff;
	int cfgDramOff;
	int cfgSize;

	if (0 < sitronix_copycmd_without_line(buff, cmd, size)) {
		sterr("copy data from user space, failed\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, cmd, &gts->pdev->dev);
	stmsg("request_firmware ret = %d\n", ret);
	if (ret)
		return -ENOENT;	
	stmsg("fw size 0x%X\n", (unsigned int)fw->size);
	
	if ( fw->size > 0x1000)
		ret =  st_check_fw(fw->data, &fwOff, &fwSize, &fwInfoOff, &cfgFlashOff, &cfgDramOff);
	else {
		ret = -1;
		
	}
	stmsg("sitronix_set_fw_buf ret = %d\n", ret);
	if (ret == 0) {
		st_set_dump(fw->data+fwOff, fwOff, fwSize);
		if (fw->size > cfgFlashOff) {
			ret = st_check_cfg(fw->data+cfgFlashOff, &cfgSize);
			stmsg("sitronix_set_cfg_buf ret = %d\n", ret);
			if (ret == 0)
				st_set_dump(fw->data+cfgFlashOff, cfgFlashOff, cfgSize);
		}
	} else {		
		ret = st_check_cfg(fw->data, &cfgSize);
		stmsg("sitronix_set_cfg_buf ret = %d\n", ret);
		if (ret == 0)
			st_set_dump(fw->data, ST_FW_LEN, cfgSize);
	}
	gts->fw_request_status = 1;
	release_firmware(fw);
	if (ret != 0)
		return ret;

	return size;
}

int st_proc_strequestfw_show(struct seq_file *m, void *v) {	
	return 0;
}

int  st_proc_strequestfw_open(struct inode *inode, struct file *filp)
{		
	return single_open(filp, st_proc_strequestfw_show, NULL);
}

static ssize_t st_proc_stupgrade_write(struct file *file, const char *buff, size_t size, loff_t *ppos)
{
#ifdef SITRONIX_TP_WITH_FLASH
	char cmd[10] = { 0 };	

	if (0 < sitronix_copycmd_without_line(buff, cmd, size)) {
		sterr("copy data from user space, failed\n");
		return -EINVAL;
	}

	if(strcmp(cmd, "1") == 0) {
		stmsg("flash_powerful_upgrade!\n");
		gts->flash_powerful_upgrade = 1;
	}
#endif
	mutex_lock(&gts->mutex);
	gts->upgrade_result = sitronix_do_upgrade();
	mutex_unlock(&gts->mutex);
#ifdef SITRONIX_TP_WITH_FLASH
	gts->flash_powerful_upgrade = 0;
#endif /* SITRONIX_TP_WITH_FLASH */
	if (gts->upgrade_result < 0)
		return gts->upgrade_result;

	return size;
}

int st_proc_stupgrade_show(struct seq_file *m, void *v) {	
	seq_printf(m, "upgrade result = %s\nmessage: %s\n", gts->upgrade_result >= 0 ? "success" : "fail", gts->upgrade_msg);
	return 0;
}

int  st_proc_stupgrade_open(struct inode *inode, struct file *filp)
{		
	return single_open(filp, st_proc_stupgrade_show, NULL);
}


int st_proc_stselftest_show(struct seq_file *m, void *v) {	
	
	sitronix_ts_irq_enable(gts, false);
	
	gts->upgrade_doing = true;
	mutex_lock(&gts->mutex);
	gts->self_test_result = st_self_test();
	mutex_unlock(&gts->mutex);
	
	sitronix_ts_mt_reset_process();
	gts->upgrade_doing = false;
	
	//Enable IRQ
	sitronix_ts_irq_enable(gts, true);
	
	seq_printf(m, "Sitronix Self Test Result = %d ( 0 == success, >0 failed sensor number, < 0 err) \n", gts->self_test_result);
	return 0;
}

int  st_proc_stselftest_open(struct inode *inode, struct file *filp)
{		
	return single_open(filp, st_proc_stselftest_show, NULL);
}


static ssize_t st_proc_stcmdio_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int s, e, i;
	char tmp[10] = {0};
	int index = 0;
	int rwflag = 0;
	int iotype = 0;
	int addr_h = 0;
	int addr_l = 0;
	int data = 0;
	int dlen = 0;
	int ret = 0;

	gts->cmdio_result = 0;
	stmsg("input: %s", buf);
	s = 0;
	for (i = 0; i < count; i++) {
		if (buf[i] == ' ') {
			e = i;
			memset(tmp, 0, sizeof(tmp));
			strlcpy(tmp, buf + s, e-s+1);

			if (index == 0) {
				rwflag = st_formatinput(tmp, strlen(tmp), index);
				stdbg("write or read %d\n", rwflag);
				if (rwflag == 0 || rwflag == 1) {
					index++;
				} else {
					sterr("write or read must be 0 or 1\n");
					strlcpy(gts->cmdio_msg, "write or read must be 0 or 1", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else if (index == 1) {
				iotype = st_formatinput(tmp, strlen(tmp), index);
				stdbg("io type %d\n", iotype);
				if (iotype >= 0 && iotype <= 6) {
					index++;
				} else {
					sterr("io type must be 0 to 6\n");
					strlcpy(gts->cmdio_msg, "io type must be 0 to 6", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else if (index == 2) {
				addr_h = st_formatinput(tmp, strlen(tmp), index);
				stdbg("addr_h 0x%x\n", addr_h);
				if (addr_h >= 0)
					index++;
				else {
					strlcpy(gts->cmdio_msg, "parser addr_h fail", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else if (index == 3) {
				addr_l = st_formatinput(tmp, strlen(tmp), index);
				stdbg("addr_l 0x%x\n", addr_l);
				if (addr_l >= 0)
					index++;
				else {
					strlcpy(gts->cmdio_msg, "parser addr_l fail", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else {
				/* data or read len */
				data = st_formatinput(tmp, strlen(tmp), index);
				stdbg("data 0x%x\n", data);
				if (data >= 0) {
					index++;
					wbuf[dlen++] = data;
				} else {
					strlcpy(gts->cmdio_msg, "parser data fail", sizeof(gts->cmdio_msg));
					return -EIO;
				}

			}


			/* stmsg("%d,%s\n" , i, tmp); */
			s = i+1;
		}
	}
	memset(tmp, 0, sizeof(tmp));
	strlcpy(tmp, buf + s, count-1-s+1);

	data = st_formatinput(tmp, strlen(tmp), index);
	stdbg("data 0x%x\n", data);
	if (data >= 0) {
		index++;
		wbuf[dlen++] = data;
	}

	if (rwflag == 1)
		dlen = (wbuf[0] << 8) + wbuf[1];

	stmsg("rwflag = %d , io type = %d , addr = 0x%02X%02X , data len = 0x%04X , data = %x\n", rwflag, iotype, addr_h, addr_l, dlen, wbuf[0]);
	if (rwflag == 0 && (iotype == 3 || iotype == 4) && (dlen%2) == 1) {
		sterr("io type 3 and 4 must write even number of data\n");
		strlcpy(gts->cmdio_msg, "io type 3 and 4 must write even number of data", sizeof(gts->cmdio_msg));
		return -EIO;
	}

	gts->cmdio_addr[0] = (u16)((addr_h<<8) + addr_l);
	gts->cmdio_rwflag = rwflag;
	gts->cmdio_len = dlen;
	gts->cmdio_iotype = iotype;

	mutex_lock(&gts->mutex);

	if (rwflag == 1 && iotype == 6)
		ret = sitronix_ts_reg_read(gts, gts->cmdio_addr[0], rbuf, gts->cmdio_len);
	else if (rwflag == 0 && iotype == 6)
		ret = sitronix_ts_reg_write(gts, gts->cmdio_addr[0], wbuf, gts->cmdio_len);
	else if (rwflag == 1 && iotype != 6)
		ret = TDU_CmdioRead(gts->cmdio_iotype, gts->cmdio_addr[0], rbuf, gts->cmdio_len);
	else if (rwflag == 0 && iotype != 6)
		ret = TDU_CmdioWrite(gts->cmdio_iotype, gts->cmdio_addr[0], wbuf, gts->cmdio_len);

	mutex_unlock(&gts->mutex);
	if (ret < 0) {
		strlcpy(gts->cmdio_msg, "I2C/SPI error" , sizeof(gts->cmdio_msg));
		return ret;
	}

	gts->cmdio_result = 1;

	return count;
}

int st_proc_stcmdio_show(struct seq_file *m, void *v) {	
	
	int i;

	if (gts->cmdio_result == 0) {
		seq_printf(m, "cmdio fail\nmesage = %s\n", gts->cmdio_msg);
	} else {
		if (gts->cmdio_rwflag == 0)
			seq_printf(m, "Write , io type = %d , addr = 0x%04X , data len = 0x%04X OK\n", gts->cmdio_iotype, gts->cmdio_addr[0], gts->cmdio_len);
		else {
			seq_printf(m, "Data :");
			for (i = 0; i < gts->cmdio_len; i++)
				seq_printf(m, " %02X", rbuf[i]);

			seq_printf(m, "\n");
		}
	}
	
	return 0;
}

int  st_proc_stcmdio_open(struct inode *inode, struct file *filp)
{		
	return single_open(filp, st_proc_stcmdio_show, NULL);
}

static ssize_t st_proc_stfwinfo_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int s, e, i;
	char tmp[10] = {0};
	int index = 0;
	int rwflag = 0;
	int iotype = 0;
	int data = 0;
	int dlen = 0;
	int ret = 0;

	gts->cmdio_result = 0;
	stmsg("input: %s", buf);
	s = 0;
	for (i = 0; i < count; i++) {
		if (buf[i] == ' ') {
			e = i;
			memset(tmp, 0, sizeof(tmp));
			strlcpy(tmp, buf + s, e-s+1);

			if (index == 0) {
				rwflag = st_formatinput(tmp, strlen(tmp), index);
				stdbg("write or read %d\n", rwflag);
				if (rwflag == 0 || rwflag == 1) {
					index++;
				} else {
					sterr("write or read must be 0 or 1\n");
					strlcpy(gts->cmdio_msg, "write or read must be 0 or 1", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else if (index == 1) {
				iotype = st_formatinput(tmp, strlen(tmp), index);
				stdbg("info ID %d\n", iotype);
				index++;
			} else {
				/* data or read len */
				data = st_formatinput(tmp, strlen(tmp), index);
				stdbg("data 0x%x\n", data);
				if (data >= 0) {
					index++;
					wbuf[dlen++] = data;
				} else {
					strlcpy(gts->cmdio_msg, "parser data fail", sizeof(gts->cmdio_msg));
					return -EIO;
				}

			}


			/* stmsg("%d,%s\n" , i, tmp); */
			s = i+1;
		}
	}
	memset(tmp, 0, sizeof(tmp));
	strlcpy(tmp, buf + s, count-1-s+1);

	data = st_formatinput(tmp, strlen(tmp), index);
	stdbg("data 0x%x\n", data);
	if (data >= 0) {
		index++;
		wbuf[dlen++] = data;
	}

	if (rwflag == 1)
		dlen = (wbuf[0] << 8) + wbuf[1];

	stmsg("rwflag = %d , info ID = %d , data len = 0x%04X , data = %x\n", rwflag, iotype, dlen, wbuf[0]);	
	
	gts->cmdio_rwflag = rwflag;
	gts->cmdio_len = dlen;
	gts->cmdio_iotype = iotype;

	mutex_lock(&gts->mutex);

	ret = TDU_FWInfoRead(gts->cmdio_iotype, rbuf, gts->cmdio_len);

	mutex_unlock(&gts->mutex);
	if (ret < 0) {
		strlcpy(gts->cmdio_msg, "I2C/SPI error" , sizeof(gts->cmdio_msg));
		return ret;
	}

	gts->cmdio_result = 1;
	return count;
}

int st_proc_stfwinfo_show(struct seq_file *m, void *v) {
	int i;

	if (gts->cmdio_result == 0) {
		seq_printf(m, "fwinfo fail\nmesage = %s\n", gts->cmdio_msg);
	} else {
		if (gts->cmdio_rwflag == 0)
			seq_printf(m, "Write , info ID = %d , data len = 0x%04X OK\n", gts->cmdio_iotype, gts->cmdio_len);
		else {
			seq_printf(m, "Data :");
			for (i = 0; i < gts->cmdio_len; i++)
				seq_printf(m, " %02X", rbuf[i]);
			seq_printf(m, "\n");
		}
	}

	return 0;
}

int  st_proc_stfwinfo_open(struct inode *inode, struct file *filp)
{		
	return single_open(filp, st_proc_stfwinfo_show, NULL);
}

static ssize_t st_proc_stdrivercmd_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int s, e, i;
	char tmp[10] = {0};
	int index = 0;
	int rwflag = 0;
	int addr = 0;
	int data = 0;
	int dlen = 0;
	int ret = 0;

	gts->cmdio_result = 0;
	stmsg("input: %s", buf);
	s = 0;
	for (i = 0; i < count; i++) {
		if (buf[i] == ' ') {
			e = i;
			memset(tmp, 0, sizeof(tmp));
			strlcpy(tmp, buf + s, e-s+1);

			if (index == 0) {
				rwflag = st_formatinput(tmp, strlen(tmp), index);
				stdbg("write or read %d\n", rwflag);
				if (rwflag == 0 || rwflag == 1) {
					index++;
				} else {
					sterr("write or read must be 0 or 1\n");
					strlcpy(gts->cmdio_msg, "write or read must be 0 or 1", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else if (index == 1) {
				addr = st_formatinput(tmp, strlen(tmp), index);
				stdbg("addr 0x%x\n", addr);
				if (addr >= 0)
					index++;
				else {
					strlcpy(gts->cmdio_msg, "parser addr fail", sizeof(gts->cmdio_msg));
					return -EIO;
				}
			} else {
				/* data or read len */
				data = st_formatinput(tmp, strlen(tmp), index);
				stdbg("data 0x%x\n", data);
				if (data >= 0) {
					index++;
					wbuf[dlen++] = data;
				} else {
					strlcpy(gts->cmdio_msg, "parser data fail", sizeof(gts->cmdio_msg));
					return -EIO;
				}

			}


			/* stmsg("%d,%s\n" , i, tmp); */
			s = i+1;
		}
	}
	memset(tmp, 0, sizeof(tmp));
	strlcpy(tmp, buf + s, count-1-s+1);

	data = st_formatinput(tmp, strlen(tmp), index);
	stdbg("data 0x%x\n", data);
	if (data >= 0) {
		index++;
		wbuf[dlen++] = data;
	}

	if (rwflag == 1)
		dlen = wbuf[0];

	stmsg("rwflag = %d , addr = 0x%02X , data len = 0x%04X , data = %x\n", rwflag, addr, dlen, wbuf[0]);
	if (rwflag == 0 && (dlen%2) == 1) {
		sterr("driver cmd must write even number of data\n");
		strlcpy(gts->cmdio_msg, "driver cmd must write even number of data", sizeof(gts->cmdio_msg));
		return -EIO;
	}

	gts->cmdio_addr[0] = (u16) addr;
	gts->cmdio_rwflag = rwflag;
	gts->cmdio_len = dlen;	

	mutex_lock(&gts->mutex);

	if (rwflag == 1 )
		ret = sitronix_read_driver_cmd(gts->cmdio_addr[0], rbuf, gts->cmdio_len);
	else
		ret = sitronix_write_driver_cmd(gts->cmdio_addr[0], wbuf, gts->cmdio_len);

	mutex_unlock(&gts->mutex);
	if (ret < 0) {
		strlcpy(gts->cmdio_msg, "I2C/SPI error" , sizeof(gts->cmdio_msg));
		return ret;
	}

	gts->cmdio_result = 1;
	return count;
}

int st_proc_stdrivercmd_show(struct seq_file *m, void *v) {
	int i;

	if (gts->cmdio_result == 0) {
		seq_printf(m, "driver cmd fail\nmesage = %s\n", gts->cmdio_msg);
	} else {
		if (gts->cmdio_rwflag == 0)
			seq_printf(m, "Write , addr = 0x%04X , data len = 0x%04X OK\n", gts->cmdio_addr[0], gts->cmdio_len);
		else {
			seq_printf(m, "Data :");
			for (i = 0; i < gts->cmdio_len; i++)
				seq_printf(m, " %02X", rbuf[i]);
			seq_printf(m, "\n");
		}
	}

	return 0;
}

int  st_proc_stdrivercmd_open(struct inode *inode, struct file *filp)
{		
	return single_open(filp, st_proc_stdrivercmd_show, NULL);
}

int st_proc_stswu_show(struct seq_file *m, void *v) {
	
	sitronix_ts_report_swu(gts->input_dev, ST_SWU_GESTURE_DOUBLECLICK);
	seq_printf(m, "swu ok\n");

	return 0;
}

int  st_proc_stswu_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, st_proc_stswu_show, NULL);
}

static ssize_t st_proc_stproximity_write(struct file *file, const char *buff, size_t size, loff_t *ppos)
{
	char cmd[10] = { 0 };	

	if (!gts->is_support_proximity) {
		sterr("Don't support proximity!\n");
		return size;
	}

	if (0 < sitronix_copycmd_without_line(buff, cmd, size)) {
		sterr("copy data from user space, failed\n");
		return -EINVAL;
	}

	mutex_lock(&gts->mutex);
	if (strcmp(cmd, "1") == 0)
		sitronix_ts_proximity_enable(gts, true);
	else if(strcmp(cmd, "0") == 0)
		sitronix_ts_proximity_enable(gts, false);
	else if(strcmp(cmd, "2") == 0 && gts->proximity_flag && gts->in_suspend) {
		stmsg("Enable Proximity Sensing in suspend\n");
		sitronix_ts_proximity_control_sensing(gts, true);
	}
		
	mutex_unlock(&gts->mutex);
	return size;
}

int st_proc_stproximity_show(struct seq_file *m, void *v) {	
	seq_printf(m, "proximity_flag = %s\n", gts->proximity_flag ? "true" : "false");
	return 0;
}

int  st_proc_stproximity_open(struct inode *inode, struct file *filp)
{		
	return single_open(filp, st_proc_stproximity_show, NULL);
}

struct file_operations proc_swu_fops = {
	.write = sitronix_proc_swu_write,
	.read = sitronix_proc_swu_read,
};

struct file_operations proc_glove_fops = {
	.write = sitronix_proc_glove_write,
	.read = sitronix_proc_glove_read,
};

struct file_operations proc_charge_fops = {
	.write = sitronix_proc_charge_write,
	.read = sitronix_proc_charge_read,
};

struct file_operations proc_jittersuppress_fops = {
	.write = sitronix_proc_jittersuppress_write,
	.read = sitronix_proc_jittersuppress_read,
};

struct file_operations proc_palm_fops = {
	.write = sitronix_proc_palm_write,
	.read = sitronix_proc_palm_read,
};

struct file_operations proc_headphone_fops = {
	.write = sitronix_proc_headphone_write,
	.read = sitronix_proc_headphone_read,
};

struct file_operations proc_grip_fops = {
	.write = sitronix_proc_grip_write,
	.read = sitronix_proc_grip_read,
};

struct file_operations proc_sticinfo_fops = {	
	.owner 		= THIS_MODULE,
	.read 		= seq_read,
	.open		= st_proc_sticinfo_open,
	.release	= single_release,
};

struct file_operations proc_stfwver_fops = {	
	.owner 		= THIS_MODULE,
	.read 		= seq_read,
	.open		= st_proc_stfwver_open,
	.release	= single_release,
};

struct file_operations proc_stmt_fops = {	
	.owner 		= THIS_MODULE,
	.write 		= st_proc_stmt_write,
	.read 		= seq_read,
	.open		= st_proc_stmt_open,
	.release	= single_release,
};

struct file_operations proc_strequestfw_fops = {	
	.owner 		= THIS_MODULE,
	.write 		= st_proc_strequestfw_write,
	.read 		= seq_read,
	.open		= st_proc_strequestfw_open,
	.release	= single_release,
};

struct file_operations proc_stupgrade_fops = {	
	.owner 		= THIS_MODULE,
	.write 		= st_proc_stupgrade_write,
	.read 		= seq_read,
	.open		= st_proc_stupgrade_open,
	.release	= single_release,
};

struct file_operations proc_stselftest_fops = {	
	.owner 		= THIS_MODULE,
	.read 		= seq_read,
	.open		= st_proc_stselftest_open,
	.release	= single_release,
};


struct file_operations proc_stcmdio_fops = {	
	.owner 		= THIS_MODULE,
	.write 		= st_proc_stcmdio_write,
	.read 		= seq_read,
	.open		= st_proc_stcmdio_open,
	.release	= single_release,
};

struct file_operations proc_stfwinfo_fops = {	
	.owner 		= THIS_MODULE,
	.write 		= st_proc_stfwinfo_write,
	.read 		= seq_read,
	.open		= st_proc_stfwinfo_open,
	.release	= single_release,
};

struct file_operations proc_stdrivercmd_fops = {	
	.owner 		= THIS_MODULE,
	.write 		= st_proc_stdrivercmd_write,
	.read 		= seq_read,
	.open		= st_proc_stdrivercmd_open,
	.release	= single_release,
};

struct file_operations proc_stswu_fops = {	
	.owner 		= THIS_MODULE,
	.read 		= seq_read,
	.open		= st_proc_stswu_open,
	.release	= single_release,
};

struct file_operations proc_stproximity_fops = {	
	.owner 		= THIS_MODULE,
	.write 		= st_proc_stproximity_write,
	.read 		= seq_read,
	.open		= st_proc_stproximity_open,
	.release	= single_release,
};

sitronix_proc_node_t sitronix_proc_table[] = {
	{"swu", NULL, &proc_swu_fops, true, "on", "off"},
	{"glove", NULL, &proc_glove_fops, true, "on", "off"},
	{"charge", NULL, &proc_charge_fops, true, "on", "off"},
	{"jittersuppress", NULL, &proc_jittersuppress_fops, true, "on", "off"},
	{"palm", NULL, &proc_palm_fops, true, "on", "off"},
	{"headphone", NULL, &proc_headphone_fops, true, "on", "off"},
	{"grip", NULL, &proc_grip_fops, true, "on", "off", "0", "90", "180", "270"},
	{"sticinfo", NULL, &proc_sticinfo_fops},
	{"stfwver", NULL, &proc_stfwver_fops},
	{"stmt", NULL, &proc_stmt_fops},
	{"strequestfw", NULL, &proc_strequestfw_fops},
	{"stupgrade", NULL, &proc_stupgrade_fops},
	{"stselftest", NULL, &proc_stselftest_fops},
	{"stcmdio", NULL, &proc_stcmdio_fops},
	{"stfwinfo", NULL, &proc_stfwinfo_fops},
	{"stdrivercmd", NULL, &proc_stdrivercmd_fops},
	{"stswu", NULL, &proc_stswu_fops},
	{"stproximity", NULL, &proc_stproximity_fops},
};

struct proc_dir_entry *proc_dir_sitronix;

/* DRV added by wangwei1, gesture mode, start */
#if defined(CONFIG_TOUCHSCREEN_SITRONIX_GESTURE_MODE)
bool sitronix_is_gesture_wakeup_enabled(void)
{
	if (((IS_ERR(gts)) ? 1 : 0) || (gts == NULL) ) {
		stmsg("Failed to allocate ts memory, gts is NULL.\n");
		return false;
	}

	stmsg("sitronix gesture status = %d\n", gts->mode_flag[ST_MODE_SWU]);

    return gts->mode_flag[ST_MODE_SWU];
}
EXPORT_SYMBOL_GPL(sitronix_is_gesture_wakeup_enabled);

static ssize_t sitronix_gesture_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;

    count = snprintf(buf, PAGE_SIZE, "Gesture Mode:%s\n",
                      gts->mode_flag[ST_MODE_SWU] ? "On" : "Off");

    return count;
}

static ssize_t sitronix_gesture_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    if (buf[0] == '1') {
        stmsg("sitronix gesture enable\n");
		sitronix_mode_switch(ST_MODE_SWU, true);
    } else if (buf[0] == '0') {
        stmsg("sitronix gesture disable\n");
		sitronix_mode_switch(ST_MODE_SWU, false);
    }

    return count;
}
#endif

#if defined(CONFIG_TOUCHSCREEN_SITRONIX_GLOVE_MODE)
static ssize_t sitronix_glove_mode_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;

    count = snprintf(buf, PAGE_SIZE, "Glove Mode:%s\n",
                     gts->mode_flag[ST_MODE_GLOVE] ? "On" : "Off");

    return count;
}

static ssize_t sitronix_glove_mode_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    if (buf[0] == '1') {
		stmsg("sitronix glove enable\n");
		sitronix_mode_switch(ST_MODE_GLOVE, true);
    } else if (buf[0] == '0') {
		stmsg("sitronix glove enable\n");
        sitronix_mode_switch(ST_MODE_GLOVE, false);

    }

    return count;
}
#endif

#if defined(CONFIG_TOUCHSCREEN_SITRONIX_GESTURE_MODE)
/* sysfs gesture node
 *   read example: cat  sitronix_gesture_mode       ---read gesture mode
 *   write example:echo 1 > sitronix_gesture_mode   --- write gesture mode to 1
 *
 */
static DEVICE_ATTR(gesture, S_IRUGO | S_IWUSR, sitronix_gesture_show,
                   sitronix_gesture_store);
#endif

#if defined(CONFIG_TOUCHSCREEN_SITRONIX_GLOVE_MODE)
/* sysfs glove node
 * read example: cat sitronix_glove_mode        ---read  glove mode
 * write example:echo 1 > sitronix_glove_mode   ---write glove mode to 01
 */
static DEVICE_ATTR(state, S_IRUGO | S_IWUSR,
                   sitronix_glove_mode_show, sitronix_glove_mode_store);
#endif


#if defined(CONFIG_TOUCHSCREEN_SITRONIX_GESTURE_MODE)
int sitronix_gesture_sysfs_add(struct platform_device *pdev)
{
	int err = 0;
    stmsg("Add device attr groups, sitronix_gesture_sysfs_add\n");

	err = device_create_file(&pdev->dev, &dev_attr_gesture);
	if (err) {
        sterr("sys file creation failed\n");
        return -ENODEV;
	}
	return 0;
}

static const struct of_device_id gesture_of_match[] = {
	{.compatible = "mediatek,gesture",},
	{},
};
MODULE_DEVICE_TABLE(of, gesture_of_match);

struct sitronix_ts_data *g_sitronix_data = NULL;

static int gesture_probe(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, g_sitronix_data);
	sitronix_gesture_sysfs_add(pdev);

	return 0;
}

static struct platform_driver gesture_driver = {
	.probe = gesture_probe,
	.driver = {
		.name = "common_node",
		.of_match_table = gesture_of_match,
	},
};

int sitronix_gesture_init(void)
{
	return platform_driver_register(&gesture_driver);
}
#endif
/* DRV added by wangwei1, gesture mode, end */

int sitronix_create_proc(void)
{
	int i = 0, ret = 0;

	/* DRV added by wangwei1, glove mode, start */
#if defined(CONFIG_TOUCHSCREEN_SITRONIX_GLOVE_MODE)
	static struct kobject *sysfs_rootdir = NULL;
	struct kobject *drv_glove = NULL;
	int err = 0;
	struct sitronix_ts_i2c_data *i2c_data = (struct sitronix_ts_i2c_data *)gts->host_if->if_data;

	if (!sysfs_rootdir) {
		// this kobject is shared between modules, do not free it when error occur
		sysfs_rootdir = kobject_create_and_add("prize", kernel_kobj);
	}

	if (!drv_glove)
		drv_glove = kobject_create_and_add("smartcover", sysfs_rootdir);

	err = sysfs_create_link(drv_glove, &i2c_data->client->dev.kobj, "common_node");
	if (err) {
		sterr("prize ilitek sysfs_create_link fail\n");
		-ENODEV;
	}
	if (sysfs_create_file(&i2c_data->client->dev.kobj, &dev_attr_state.attr)) {
		-ENODEV;
	}
#endif
	/* DRV added by wangwei1, glove mode, end */

#ifdef SITRONIX_PROC_DIR_CREATE
	proc_dir_sitronix = proc_mkdir(SITRONIX_PROC_DIR_NAME, NULL);
#else
	proc_dir_sitronix = NULL;
#endif /* SITRONIX_PROC_DIR_CREATE */

	for (i = 0; i < ARRAY_SIZE(sitronix_proc_table); i++) {
		sitronix_proc_table[i].node = proc_create(sitronix_proc_table[i].name, 0666, proc_dir_sitronix, sitronix_proc_table[i].fops);

		if (sitronix_proc_table[i].node == NULL) {
			sitronix_proc_table[i].isCreated = false;
			sterr("Failed to create %s under /proc\n", sitronix_proc_table[i].name);
			ret = -ENODEV;
		} else {
			sitronix_proc_table[i].isCreated = true;
			stdbg("Succeed to create %s under /proc\n", sitronix_proc_table[i].name);
		}
	}

	/* DRV added by wangwei1, gesture mode, start */
#if defined(CONFIG_TOUCHSCREEN_SITRONIX_GESTURE_MODE)
	g_sitronix_data = gts;
	sitronix_gesture_init();
#endif
	/* DRV added by wangwei1, gesture mode, end */

	return ret;
}
EXPORT_SYMBOL(sitronix_create_proc);

void sitronix_remove_proc(void)
{
	int i = 0;

	for (; i < ARRAY_SIZE(sitronix_proc_table); i++) {
		if (sitronix_proc_table[i].isCreated == true) {
			stdbg("Removed %s under /proc\n", sitronix_proc_table[i].name);
			remove_proc_entry(sitronix_proc_table[i].name, proc_dir_sitronix);
		}
	}
#ifdef SITRONIX_PROC_DIR_CREATE
	remove_proc_entry(SITRONIX_PROC_DIR_NAME, NULL);
#endif /* SITRONIX_PROC_DIR_CREATE */
}
EXPORT_SYMBOL(sitronix_remove_proc);
