#include "sitronix_ts.h"
#include "sitronix_st7123_test.h"
#ifdef ST_REPLACE_TEST_CMD_BY_DISPLAY_ID
#include "sitronix_st7123_test_id1.h"
#endif /* ST_REPLACE_TEST_CMD_BY_DISPLAY_ID */
#include <linux/export.h>


//#define USE_ST_SQRT
#ifdef USE_ST_SQRT
int st_sqrt(int x);
#endif

int st_address_mode_hardcode_write(uint8_t *code, int max_len)
{
	int ret = 0;
	int index = 0;
	uint16_t key = 0;
	uint16_t len = 0;
	uint32_t addr = 0;

	while (index < max_len - 1 ) {
		key = code[index] << 8 | code[index + 1];		
		if (key == 0x5354){
			index += 2;
			switch(code[index]) {
				case 0x01:
					index += 1;
					addr = code[index] << 16 | code[index + 1] << 8 | code[index + 2];
					len = code[index + 3] << 8 | code[index + 4];
					index += 5;
					ret = sitronix_spi_pram_rw(false, addr, code + index, NULL, len);
					index += len;
					//stmsg("write addr: %x , len :%x \n", addr, len);
					break;
				case 0x02:
					index += 1;
					len = code[index] << 8 | code[index + 1];
					//stmsg("delay 0x%x ms \n", len);
					msleep(len);
					index += 2;
					break;
				default:
					index = max_len;
					break;
			}
		} else {
			index ++;
		}
	}
	return ret;
}

bool st_get_test_enable(int col, int row)
{
	int index = col * 4 + row / 8;
	unsigned char mask = 0x80 >> (row % 8);
	
	if( index < sizeof(test_disable_sensor))
		return !(test_disable_sensor[index] & mask);
	else
		return false;
}

#ifdef ST_SELFTEST_LOG_FILE
bool sitonix_createlogfileok = true; // FIH self test open log file success or not

void sitronix_vfswrite(struct file *filp, char *data1, int str_len, loff_t *ppos)
{
	if(sitonix_createlogfileok)
		vfs_write(filp, data1, str_len, ppos);
}
#endif /* ST_SELFTEST_LOG_FILE */

