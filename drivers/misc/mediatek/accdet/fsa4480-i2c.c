// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include "fsa4480-i2c.h"
#include "tcpm.h"

/* prize added by hanjiuping for call codec do accdet start */
#define EINT_PIN_PLUG_OUT       (0)
#define EINT_PIN_PLUG_IN        (1)

extern void accdet_eint_func_extern(int state);
extern int  accdet_auxadc_get_val(void);
/* prize added by hanjiuping for call codec do accdet end */

#define FSA4480_I2C_NAME	"fsa4480-driver"

#define HL5280_DEVICE_REG_VALUE           0x49
#define ASW5480_DEVICE_ID                 0x59

/* Registers Map */
#define FSA4480_DEVICE_ID                 0x00
#define FSA4480_SWITCH_SETTINGS           0x04
#define FSA4480_SWITCH_CONTROL            0x05
#define FSA4480_SWITCH_STATUS0            0x06
#define FSA4480_SWITCH_STATUS1            0x07
#define FSA4480_SLOW_L                    0x08
#define FSA4480_SLOW_R                    0x09
#define FSA4480_SLOW_MIC                  0x0A
#define FSA4480_SLOW_SENSE                0x0B
#define FSA4480_SLOW_GND                  0x0C
#define FSA4480_DELAY_L_R                 0x0D
#define FSA4480_DELAY_L_MIC               0x0E
#define FSA4480_DELAY_L_SENSE             0x0F
#define FSA4480_DELAY_L_AGND              0x10
#define FSA4480_FUN_EN                    0x12
#define FSA4480_JACK_STATUS               0x17
#define FSA4480_RESET                     0x1E
#define FSA4480_CURRENT_SOURCE_SETTING    0x1F

#undef dev_dbg
#define dev_dbg dev_info

enum switch_vendor {
	FSA4480 = 0,
	HL5280,
	ASW5480,
};

struct fsa4480_priv {
	struct regmap *regmap;
	struct device *dev;
	struct tcpc_device *tcpc_dev;
	struct notifier_block pd_nb;
	atomic_t usbc_mode;
	struct work_struct usbc_analog_work;
	struct blocking_notifier_head fsa4480_notifier;
	struct mutex notification_lock;
	unsigned int hs_det_pin;
	enum switch_vendor vendor;
	bool plug_state;
#ifdef FSA4480_SWITCH_BY_ACCDET_ADC
	int mic_swap_thr;
#endif
};

struct fsa4480_reg_val {
	uint8_t reg;
	uint8_t val;
};

struct fsa4480_priv *g_fsa_priv = NULL;

static const struct regmap_config fsa4480_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = FSA4480_CURRENT_SOURCE_SETTING,
};

static const struct fsa4480_reg_val fsa_reg_i2c_defaults[] = {
	{FSA4480_SLOW_L, 0x00},
	{FSA4480_SLOW_R, 0x00},
	{FSA4480_SLOW_MIC, 0x00},
	{FSA4480_SLOW_SENSE, 0x00},
	{FSA4480_SLOW_GND, 0x00},
	{FSA4480_DELAY_L_R, 0x00},
	{FSA4480_DELAY_L_MIC, 0x00},
	{FSA4480_DELAY_L_SENSE, 0x00},
	{FSA4480_DELAY_L_AGND, 0x09},
};

