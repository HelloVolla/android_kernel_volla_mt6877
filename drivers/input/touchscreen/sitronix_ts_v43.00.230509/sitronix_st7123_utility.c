#include "sitronix_ts.h"
#include "sitronix_st7123.h"

/*drv-20230519-wangwei1 for TP info start */
#if defined(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/hardware_info/hardware_info.h"
extern struct hardware_info current_tp_info;
#endif
/*drv-20230519-wangwei1 for TP info end */

int sitronix_ts_get_device_status(struct sitronix_ts_data *ts_data)
{
	int ret;
	uint8_t buf[8];

	ret = sitronix_ts_reg_read(ts_data, STATUS_REG, buf, sizeof(buf));
	if (ret < 0) {
		sterr("%s: Read Status register error!(%d)\n", __func__, ret);
		return ret;
	}

	stmsg("buf = %X, %X, %X, %X, %X, %X, %X, %X\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
	stmsg("Status register = %d.\n", (buf[0] & 0xFF));

	return (int)(buf[0] & 0x0F);
}

int sitronix_ts_get_fw_revision(struct sitronix_ts_data *ts_data)
{
	int ret = 0;
	uint8_t buf[4];

	ret = sitronix_ts_reg_read(ts_data, FIRMWARE_VERSION, buf, 1);
	if (ret < 0) {
		sterr("%s: Read FW Version error!(%d)\n", __func__, ret);
		return ret;
	}

	ts_data->ts_dev_info.fw_version = buf[0];
	stmsg("FW Version (hex) = %x\n", buf[0]);

/*drv-20230531-wangwei1 for TP info start */
#if defined(CONFIG_PRIZE_HARDWARE_INFO)
	sprintf(current_tp_info.chip,"st7123,FW_VER:0x%2x",buf[0]);
	sprintf(current_tp_info.id,"0x7123");
	strcpy(current_tp_info.vendor,"sitronix");
	strcpy(current_tp_info.more,"720*1600");
#endif
/*drv-20230531-wangwei1 for TP info end */

	ret = sitronix_ts_reg_read(ts_data, FIRMWARE_REVISION_3, buf, sizeof(buf));
	if (ret < 0) {
		sterr("%s: Read FW revision error!(%d)\n", __func__, ret);
		return ret;
	}

	memcpy(&ts_data->ts_dev_info.fw_revision, buf, 4);
	stmsg("FW revision (hex) = %x %x %x %x\n", buf[0], buf[1], buf[2], buf[3]);

	return 0;
}

int sitronix_ts_get_max_touches(struct sitronix_ts_data *ts_data)
{
	int ret = 0;
	uint8_t max_touches;

	ret = sitronix_ts_reg_read(ts_data, MAX_NUM_TOUCHES, &max_touches, sizeof(max_touches));
	if (ret < 0) {
		sterr("%s: Read max touches error!(%d)\n", __func__, ret);
		return ret;
	}

	ts_data->ts_dev_info.max_touches = max_touches;
	stmsg("Max touches = %d.\n", ts_data->ts_dev_info.max_touches);

	return 0;
}

int sitronix_ts_get_chip_id(struct sitronix_ts_data *ts_data)
{
	int ret = 0;
	uint8_t chip_id;

	ret = sitronix_ts_reg_read(ts_data, CHIP_ID, &chip_id, sizeof(chip_id));
	if (ret < 0) {
		sterr("%s: Read chip ID error!(%d)\n", __func__, ret);
		return ret;
	}

	ts_data->ts_dev_info.chip_id = chip_id;
	stmsg("Chip ID = 0x%X.\n", ts_data->ts_dev_info.chip_id);

	return 0;
}

int sitronix_ts_get_xy_chs(struct sitronix_ts_data *ts_data)
{
	int ret = 0;
	uint8_t buf[5];

	ret = TDU_FWInfoRead(2, buf, sizeof(buf));	
	if (ret < 0) {
		sterr("%s: Read XY channels error!(%d)\n", __func__, ret);
		//return ret;
	}

	ts_data->ts_dev_info.x_chs = buf[2];
	ts_data->ts_dev_info.y_chs = buf[3];
	stmsg("X_chs = %d.\n", ts_data->ts_dev_info.x_chs);
	stmsg("Y_chs = %d.\n", ts_data->ts_dev_info.y_chs);

	return 0;
}

int sitronix_ts_get_resolution(struct sitronix_ts_data *ts_data)
{
	int ret = 0;
	uint8_t buf[4];

	ret = sitronix_ts_reg_read(ts_data, X_RESOLUTION_HIGH, buf, sizeof(buf));
	if (ret < 0) {
		sterr("%s: Read resolution error!(%d)\n", __func__, ret);
		return ret;
	}

	ts_data->ts_dev_info.x_res = (((uint16_t)buf[0] & 0x3F) << 8) | buf[1];
	ts_data->ts_dev_info.y_res = (((uint16_t)buf[2] & 0x3F) << 8) | buf[3];
	stmsg("Resolution = %u x %u\n", ts_data->ts_dev_info.x_res, ts_data->ts_dev_info.y_res);

	return 0;
}

int sitronix_ts_get_customer_info(struct sitronix_ts_data *ts_data)
{
	int ret = 0;
	uint8_t buf[4];
	uint8_t bank;

	ret = sitronix_ts_reg_read(ts_data, MISC_CONTROL, buf, 1);
	if (ret < 0) {
		sterr("%s: Read MISC_CONTROL error!(%d)\n", __func__, ret);
		return ret;
	}

	bank = buf[0];	
	buf[0] = (buf[0] & 0xFC) | 1;
	ret = sitronix_ts_reg_write(ts_data, MISC_CONTROL, buf, 1);

	ret = sitronix_ts_reg_read(ts_data, FIRMWARE_REVISION_3, buf, sizeof(buf));
	memcpy(&ts_data->ts_dev_info.customer_info, buf, 4);
	stmsg("Customer Info (hex) = %x %x %x %x\n", buf[0], buf[1], buf[2], buf[3]);

	buf[0] = bank;
	ret = sitronix_ts_reg_write(ts_data, MISC_CONTROL, buf, 1);
	return 0;
}


int sitronix_ts_get_misc_info(struct sitronix_ts_data *ts_data)
{
	int ret = 0;
	uint8_t misc_info;

	ts_data->swu_flag = false;
	ts_data->is_support_proximity = false;
	ret = sitronix_ts_reg_read(ts_data, MISC_INFO, &misc_info, sizeof(misc_info));
	if (ret < 0) {
		sterr("%s: Read Misc. Info error!(%d)\n", __func__, ret);
		return ret;
	}

	ts_data->ts_dev_info.misc_info = misc_info;
	ts_data->swu_flag = (misc_info & ST_MISC_INFO_SWU_FLAG);
	ts_data->is_support_proximity = (misc_info & ST_MISC_INFO_PROX_FLAG);

#if SPRD_SYSFS_SUSPEND_RESUME
	ts_data->swu_flag = false;
#endif /*SPRD_SYSFS_SUSPEND_RESUME*/
#ifndef SITRONIX_SUPPORT_SWU
	ts_data->swu_flag = false;
#endif /*SITRONIX_SUPPORT_SWU*/
#ifndef SITRONIX_SUPPORT_PROXIMITY
	ts_data->is_support_proximity = false;
#endif /*SITRONIX_SUPPORT_PROXIMITY*/
	ts_data->mode_flag[ST_MODE_SWU] = ts_data->swu_flag;
	stmsg("Misc. Info = 0x%X.\n", ts_data->ts_dev_info.misc_info);

	return 0;
}

int sitronix_ts_get_device_info(struct sitronix_ts_data *ts_data)
{
	int ret;

	ret = sitronix_ts_get_fw_revision(ts_data);
	if (ret)
		return ret;
	ret = sitronix_ts_get_customer_info(ts_data);
	if (ret)
		return ret;
	ret = sitronix_ts_get_resolution(ts_data);
	if (ret)
		return ret;
	ret = sitronix_ts_get_max_touches(ts_data);
	if (ret)
		return ret;
	ret = sitronix_ts_get_chip_id(ts_data);
	if (ret)
		return ret;
	ret = sitronix_ts_get_xy_chs(ts_data);
	if (ret)
		return ret;
	ret = sitronix_ts_get_misc_info(ts_data);
	if (ret)
		return ret;

	return 0;
}

int sitronix_ts_set_smart_wake_up(struct sitronix_ts_data *ts_data, bool enable)
{
	int ret = 0;
	uint8_t ctrl;

	ret = sitronix_ts_reg_read(ts_data, MISC_CONTROL, &ctrl, sizeof(ctrl));
	if (ret < 0) {
		sterr("%s: Read SWU error!(%d)\n", __func__, ret);
		return ret;
	}

	stmsg("%s: Misc. Control=0x%X. enable=%u\n", __func__, ctrl, (uint32_t)enable);

	if (enable)
		ctrl |= ST_MISC_INFO_SWU_FLAG;
	else
		ctrl &= ~ST_MISC_INFO_SWU_FLAG;

	ret = sitronix_ts_reg_write(ts_data, MISC_CONTROL, &ctrl, sizeof(ctrl));
	if (ret < 0) {
		sterr("%s: Set SWU %s error!(%d)\n",
					__func__, (enable ? "enabled" : "disabled"), ret);
		return ret;
	}

	ret = sitronix_ts_reg_read(ts_data, MISC_CONTROL, &ctrl, sizeof(ctrl));
	stdbg("%s:%d Misc. Control=0x%X. enable=%u\n", __func__, __LINE__, ctrl, (uint32_t)enable);

	return 0;
}

int sitronix_ts_get_swu_gesture(struct sitronix_ts_data *ts_data)
{
	int ret = 0;
	uint8_t swu_id;

	ret = sitronix_ts_reg_read(ts_data, GESTURES, &swu_id, sizeof(swu_id));
	if (ret < 0) {
		sterr("%s: Read SWU ID error!(%d)\n", __func__, ret);
		return ret;
	}

	ts_data->swu_gesture_id = swu_id;

	stmsg("%s: SWU ID = 0x%X.\n", __func__, swu_id);

	return 0;
}

int sitronix_ts_get_swu_keycode(uint8_t swu_id)
{
	int key = 0;

	switch (swu_id) {
	case ST_SWU_GESTURE_DOUBLECLICK:
	case ST_SWU_GESTURE_SINGLECLICK:
		key = ST_KEY_GESTURE_POWER;
		break;
	case ST_SWU_GESTURE_LEFT:
	case ST_SWU_GESTURE_ARROW_LEFT:
		key = ST_KEY_GESTURE_LEFT;
		break;
	case ST_SWU_GESTURE_RIGHT:
	case ST_SWU_GESTURE_ARROW_RIGHT:
		key = ST_KEY_GESTURE_RIGHT;
		break;
	case ST_SWU_GESTURE_UP:
	case ST_SWU_GESTURE_ARROW_UP:
		key = ST_KEY_GESTURE_UP;
		break;
	case ST_SWU_GESTURE_DOWN:
	case ST_SWU_GESTURE_ARROW_DOWN:
		key = ST_KEY_GESTURE_DOWN;
		break;
	case ST_SWU_GESTURE_TWO_FINGER_DOWN:
		key = ST_KEY_GESTURE_TWO_FINGER_DOWN;
		break;
	case ST_SWU_GESTURE_O:
		key = ST_KEY_GESTURE_O;
		break;
	case ST_SWU_GESTURE_W:
		key = ST_KEY_GESTURE_W;
		break;
	case ST_SWU_GESTURE_M:
		key = ST_KEY_GESTURE_M;
		break;
	case ST_SWU_GESTURE_E:
		key = ST_KEY_GESTURE_E;
		break;
	case ST_SWU_GESTURE_S:
		key = ST_KEY_GESTURE_S;
		break;
	case ST_SWU_GESTURE_V:
		key = ST_KEY_GESTURE_V;
		break;
	case ST_SWU_GESTURE_Z:
		key = ST_KEY_GESTURE_Z;
		break;
	case ST_SWU_GESTURE_C:
		key = ST_KEY_GESTURE_C;
		break;
	default:
		break;
	}

	return key;
}

int sitronix_ts_powerdown(struct sitronix_ts_data *ts_data, bool powerdown)
{
	int ret = 0;
	uint8_t ctrl;

	ret = sitronix_ts_reg_read(ts_data, DEVICE_CONTROL_REG, &ctrl, 1);
	if (ret < 0) {
		sterr("%s: Read device control status error! (%d)\n", __func__, ret);
		return ret;
	}

	ctrl = powerdown ? (ctrl | 0x02) : (ctrl & ~0x02);

	ret = sitronix_ts_reg_write(ts_data, DEVICE_CONTROL_REG, &ctrl, 1);
	if (ret < 0) {
		sterr("%s: Write power down error! (%d)\n", __func__, ret);
		return ret;
	}

	return 0;
}

int sitronix_ts_proximity_enable(struct sitronix_ts_data *ts_data, bool proximity_enable)
{
	int ret = 0;
	uint8_t ctrl;

	ret = sitronix_ts_reg_read(ts_data, DEVICE_CONTROL_REG, &ctrl, 1);
	if (ret < 0) {
		sterr("%s: Read device control status error! (%d)\n", __func__, ret);
		return ret;
	}

	ctrl = proximity_enable ? (ctrl | 0x20) : (ctrl & ~0x20);

	ret = sitronix_ts_reg_write(ts_data, DEVICE_CONTROL_REG, &ctrl, 1);
	if (ret < 0) {
		sterr("%s: Write proximity_enable error! (%d)\n", __func__, ret);
		return ret;
	}
	stmsg("sitronix_ts_proximity_enable %s\n",(proximity_enable?"On":"Off"));
	gts->proximity_flag = proximity_enable;

	return 0;
}

int sitronix_ts_proximity_control_sensing(struct sitronix_ts_data *ts_data, bool sensing_enable)
{
	int ret = 0;
	unsigned char buf[8] = {0};		
	stmsg("sitronix_ts_proximity_control_sensing : %d\n", sensing_enable);
	if (sensing_enable)
		sitronix_write_driver_cmd(0X11, buf, 0);
	else
		sitronix_write_driver_cmd(0X10, buf, 0);
	return ret;
}

struct CommandIoPacket {
unsigned char CmdID;
unsigned char ValidDataSize;
unsigned char CmdData[30];
};

void STChecksumCalculation(unsigned short *pChecksum, unsigned char *pInData, unsigned long Len)
{
	unsigned long i;
	unsigned char LowByteChecksum;

	for (i = 0; i < Len; i++) {
		*pChecksum += (unsigned short)pInData[i];
		LowByteChecksum = (unsigned char)(*pChecksum & 0xFF);
		LowByteChecksum = (LowByteChecksum) >> 7 | (LowByteChecksum) << 1;
		*pChecksum = (*pChecksum & 0xFF00) | LowByteChecksum;
	}
}

bool TDU_SetH2DReady(void)
{
	int ret;
	bool bRet = false;
	unsigned char buf[2];

	buf[0] = 0xF8;
	buf[1] = 0x01;

	/* ret = st_spi_write_bytes(0xF8, buf+1, 1); */
	ret = sitronix_ts_reg_write(gts, CMDIO_CONTROL, buf+1, 1);
	if (ret <= 0) {
		sterr("%s: write ready error.\n", __func__);
		bRet = false;
	} else{
		bRet = true;
	}
	return bRet;
}

bool TDU_GetH2DReady(void)
{
	bool bRet = false;
	unsigned char tmp = 0xff;
	int ret, retry = 0;

	do {
		msleep(1);
		/* ret = st_spi_read_bytes(0xF8, &tmp, 1); */
		ret = sitronix_ts_reg_read(gts, CMDIO_CONTROL, &tmp, 1);

		if (ret <= 0) {
			sterr("%s: retry(%d) read ready.\n", __func__, retry++);
		} else{
			if (tmp == 0x01) {
				stdbg("retry  ............\n");
				retry++;
				ret = 0;
			/* continue; */
			}
		}
		if (retry > 1000) {
			sterr("%s: time out\n", __func__);
			/* break; */
			bRet = false;
			return bRet;
		}

	} while (ret <= 0);

	if (tmp == 0x00) {	/* OK */
		bRet = true;
	} else if (tmp == 0x80) {
		sterr("TDU_ReadIOCommand: Unknown Command ID.\n");
		bRet = false;
	} else if (tmp == 0x81) {
		sterr("TDU_ReadIOCommand: Host to device command checksum error.\n");
		bRet = false;
	} else{
		sterr("Unknown Error(0x%02X).\n", tmp);
		bRet = false;
	}
	return bRet;
}


bool TDU_ReadIOCommand(struct CommandIoPacket *packet)
{
	bool bRet = false;
	int ret;
	unsigned char tmp[32];
	/* process commmand */
	memset(tmp, 0, 32);
	/* ret = st_spi_read_bytes(0xD0, tmp, 32); */
	ret = sitronix_ts_reg_read(gts, CMDIO_PORT, tmp, 32);
	if (ret <= 0) {
		sterr("%s: read packet error.\n", __func__);
		bRet = false;
	} else{
		memcpy((void *)packet, (const void *)tmp, 32);
		bRet = true;
	}
	return bRet;
}

bool TDU_WriteIOCommand(struct CommandIoPacket *packet)
{
	bool bRet = false;
	int ret;
	unsigned char tmp[33];

	memset(tmp, 0x00, 33);
	memcpy((void *)tmp+1, (const void *)packet, 32);

	tmp[0] = 0xD0;
	/* ret = st_spi_write_bytes(0xD0, tmp+1, 32); */
	ret = sitronix_ts_reg_write(gts, CMDIO_PORT, tmp+1, 32);

	if (ret <= 0) {
		sterr("%s: write packet error.\n", __func__);
		bRet = false;
	} else{
		bRet = true;
	}

	return bRet;
}

int TDU_CmdioRead(int type, int address, unsigned char *buf, int len)
{
	int getLen = 0, offset = 0;
	struct CommandIoPacket outPacket;
	struct CommandIoPacket inPacket;
	int remain = len;
	int pktDataSize = 0;
	unsigned short chksum, vchksum;
	int retry = 0;

	do {
		pktDataSize = (remain > 24) ? 24 : remain;
		outPacket.CmdID = 0x02;	/* read RAM/ROM */
		outPacket.ValidDataSize = 5;
		outPacket.CmdData[0] = type;	/* RAM */
		if (type == 4 || type == 3) {	/* AFE type (word address)*/
			outPacket.CmdData[1] = (((address + offset/2) >> 8) & 0xFF);	/* high byte */
			outPacket.CmdData[2] = (((address + offset/2)) & 0xFF);		/* low byte */
		} else {		/*byte address*/
			outPacket.CmdData[1] = (((address + offset) >> 8) & 0xFF);	/* high byte */
			outPacket.CmdData[2] = (((address + offset)) & 0xFF);		/* low byte */
		}
		/* outPacket.CmdData[1] = (((address + offset) >> 8) & 0xFF);		//high byte */ 
		/* outPacket.CmdData[2] = (((address + offset)) & 0xFF);		//low byte  */
		outPacket.CmdData[3] = pktDataSize;
		chksum = 0;
		STChecksumCalculation(&chksum, (unsigned char *)&outPacket, 6);
		outPacket.CmdData[4] = (chksum & 0xFF);
		/* stmsg("add = %x, size = %d ", (address + offset), pktDataSize); */
		/* stmsg("add[0] = %x,add[1] = %x,\n", outPacket.CmdData[1], outPacket.CmdData[2]); */
		if (!TDU_WriteIOCommand(&outPacket)) {
			stmsg("TDU_CmdioRead: (E)TDU_WriteIOCommand.\n");
			goto TDU_CmdioRead_retry;
			/* return getLen; */
		}
		if (!TDU_SetH2DReady()) {
			stmsg("TDU_CmdioRead: (E)TDU_SetH2DReady.\n");
			goto TDU_CmdioRead_retry;
			/* return getLen; */
		}
		if (!TDU_GetH2DReady()) {
			stmsg("TDU_CmdioRead: (E)TDU_GetH2DReady.\n");
			goto TDU_CmdioRead_retry;
			/* return getLen; */
		}
		if (!TDU_ReadIOCommand(&inPacket)) {
			stmsg("TDU_CmdioRead: (E)TDU_ReadIOCommand.\n");
			goto TDU_CmdioRead_retry;
			/* return getLen; */
		}

		if (inPacket.CmdID == 0x82
			&& inPacket.CmdData[0] == type){
			vchksum = 0;
			STChecksumCalculation(&vchksum, (unsigned char *)&inPacket, inPacket.ValidDataSize + 1);
			vchksum = (vchksum & 0xFF);
			if (vchksum == inPacket.CmdData[inPacket.ValidDataSize - 1]) {
				memcpy(buf+offset, &(inPacket.CmdData[2]), inPacket.CmdData[1]);
				remain -= inPacket.CmdData[1];	/* data size */
				offset += inPacket.CmdData[1];
				getLen += inPacket.CmdData[1];
				retry = 0;

			} else{
				/* drop packet */
				stmsg("Invalid Cheksum Expect(0x%02x) Get(0x%02X)\n", vchksum, inPacket.CmdData[inPacket.ValidDataSize - 1]);
				goto TDU_CmdioRead_retry;
			}
		} else{
			/* drop packet */
			stmsg("Unexpect CmdID (0x%02x) or Type (0x%02X)\n", inPacket.CmdID, inPacket.CmdData[0]);
TDU_CmdioRead_retry:
			retry++;
			sterr("TDU_CmdioRead_retry %d with add = %x, size = %d\n", retry, (address + offset), pktDataSize);
			if (retry > 10)
				return -EIO;
		}

	} while (remain > 0);
	return getLen;

}

int TDU_CmdioWrite(int type, int address, unsigned char *buf, int len)
{
	int setLen = 0, offset = 0;
	struct CommandIoPacket outPacket;
	/* CommandIoPacket inPacket; */
	int remain = len;
	int pktDataSize = 0;
	unsigned short chksum;/* , vchksum; */
	int retry = 0;

	do {
		pktDataSize = (remain > 24) ? 24 : remain;
		outPacket.CmdID = 0x01;	/* write RAM/ROM */
		outPacket.ValidDataSize = pktDataSize + 5;
		outPacket.CmdData[0] = type;
		if (type == 4 || type == 3) {	/* AFE type (word address)*/
			outPacket.CmdData[1] = (((address + offset/2) >> 8) & 0xFF);	/* high byte */
			outPacket.CmdData[2] = (((address + offset/2)) & 0xFF);		/* low byte */
		} else {	/*byte address*/
			outPacket.CmdData[1] = (((address + offset) >> 8) & 0xFF);	/* high byte */
			outPacket.CmdData[2] = (((address + offset)) & 0xFF);		/* low byte */
		}
		/* outPacket.CmdData[1] = (((address + offset) >> 8) & 0xFF);		// high byte */
		/* outPacket.CmdData[2] = (((address + offset)) & 0xFF);		// low byte */
		outPacket.CmdData[3] = pktDataSize;
		memcpy((void *)&(outPacket.CmdData[4]), (const void *)(buf+offset), pktDataSize);
		chksum = 0;
		STChecksumCalculation(&chksum, (unsigned char *)&outPacket, outPacket.ValidDataSize + 1);
		outPacket.CmdData[outPacket.ValidDataSize - 1] = (chksum & 0xFF);
		/* stmsg("outPacket.CmdData[1] outPacket.CmdData[2] %x %x\n",outPacket.CmdData[1],outPacket.CmdData[2]); */
		if (!TDU_WriteIOCommand(&outPacket)) {
			sterr("TDU_SetDeviceRam: (E)TDU_WriteIOCommand.\n");
			goto TDU_CmdioWrite_retry;
			/* return setLen; */
		}
		if (!TDU_SetH2DReady()) {
			sterr("TDU_SetDeviceRam: (E)TDU_SetH2DReady.\n");
			goto TDU_CmdioWrite_retry;
			/* return setLen; */
		}
		/* check processing result */
		if (!TDU_GetH2DReady()) {
			sterr("TDU_SetDeviceRam: (E)TDU_GetH2DReady.\n");
			/* return setLen; */
TDU_CmdioWrite_retry:
			retry++;
			sterr("TDU_CmdioWrite_retry %d with add = %x, size = %d\n", retry, (address + offset), pktDataSize);
			if (retry > 10)
				return -EIO;
		} else {
			/* processing write command OK */
			remain -= pktDataSize;
			offset += pktDataSize;
			setLen += pktDataSize;
			retry = 0;
		}

	} while (remain > 0);

	return setLen;
}
int TDU_Cmdio_ReadFwControl(int ctrlID, unsigned char *inBuf, int inLen, unsigned char *getBuf){
	int getLen = 0;
	struct CommandIoPacket outPacket;
	struct CommandIoPacket inPacket;
	unsigned short chksum, vchksum;

	if(inLen > 28) inLen = 28;

	outPacket.CmdID = 0x05;		//FW control command
	outPacket.ValidDataSize = inLen + 2;	//1 for control id, 1 for checksum
	outPacket.CmdData[0] = 0x80 | (ctrlID & 0xFF);	//read

    memcpy((void *)&(outPacket.CmdData[1]), (const void *)inBuf, inLen);	//control data
	chksum = 0;
    STChecksumCalculation(&chksum, (unsigned char *)&outPacket, outPacket.ValidDataSize+1);

	outPacket.CmdData[outPacket.ValidDataSize -1] =(chksum & 0xFF) ;

	if(!TDU_WriteIOCommand(&outPacket)){
		sterr("TDU_Cmdio_ReadFwControl(NewA): (E)WriteIOCommand.\n");
		return getLen;
	}
	if(!TDU_SetH2DReady()){
		sterr("TDU_Cmdio_ReadFwControl(NewA): (E)SetH2DReady.\n");
		return getLen;
	}
	if(!TDU_GetH2DReady()){
		sterr("TDU_Cmdio_ReadFwControl(NewA): (E)GetH2DReady.\n");
		return getLen;
	}
	if(!TDU_ReadIOCommand(&inPacket)){
		sterr("TDU_Cmdio_ReadFwControl(NewA): (E)ReadIOCommand.\n");
		return getLen;
	}

	if((inPacket.CmdID == 0x85)
		&& (inPacket.CmdData[0] == (0x80 | (ctrlID & 0xFF))) ){
		vchksum = 0;
		STChecksumCalculation(&vchksum, (unsigned char *)&inPacket, inPacket.ValidDataSize + 1);
		vchksum = (vchksum & 0xFF);
		if(vchksum == inPacket.CmdData[inPacket.ValidDataSize -1]){
			memcpy(getBuf, &(inPacket.CmdData[1]), inPacket.ValidDataSize - 2);
			getLen += (inPacket.ValidDataSize - 2);
		}
		else{
			//drop packet
			sterr("TDU_Cmdio_ReadFwControl(NewA): Invalid Cheksum Expect(0x%02x) Get(0x%02X)\n", vchksum, inPacket.CmdData[inPacket.ValidDataSize -1]);
			return -1;
		}

	}
	else{
		sterr("TDU_Cmdio_ReadFwControl(NewA): Unknown Commad(0x%02X) or control info(0x%02X)\n", inPacket.CmdID, inPacket.CmdData[0]);
		return -1;
	}
	return getLen;
}

int TDU_Cmdio_WriteFwControl(int ctrlID, unsigned char *buf, int len){
	int setLen = 0;
	struct CommandIoPacket outPacket;
	unsigned short chksum;

	if(len > 28) len = 28;

	outPacket.CmdID = 0x05;		//FW control command
	outPacket.ValidDataSize = len + 2;		//1 for CtrlID, 1 for checksum
	outPacket.CmdData[0] = (ctrlID & 0xFF);
	memcpy((void *)&(outPacket.CmdData[1]), (const void *)buf, len);

	chksum = 0;
	STChecksumCalculation(&chksum, (unsigned char *)&outPacket, outPacket.ValidDataSize + 1);
	outPacket.CmdData[ outPacket.ValidDataSize -1 ] =(chksum & 0xFF) ;

	if(!TDU_WriteIOCommand(&outPacket)){
		sterr("TDU_Cmdio_WriteFwControl(NewA): (E)WriteIOCommand.\n");
		return setLen;
	}
	if(!TDU_SetH2DReady()){
		sterr("TDU_Cmdio_WriteFwControl(NewA): (E)SetH2DReady.\n");
		return setLen;
	}
	//check processing result
	if(!TDU_GetH2DReady()){
		sterr("TDU_Cmdio_WriteFwControl(NewA): (E)GetH2DReady.\n");
		return setLen;
	}
	else{
		//processing write command OK
		setLen += len;
	}

	return setLen;
}

int TDU_FWInfoRead(int type, unsigned char *buf, int len)
{
	int getLen = 0, offset = 0;
	struct CommandIoPacket outPacket;
	struct CommandIoPacket inPacket;
	int remain = len;
	int pktDataSize = 0;
	unsigned short chksum, vchksum;
	int retry = 0;

	do {
		pktDataSize = (remain > 24) ? 24 : remain;
		outPacket.CmdID = 0x04;	/* FW information read */
		outPacket.ValidDataSize = 2;
		outPacket.CmdData[0] = type;	/* RAM */
		chksum = 0;
		STChecksumCalculation(&chksum, (unsigned char *)&outPacket, 3);
		outPacket.CmdData[1] = (chksum & 0xFF);

		if (!TDU_WriteIOCommand(&outPacket)) {
			stmsg("TDU_FWInfoRead: (E)TDU_WriteIOCommand.\n");
			goto TDU_FWInfoRead_retry;
			/* return getLen; */
		}
		if (!TDU_SetH2DReady()) {
			stmsg("TDU_FWInfoRead: (E)TDU_SetH2DReady.\n");
			goto TDU_FWInfoRead_retry;
			/* return getLen; */
		}
		if (!TDU_GetH2DReady()) {
			stmsg("TDU_FWInfoRead: (E)TDU_GetH2DReady.\n");
			goto TDU_FWInfoRead_retry;
			/* return getLen; */
		}
		if (!TDU_ReadIOCommand(&inPacket)) {
			stmsg("TDU_FWInfoRead: (E)TDU_ReadIOCommand.\n");
			goto TDU_FWInfoRead_retry;
			/* return getLen; */
		}

		if (inPacket.CmdID == 0x84
			&& inPacket.CmdData[0] == type){
			vchksum = 0;
			STChecksumCalculation(&vchksum, (unsigned char *)&inPacket, inPacket.ValidDataSize + 1);
			vchksum = (vchksum & 0xFF);
			if (vchksum == inPacket.CmdData[inPacket.ValidDataSize - 1]) {
				memcpy(buf+offset, &(inPacket.CmdData[0]), len);
				remain = 0;
				getLen = len;
				retry = 0;
			} else{
				/* drop packet */
				stmsg("Invalid Cheksum Expect(0x%02x) Get(0x%02X)\n", vchksum, inPacket.CmdData[inPacket.ValidDataSize - 1]);
				goto TDU_FWInfoRead_retry;
			}
		} else{
			/* drop packet */
			stmsg("Unexpect CmdID (0x%02x) or Type (0x%02X)\n", inPacket.CmdID, inPacket.CmdData[0]);
TDU_FWInfoRead_retry:
			retry++;
			sterr("TDU_FWInfoRead_retry %d with add = %x, size = %d\n", retry, offset, pktDataSize);
			if (retry > 10)
				return -EIO;
		}

	} while (remain > 0);
	return getLen;
}

int TDU_FWControlRead(int modeID, unsigned char *buf, int len)
{
	//buf[0] = 0x80;
	unsigned char rbuf[30];
	unsigned char wbuf[30];
	unsigned char rLen = 0;
	unsigned char wLen = 0;

	memset(rbuf, 0, sizeof(rbuf));
	memset(wbuf, 0, sizeof(wbuf));

	wbuf[wLen++] = modeID & 0xff;	//mode id

	rLen =  TDU_Cmdio_ReadFwControl(0x01, wbuf, wLen ,rbuf);
	memcpy(buf, rbuf + 1, len);
	stmsg("Read Mode[%02X] = %02X\n",rbuf[0], buf[0]);
	return 0;
}
int TDU_FWControlWrite(int modeID, unsigned char *buf, int len)
{
	unsigned char wbuf[30];
	int wLen = 0, ret = 0;
	memset(wbuf, 0, sizeof(wbuf));
	wbuf[wLen++] = modeID & 0xFF; //mode id
	memcpy(wbuf+wLen, buf, len);	//flag
	wLen = wLen + len;
	ret = TDU_Cmdio_WriteFwControl(0x01, wbuf, wLen);
	stmsg("Write Mode[%02X] = %02X (wLen = %d)\n", wbuf[0], wbuf[1], wLen);
	return ret;
}
int sitronix_mode_switch_value(int modeID, bool flag, unsigned char value)
{
	int ret = 0;
	unsigned char buf[2];

	stmsg("modeID: %d , flag: %d , value: %x\n", modeID, flag, value);
	if (modeID > ST_MODE_SIZE) {
		sterr("%s modeID range error : %x\n", __func__, modeID);
		return -EINVAL;
	}

	if (modeID == ST_MODE_SWU) {
		gts->swu_flag = flag;
		gts->mode_flag[modeID] = flag;
		return ret;
	}

	buf[0] = flag ? ST_MODE_SWITCH_ON : ST_MODE_SWITCH_OFF;
	buf[0] |= value;

	ret = TDU_FWControlWrite(modeID, buf, 1);
	if (ret < 0) {
		sterr("TDU_FWControlWrite fail with error : %d\n", ret);
		return ret;
	}
	msleep(20);
	ret = TDU_FWControlRead(modeID, buf+1, 1);
	if (ret < 0) {
		sterr("TDU_FWControlRead fail with error : %d\n", ret);
		return ret;
	}

	if (buf[1] != buf[0]) {
		sterr("Mode swith fail with compare different\n");
		return -EINVAL;
	}
	gts->mode_flag[modeID] = flag;
	gts->mode_value[modeID] = value;

	stmsg("%s success with modeID: %x , flag : %d , value : %x\n", __func__, modeID, flag, value);
	return 0;
}

int sitronix_mode_switch(int modeID, bool flag)
{
	return sitronix_mode_switch_value( modeID, flag, 0);
}

void sitronix_mode_restore(void)
{

	int i = 0;
	int ret = 0;
	unsigned char buf[2];

	for (i = ST_MODE_RESTORE_START; i < ST_MODE_SIZE; i++) {

		buf[0] = gts->mode_flag[i] ? ST_MODE_SWITCH_ON : ST_MODE_SWITCH_OFF;
		buf[0] |= gts->mode_value[i];

		TDU_FWControlRead(i, buf+1, 1);

		if (buf[1] != buf[0])
			ret = TDU_FWControlWrite(i, buf, 1);
		//msleep(30);
	}
	
	if (gts->proximity_flag)
		sitronix_ts_proximity_enable(gts, true);

	if (ret < 0)
		stmsg("%s fail\n", __func__);
	else
		stmsg("%s success\n", __func__);
}

int sitronix_get_display_id(unsigned char *buf)
{
	uint8_t cmd[8] = {0};
	int ret = 0;
	int off = 0;

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0xA5;
	cmd[5] = 0x3C;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x14;
	cmd[5] = 0x55;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFC;
	cmd[4] = 0x5A;
	cmd[5] = 0x9E;
	cmd[6] = 0x04;
	cmd[7] = 0x00;

	ret = sitronix_ts_addrmode_write(gts, cmd, 7);
	if (ret < 0) {
		sterr("read display CMD1 fail - 1\n");
		return -1;
	}

	wbuf[1] = 0x83;
	wbuf[2] = 0x07;
	wbuf[3] = 0x00;

	off = sitronix_ts_addrmode_split_read(gts, wbuf, 3, rbuf, 3);

	if (off < 0) {
		sterr("read display CMD1 fail - 2\n");
		ret = -1;
	}

	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFE;
	cmd[4] = 0x00;
	cmd[5] = 0xA5;

	sitronix_ts_addrmode_write(gts, cmd,5);

	if (ret < 0) {
		sterr("read display CMD1 fail - 3\n");
		return -1;
	}

	stmsg("Display id = 0x%X 0x%X 0x%X \n", rbuf[off], rbuf[off+1], rbuf[off+2]);
	memcpy(buf, rbuf + off , 3);
	
	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x41;
	cmd[5] = 0x4C;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x5A;
	cmd[5] = 0xC3;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	return ret;
}

int sitronix_get_ic_position(unsigned char *buf)
{
	uint8_t cmd[10] = {0};
	int ret = 0;
	int off = 0;

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0xA5;
	cmd[5] = 0x3C;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x14;
	cmd[5] = 0x55;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFC;
	cmd[4] = 0x5A;
	cmd[5] = 0x9D;
	cmd[6] = 0xBD;
	cmd[7] = 0x00;
	cmd[8] = 0x00;
	cmd[9] = 0x00;

	ret = sitronix_ts_addrmode_write(gts, cmd, 9);
	if (ret < 0) {
		sterr("read ic position step 1 fail - 1\n");
		return -1;
	}

	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFE;
	cmd[4] = 0x00;
	cmd[5] = 0xA5;

	ret = sitronix_ts_addrmode_write(gts, cmd, 5);
	if (ret < 0) {
		sterr("read ic position step 2 fail - 1\n");
		return -1;
	}

	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFC;
	cmd[4] = 0x5A;
	cmd[5] = 0x9E;
	cmd[6] = 0x6C;
	cmd[7] = 0x00;

	ret = sitronix_ts_addrmode_write(gts, cmd, 7);
	if (ret < 0) {
		sterr("read ic position step 3 fail - 1\n");
		return -1;
	}

	wbuf[1] = 0x83;
	wbuf[2] = 0x07;
	wbuf[3] = 0x04;

	off = sitronix_ts_addrmode_split_read(gts, wbuf, 3, rbuf, 6);

	if (off < 0) {
		sterr("read ic position step 4 fail - 2\n");
		ret = -1;
	}

	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFE;
	cmd[4] = 0x00;
	cmd[5] = 0xA5;

	sitronix_ts_addrmode_write(gts, cmd,5);
	
	if (ret < 0) {
		sterr("read ic position step 5 fail - 3\n");
		return -1;
	}

	stmsg("ic_position buf = 0x%X 0x%X \n", rbuf[off + 4], rbuf[off + 5]);
	memcpy(buf, rbuf + off + 4, 2);

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x41;
	cmd[5] = 0x4C;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x5A;
	cmd[5] = 0xC3;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);
	return ret;
}

