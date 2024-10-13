#define PRI_PLATFORM_MT6789 0
#define PRI_PLATFORM_MT6891 0

//module_setting  ++++++++++++
#define MODULE_FINGERPRINT 0
#define MODULE_SMART_PA 0
#define MODULE_TYPEC_ACCDET 0
#define MODULE_TEE 0
//module_setting  ------------


/******  
*******fingerprint_config +++++++++++++++++++++++++  
******/
#if MODULE_FINGERPRINT

//cdfinger_98s_config ++++++++++++++++++++++++
#define PRI_CD_980S_ATA 0

#if PRI_CD_980S_ATA
#define PRI_FP_VDD_PMIC 0
#define PRI_CD_980S_ATA_CONFIG_1 0
#define PRI_CD_980S_ATA_CONFIG_2 0
#define PRI_CD_980S_ATA_CONFIG_3 0
#endif
//cdfinger_98s_config ------------------------


//fortsense_driver_all_in_one ++++++++++++++++++++++++

#define PRI_FORTSENSE_DRIVER_ALL_IN_ONE 0

#if PRI_FORTSENSE_DRIVER_ALL_IN_ONE
#define PRI_FP_VDD_PMIC 0
#define PRI_FORTSENSE_DRIVER_ALL_IN_ONE_CONFIG_1 0
#define PRI_FORTSENSE_DRIVER_ALL_IN_ONE_CONFIG_2 0
#define PRI_FORTSENSE_DRIVER_ALL_IN_ONE_CONFIG_3 0
#endif

//fortsense_driver_all_in_one ------------------------




#endif
/******  
*******fingerprint_config -------------------------  
******/


/******  
*******smart_pa_config +++++++++++++++++++++++++  
******/

#if MODULE_SMART_PA

//smart_pa_k_i2c  ++++++++++++++++++++++++++++
#define PRI_SMART_PA_K_I2C 0
#if PRI_SMART_PA_K_I2C

#define PRI_AW87XXX_NEED_RESET_PIN 0
//pri_aw87xxx_single ++++++++++++++++++++++
#define PRI_AW87XXX_SINGLE 0
#if PRI_AW87XXX_SINGLE
#define PRI_AW87XXX_SINGLE_CONFIG_1 0
#define PRI_AW87XXX_SINGLE_CONFIG_2 0
#define PRI_AW87XXX_SINGLE_CONFIG_3 0
#endif
//pri_aw87xxx_single ----------------------


//pri_aw87xxx_quadruple ++++++++++++++++++++++
#define PRI_AW87XXX_QUADRUPLE 0
#if PRI_AW87XXX_QUADRUPLE
#define PRI_AW87XXX_QUADRUPLE_CONFIG_1 0
#define PRI_AW87XXX_QUADRUPLE_CONFIG_2 0
#define PRI_AW87XXX_QUADRUPLE_CONFIG_3 0
#endif
//pri_aw87xxx_quadruple ----------------------



#endif
//smart_pa_k_i2c  ----------------------------


//pri_smart_pa_i2s ++++++++++++++++++++++++++++
#define PRI_SMART_PA_I2S 0
#if PRI_SMART_PA_I2S

#define PRI_AW88394_SINGLE 0
#if PRI_AW88394_SINGLE
#define PRI_AW88394_SINGLE_CONFIG_1 0
#define PRI_AW88394_SINGLE_CONFIG_2 0
#define PRI_AW88394_SINGLE_CONFIG_3 0
#endif

#endif

//pri_smart_pa_i2s ----------------------------
#endif
/******  
*******smart_pa_config ------------------------  
******/

/******  
*******typec accdet_config ++++++++++++++++++++++++++++  
******/
#if MODULE_TYPEC_ACCDET
#define PRI_TYPEC_ACCDET_SGM3978 0
#if PRI_TYPEC_ACCDET_SGM3978
#define PRI_TYPEC_ACCDET_SGM3978_CONFIG_1 0
#endif
#endif
/******  
*******typec accdet_config ----------------------------  
******/


/******  
*******tee_config ++++++++++++++++++++++++++++  
******/
#if MODULE_TEE
#define PRI_MICROTRUST_TEE 0
#if PRI_MICROTRUST_TEE
#define PRI_MICROTRUST_TEE_CONFIG_1 0
#endif
#endif
/******  
*******tee_config ----------------------------  
******/