static void fsa4480_usbc_update_settings(struct fsa4480_priv *fsa_priv,
		u32 switch_control, u32 switch_enable)
{
	if (!fsa_priv->regmap) {
		dev_err(fsa_priv->dev, "%s: regmap invalid\n", __func__);
		return;
	}

	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, 0x80);
	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, switch_control);
	/* FSA4480 chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, switch_enable);
}

static int fsa4480_usbc_event_changed(struct notifier_block *nb,
					  unsigned long evt, void *ptr)
{
	struct fsa4480_priv *fsa_priv =
			container_of(nb, struct fsa4480_priv, pd_nb);
	struct device *dev;
	struct tcp_notify *noti = ptr;

	if (!fsa_priv)
		return -EINVAL;

	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	if (fsa_priv->vendor == HL5280) {
		dev_info(dev, "%s: switch chip is HL5280\n", __func__);
	}
	else if (fsa_priv->vendor == ASW5480) {
		dev_info(dev, "%s: switch chip is ASW5480\n", __func__);
	}

	dev_info(dev, "%s: typeC event: %d\n", __func__, evt);

	switch (evt) {
	case TCP_NOTIFY_TYPEC_STATE:
		dev_info(dev, "%s: old_state: %d, new_state: %d\n",
			__func__, noti->typec_state.old_state, noti->typec_state.new_state);
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_AUDIO) {
			/* AUDIO plug in */
			dev_info(dev, "%s: audio plug in\n", __func__);
			fsa_priv->plug_state = true;
			dev_info(dev, "%s: tcpc polarity = %d\n", __func__, noti->typec_state.polarity);
			pm_stay_awake(fsa_priv->dev);
			schedule_work(&fsa_priv->usbc_analog_work);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			/* AUDIO plug out */
			dev_err(dev, "%s: audio plug out\n", __func__);
			fsa_priv->plug_state = false;
			pm_stay_awake(fsa_priv->dev);
			schedule_work(&fsa_priv->usbc_analog_work);
		}
		else {
			dev_dbg(dev, "%s: ignore tcpc non-audio notification\n", __func__);
		}
		break;
	default:
		break;
	};

	return NOTIFY_OK;
}

static int fsa4480_usbc_analog_setup_switches(struct fsa4480_priv *fsa_priv)
{
	struct device *dev;
	unsigned int switch_status = 0;
#ifdef FSA4480_SWITCH_AUTONOMOUSLY
	unsigned int jack_status = 0;
#endif

	if (!fsa_priv)
		return -EINVAL;
	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	mutex_lock(&fsa_priv->notification_lock);

	dev_info(dev, "%s: plug_state %d\n", __func__, fsa_priv->plug_state);
	if (fsa_priv->plug_state) {

#ifdef FSA4480_SWITCH_AUTONOMOUSLY
		/* activate switches */
		fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9F);

		regmap_write(fsa_priv->regmap, FSA4480_CURRENT_SOURCE_SETTING, 0x07);
		usleep_range(1000, 1005);
		regmap_write(fsa_priv->regmap, FSA4480_FUN_EN, 0x45);
		usleep_range(10000, 10005);
		dev_info(dev, "%s: set reg[0x%x] done.\n", __func__, FSA4480_FUN_EN);

		regmap_read(fsa_priv->regmap, FSA4480_JACK_STATUS, &jack_status);
		dev_info(dev, "%s: jack status with 700uA: 0x%x.\n", __func__, jack_status);

		/* if detect fail under 700uA, use 100uA detect again */
		if (unlikely(1 == jack_status)) {
			dev_info(dev, "%s: use 100uA detect again\n", __func__);
			fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9F);

			regmap_write(fsa_priv->regmap, FSA4480_CURRENT_SOURCE_SETTING, 0x01);
			usleep_range(1000, 1005);
			regmap_write(fsa_priv->regmap, FSA4480_FUN_EN, 0x45);
			usleep_range(10000, 10005);
			regmap_read(fsa_priv->regmap, FSA4480_JACK_STATUS, &jack_status);
			dev_info(dev, "%s: jack status with 100uA: 0x%x.\n", __func__, jack_status);
		}

		if (jack_status & 0x2) {
			/* for 3 pole, mic switch to SBU2 */
			dev_info(dev, "%s: set mic to sbu2 for 3 pole.\n", __func__);
			fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9F);
			usleep_range(4000, 4005);
		}
#elif defined(FSA4480_SWITCH_BY_ACCDET_ADC)
		/* first activate switches: SENSE(AGND) <-> (G)SBU1, MIC <-> SBU2,
		 and then transit switch status per accdet ADC value */
		fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9F);