#ifdef ST_SELFTEST_LOG_FILE
int st_open_short_test(char func, int skipcol, struct file *filp,loff_t *ppos)
#else
int st_open_short_test(char func, int skipcol)
#endif
{
	unsigned char cmd[0x08];
	int tMode[8];
	int read_len;
	int ret = 0;
	unsigned char *raw_buf;
	signed short *rawI;
	signed short *rawP;
	int frameCounter;
	int retryCounter;
	int max_retry = 5;
	unsigned char raw_dat_rd_on[2] = { 0x02, 0x00 };
	unsigned char raw_dat_rd_off[2] = { 0x00, 0x00 };
	unsigned char raw_header[18];
	unsigned char frame_counter = 0;
	int total_error = 0;
	int error_count = 0;
	int count_frame = 1;
	int buf_index , aa_index;
	int col , row;
#ifdef ST_SELFTEST_LOG_FILE
	char data1[150];
#endif
	int percentage = 0;	
	int *rawS; //Raw data STD for STD test
	int raw_avg;	
	unsigned long sqrt;

	/* ROW_CNT */
	sitronix_spi_pram_rw(true, 0xF04A, NULL, cmd, 2);
	tMode[2] = cmd[0]&0x3F;

	/* CMNC_CH_WR_EN */
	tMode[7] = cmd[1]&0x01;

	/* AA_UNIT */
	sitronix_spi_pram_rw(true, 0xF024, NULL, cmd, 2);
	tMode[1] = (cmd[0]&0xF0)>>3;

	/* SELF_UNIT */		
	tMode[5] = (cmd[0]&0xC)>>1;

	/* NOISE_UNIT */
	tMode[4] = (cmd[0]&0x3)<<2;

	/* KEY */
	tMode[3] = 0;

	stmsg("ROW_CNT : %d\n",tMode[2]);
	stmsg("AA_UNIT : %d\n",tMode[1]);
	stmsg("SELF_UNIT : %d\n",tMode[5]);
	stmsg("NOISE_UNIT : %d\n",tMode[4]);
	stmsg("CMNC_CH_WR_EN : %d\n",tMode[7]);

	read_len = (tMode[2]+tMode[7]) * (tMode[1] + tMode[5] + tMode[4] + skipcol) *2;
	raw_buf = kmalloc(read_len, GFP_KERNEL);
	rawI = (signed short *)kmalloc((tMode[2]) * sizeof(short), GFP_KERNEL);	

	//Ignore
	frameCounter = 0;
	retryCounter = 0;

	while (ST_SELFTEST_IGNORE_FRAME > 0 && retryCounter++ < max_retry)
	{
		msleep(10);
		sitronix_spi_pram_rw(false, 0xF004, raw_dat_rd_on, NULL, 2);
		sitronix_spi_pram_rw(true, 0xF180, NULL, raw_header, 18);

		stmsg("header %x \n",raw_header[1]);
		if(frame_counter != raw_header[1])
		{
			frameCounter++;
			retryCounter = 0;
			frame_counter = raw_header[1];
		}

		sitronix_spi_pram_rw(false, 0xF004, raw_dat_rd_off, NULL, 2);

		if (frameCounter >= ST_SELFTEST_IGNORE_FRAME + 1)
			break;
	}

	if( retryCounter >=  max_retry)
	{
		if(func == 0) {
			sterr("st open test fail ,  can't wait IRQ \n");
#ifdef ST_SELFTEST_LOG_FILE
			snprintf(data1, 50, "st open test fail ,  can't wait IRQ\n");
			sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
		} else if(func == 1 || func == 2) {
			sterr("st short test fail ,  can't wait IRQ \n");
#ifdef ST_SELFTEST_LOG_FILE
			snprintf(data1, 50, "st short test fail ,  can't wait IRQ \n");
			sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
		} else if(func == 3) {
			sterr("st uniformity test fail ,  can't wait IRQ \n");
#ifdef ST_SELFTEST_LOG_FILE
			snprintf(data1, 50, "st uniformity test fail ,  can't wait IRQ \n");
			sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
		} else if(func == 4) {
			sterr("st STD test fail ,  can't wait IRQ \n");
#ifdef ST_SELFTEST_LOG_FILE
			snprintf(data1, 50, "st STD test fail ,  can't wait IRQ \n");
			sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
		}
		ret = -1;
		goto st_open_short_test_finish;
	}

	//uniformity test
	if( func == 3) {
		rawP = (signed short *)kmalloc((tMode[2]) * sizeof(short), GFP_KERNEL);
	}
	
	//STD test
	if (func == 4) {
		retryCounter = 0;
		count_frame = ST_SELFTEST_STD_FRAME_CNT;
		rawS = (int *)kmalloc((tMode[2] * tMode[1]) * sizeof(int), GFP_KERNEL);
		while (retryCounter++ < max_retry) {
			rawP = (signed short *)kmalloc((tMode[2] * tMode[1] * ST_SELFTEST_STD_FRAME_CNT) * sizeof(short), GFP_KERNEL);
			if ( rawP != NULL)
				break;
		}
		
		if( retryCounter >=  max_retry) {
			sterr("st STD test fail for can not kmalloc\n");
#ifdef ST_SELFTEST_LOG_FILE
			snprintf(data1, 50, "st STD test fail for can not kmalloc\n");
			sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
			ret = -1;
			goto st_open_short_test_finish;
		}
	}

	//raw
	frameCounter = 0;
	retryCounter = 0;

	while (count_frame > 0 && retryCounter++ < max_retry)
	{
		msleep(10);
		sitronix_spi_pram_rw(false, 0xF004, raw_dat_rd_on, NULL, 2);
		sitronix_spi_pram_rw(true, 0xF180, NULL, raw_header, 18);
		if(frame_counter != raw_header[1])
		{
			frameCounter++;
			retryCounter = 0;
			frame_counter = raw_header[1];

			sitronix_spi_pram_rw(true, 0xD000, NULL, raw_buf, read_len);
			sitronix_spi_pram_rw(false, 0xF004, raw_dat_rd_off, NULL, 2);

			aa_index = (tMode[1] / 2) * (tMode[2] + tMode[7]) *2;
			buf_index = (skipcol) * (tMode[2] + tMode[7]) *2;

			if (skipcol != 0)
				for(col = 0; col < aa_index; col++)
					raw_buf[aa_index + col ] = raw_buf[aa_index + col + buf_index];

			buf_index = 0;
			aa_index = 0;

			//AA A
			for (col = 0; col < tMode[1] / 2; col++)
			{
				error_count = 0;
				for (row = 0; row < tMode[2]; row++)
				{
					rawI[row] = (signed short)((raw_buf[buf_index] << 8) + raw_buf[buf_index + 1]);
					//stmsg("sensor (%2d,%2d) RAW (%4d) \n" , col, row, rawI[row]);
					if(func == 3) // uniformity
					{
						//judge
						if (sizeof(golden_buf) > 0) {
							if( sizeof(golden_buf) > buf_index + 1)
								rawP[row] = (signed short)((golden_buf[buf_index]) + (golden_buf[buf_index + 1]<<8));
							else
								rawP[row] = rawI[row];

							rawI[row] = rawI[row] + gts->self_test_uniformity_shift;
							if(rawP[row] < 1)
								percentage = 100;
							else
								percentage = rawI[row] * 100 / rawP[row];							
							if(st_get_test_enable(col, row) && (percentage < gts->self_test_uniformity_min || percentage > gts->self_test_uniformity_max))
								error_count++;
						}
					}
					else if(func == 1) // short odd
					{
						if(row %2 == 0 && rawI[row] > gts->self_test_short_max)
							error_count++;
					}
					else if(func == 2) //short even
					{
						if(row %2 == 1 && rawI[row] > gts->self_test_short_max)
							error_count++;
					}
					else if(func == 4)
					{
						percentage = ((frameCounter - 1) * tMode[1] * tMode[2]) + (col * tMode[2]) + row;
						rawP[percentage] = rawI[row];						
						//stmsg("sensor (%2d,%2d) , index %2d RAW (%4d) \n" , col, row, percentage, rawI[row]);
					}
					else //open
					{
						if(st_get_test_enable(col, row) && (rawI[row] < gts->self_test_open_min || rawI[row] > gts->self_test_open_max))
							error_count++;
					}

					buf_index +=2 ;
				}

				if(error_count > ST_SELFTEST_ADJUST_COUNT)
				{
					for (row = 0; row < tMode[2]; row++)
					{
						if(func == 3)
						{	
							if(rawP[row] < 1)
								percentage = 100;
							else
								percentage = rawI[row] * 100 / rawP[row];
							if(st_get_test_enable(col, row) && (percentage < gts->self_test_uniformity_min || percentage > gts->self_test_uniformity_max))
							{
								total_error++;
								sterr("sensor (%2d,%2d) RAW (%3d%%) out of percentage (%d%% ~ %d%%) in uniformity test\n" , col, row, percentage, gts->self_test_uniformity_min , gts->self_test_uniformity_max);
#ifdef ST_SELFTEST_LOG_FILE
								snprintf(data1, 150, "sensor (%2d,%2d) RAW (%3d%%) out of percentage (%d%% ~ %d%%) in uniformity test\n" , col, row, percentage, gts->self_test_uniformity_min , gts->self_test_uniformity_max);
								sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
							}
						}
						else if(func == 1)
						{
							if(row %2 == 0 && rawI[row] > gts->self_test_short_max) {
								total_error++;
								sterr("sensor (%2d,%2d) RAW (%4d) > standard value (%d) in short_odd test\n" , col, row, rawI[row], gts->self_test_short_max);
#ifdef ST_SELFTEST_LOG_FILE
								snprintf(data1, 100, "sensor (%2d,%2d) RAW (%4d) > standard value (%d) in short_odd test\n", col, row, rawI[row], gts->self_test_short_max);
								sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
							}
						}
						else if(func == 2 )
						{
							if(row %2 == 1 && rawI[row] > gts->self_test_short_max) {
								total_error++;
								sterr("sensor (%2d,%2d) RAW (%4d) > standard value (%d) in short_even test\n" , col, row, rawI[row], gts->self_test_short_max);
#ifdef ST_SELFTEST_LOG_FILE
								snprintf(data1, 100, "sensor (%2d,%2d) RAW (%4d) > standard value (%d) in short_even test\n", col, row, rawI[row], gts->self_test_short_max);
								sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
							}
						}
						else if(st_get_test_enable(col, row) && (rawI[row] < gts->self_test_open_min || rawI[row] > gts->self_test_open_max))
						{
							total_error++;
							sterr("sensor (%2d,%2d) RAW (%4d) out of range (%d ~ %d) in open test\n" , col, row, rawI[row], gts->self_test_open_min, gts->self_test_open_max);
#ifdef ST_SELFTEST_LOG_FILE
							snprintf(data1, 100, "sensor (%2d,%2d) RAW (%4d) out of range (%d ~ %d) in open test\n", col, row, rawI[row], gts->self_test_open_min, gts->self_test_open_max);
							sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
						}
					}
				}

				if (tMode[7] != 0)
					buf_index += 2;
			}

			//SE A
			for (col = 0; col < tMode[5] / 2; col++)
			{
				for (row = 0; row < tMode[2]; row++)
				{
					buf_index +=2 ; 
				}

				if (tMode[7] != 0)
					buf_index += 2;
			}

			//AA B
			for (col = tMode[1] / 2; col < tMode[1]; col++)
			{
				error_count = 0;
				for (row = 0; row < tMode[2]; row++)
				{
					rawI[row] = (signed short)((raw_buf[buf_index] << 8) + raw_buf[buf_index + 1]);
					//stmsg("sensor (%2d,%2d) RAW (%4d) \n" , col, row, rawI[row]);
					if(func == 3) // uniformity
					{
						//judge
						if (sizeof(golden_buf) > 0) {
							if( sizeof(golden_buf) > buf_index + 1)
								rawP[row] = (signed short)((golden_buf[buf_index]) + (golden_buf[buf_index + 1]<<8));
							else
								rawP[row] = rawI[row];
							
							rawI[row] = rawI[row] + gts->self_test_uniformity_shift;
							if(rawP[row] < 1)
								percentage = 100;
							else
								percentage = rawI[row] * 100 / rawP[row];
							if(st_get_test_enable(col, row) && (percentage < gts->self_test_uniformity_min || percentage > gts->self_test_uniformity_max))
								error_count++;
						}
					}
					else if(func == 1) // short odd
					{
						if(row %2 == 0 && rawI[row] > gts->self_test_short_max)
							error_count++;
					}
					else if(func == 2) //short even
					{
						if(row %2 == 1 && rawI[row] > gts->self_test_short_max)
							error_count++;
					}
					else if(func == 4)
					{
						percentage = ((frameCounter - 1) * tMode[1] * tMode[2]) + (col * tMode[2]) + row;
						rawP[percentage] = rawI[row];
					}
					else //open
					{
						if(st_get_test_enable(col, row) && (rawI[row] < gts->self_test_open_min || rawI[row] > gts->self_test_open_max))
							error_count++;
					}

					buf_index +=2 ;
				}

				if(error_count > ST_SELFTEST_ADJUST_COUNT)
				{
					for (row = 0; row < tMode[2]; row++)
					{
						if(func == 3)
						{
							if(rawP[row] < 1)
								percentage = 100;
							else
								percentage = rawI[row] * 100 / rawP[row];
							if(st_get_test_enable(col, row) && (percentage < gts->self_test_uniformity_min || percentage > gts->self_test_uniformity_max))
							{
								total_error++;
								sterr("sensor (%2d,%2d) RAW (%3d%%) out of percentage (%d%% ~ %d%%) in uniformity test\n" , col, row, percentage, gts->self_test_uniformity_min , gts->self_test_uniformity_max);
#ifdef ST_SELFTEST_LOG_FILE
								snprintf(data1, 150, "sensor (%2d,%2d) RAW (%3d%%) out of percentage (%d%% ~ %d%%) in uniformity test\n" , col, row, percentage, gts->self_test_uniformity_min , gts->self_test_uniformity_max);
								sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
							}
						}
						else if(func == 1)
						{
							if(row %2 == 0 && rawI[row] > gts->self_test_short_max) {
								total_error++;
								sterr("sensor (%2d,%2d) RAW (%4d) > standard value (%d) in short_odd test\n" , col, row, rawI[row], gts->self_test_short_max);
#ifdef ST_SELFTEST_LOG_FILE
								snprintf(data1, 100, "sensor (%2d,%2d) RAW (%4d) > standard value (%d) in short_odd test\n", col, row, rawI[row], gts->self_test_short_max);
								sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
							}
						}
						else if(func == 2 )
						{
							if(row %2 == 1 && rawI[row] > gts->self_test_short_max) {
								total_error++;
								sterr("sensor (%2d,%2d) RAW (%4d) > standard value (%d) in short_even test\n" , col, row, rawI[row], gts->self_test_short_max);
#ifdef ST_SELFTEST_LOG_FILE
								snprintf(data1, 100, "sensor (%2d,%2d) RAW (%4d) > standard value (%d) in short_even test\n", col, row, rawI[row], gts->self_test_short_max);
								sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
							}
						}
						else if(st_get_test_enable(col, row) && (rawI[row] < gts->self_test_open_min || rawI[row] > gts->self_test_open_max))
						{
							total_error++;
							sterr("sensor (%2d,%2d) RAW (%4d) out of range (%d ~ %d) in open test\n" , col, row, rawI[row], gts->self_test_open_min, gts->self_test_open_max);
#ifdef ST_SELFTEST_LOG_FILE
							snprintf(data1, 100, "sensor (%2d,%2d) RAW (%4d) out of range (%d ~ %d) in open test\n", col, row, rawI[row], gts->self_test_open_min, gts->self_test_open_max);
							sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
						}
					}
				}

				if (tMode[7] != 0)
					buf_index += 2;
			}

			//SE B
			for (col = tMode[5] / 2; col < tMode[5] ; col++)
			{
				for (row = 0; row < tMode[2]; row++)
				{
					buf_index +=2 ; 
				}

				if (tMode[7] != 0)
					buf_index += 2;
			}

			if (func == 3 || func == 2 || func == 1 || func == 0)
			{
#ifdef ST_SELFTEST_LOG_FILE
				if (func == 0)
					snprintf(data1, 100, "[OPEN RAW Data start]\n");
				else if(func == 1)
					snprintf(data1, 100, "[SHORT_ODD RAW Data start]\n");
				else if(func == 2)
					snprintf(data1, 100, "[SHORT_EVEN RAW Data start]\n");
				else
					snprintf(data1, 100, "[UNIFORMITY RAW Data start]\n");
				sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
				buf_index = 0;
				aa_index = 0;
				//AA A
				for (col = 0; col < tMode[1] / 2; col++)
				{
					for (row = 0; row < tMode[2]; row++)
					{
						rawI[row] = (signed short)((raw_buf[buf_index] << 8) + raw_buf[buf_index + 1]);	
#ifdef ST_SELFTEST_LOG_FILE
						if (row == tMode[2] - 1 )
							snprintf(data1, 100, "%6d \n", rawI[row]);
						else
							snprintf(data1, 100, "%6d ", rawI[row]);
						sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
						buf_index +=2;
					}

					if (tMode[7] != 0)
						buf_index += 2;
				}
				//SE A
				for (col = 0; col < tMode[5] / 2; col++)
				{
					for (row = 0; row < tMode[2]; row++)
					{
						buf_index +=2;
					}

					if (tMode[7] != 0)
						buf_index += 2;
				}
				//AA B
				for (col = tMode[1] / 2; col < tMode[1]; col++)
				{
					for (row = 0; row < tMode[2]; row++)
					{
						rawI[row] = (signed short)((raw_buf[buf_index] << 8) + raw_buf[buf_index + 1]);
#ifdef ST_SELFTEST_LOG_FILE
						if (row == tMode[2] - 1 )
							snprintf(data1, 100, "%6d \n", rawI[row]);
						else
							snprintf(data1, 100, "%6d ", rawI[row]);
						sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
						buf_index +=2 ;
					}
					if (tMode[7] != 0)
						buf_index += 2;
				}

#ifdef ST_SELFTEST_LOG_FILE
				snprintf(data1, 100, "[RAW Data end]\n");
				sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
			}
		}
		else
		{
			sitronix_spi_pram_rw(false, 0xF004, raw_dat_rd_off, NULL, 2);
		}

		if (frameCounter >= count_frame)
			break;
	}

	if (func == 4) {
#ifdef ST_SELFTEST_LOG_FILE
		snprintf(data1, 100, "[STD start]\n");
		sitronix_vfswrite(filp, data1, strlen(data1), ppos);		
#endif
		for (col = 0; col < tMode[1]; col++)
		{
			for (row = 0; row < tMode[2]; row++)
			{
				aa_index = col * tMode[2] + row;
				percentage = 0;
				for (frameCounter = 0 ; frameCounter < ST_SELFTEST_STD_FRAME_CNT ; frameCounter++) {
					buf_index = frameCounter * tMode[1] * tMode[2];
					percentage += (int)rawP[buf_index + aa_index];
				}

				raw_avg = percentage * 10 / ST_SELFTEST_STD_FRAME_CNT;

				rawS[aa_index] = 0;
				for (frameCounter = 0 ; frameCounter < ST_SELFTEST_STD_FRAME_CNT ; frameCounter++) {					
					buf_index = frameCounter * tMode[1] * tMode[2];
					if( raw_avg >= (int)(rawP[buf_index + aa_index] * 10))
						percentage = raw_avg - (int)(rawP[buf_index + aa_index] * 10);
					else
						percentage = (int)(rawP[buf_index + aa_index] * 10) - raw_avg;
					if (percentage > ST_SELFTEST_STD_CALCULATE_LIMIT)
						percentage = ST_SELFTEST_STD_CALCULATE_LIMIT;
					rawS[aa_index] += percentage * percentage;
				}
				rawS[aa_index] = rawS[aa_index] / ST_SELFTEST_STD_FRAME_CNT;		
#ifndef USE_ST_SQRT		
				sqrt = int_sqrt((unsigned long) rawS[aa_index]);
#else
				sqrt = st_sqrt(rawS[aa_index]);
#endif	//end of USE_ST_SQRT

#ifdef ST_SELFTEST_LOG_FILE
				if (row == tMode[2] - 1 )
					snprintf(data1, 100, "%4ld.%1ld \n", sqrt/10, sqrt%10); //snprintf(data1, 100, "%6d \n", rawS[aa_index]);
				else
					snprintf(data1, 100, "%4ld.%1ld ", sqrt/10, sqrt%10);
				sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
			}
		}
		
		for (col = 0; col < tMode[1]; col++)
		{
			for (row = 0; row < tMode[2]; row++)
			{
				aa_index = col * tMode[2] + row;
				if(st_get_test_enable(col, row) && (rawS[aa_index] > gts->self_test_std_square100_max)) {
					total_error++;
#ifndef USE_ST_SQRT	
					sqrt = int_sqrt((unsigned long) rawS[aa_index]);
#else
					sqrt = st_sqrt(rawS[aa_index]);
#endif//end of USE_ST_SQRT
					sterr("sensor (%2d,%2d) STD (%4ld.%1ld) > standard value (%4d.%1d) in STD test\n" , col, row, sqrt/10, sqrt%10, gts->self_test_std_max/10, gts->self_test_std_max%10);
#ifdef ST_SELFTEST_LOG_FILE
					snprintf(data1, 150, "sensor (%2d,%2d) STD (%4ld.%1ld) > standard value (%4d.%1d) in STD test\n" , col, row, sqrt/10, sqrt%10, gts->self_test_std_max/10, gts->self_test_std_max%10);
					sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
				}
			}
		}
#ifdef ST_SELFTEST_LOG_FILE
		snprintf(data1, 100, "[STD end]\n");
		sitronix_vfswrite(filp, data1, strlen(data1), ppos);
#endif
	}

st_open_short_test_finish:

	kfree(rawI);
	kfree(raw_buf);
	if (func == 3)
		kfree(rawP);
	if (func == 4) {
		kfree(rawS);
		if( rawP != NULL )
			kfree(rawP);
	}

	if ( ret < 0)
		return ret;
	else if(total_error == 0)
		return 0;
	else
		return total_error;
}

