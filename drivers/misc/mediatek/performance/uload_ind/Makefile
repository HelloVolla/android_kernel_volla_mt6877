#copyright (C) 2018 MediaTek Inc.
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
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#

obj-y += uload_ind.o

# prize add by lihuangyuan,for PRIZE_CPULOAD_MONITOR,start
ifeq ($(CONFIG_MACH_MT6833),y)
	PDIR = mt6833
else ifeq ($(CONFIG_MACH_MT6893),y)
	PDIR = mt6893
else ifeq ($(CONFIG_MACH_MT6885),y)
	PDIR = mt6885
else ifeq ($(CONFIG_MACH_MT6781),y)
	PDIR = mt6781
else ifeq ($(CONFIG_MACH_MT6768),y)
	PDIR = mt6768
else ifeq ($(CONFIG_MACH_MT6877),y)
	PDIR = mt6877
else ifeq ($(CONFIG_MACH_MT6739),y)
	PDIR = mt6739
else ifeq ($(CONFIG_MACH_MT6873),y)
	PDIR = mt6873
else ifeq ($(CONFIG_MACH_MT6853),y)
	PDIR = mt6853
else ifeq ($(CONFIG_MACH_MT6785),y)
	PDIR = mt6785
else
	PDIR = $(CONFIG_MTK_PLATFORM)
endif
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/$(PDIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/$(PDIR)
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
# # prize add by lihuangyuan,for PRIZE_CPULOAD_MONITOR,end
