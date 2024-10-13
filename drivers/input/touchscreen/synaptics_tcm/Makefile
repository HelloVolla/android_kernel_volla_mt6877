#
# Makefile for the Goodix GT9886 touchscreen driver.
#

#For display ESD recovery

ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/

obj-y += synaptics_tcm_core.o
obj-y += synaptics_tcm_device.o
obj-y += synaptics_tcm_recovery.o
obj-y += synaptics_tcm_reflash.o
obj-y += synaptics_tcm_spi.o
obj-y += synaptics_tcm_touch.o
obj-y += synaptics_tcm_zeroflash.o
obj-y += synaptics_tcm_diagnostics.o
obj-y += synaptics_tcm_testing.o