#ifdef ST_SELFTEST_LOG_FILE
int st_test_open(struct file *filp,loff_t *ppos)
#else
int st_test_open(void)
#endif
{
	int ret = 0;

	sitronix_ts_reset_device(gts);
#ifdef SITRONIX_TP_WITH_FLASH
	ret = st_address_mode_hardcode_write(test_flash_afe_df, sizeof(test_flash_afe_df));
	if (ret < 0)
		return ret;
#endif /* SITRONIX_TP_WITH_FLASH */
	ret = st_address_mode_hardcode_write(test_cmd_open, sizeof(test_cmd_open));

	if (ret < 0)
		return ret;
#ifdef ST_SELFTEST_LOG_FILE
	ret = st_open_short_test(0, 0, filp, ppos);
#else
	ret = st_open_short_test(0, 0);
#endif
	return ret;
}

#ifdef ST_SELFTEST_LOG_FILE
int st_test_short_odd(struct file *filp,loff_t *ppos)
#else
int st_test_short_odd(void)
#endif
{
	int ret = 0;

	sitronix_ts_reset_device(gts);
#ifdef SITRONIX_TP_WITH_FLASH
	ret = st_address_mode_hardcode_write(test_flash_afe_df, sizeof(test_flash_afe_df));
	if (ret < 0)
		return ret;
#endif /* SITRONIX_TP_WITH_FLASH */
	ret = st_address_mode_hardcode_write(test_cmd_short_odd, sizeof(test_cmd_short_odd));

	if (ret < 0)
		return ret;
#ifdef ST_SELFTEST_LOG_FILE
	ret = st_open_short_test(1, ST_SELFTEST_SKIP_COLS, filp, ppos);
#else
	ret = st_open_short_test(1, ST_SELFTEST_SKIP_COLS);
#endif
	return ret;
}

