# include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/alsps/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/

obj-$(CONFIG_TOUCHSCREEN_TLSC6X) += tlsc6x_comp.o tlsc6x_main.o
