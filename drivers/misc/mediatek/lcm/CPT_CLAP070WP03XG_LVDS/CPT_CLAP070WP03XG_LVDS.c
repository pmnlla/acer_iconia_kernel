#ifndef BUILD_LK
#include <linux/string.h>
#endif
#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#include <debug.h>
#include <platform/upmu_common.h>

#elif (defined BUILD_UBOOT)
#include <asm/arch/mt6577_gpio.h>
#else
#include <mach/mt_gpio.h>
#include <linux/xlog.h>
#include <mach/mt_pm_ldo.h>
#endif
#include "lcm_drv.h"
#include "mt8193_lvds.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#if defined(MTK_ALPS_BOX_SUPPORT)
/*for 8127 box hdmi main path */
#define FRAME_WIDTH  (1920)
#define FRAME_HEIGHT (1080)
#else
#define FRAME_WIDTH  (1024)
#define FRAME_HEIGHT (600)
#endif

#define REGFLAG_DELAY                                                                   0xFE
#define REGFLAG_END_OF_TABLE                                                            0xFD   // END OF REGISTE

//#define GPIO_LCD_RST_EN      GPIO90
//#define GPIO_LCD_STB_EN      GPIO89


#ifdef GPIO_LCM_PWR
#define GPIO_LCD_PWR      GPIO_LCM_PWR
#else
#define GPIO_LCD_PWR      0xFFFFFFFF
#endif

#ifdef GPIO_LCM_PWR_EN
#define GPIO_LCD_PWR_EN      GPIO_LCM_PWR_EN
#else
#define GPIO_LCD_PWR_EN      0xFFFFFFFF
#endif

#ifdef GPIO_LCM_PWR2_EN
#define GPIO_LCD_PWR2_EN      GPIO_LCM_PWR2_EN
#else
#define GPIO_LCD_PWR2_EN      0xFFFFFFFF
#endif


#ifdef GPIO_LCM_RST
#define GPIO_LCD_RST_EN      GPIO_LCM_RST
#else
#define GPIO_LCD_RST_EN      0xFFFFFFFF
#endif

#ifdef GPIO_LCM_STB
#define GPIO_LCD_STB_EN      GPIO_LCM_STB
#else
#define GPIO_LCD_STB_EN      0xFFFFFFFF
#endif


#ifdef GPIO_LCM_LVL_SHIFT_EN
#define GPIO_SHIFT_EN      GPIO_LCM_LVL_SHIFT_EN
#else
#define GPIO_SHIFT_EN      0xFFFFFFFF
#endif

#ifdef GPIO_LCM_BL_EN
#define GPIO_LCD_BL_EN      GPIO_LCM_BL_EN
#else
#define GPIO_LCD_BL_EN      0xFFFFFFFF
#endif


#ifdef GPIO_LCM_BRIDGE_EN
#define GPIO_LCD_BRIDGE_EN      GPIO_LCM_BRIDGE_EN
#else
#define GPIO_LCD_BRIDGE_EN      0xFFFFFFFF
#endif



#define HSYNC_PULSE_WIDTH 16 
#define HSYNC_BACK_PORCH  16
#define HSYNC_FRONT_PORCH 32
#define VSYNC_PULSE_WIDTH 2
#define VSYNC_BACK_PORCH  2
#define VSYNC_FRONT_PORCH 4

#define LCM_DSI_CMD_MODE	0

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (mt_set_reset_pin((v)))

#define UDELAY(n) 
#define MDELAY(n) 

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)           lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
};


static struct LCM_setting_table lcm_resume_setting[] = {
#if 0
	{0xbf,0x87,{0x5A}},
	//{0xbf,0x25,0x55},
	{0xbf,0xb1,{0x08}},
	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x80,{0x47}},

	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x81,{0x40}},

	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x82,{0x04}},

	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x83,{0x77}},

	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x84,{0x0F}},

	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x85,{0x70}},

	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x86,{0x70}},