#ifdef ST_SELFTEST_LOG_FILE
int st_test_short_even(struct file *filp,loff_t *ppos)
#else
int st_test_short_even(void)
#endif
{
	int ret = 0;

	sitronix_ts_reset_device(gts);
#ifdef SITRONIX_TP_WITH_FLASH
	ret = st_address_mode_hardcode_write(test_flash_afe_df, sizeof(test_flash_afe_df));
	if (ret < 0)
		return ret;
#endif /* SITRONIX_TP_WITH_FLASH */
	ret = st_address_mode_hardcode_write(test_cmd_short_even, sizeof(test_cmd_short_even));

	if (ret < 0)
		return ret;
#ifdef ST_SELFTEST_LOG_FILE
	ret = st_open_short_test(2, ST_SELFTEST_SKIP_COLS, filp, ppos);
#else
	ret = st_open_short_test(2, ST_SELFTEST_SKIP_COLS);
#endif
	return ret;
}

#ifdef ST_SELFTEST_LOG_FILE
int st_test_uniformity(struct file *filp,loff_t *ppos)
#else
int st_test_uniformity(void)
#endif
{
	int ret = 0;

	sitronix_ts_reset_device(gts);
#ifdef SITRONIX_TP_WITH_FLASH
	ret = st_address_mode_hardcode_write(test_flash_afe_df, sizeof(test_flash_afe_df));
	if (ret < 0)
		return ret;
#endif /* SITRONIX_TP_WITH_FLASH */
	ret = st_address_mode_hardcode_write(test_cmd_uniformity, sizeof(test_cmd_uniformity));

	if (ret < 0)
		return ret;
#ifdef ST_SELFTEST_LOG_FILE
	ret = st_open_short_test(3, 0, filp, ppos);
#else
	ret = st_open_short_test(3, 0);
#endif
	return ret;
}