#else
#error ERROR!!! YOU SHOULD CHOOSE AT LEAST ONE SWITCH METHOD!!!
#endif

		/* call external accdet_eint_func to handle accdet function */
		accdet_eint_func_extern(EINT_PIN_PLUG_IN);

		regmap_read(fsa_priv->regmap, FSA4480_SWITCH_STATUS0, &switch_status);
		dev_info(dev, "%s: switch status0: 0x%x.\n", __func__, switch_status);
		regmap_read(fsa_priv->regmap, FSA4480_SWITCH_STATUS1, &switch_status);
		dev_info(dev, "%s: switch status1: 0x%x.\n", __func__, switch_status);
	} else {
		/* take different sense to AGND setting per switch-IC vendor when deactivate */
		if (unlikely(fsa_priv->vendor == ASW5480)) {
			fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x9D);
		}
		else {
			fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
		}

		/* call external accdet_eint_func to handle accdet function */
		accdet_eint_func_extern(EINT_PIN_PLUG_OUT);
	}

	mutex_unlock(&fsa_priv->notification_lock);
	return 0;
}

/*
 * fsa4480_reg_notifier - register notifier block with fsa driver
 *
 * @nb - notifier block of fsa4480
 * @node - phandle node to fsa4480 device
 *
 * Returns 0 on success, or error code
 */
int fsa4480_reg_notifier(struct notifier_block *nb,
			 struct device_node *node)
{
	int rc = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;

	rc = blocking_notifier_chain_register
				(&fsa_priv->fsa4480_notifier, nb);
	if (rc)
		return rc;

	/*
	 * as part of the init sequence check if there is a connected
	 * USB C analog adapter
	 */
	dev_dbg(fsa_priv->dev, "%s: verify if USB adapter is already inserted\n",
		__func__);
	rc = fsa4480_usbc_analog_setup_switches(fsa_priv);

	return rc;
}
EXPORT_SYMBOL(fsa4480_reg_notifier);

/*
 * fsa4480_unreg_notifier - unregister notifier block with fsa driver
 *
 * @nb - notifier block of fsa4480
 * @node - phandle node to fsa4480 device
 *
 * Returns 0 on pass, or error code
 */
int fsa4480_unreg_notifier(struct notifier_block *nb,
				 struct device_node *node)
{
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;

	fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
	return blocking_notifier_chain_unregister
					(&fsa_priv->fsa4480_notifier, nb);
}
EXPORT_SYMBOL(fsa4480_unreg_notifier);

static int fsa4480_validate_display_port_settings(struct fsa4480_priv *fsa_priv)
{
	u32 switch_status = 0;

	regmap_read(fsa_priv->regmap, FSA4480_SWITCH_STATUS1, &switch_status);

	if ((switch_status != 0x23) && (switch_status != 0x1C)) {
		dev_err(fsa_priv->dev, "%s: AUX SBU1/2 switch status is invalid = %u\n",
				__func__, switch_status);
		return -EIO;
	}

	return 0;
}
/*
 * fsa4480_switch_event - configure FSA switch position based on event
 *
 * @node - phandle node to fsa4480 device
 * @event - fsa_function enum
 *
 * Returns int on whether the switch happened or not
 */
int fsa4480_switch_event(struct device_node *node,
			 enum fsa_function event)
{
	int switch_control = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;
	if (!fsa_priv->regmap)
		return -EINVAL;

