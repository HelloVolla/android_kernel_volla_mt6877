/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#include <linux/types.h>
#include <linux/delay.h>
#include <mt-plat/v1/mtk_battery.h>
#include <mt-plat/v1/mtk_charger.h>
#include <mt-plat/mtk_boot.h>
#include <mtk_gauge_class.h>
#include <mtk_battery_internal.h>

#ifdef CONFIG_CUSTOM_BATTERY_EXTERNAL_CHANNEL
#include <custome_external_battery.h>
#endif

int __attribute__((weak)) charger_get_vbus(void)
{
	return 4500;
}

#if (CONFIG_MTK_GAUGE_VERSION != 30)
signed int battery_get_bat_voltage(void)
{
	return 4000;
}

signed int battery_get_bat_current(void)
{
	return 0;
}

signed int battery_get_bat_current_mA(void)
{
	return 0;
}

signed int battery_get_soc(void)
{
	return 50;
}

signed int battery_get_uisoc(void)
{
	struct mtk_battery *gm = get_mtk_battery();
	if (gm != NULL) {
		int boot_mode = gm->boot_mode;

		if ((boot_mode == META_BOOT) ||
			(boot_mode == ADVMETA_BOOT) ||
			(boot_mode == FACTORY_BOOT) ||
			(boot_mode == ATE_FACTORY_BOOT))
			return 75;
		else if (boot_mode == 0)
			return gm->ui_soc;
	}

	return 50;
}

signed int battery_get_bat_temperature(void)
{
	return 25;
}

signed int battery_get_ibus(void)
{
	return 0;
}

signed int battery_get_vbus(void)
{
	return 0;
}

signed int battery_get_bat_avg_current(void)
{
	return 0;
}
#else

	#ifdef CONFIG_CUSTOM_BATTERY_EXTERNAL_CHANNEL
_CODE_DEFINEDE
	#else

signed int battery_is_present(void)
{
#if defined(CONFIG_MTK_CW2217_SUPPORT)
	union power_supply_propval value;
	struct power_supply *cwfg_psy = power_supply_get_by_name("cw-bat");
	if (cwfg_psy) {
		power_supply_get_property(cwfg_psy, POWER_SUPPLY_PROP_PRESENT, &value);
		pr_info("%s:get cw-bat success, present(%d)\n",__func__, value.intval);
		if(value.intval){
			return 1;
		}
	}
#endif
	return 0;
}