int sitronix_get_ic_sfrver(void)
{
	uint8_t cmd[8] = {0};
	int ret , off;

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0xA5;
	cmd[5] = 0x3C;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x14;
	cmd[5] = 0x55;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x55;
	cmd[5] = 0x55;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	wbuf[1] = 0x81;
	wbuf[2] = 0x01;
	wbuf[3] = 0xC2;
	
	off = sitronix_ts_addrmode_read(gts, wbuf, 3, rbuf, 3);
	if (off < 0) {
		sterr("read IC VER fail \n");
		ret = -1;
	}
	else
		ret = rbuf[off + 1];

	return ret;
}

int sitronix_write_driver_cmd(unsigned char dc, unsigned char *buf, int len)
{
	uint8_t cmd[128] = {0};
	int ret = 0;	
	
	if( len > sizeof(cmd) - 6) {
		sterr("The max len of driver cmd is %d , now is %d\n", sizeof(cmd) - 6 , len);
		return -1;
	}

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0xA5;
	cmd[5] = 0x3C;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x14;
	cmd[5] = 0x55;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFC;
	cmd[4] = 0x5A;
	cmd[5] = 0x9D;		

	ret = sitronix_ts_addrmode_write(gts, cmd, 5);
	if (ret < 0) {
		sterr("write_driver_cmd step 1 fail - 1\n");
		return -1;
	}

	if(len%2 ==1)
		len = len+1;
		
	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFE;
	cmd[4] = dc;
	cmd[5] = 0x00;
	memcpy(cmd + 6, buf, len);	

	ret = sitronix_ts_addrmode_write(gts, cmd, 5 + len);
	if (ret < 0) {
		sterr("write_driver_cmd step 2 fail - 1\n");
		return -1;
	}

	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFE;
	cmd[4] = 0x00;
	cmd[5] = 0xA5;

	sitronix_ts_addrmode_write(gts, cmd,5);

	if (ret < 0) {
		sterr("write_driver_cmd step 3 fail - 3\n");
		return -1;
	}

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x41;
	cmd[5] = 0x4C;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x5A;
	cmd[5] = 0xC3;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);
	return ret;
}