#endif
//	{0x15,0xb1,1,{0x08}},

	{0x11,0,{0x00}}, // Sleep-Out
	{REGFLAG_DELAY, 50, {}},
	{0x29,0,{0x00}}, // Display On 
	//{0x2C,1,{0x00}},
	{REGFLAG_DELAY, 20, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_initialization_setting[] = {
	//GAMMA
	//{0xbf,0x87,{0x5A}},
	//{0xbf,0x01,{}},
	//{0x15,0x25,0x55},
	//{0x15,0xb1,1,{0x08}},
#if 0	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x80,{0x47}},

	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x81,{0x40}},

	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x82,{0x04}},

	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x83,{0x77}},

	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x84,{0x0F}},

	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x85,{0x70}},

	/*{0xbc,0x02,0x00},
	{0xbd,0x00,0x00},
	{0xbe,0x02,0x00},*/
	{0xbf,0x86,{0x70}},
#endif
	{0x11,0,{0x00}}, // Sleep-Out
	{REGFLAG_DELAY, 50, {}},
	{0x29,0,{0x00}}, // Display On 
	//{0x2C,1,{0x00}},
	{REGFLAG_DELAY, 30, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
    // Display off sequence
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 10, {}},

    // Sleep Mode On
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static __inline void send_ctrl_cmd(unsigned int cmd)
{

}

static __inline void send_data_cmd(unsigned int data)
{

}

static __inline void set_lcm_register(unsigned int regIndex,
                                      unsigned int regData)
{

}


void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
        unsigned int i;
    
    for(i = 0; i < count; i++) {
                
        unsigned cmd;
        cmd = table[i].cmd;
                        switch (cmd) {

            case REGFLAG_DELAY :
                MDELAY(table[i].count);
                break;

            case REGFLAG_END_OF_TABLE :
                break;

            default:
                                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
                                MDELAY(10);
        }
    }
}

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{

    if(GPIO == 0xFFFFFFFF)
    {
    #ifdef BUILD_LK
	 printf("[LK/LCM] GPIO_LCD_PWR =  0x%x \n",GPIO_LCD_PWR);
        printf("[LK/LCM] GPIO_LCD_PWR_EN =   0x%x\n",GPIO_LCD_PWR_EN);
        printf("[LK/LCM] GPIO_LCD_PWR2_EN =  0x%x \n",GPIO_LCD_PWR2_EN);
        printf("[LK/LCM] GPIO_LCD_RST_EN =  0x%x \n",GPIO_LCD_RST_EN);
        printf("[LK/LCM] GPIO_LCD_STB_EN =   0x%x\n",GPIO_LCD_STB_EN);
        printf("[LK/LCM] GPIO_SHIFT_EN =   0x%x\n",GPIO_SHIFT_EN);
        printf("[LK/LCM] GPIO_LCD_BL_EN =   0x%x\n",GPIO_LCD_BL_EN);
        printf("[LK/LCM] GPIO_LCD_BRIDGE_EN =  0x%x \n",GPIO_LCD_BRIDGE_EN);
	
    #elif (defined BUILD_UBOOT)
         // do nothing in uboot
    #else	
	 //printf("kernel] lcm_set gpio()  \n");
    #endif
        return;
    }

    mt_set_gpio_mode(GPIO, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO, (output>0)? GPIO_OUT_ONE: GPIO_OUT_ZERO);
}

#ifndef BUILD_LK
static bool fgisFirst = TRUE;
#endif
// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
    memset(params, 0, sizeof(LCM_PARAMS));

    params->type   = LCM_TYPE_DSI;
    //params->ctrl   = LCM_CTRL_SERIAL_DBI;
    params->width  = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;
    //params->io_select_mode = 0;	

    //params->dpi.PLL_CLOCK = 67;  //67MHz

