#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include

obj-y   +=  barometer.o barometer_factory.o

obj-$(CONFIG_MTK_BAROHUB) += barohub/
ifneq ($(CONFIG_MTK_BAROHUB),y)
obj-$(CONFIG_MTK_BMP280_NEW) += BMP280-new/
# prize added by chenjiaxi, add BKTF_2ND_SPL07 drivers, 20220407-start
obj-$(CONFIG_BKTF_2ND_SPL07) += BKTF_2ND_SPL07/
# prize added by chenjiaxi, add BKTF_2ND_SPL07 drivers, 20220407-end
# prize added by chenjiaxi, add icp201xx drivers, 20220606-start
obj-$(CONFIG_MTK_ICP201XX) += icp201xx/
# prize added by chenjiaxi, add icp201xx drivers, 20220606-end
endif
