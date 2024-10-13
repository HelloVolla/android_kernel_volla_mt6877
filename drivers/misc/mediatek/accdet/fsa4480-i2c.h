// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef FSA4480_I2C_H
#define FSA4480_I2C_H

#include <linux/of.h>
#include <linux/notifier.h>

/****** switch method select ******/
// #define FSA4480_SWITCH_AUTONOMOUSLY
#define FSA4480_SWITCH_BY_ACCDET_ADC

enum fsa_function {
	FSA_MIC_GND_SWAP,
	FSA_USBC_ORIENTATION_CC1,
	FSA_USBC_ORIENTATION_CC2,
	FSA_USBC_DISPLAYPORT_DISCONNECTED,
	FSA_EVENT_MAX,
};

#ifdef CONFIG_TYPEC_AUDIO_FSA4480_SWITCH
int fsa4480_switch_event(struct device_node *node,
					enum fsa_function event);
int fsa4480_reg_notifier(struct notifier_block *nb,
					struct device_node *node);
int fsa4480_unreg_notifier(struct notifier_block *nb,
					struct device_node *node);
#ifdef FSA4480_SWITCH_BY_ACCDET_ADC
int fsa4480_mic_gnd_swap_by_adc(void);
#endif
#else
static inline int fsa4480_switch_event(struct device_node *node,
								enum fsa_function event)
{
		return 0;
}

static inline int fsa4480_reg_notifier(struct notifier_block *node,
								enum device_node event)
{
		return 0;
}

static inline int fsa4480_unreg_notifier(struct notifier_block *node,
								enum device_node event)
{
		return 0;
}
#endif /* CONFIG_TYPEC_AUDIO_FSA4480_SWITCH */
#endif /* FSA4480_I2C_H */