int sitronix_read_driver_cmd(unsigned char dc, unsigned char *buf, int len)
{
	uint8_t cmd[32] = {0};
	int ret = 0 , off;

	if( len > sizeof(cmd) - 6) {
		sterr("The max len of driver cmd is %d , now is %d\n", sizeof(cmd) - 6 , len);
		return -1;
	}

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0xA5;
	cmd[5] = 0x3C;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x14;
	cmd[5] = 0x55;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	//BD 00
	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFC;
	cmd[4] = 0x5A;
	cmd[5] = 0x9D;
	cmd[6] = 0xBD;
	cmd[7] = 0x00;
	cmd[8] = 0x00;
	cmd[9] = 0x00;
	ret = sitronix_ts_addrmode_write(gts, cmd, 9);

	//Transition Complete
	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFE;
	cmd[4] = 0x00;
	cmd[5] = 0xA5;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFC;
	cmd[4] = 0x5A;
	cmd[5] = 0x9E;
	cmd[6] = dc;
	cmd[7] = 0x00;

	ret = sitronix_ts_addrmode_write(gts, cmd, 7);


	cmd[1] = 0x83;
	cmd[2] = 0x07;
	cmd[3] = 0x00;

	off = sitronix_ts_addrmode_split_read(gts, cmd, 3, wbuf, len);

	if (off < 0) {
		sterr("read display CMD1 fail - 2\n");
	}
	memcpy(buf, wbuf + off, len);

	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFE;
	cmd[4] = 0x00;
	cmd[5] = 0xA5;

	sitronix_ts_addrmode_write(gts, cmd,5);

	//BD 03
	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFC;
	cmd[4] = 0x5A;
	cmd[5] = 0x9D;
	cmd[6] = 0xBD;
	cmd[7] = 0x00;
	cmd[8] = 0x03;
	cmd[9] = 0x00;
	ret = sitronix_ts_addrmode_write(gts, cmd, 9);

	//Transition Complete
	cmd[1] = 0x03;
	cmd[2] = 0x06;
	cmd[3] = 0xFE;
	cmd[4] = 0x00;
	cmd[5] = 0xA5;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x41;
	cmd[5] = 0x4C;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);

	cmd[1] = 0x53;
	cmd[2] = 0x71;
	cmd[3] = 0x23;
	cmd[4] = 0x5A;
	cmd[5] = 0xC3;
	ret = sitronix_ts_addrmode_write(gts, cmd, 5);
	return ret;
}