signed int battery_get_bat_voltage(void)
{
#if defined(CONFIG_MTK_CW2217_SUPPORT)
	union power_supply_propval value;
	struct power_supply *cwfg_psy = power_supply_get_by_name("cw-bat");
	if (cwfg_psy) {
		power_supply_get_property(cwfg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
		pr_info("%s:get cw-bat success, vol(%d)\n",__func__, value.intval);
		return value.intval/1000;
	}
	return pmic_get_battery_voltage();
#endif
	return 0;
}

signed int battery_get_bat_current(void)
{
	int curr_val;
	bool is_charging;
#if defined(CONFIG_MTK_CW2217_SUPPORT)
	union power_supply_propval value;
	struct power_supply *cwfg_psy = power_supply_get_by_name("cw-bat");
	if (cwfg_psy) {
		power_supply_get_property(cwfg_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &value);
		pr_info("%s:get cw-bat success, curr(%d)\n",__func__, value.intval);

		return value.intval;
	}
#endif
	is_charging = gauge_get_current(&curr_val);
	if (is_charging == false)
		curr_val = 0 - curr_val;
	return curr_val;
}

signed int battery_get_bat_current_mA(void)
{
	return battery_get_bat_current() / 10;
}

signed int battery_get_soc(void)
{
	struct mtk_battery *gm = get_mtk_battery();

#if defined(CONFIG_MTK_CW2217_SUPPORT)
	union power_supply_propval value;
	struct power_supply *cwfg_psy = power_supply_get_by_name("cw-bat");
	if (cwfg_psy) {
		power_supply_get_property(cwfg_psy, POWER_SUPPLY_PROP_CAPACITY, &value);
		pr_info("%s:get cw-bat success, soc(%d)\n",__func__, value.intval);
		/*debug for cp*/
		return value.intval;
	}
	return 50;
#endif
	if (gm != NULL)
		return gm->soc;
	else
		return 50;
}

signed int battery_get_uisoc(void)
{
	struct mtk_battery *gm = get_mtk_battery();
/*prize added by lvyuanchuan,X9LAVA-1234,20230516 start*/
#if defined(CONFIG_MTK_CW2217_SUPPORT)
	#define FG_RETRY_COUNT          9
	int loop = 0;
	union power_supply_propval value;
	struct power_supply *cwfg_psy = NULL;

retry:
	cwfg_psy = power_supply_get_by_name("cw-bat");
	if (cwfg_psy) {
		power_supply_get_property(cwfg_psy, POWER_SUPPLY_PROP_CAPACITY, &value);
		pr_info("%s:get cw-bat success, ui_soc(%d)\n",__func__, value.intval);
		/*debug for cp*/
		return value.intval;
	}else{
			if(loop++ < FG_RETRY_COUNT) {
				mdelay(100);
				pr_info("%s:waiting [%d] times ...\n",__func__, loop);
				goto retry;
			}
	}
	pr_info("%s:waiting overtimes ...\n",__func__);
	return 2;
/*prize added by lvyuanchuan,X9LAVA-1234,20230516 end*/
#endif
	if (gm != NULL) {
		int boot_mode = gm->boot_mode;

		if ((boot_mode == META_BOOT) ||
			(boot_mode == ADVMETA_BOOT) ||
			(boot_mode == FACTORY_BOOT) ||
			(boot_mode == ATE_FACTORY_BOOT))
			return 75;
		else if (boot_mode == 0)
			return gm->ui_soc;
	}

	return 50;
}

signed int battery_get_bat_temperature(void)
{
	/* TODO */
#if defined(CONFIG_MTK_CW2217_SUPPORT)
	union power_supply_propval value;
	struct power_supply *cwfg_psy = power_supply_get_by_name("cw-bat");
	if (cwfg_psy) {
		power_supply_get_property(cwfg_psy, POWER_SUPPLY_PROP_TEMP, &value);
		pr_info("%s:get cw-bat success, temp(%d)\n",__func__, value.intval);

		return value.intval/10;
	}
	return 25;
#else
	if (is_battery_init_done())
		return force_get_tbat(true);
	else
		return -127;
#endif
}

signed int battery_get_ibus(void)
{
	return pmic_get_ibus();
}

signed int battery_get_vbus(void)
{
	return charger_get_vbus();
}

signed int battery_get_bat_avg_current(void)
{
	bool valid;
#if defined(CONFIG_MTK_CW2217_SUPPORT)
	union power_supply_propval value;
	struct power_supply *cwfg_psy = power_supply_get_by_name("cw-bat");
	if (cwfg_psy) {
		power_supply_get_property(cwfg_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &value);
		pr_info("%s:get cw-bat success, curr(%d)\n",__func__, value.intval);

		return value.intval;
	}
#endif
	return gauge_get_average_current(&valid);
}

/* 4.9 already remove, only leave in 4.4 */
signed int battery_get_bat_uisoc(void)
{
	return battery_get_uisoc();
}

signed int battery_get_bat_soc(void)
{
	return battery_get_soc();
}

bool battery_get_bat_current_sign(void)
{
	int curr_val;

	return gauge_get_current(&curr_val);
}

int get_ui_soc(void)
{
	return battery_get_uisoc();
}

signed int battery_get_bat_avg_voltage(void)
{
	return gauge_get_nag_vbat();
}

signed int battery_meter_get_battery_current(void)
{
	return battery_get_bat_current();
}

bool battery_meter_get_battery_current_sign(void)
{
	return battery_get_bat_current_sign();
}

signed int battery_meter_get_battery_temperature(void)
{
	return battery_get_bat_temperature();
}

signed int battery_meter_get_charger_voltage(void)
{
	return battery_get_vbus();
}

unsigned long BAT_Get_Battery_Current(int polling_mode)
{
	return (long)battery_get_bat_avg_current();
}

unsigned long BAT_Get_Battery_Voltage(int polling_mode)
{
	long int ret;

	ret = (long)battery_get_bat_voltage();
	return ret;
}

unsigned int bat_get_ui_percentage(void)
{
	return battery_get_uisoc();
}

unsigned int battery_get_is_kpoc(void)
{
	return is_kernel_power_off_charging();
}

bool battery_is_battery_exist(void)
{
#if defined(CONFIG_MTK_CW2217_SUPPORT)
	union power_supply_propval value;
	struct power_supply *cwfg_psy = power_supply_get_by_name("cw-bat");
	if (cwfg_psy) {
		power_supply_get_property(cwfg_psy, POWER_SUPPLY_PROP_PRESENT, &value);
		pr_info("%s:get cw-bat success, present(%d)\n",__func__, value.intval);

		return value.intval;
	}
#endif
	return pmic_is_battery_exist();
}


void wake_up_bat(void)
{
}
EXPORT_SYMBOL(wake_up_bat);
/* end of legacy API */


/* GM25 only */
int en_intr_VBATON_UNDET(int en)
{
	return 0;
}

int reg_VBATON_UNDET(void (*callback)(void))
{
	return 0;
}

#endif
#endif