#if 0
    /* RGB interface configurations */
    params->dpi.mipi_pll_clk_ref  = 0;
    params->dpi.mipi_pll_clk_div1 = 0x80000101;  //lvds pll 65M
    params->dpi.mipi_pll_clk_div2 = 0x800a0000;
    //params->dpi.dpi_clk_div       = 2;          
    //params->dpi.dpi_clk_duty      = 1;

    params->dpi.clk_pol           = LCM_POLARITY_FALLING;
    params->dpi.de_pol            = LCM_POLARITY_RISING;
    params->dpi.vsync_pol         = LCM_POLARITY_FALLING;
    params->dpi.hsync_pol         = LCM_POLARITY_FALLING;

    params->dpi.hsync_pulse_width = HSYNC_PULSE_WIDTH;
    params->dpi.hsync_back_porch  = HSYNC_BACK_PORCH;
    params->dpi.hsync_front_porch = HSYNC_FRONT_PORCH;
    params->dpi.vsync_pulse_width = VSYNC_PULSE_WIDTH;
    params->dpi.vsync_back_porch  = VSYNC_BACK_PORCH;
    params->dpi.vsync_front_porch = VSYNC_FRONT_PORCH;

	
    //params->dpi.i2x_en = 1;
    params->dpi.lvds_tx_en = 1;
    params->dpi.ssc_disable = 1;
    params->dpi.format            = LCM_DPI_FORMAT_RGB888;   // format is 24 bit
    params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;
    params->dpi.is_serial_output  = 0;

    params->dpi.intermediat_buffer_num = 0;

    params->dpi.io_driving_current = LCM_DRIVING_CURRENT_2MA;
#endif
                
		// DSI
                /* Command mode setting */
                //1 Three lane or Four lane
        
   #if (LCM_DSI_CMD_MODE) 
        params->dsi.mode   = CMD_MODE;
   #else
        params->dsi.mode   = BURST_VDO_MODE; //SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE; 
   #endif

   // DSI
  /* Command mode setting */
  //1 Three lane or Four lane
                params->dsi.LANE_NUM                            = LCM_FOUR_LANE;
                //The following defined the fomat for data coming from LCD engine.
                params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
        
                // Video mode setting           
                params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

                params->dsi.vertical_sync_active                                = 1;// 3    2
                params->dsi.vertical_backporch                                  = 32;// 20   1
                params->dsi.vertical_frontporch                                 = 12; // 1  12
                params->dsi.vertical_active_line                                = FRAME_HEIGHT;
                
                params->dsi.horizontal_sync_active                              = 1;// 50  2
                params->dsi.horizontal_backporch                                = 160;
                params->dsi.horizontal_frontporch                               = 160;
                params->dsi.horizontal_active_pixel                             = FRAME_WIDTH;

                params->dsi.pll_div1=0;         // div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
                params->dsi.pll_div2=1;         // div2=0,1,2,3;div1_real=1,2,4,4       
                params->dsi.fbk_div =21;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)
}


static void lcm_init(void)
{ 
unsigned int data_array[16];
#ifdef BUILD_LK
    printf("[LK/LCM] lcm_init() enter\n");

lcm_set_gpio_output(GPIO_LCM_PWR, 1);
lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
lcm_set_gpio_output(GPIO_LCD_PWR2_EN, 1);

//VGP6 3.3V
//pmic_config_interface(0x424, 0x1, 0x1, 15); 
//pmic_config_interface(0x45a, 0x07, 0x07, 5);

#ifdef CONFIG_MTK_PMIC_MT6397
    upmu_set_rg_vgp6_vosel(0x7);
    upmu_set_rg_vgp6_sw_en(0x1);
#else
    upmu_set_rg_vgp1_vosel(0x3);//0x7 Jorge 2015.05.08 set voltage 1.8V
    upmu_set_rg_vgp1_en(0x1);
#endif
//hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM");


lcm_set_gpio_output(GPIO_LCM_STB,GPIO_OUT_ONE);
MDELAY(20);

lcm_set_gpio_output(GPIO_LCM_RST,GPIO_OUT_ONE);

MDELAY(20);   

lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ONE);


#elif (defined BUILD_UBOOT)
    // do nothing in uboot
#else
    printk("[LCM] lcm_init() enter\n");



lcm_set_gpio_output(GPIO_LCM_STB,GPIO_OUT_ONE);
	
    MDELAY(20);
    
lcm_set_gpio_output(GPIO_LCM_RST,GPIO_OUT_ONE);


	
    MDELAY(20);
   


data_array[0] = 0x47801500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x40811500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x04821500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x77831500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x0f841500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x70851500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x70861500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);

lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ONE);

	
    MDELAY(20);        	

	

#endif 
 	
}