	pr_info("%s - switch event: %d\n", __func__, event);
	switch (event) {
	case FSA_MIC_GND_SWAP:
		regmap_read(fsa_priv->regmap, FSA4480_SWITCH_CONTROL,
				&switch_control);
		if ((switch_control & 0x07) == 0x07)
			switch_control = 0x0;
		else
			switch_control = 0x7;
		fsa4480_usbc_update_settings(fsa_priv, switch_control, 0x9F);
		break;
	case FSA_USBC_ORIENTATION_CC1:
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0xF8);
		return fsa4480_validate_display_port_settings(fsa_priv);
	case FSA_USBC_ORIENTATION_CC2:
		fsa4480_usbc_update_settings(fsa_priv, 0x78, 0xF8);
		return fsa4480_validate_display_port_settings(fsa_priv);
	case FSA_USBC_DISPLAYPORT_DISCONNECTED:
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(fsa4480_switch_event);

/*
 * fsa4480_mic_gnd_swap_by_adc - swap GND and Mic according to accdet ADC
 *
 * @none
 *
 * Returns 0 for success, or error code
 */
#ifdef FSA4480_SWITCH_BY_ACCDET_ADC
int fsa4480_mic_gnd_swap_by_adc(void)
{
	struct fsa4480_priv *fsa_priv = g_fsa_priv;
	int accdet_adc_val = 0;
	unsigned int switch_control = 0;

	if (unlikely(fsa_priv == NULL)) {
		pr_err("%s: fsa_priv is NULL\n", __func__);
		return -1;
	}

	if (unlikely(!fsa_priv->plug_state)) {
		dev_warn(fsa_priv->dev, "%s while audio accessory is absent\n", __func__);
		return 0;
	}

	/* delay before read the ADC value */
	mdelay(2);
	accdet_adc_val = accdet_auxadc_get_val();
	dev_dbg(fsa_priv->dev, "%s: accdet adc val: %dmV\n", __func__, accdet_adc_val);

	if (accdet_adc_val <= fsa_priv->mic_swap_thr) {
		dev_dbg(fsa_priv->dev, "%s: swap gnd and mic now\n", __func__);
		regmap_read(fsa_priv->regmap, FSA4480_SWITCH_CONTROL,
				&switch_control);
		if ((switch_control & 0x07) == 0x07)
			switch_control = 0x0;
		else
			switch_control = 0x7;
		fsa4480_usbc_update_settings(fsa_priv, switch_control, 0x9F);
	}
	else {
		dev_dbg(fsa_priv->dev, "%s: correct switch, no need swap\n", __func__);
	}

	return 0;
}
EXPORT_SYMBOL(fsa4480_mic_gnd_swap_by_adc);
#endif

static int fsa4480_parse_dt(struct fsa4480_priv *fsa_priv,
	struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	int ret = 0;

	if (dNode == NULL) {
		pr_err("%s: device node is NULL\n", __func__);
		return -ENODEV;
	}

#ifdef FSA4480_SWITCH_BY_ACCDET_ADC
	ret = of_property_read_u32(dNode, "mic_swap_thr", (unsigned int *)&fsa_priv->mic_swap_thr);
	if (ret) {
		fsa_priv->mic_swap_thr = 300;
		dev_warn(dev, "%s: of read mic_swap_thr fail %d, user default val 300\n",
		__func__, ret);
	}

	dev_dbg(dev, "%s: of read mic_swap_thr is %dmv\n",
		__func__, fsa_priv->mic_swap_thr);
#endif
	return ret;
}

static void fsa4480_usbc_analog_work_fn(struct work_struct *work)
{
	struct fsa4480_priv *fsa_priv =
		container_of(work, struct fsa4480_priv, usbc_analog_work);

	if (!fsa_priv) {
		pr_err("%s: fsa container invalid\n", __func__);
		return;
	}
	fsa4480_usbc_analog_setup_switches(fsa_priv);
	pm_relax(fsa_priv->dev);
}

static void fsa4480_update_reg_defaults(struct fsa4480_priv *fsa_priv)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(fsa_reg_i2c_defaults); i++)
		regmap_write(fsa_priv->regmap, fsa_reg_i2c_defaults[i].reg,
				   fsa_reg_i2c_defaults[i].val);

	/* take different default sense to AGND setting per switch-IC vendor */
	if (unlikely(fsa_priv->vendor == ASW5480)) {
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x9D);
	}
	else {
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
	}
}