#ifdef ST_SELFTEST_LOG_FILE
int st_test_std(struct file *filp,loff_t *ppos)
#else
int st_test_std(void)
#endif
{
	int ret = 0;

	sitronix_ts_reset_device(gts);
#ifdef SITRONIX_TP_WITH_FLASH
	ret = st_address_mode_hardcode_write(test_flash_afe_df, sizeof(test_flash_afe_df));
	if (ret < 0)
		return ret;
#endif /* SITRONIX_TP_WITH_FLASH */
	ret = st_address_mode_hardcode_write(test_cmd_std, sizeof(test_cmd_std));

	if (ret < 0)
		return ret;
#ifdef ST_SELFTEST_LOG_FILE
	ret = st_open_short_test(4, 0, filp, ppos);
#else
	ret = st_open_short_test(4, 0);
#endif
	return ret;
}


#ifdef ST_SELFTEST_LOG_FILE
void st_record_ic_info(struct file *filp,loff_t *pos)
{
	int ret = 0;
	char data1[50];

	ret = sitronix_ts_get_device_info(gts);
	if (ret < 0 ) {
		snprintf(data1, 50, "sitronix_ts_get_device_info failed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), pos);
	}

	snprintf(data1, 50, "Chip ID = %02X\n", gts->ts_dev_info.chip_id);
	sitronix_vfswrite(filp, data1, strlen(data1), pos);
	snprintf(data1, 50, "FW Verison = %02X\n", gts->ts_dev_info.fw_version);
	sitronix_vfswrite(filp, data1, strlen(data1), pos);
	snprintf(data1, 50, "FW Revision = %02X %02X %02X %02X\n", gts->ts_dev_info.fw_revision[0], gts->ts_dev_info.fw_revision[1], gts->ts_dev_info.fw_revision[2], gts->ts_dev_info.fw_revision[3]);
	sitronix_vfswrite(filp, data1, strlen(data1), pos);
	snprintf(data1, 50, "Resolution = %d x %d\n", gts->ts_dev_info.x_res, gts->ts_dev_info.y_res);
	sitronix_vfswrite(filp, data1, strlen(data1), pos);
	snprintf(data1, 50, "Channels = %d x %d\n", gts->ts_dev_info.x_chs, gts->ts_dev_info.y_chs);
	sitronix_vfswrite(filp, data1, strlen(data1), pos);
	snprintf(data1, 50, "Max touches = %d\n", gts->ts_dev_info.max_touches);
	sitronix_vfswrite(filp, data1, strlen(data1), pos);
	snprintf(data1, 50, "Misc. Info = 0x%X\n", gts->ts_dev_info.misc_info);
	sitronix_vfswrite(filp, data1, strlen(data1), pos);

	ret = sitronix_get_ic_sfrver();
	if (ret < 0 ) {
		snprintf(data1, 50, "sitronix_get_ic_sfrver failed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), pos);
	} else {
		snprintf(data1, 50, "IC SFR VER = 0x%X\n", ret);
		sitronix_vfswrite(filp, data1, strlen(data1), pos);
	}
}
#endif
/*
return of st_self_test
/ ret = 0 : test success
/ ret < 0 : test failed with error
/ ret > 0 : test failed with N sensors :
*/
int st_self_test(void)
{
	int ret = 0;
	int result_open, result_short_odd, result_short_even, result_uniformity, result_std;
	char ic_position[2];
#ifdef ST_SELFTEST_LOG_FILE
	struct file *filp;
	char data1[50];
	mm_segment_t fs;
	loff_t pos;

	filp = filp_open(ST_SELFTEST_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	sitonix_createlogfileok = true;
	if (IS_ERR(filp))
	{
		sitonix_createlogfileok = false;
		sterr("ST open %s error...\n", ST_SELFTEST_LOG_PATH);
		//return -1;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	st_record_ic_info(filp, &pos);
#endif	
	//get IC position
	sitronix_get_ic_position(ic_position);
	stmsg("IC position X,Y = (%03d,%03d)\n", (((ic_position[0] & 0x1) <<4) | ic_position[1]),(ic_position[0] >> 4));
#ifdef ST_SELFTEST_LOG_FILE
	snprintf(data1, 50, "IC position X,Y = (%03d,%03d)\n", (((ic_position[0] & 0x1) <<4) | ic_position[1]),(ic_position[0] >> 4));
	sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	//open test
	result_open = st_test_open(filp, &pos);
	if( result_open == 0) {
		stmsg("Test open successed!\n");
		snprintf(data1, 50, "Test open successed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	} else {
		stmsg("Test open failed!\n");
		snprintf(data1, 50, "Test open failed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	}
	//short test odd
	result_short_odd = st_test_short_odd(filp, &pos);
	if( result_short_odd == 0) {
		stmsg("Test short_odd successed!\n");
		snprintf(data1, 50, "Test short_odd successed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	} else {
		stmsg("Test short_odd failed!\n");
		snprintf(data1, 50, "Test short_odd failed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	}
	//short test even
	result_short_even = st_test_short_even(filp, &pos);
	if( result_short_even == 0) {
		stmsg("Test short_even successed!\n");
		snprintf(data1, 50, "Test short_even successed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	} else {
		stmsg("Test short_even failed!\n");
		snprintf(data1, 50, "Test short_even failed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	}
	//uniformity test
	result_uniformity = st_test_uniformity(filp, &pos);
	if( result_uniformity == 0) {
		stmsg("Test uniformity successed!\n");
		snprintf(data1, 50, "Test uniformity successed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	} else {
		stmsg("Test uniformity failed!\n");
		snprintf(data1, 50, "Test uniformity failed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	}
	//STD test
	result_std = st_test_std(filp, &pos);
	if( result_std == 0) {
		stmsg("Test STD successed!\n");
		snprintf(data1, 50, "Test STD successed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	} else {
		stmsg("Test STD failed!\n");
		snprintf(data1, 50, "Test STD failed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	}
#else
	//open test
	result_open = st_test_open();
	if( result_open == 0)
		stmsg("Test open successed!\n");
	else
		stmsg("Test open failed!\n");
	//short test odd
	result_short_odd = st_test_short_odd();
	if( result_short_odd == 0)
		stmsg("Test short_odd successed!\n");
	else
		stmsg("Test short_odd failed!\n");	
	//short test even
	result_short_even = st_test_short_even();
	if( result_short_even == 0)
		stmsg("Test short_even successed!\n");
	else
		stmsg("Test short_even failed!\n");
	//uniformity test
	result_uniformity = st_test_uniformity();
	if( result_uniformity == 0)
		stmsg("Test uniformity successed!\n");
	else
		stmsg("Test uniformity failed!\n");
	//STD test
	result_std = st_test_std();
	if( result_std == 0)
		stmsg("Test STD successed!\n");
	else
		stmsg("Test STD failed!\n");

#endif
	if ( result_open < 0 || result_short_odd < 0 || result_short_even < 0 || result_uniformity < 0 || result_std < 0)
		ret = -1;
	else
		ret = result_open + result_short_odd + result_short_even + result_uniformity + result_std;

#ifdef ST_SELFTEST_LOG_FILE
	set_fs(fs);
	if(sitonix_createlogfileok) {
		filp_close(filp, NULL);
		stmsg("Test log file : %s\n", ST_SELFTEST_LOG_PATH);
	}
#endif

	return ret;
}

void sitronix_set_default_test_criteria(void)
{
	gts->self_test_short_max = ST_SELFTEST_SHORT_MAX;
	gts->self_test_open_min = ST_SELFTEST_OPEN_MIN;
	gts->self_test_open_max = ST_SELFTEST_OPEN_MAX;
	gts->self_test_uniformity_shift = ST_SELFTEST_UNIFORMITY_SHIFT;
	gts->self_test_uniformity_min = ST_SELFTEST_UNIFORMITY_MIN;
	gts->self_test_uniformity_max = ST_SELFTEST_UNIFORMITY_MAX;
	gts->self_test_std_max = ST_SELFTEST_STD_MAX;
	gts->self_test_std_square100_max = gts->self_test_std_max * gts->self_test_std_max;
}

void sitronix_replace_test_cmd(unsigned char *id)
{
#ifdef ST_REPLACE_TEST_CMD_BY_DISPLAY_ID
	if(id[0] == test_id_1[0] && id[1] == test_id_1[1] && id[2] == test_id_1[2]) {
		//do replace here
		stmsg("find display id = %X %X %X , replace test_cmd_*_id1 to test_cmd_*  \n",test_id_1[0], test_id_1[1], test_id_1[2]);
		memcpy(test_cmd_open , test_cmd_open_id1, SITRONIX_TEST_CMD_OPEN_MAX_LEN);
		memcpy(test_cmd_short_odd , test_cmd_short_odd_id1, SITRONIX_TEST_CMD_SHORT_ODD_MAX_LEN);
		memcpy(test_cmd_short_even , test_cmd_short_even_id1, SITRONIX_TEST_CMD_SHORT_EVEN_MAX_LEN);
		memcpy(test_cmd_uniformity , test_cmd_uniformity_id1, SITRONIX_TEST_CMD_UNIFORMITY_MAX_LEN);
		memcpy(test_cmd_std , test_cmd_std_id1, SITRONIX_TEST_CMD_STD_MAX_LEN);
		memcpy(golden_buf , golden_buf_id1, sizeof(golden_buf));
		
		gts->self_test_short_max = ST_SELFTEST_SHORT_MAX_ID1;
		gts->self_test_open_min = ST_SELFTEST_OPEN_MIN_ID1;
		gts->self_test_open_max = ST_SELFTEST_OPEN_MAX_ID1;
		gts->self_test_uniformity_shift = ST_SELFTEST_UNIFORMITY_SHIFT_ID1;
		gts->self_test_uniformity_min = ST_SELFTEST_UNIFORMITY_MIN_ID1;
		gts->self_test_uniformity_max = ST_SELFTEST_UNIFORMITY_MAX_ID1;
		gts->self_test_std_max = ST_SELFTEST_STD_MAX_ID1;
		gts->self_test_std_square100_max = gts->self_test_std_max * gts->self_test_std_max;
	}
	
#endif	/* ST_REPLACE_TEST_CMD_BY_DISPLAY_ID */
}


//[CC]FIH factory functions
bool sitonix_resultselftest = false; // FIH self test result


void sitronix_touch_selftest(void)
{
	int ret = 0;
	int result_open, result_short_odd, result_short_even, result_uniformity, result_std;
	char ic_position[2];
#ifdef ST_SELFTEST_LOG_FILE
	struct file *filp;
	char data1[50];
	mm_segment_t fs;
	loff_t pos;
	sitonix_createlogfileok = true;
	filp = filp_open(ST_SELFTEST_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (IS_ERR(filp))
	{
		sitonix_createlogfileok = false;
		sterr("ST open %s error...\n", ST_SELFTEST_LOG_PATH);
		filp = NULL;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
#endif

	sitronix_ts_irq_enable(gts, false);
	gts->upgrade_doing = true;
	mutex_lock(&gts->mutex);
#ifdef ST_SELFTEST_LOG_FILE
	st_record_ic_info(filp, &pos);
#endif
	//get IC position
	sitronix_get_ic_position(ic_position);
	stmsg("IC position X,Y = (%03d,%03d)\n", (((ic_position[0] & 0x1) <<4) | ic_position[1]),(ic_position[0] >> 4));
#ifdef ST_SELFTEST_LOG_FILE
	snprintf(data1, 50, "IC position X,Y = (%03d,%03d)\n", (((ic_position[0] & 0x1) <<4) | ic_position[1]),(ic_position[0] >> 4));
	sitronix_vfswrite(filp, data1, strlen(data1), &pos);

	//open test
	result_open = st_test_open(filp, &pos);
	if( result_open == 0) {
		stmsg("Test open successed!\n");
		snprintf(data1, 50, "Test open successed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	} else {
		stmsg("Test open failed!\n");
		snprintf(data1, 50, "Test open failed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	}
	//short test odd
	result_short_odd = st_test_short_odd(filp, &pos);
	if( result_short_odd == 0) {
		stmsg("Test short_odd successed!\n");
		snprintf(data1, 50, "Test short_odd successed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	} else {
		stmsg("Test short_odd failed!\n");
		snprintf(data1, 50, "Test short_odd failed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	}
	//short test even
	result_short_even = st_test_short_even(filp, &pos);
	if( result_short_even == 0) {
		stmsg("Test short_even successed!\n");
		snprintf(data1, 50, "Test short_even successed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	} else {
		stmsg("Test short_even failed!\n");
		snprintf(data1, 50, "Test short_even failed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	}
	//uniformity test
	result_uniformity = st_test_uniformity(filp, &pos);
	if( result_uniformity == 0) {
		stmsg("Test uniformity successed!\n");
		snprintf(data1, 50, "Test uniformity successed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	} else {
		stmsg("Test uniformity failed!\n");
		snprintf(data1, 50, "Test uniformity failed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	}
	//STD test
	result_std = st_test_std(filp, &pos);
	if( result_std == 0) {
		stmsg("Test STD successed!\n");
		snprintf(data1, 50, "Test STD successed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	} else {
		stmsg("Test STD failed!\n");
		snprintf(data1, 50, "Test STD failed!\n");
		sitronix_vfswrite(filp, data1, strlen(data1), &pos);
	}
#else
	//open test
	result_open = st_test_open();
	if( result_open == 0)
		stmsg("Test open successed!\n");
	else
		stmsg("Test open failed!\n");
	//short test odd
	result_short_odd = st_test_short_odd();
	if( result_short_odd == 0)
		stmsg("Test short_odd successed!\n");
	else
		stmsg("Test short_odd failed!\n");	
	//short test even
	result_short_even = st_test_short_even();
	if( result_short_even == 0)
		stmsg("Test short_even successed!\n");
	else
		stmsg("Test short_even failed!\n");
	//uniformity test
	result_uniformity = st_test_uniformity();
	if( result_uniformity == 0)
		stmsg("Test uniformity successed!\n");
	else
		stmsg("Test uniformity failed!\n");
	//STD test
	result_std = st_test_std();
	if( result_std == 0)
		stmsg("Test STD successed!\n");
	else
		stmsg("Test STD failed!\n");
#endif	
	if ( result_open < 0 || result_short_odd < 0 || result_short_even < 0 || result_uniformity < 0 || result_std < 0)
		ret = -1;
	else
		ret = result_open + result_short_odd + result_short_even + result_uniformity + result_std;

#ifdef ST_SELFTEST_LOG_FILE
	set_fs(fs);
	if(sitonix_createlogfileok) {
		filp_close(filp, NULL);
		stmsg("Test log file : %s\n", ST_SELFTEST_LOG_PATH);
	}
#endif

	if(ret == 0)
		sitonix_resultselftest = 1;
	else
		sitonix_resultselftest = 0;

	mutex_unlock(&gts->mutex);

	sitronix_ts_mt_reset_process();
	gts->upgrade_doing = false;
	sitronix_ts_irq_enable(gts, true);

}

int sitronix_selftest_result_read(void)
{
    int num_read_chars = 0;
    if(sitonix_resultselftest)
        num_read_chars = 0;
    else
        num_read_chars = 1;
    return num_read_chars;
}

#ifdef USE_ST_SQRT
int st_sqrt(int x) {
	int l , h, mid, sqrt;
	if (x <= 1) {
        return x;
    }
    l = 1;
	h = x;
    while (l <= h) {
        mid = l + (h - l) / 2;
        sqrt = x / mid;
        if (sqrt == mid) {
            return mid;
        } else if (mid > sqrt) {
            h = mid - 1;
        } else {
            l = mid + 1;
        }
    }
    return h;
}
#endif