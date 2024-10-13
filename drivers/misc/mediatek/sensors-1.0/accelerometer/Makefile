# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/include

obj-y   +=  accel.o accel_factory.o

obj-$(CONFIG_MTK_ACCELHUB) += accelhub/
#obj-$(CONFIG_MTK_BMI160_I2C)   += bmi160-i2c/
#obj-$(CONFIG_MTK_LIS3DH_NEW) += lis3dh/
obj-$(CONFIG_MTK_MC3410_I2C) += mc3410-i2c/
#obj-$(CONFIG_MTK_MC3433) += mc3433/
#obj-$(CONFIG_MTK_MPU6050G) += mpu6050g/

#prize added by chenjiaxi, stk8xxx drivers, 20220408-start
obj-$(CONFIG_MTK_STK8XXX) += stk8xxx/
#prize added by chenjiaxi, stk8xxx drivers, 20220408-end
#prize added by chenjiaxi, mir3da drivers, 20220416-start
obj-$(CONFIG_MTK_MIR3DA) += mir3da/
#prize added by chenjiaxi, mir3da drivers, 20220416-end