static void lcm_suspend(void)
{
#ifdef BUILD_LK
    printf("[LK/LCM] lcm_suspend() enter\n");

lcm_set_gpio_output(GPIO_LCD_BL_EN, 0);
MDELAY(20);



lcm_set_gpio_output(GPIO_LCD_RST_EN,GPIO_OUT_ZERO);
lcm_set_gpio_output(GPIO_LCD_STB_EN,GPIO_OUT_ZERO);

lcm_set_gpio_output(GPIO_LCD_PWR, 0);
lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
lcm_set_gpio_output(GPIO_LCD_PWR2_EN, 0);


MDELAY(20); 

#elif (defined BUILD_UBOOT)
    // do nothing in uboot
#else
    printk("[LCM] lcm_suspend() enter\n");

    push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);

    lcm_set_gpio_output(GPIO_LCD_BL_EN, 0);

    MDELAY(20);

lcm_set_gpio_output(GPIO_LCD_RST_EN,GPIO_OUT_ZERO);
lcm_set_gpio_output(GPIO_LCD_STB_EN,GPIO_OUT_ZERO);

     lcm_set_gpio_output(GPIO_LCD_PWR, 0);
     lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
     lcm_set_gpio_output(GPIO_LCD_PWR2_EN, 0);

if(fgisFirst == TRUE)
{
     fgisFirst = FALSE;
    #ifdef CONFIG_MTK_PMIC_MT6397
        hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM");
    #else
        hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_1800, "LCM");
    #endif
}

    MDELAY(20);     
#ifdef CONFIG_MTK_PMIC_MT6397
	hwPowerDown(MT65XX_POWER_LDO_VGP6, "LCM");//
#else
	hwPowerDown(MT6323_POWER_LDO_VGP1, "LCM");//
#endif
	
    MDELAY(20); 

#endif
  
}


static void lcm_resume(void)
{
unsigned int data_array[16];
#ifdef BUILD_LK
    printf("[LK/LCM] lcm_resume() enter\n");
//VGP6 3.3V

#ifdef CONFIG_MTK_PMIC_MT6397
    upmu_set_rg_vgp6_vosel(0x7);
    upmu_set_rg_vgp6_sw_en(0x1);
#else
    upmu_set_rg_vgp1_vosel(0x3);
    upmu_set_rg_vgp1_en(0x1);
#endif
//pmic_config_interface(0x424, 0x1, 0x1, 15); 
//pmic_config_interface(0x45a, 0x07, 0x07, 5);

//    hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM");

    lcm_set_gpio_output(GPIO_LCM_STB,GPIO_OUT_ONE);
   // MDELAY(20);   

    lcm_set_gpio_output(GPIO_LCM_PWR, 1);
    lcm_set_gpio_output(GPIO_LCD_PWR_EN, 1);
    lcm_set_gpio_output(GPIO_LCD_PWR2_EN, 1);
	
    MDELAY(5);


    lcm_set_gpio_output(GPIO_LCM_RST,GPIO_OUT_ONE);
    MDELAY(5);

    lcm_set_gpio_output(GPIO_LCD_BL_EN, 1);

#elif (defined BUILD_UBOOT)
    // do nothing in uboot
#else
    printk("[LCM] lcm_resume() enter\n");

#ifdef CONFIG_MTK_PMIC_MT6397
    hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM");
#else
    hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_1800, "LCM");
#endif

    MDELAY(5);	

    lcm_set_gpio_output(GPIO_LCM_PWR, 1);
    lcm_set_gpio_output(GPIO_LCD_PWR_EN, 1);
    lcm_set_gpio_output(GPIO_LCD_PWR2_EN, 1);
	
    MDELAY(5);
    
    lcm_set_gpio_output(GPIO_LCM_STB, 1);
    MDELAY(5);	
    lcm_set_gpio_output(GPIO_LCM_RST, 1);
    MDELAY(5);	


    lcm_set_gpio_output(GPIO_LCD_BL_EN, 1);
/*data_array[0] = 0x55251500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x08b11500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
    push_table(lcm_resume_setting, sizeof(lcm_resume_setting) / sizeof(struct LCM_setting_table), 1);
*/
data_array[0] = 0x47801500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x40811500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x04821500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x77831500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x0f841500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x70851500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
data_array[0] = 0x70861500;
dsi_set_cmdq(&data_array,1,1);
MDELAY(1);
//   lcm_init(); 
#endif

}

LCM_DRIVER cpt_clap070wp03xg_lvds_lcm_drv = 
{
    .name		= "cpt_clap070wp03xg_lvds",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
};