static int fsa4480_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	struct fsa4480_priv *fsa_priv;
	int rc = 0;
	unsigned int reg_value = 0;

	fsa_priv = devm_kzalloc(&i2c->dev, sizeof(*fsa_priv),
				GFP_KERNEL);
	if (!fsa_priv)
		return -ENOMEM;

	fsa_priv->dev = &i2c->dev;

	fsa4480_parse_dt(fsa_priv, &i2c->dev);

	fsa_priv->regmap = devm_regmap_init_i2c(i2c, &fsa4480_regmap_config);
	if (IS_ERR_OR_NULL(fsa_priv->regmap)) {
		dev_err(fsa_priv->dev, "%s: Failed to initialize regmap: %d\n",
			__func__, rc);
		if (!fsa_priv->regmap) {
			rc = -EINVAL;
			goto err_data;
		}
		rc = PTR_ERR(fsa_priv->regmap);
		goto err_data;
	}

	regmap_read(fsa_priv->regmap, FSA4480_DEVICE_ID, &reg_value);
	dev_dbg(fsa_priv->dev, "%s: device id reg value: 0x%x\n", __func__, reg_value);
	if (HL5280_DEVICE_REG_VALUE == reg_value) {
		fsa_priv->vendor = HL5280;
		dev_info(fsa_priv->dev, "%s: switch chip is HL5280\n", __func__);
	}
	else if (ASW5480_DEVICE_ID == reg_value) {
		fsa_priv->vendor = ASW5480;
		dev_info(fsa_priv->dev, "%s: switch chip is ASW5480\n", __func__);
	}
	else {
		fsa_priv->vendor = FSA4480;
		dev_info(fsa_priv->dev, "%s: switch chip is FSA4480 or other[0x%x]\n", __func__, reg_value);
	}

	fsa4480_update_reg_defaults(fsa_priv);

	fsa_priv->plug_state = false;
	fsa_priv->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!fsa_priv->tcpc_dev) {
		pr_err("%s get tcpc device type_c_port0 fail\n", __func__);
		goto err_data;
	}

	fsa_priv->pd_nb.notifier_call = fsa4480_usbc_event_changed;
	fsa_priv->pd_nb.priority = 0;
	rc = register_tcp_dev_notifier(fsa_priv->tcpc_dev, &fsa_priv->pd_nb, TCP_NOTIFY_TYPE_ALL);
	if (rc < 0) {
		pr_err("%s: register tcpc notifer fail\n", __func__);
		goto err_data;
	}

	mutex_init(&fsa_priv->notification_lock);
	i2c_set_clientdata(i2c, fsa_priv);

	INIT_WORK(&fsa_priv->usbc_analog_work,
		  fsa4480_usbc_analog_work_fn);

	fsa_priv->fsa4480_notifier.rwsem =
		(struct rw_semaphore)__RWSEM_INITIALIZER
		((fsa_priv->fsa4480_notifier).rwsem);
	fsa_priv->fsa4480_notifier.head = NULL;

	g_fsa_priv = fsa_priv;
	return 0;

err_data:
	devm_kfree(&i2c->dev, fsa_priv);
	return rc;
}

static int fsa4480_remove(struct i2c_client *i2c)
{
	struct fsa4480_priv *fsa_priv =
			(struct fsa4480_priv *)i2c_get_clientdata(i2c);

	if (!fsa_priv)
		return -EINVAL;

	fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
	cancel_work_sync(&fsa_priv->usbc_analog_work);
	pm_relax(fsa_priv->dev);
	mutex_destroy(&fsa_priv->notification_lock);
	dev_set_drvdata(&i2c->dev, NULL);

	return 0;
}

static const struct of_device_id fsa4480_i2c_dt_match[] = {
	{ .compatible = "mediatek,fsa4480-i2c", },
	{ .compatible = "mediatek,hl5280-i2c",  },
	{}
};

static struct i2c_driver fsa4480_i2c_driver = {
	.driver = {
		.name = FSA4480_I2C_NAME,
		.of_match_table = fsa4480_i2c_dt_match,
	},
	.probe = fsa4480_probe,
	.remove = fsa4480_remove,
};

static int __init fsa4480_init(void)
{
	int rc;

	rc = i2c_add_driver(&fsa4480_i2c_driver);
	if (rc)
		pr_err("fsa4480: Failed to register I2C driver: %d\n", rc);

	return rc;
}

static void __exit fsa4480_exit(void)
{
	i2c_del_driver(&fsa4480_i2c_driver);
}

late_initcall_sync(fsa4480_init);
module_exit(fsa4480_exit);

MODULE_DESCRIPTION("FSA4480 I2C driver");
MODULE_LICENSE("GPL v2");
