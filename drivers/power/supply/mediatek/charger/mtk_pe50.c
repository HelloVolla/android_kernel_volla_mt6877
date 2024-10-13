/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <mt-plat/v1/prop_chgalgo_class.h>
#include "mtk_intf.h"

static struct pe50 *pe5;

static int __pe50_notifier_call(struct notifier_block *nb, unsigned long event,
				void *data)
{
	chr_info("%s %s\n", __func__, prop_chgalgo_notify_evt_tostring(event));
	switch (event) {
	case PCA_NOTIEVT_ALGO_STOP:
		wake_up_charger();
		break;
	default:
		break;
	}
	return 0;
}

int pe50_stop(void)
{
	if (pe5 == NULL)
		return -ENODEV;

	if (pe5->online == true) {
		chr_err("%s\n", __func__);
		/*prize modified by lvyuanchuan,X9LAVA-1274*/
		//enable_vbus_ovp(true);
		pe5->online = false;
		pe5->state = PE50_INIT;
	}

	return 0;
}

bool pe50_is_ready(void)
{
	if (pe5 == NULL)
		return false;

	return prop_chgalgo_is_algo_ready(pe5->pca_algo);
}

int pe50_init(void)
{
	int ret = -EBUSY;
	struct pe50 *pe50 = NULL;

	if (pe5 == NULL) {
		pe50 = kzalloc(sizeof(struct pe50), GFP_KERNEL);
		if (pe50 == NULL)
			return -ENOMEM;

		pe5 = pe50;
		pe50->pca_algo = prop_chgalgo_dev_get_by_name("pca_algo_dv2");
		if (!pe50->pca_algo) {
			chr_err("[PE50] Get pca_algo fail\n");
			ret = -EINVAL;
		} else {
			ret = prop_chgalgo_init_algo(pe50->pca_algo);
			if (ret < 0) {
				chr_err("[PE50] Init algo fail\n");
				pe50->pca_algo = NULL;
				goto out;
			}
			pe50->nb.notifier_call = __pe50_notifier_call;
			ret = prop_chgalgo_notifier_register(pe50->pca_algo,
							     &pe50->nb);
		}
		return ret;
	}

out:
	return ret;
}
/*prize add by lvyuanchuan for thermal limiting,20221202 start*/
int pe50_set_charging_current_limit(struct charger_manager *pinfo)
{
	struct charger_data *chg1_data = NULL;
	if(!pinfo){
		return 0;
	}
	chg1_data = &pinfo->chg1_data;
	if(chg1_data){
		/*
			abcct_lcmoff 1.2~3A
			abcct 600~3A
		*/
		return chg1_data->thermal_charging_current_limit;
	}
	return -1;
}
/*prize add by lvyuanchuan for thermal limiting,20221202 end*/
/*prize add by lvyuanchuan for master-slave charger switching ,20221123*/
#define PE50_ALGO_RERUN	(1)
#define PE50_ALGO_DONE  (2)
/*prize add by lvyuanchuan for limiting the input charging current at screen on, 20221129 start*/
/*uA*/
#define THERMAL_INPUT_LIMIT_AT_SCREENON		(3000000)
#define THERMAL_INPUT_LIMIT_AT_VIDEO			(1500000)
#define THERMAL_INPUT_LIMIT_AT_SCREENOFF	(6000000)
#define THERMAL_INPUT_ITEM								(1200000)
#define HW_TEMP_LEVEL_1	(35)
#define HW_TEMP_LEVEL_2	(40)
#define HW_TEMP_LEVEL_3	(45)
#define HW_TEMP_LEVEL_4	(50)
#define INPUT_CURR_OFFSET (1000)

