#ifndef PANEL_RM692E5_FHDP_DSI_CMD
#define PANEL_RM692E5_FHDP_DSI_CMD


#define REGFLAG_DELAY           0xFFFC
#define REGFLAG_UDELAY          0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW       0xFFFE
#define REGFLAG_RESET_HIGH      0xFFFF

#define FRAME_WIDTH                 1080
#define FRAME_HEIGHT                2400

#define PHYSICAL_WIDTH              70697
#define PHYSICAL_HEIGHT             157104

#define DATA_RATE                   373*2
#define HSA                         8
#define HBP                         92
#define VSA                         4
#define VBP                         8

/*Parameter setting for mode 0 Start*/
#define MODE_0_FPS                  60
#define MODE_0_VFP                  12
#define MODE_0_HFP                  100
#define MODE_0_DATA_RATE            373*2
/*Parameter setting for mode 0 End*/

/*Parameter setting for mode 1 Start*/
#define MODE_1_FPS                  90
#define MODE_1_VFP                  12
#define MODE_1_HFP                  100
#define MODE_1_DATA_RATE            250*2
/*Parameter setting for mode 1 End*/

/*Parameter setting for mode 2 Start*/
#define MODE_2_FPS                  120
#define MODE_2_VFP                  40
#define MODE_2_HFP                  155
#define MODE_2_DATA_RATE            1000 //1000 prize add by xuejian for 10bit data_rate 20230427
/*Parameter setting for mode 2 End*/

#define LFR_EN                      1
/* DSC RELATED */

#define DSC_ENABLE                  1
#define DSC_VER                     17
#define DSC_SLICE_MODE              0
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 2088
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         10
#define DSC_DSC_LINE_BUF_DEPTH      11
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           160
//define DSC_PIC_HEIGHT
//define DSC_PIC_WIDTH
#define DSC_SLICE_HEIGHT            40
#define DSC_SLICE_WIDTH             1080
#define DSC_CHUNK_SIZE              1350
#define DSC_XMIT_DELAY              410
#define DSC_DEC_DELAY               688
#define DSC_SCALE_VALUE             25
#define DSC_INCREMENT_INTERVAL      1230
#define DSC_DECREMENT_INTERVAL      21
#define DSC_LINE_BPG_OFFSET         12
#define DSC_NFL_BPG_OFFSET          631
#define DSC_SLICE_BPG_OFFSET        399
#define DSC_INITIAL_OFFSET          5632
#define DSC_FINAL_OFFSET            4332
#define DSC_FLATNESS_MINQP          7
#define DSC_FLATNESS_MAXQP          16
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    15
#define DSC_RC_QUANT_INCR_LIMIT1    15
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3

#endif //end of PANEL_RM692E5_FHDP_DSI_CMD