extern int mtktspmic_get_hw_temp(void);
int pe50_run(void)
{
	int ret = 0;
	int thr_lmt = THERMAL_INPUT_LIMIT_AT_SCREENON;
	int thr_lmt_new = THERMAL_INPUT_LIMIT_AT_SCREENON;
	bool running;
	struct charger_consumer *chg_consumer = NULL;
	struct charger_manager *pinfo = NULL;
	struct charger_data *dvchg_data = NULL;
	int temp = mtktspmic_get_hw_temp()/1000;

	if(pe5){
		chg_consumer = charger_manager_get_by_name(&pe5->pca_algo->dev,"charger_port1");
		if(chg_consumer){
			pinfo = chg_consumer->cm;
		}
	}else{
		chr_err("[PE50] pe50 is null!\n");
		return 0;
	}
	chr_info("[PE50]state:%d \n",pe5->state);
	switch (pe5->state) {
	case PE50_INIT:
		enable_vbus_ovp(false);
		pe5->online = true;
		ret = prop_chgalgo_start_algo(pe5->pca_algo);
		/*prize added by lvyuanchuan,X9-489,start*/
		if (ret < 0){
			enable_vbus_ovp(true);
		}else{
			pe5->state = PE50_RUNNING;
		}
		/*prize added by lvyuanchuan,X9-489,end*/
		break;

	case PE50_RUNNING:
		running = prop_chgalgo_is_algo_running(pe5->pca_algo);
		if (!running)
			enable_vbus_ovp(true);
		if(pinfo){
			/*prize add by lvyuanchuan for master-slave charger switching ,20230203 start*/
			/*
				Reaching the fast charge cut-off current is the real stop
			*/
			if(!running){
				if(pinfo->finish_pe5){
					return PE50_ALGO_DONE;
				}else{
					pe5->state = PE50_INIT;
					chr_info("[PE50] rerun!!! \n");
					return PE50_ALGO_RERUN;
				}
			}
			/*prize add by lvyuanchuan for master-slave charger switching ,20230203 end*/
			dvchg_data = &pinfo->dvchg1_data;
			if(dvchg_data){
				/*prize added by lvyuanchuan,X9-489,start*/
				thr_lmt = pe50_set_charging_current_limit(pinfo);
				if(charger_is_screenBlank()){

					if(pinfo->chg_scenario == 0){
						/*screen on*/
						if(temp <= HW_TEMP_LEVEL_1){
							thr_lmt_new = THERMAL_INPUT_LIMIT_AT_SCREENON;
						}else if((temp > HW_TEMP_LEVEL_1) && (temp <= HW_TEMP_LEVEL_2)){
							thr_lmt_new = THERMAL_INPUT_LIMIT_AT_SCREENON - 300*INPUT_CURR_OFFSET;
						}else if((temp > HW_TEMP_LEVEL_2) && (temp <= HW_TEMP_LEVEL_3)){
							thr_lmt_new = THERMAL_INPUT_LIMIT_AT_SCREENON - 500*INPUT_CURR_OFFSET;
						}else if(temp > HW_TEMP_LEVEL_3){
							thr_lmt_new = THERMAL_INPUT_LIMIT_AT_SCREENON - 700*INPUT_CURR_OFFSET;
						}
					}else{
						/*PRIZE:modified by lvyuanchuan,x9-761,20230111 */
						/*video game
						if(temp <= HW_TEMP_LEVEL_1){
							thr_lmt_new = THERMAL_INPUT_LIMIT_AT_VIDEO;
						}else if((temp > HW_TEMP_LEVEL_1) && (temp <= HW_TEMP_LEVEL_2)){
							thr_lmt_new = THERMAL_INPUT_LIMIT_AT_VIDEO - 200*INPUT_CURR_OFFSET;
						}else if(temp > HW_TEMP_LEVEL_2){
							thr_lmt_new = THERMAL_INPUT_LIMIT_AT_VIDEO - 300*INPUT_CURR_OFFSET;
						}*/
					}
					chr_info("[PE50]thr_input_lmt: %d ,thr_lmt_new:%d\n",dvchg_data->thermal_input_current_limit,thr_lmt_new);
					dvchg_data->thermal_input_current_limit = thr_lmt_new;
				}else{
					/* screen off*/
					thr_lmt_new = (thr_lmt != -1)? thr_lmt*2 : -1;
					if((thr_lmt_new != -1) && (thr_lmt_new < THERMAL_INPUT_ITEM*2)){
						thr_lmt_new = THERMAL_INPUT_ITEM*2;
					}
					dvchg_data->thermal_input_current_limit = thr_lmt_new;
				}
				/*prize added by lvyuanchuan,X9-489,end*/
				mtk_pe50_thermal_throttling(pinfo,dvchg_data->thermal_input_current_limit);
			}
			/*cv setting at ita_lmt*/
			chr_info("[PE50]temp: %d ,thr_lmt:%d,thermal_input_current_limit:%d , sw_jeita.cv:%d,chg_scenario:%d\n",temp,
								thr_lmt,dvchg_data->thermal_input_current_limit,pinfo->sw_jeita.cv,pinfo->chg_scenario);
		}
		break;
	}
	return ret;
}
/*prize add by lvyuanchuan for limiting the input charging current at screen on, 20221129 end*/