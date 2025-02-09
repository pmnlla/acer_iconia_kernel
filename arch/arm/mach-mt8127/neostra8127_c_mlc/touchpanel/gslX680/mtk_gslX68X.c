/******************************************************************************

  Copyright (C), 2010-2012, Silead, Inc.

 ******************************************************************************
Filename      : gsl1680-d0.c
Version       : R2.0
Aurthor       : mark_huang
Creattime     : 2012.6.20
Description   : Driver for Silead I2C touchscreen.

 ******************************************************************************/

#include "tpd.h"
#include <linux/interrupt.h>
#include <cust_eint.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/time.h>

#ifdef MT6573
#include <mach/mt6573_boot.h>
#endif
#ifdef MT6575
#include <mach/mt6575_boot.h>
#include <mach/mt6575_pm_ldo.h>
#include <mach/mt6575_typedefs.h>
#endif
#ifdef MT6577
#include <mach/mt6577_boot.h>
#include <mach/mt6577_pm_ldo.h>
#include <mach/mt6577_typedefs.h>
#endif
#ifdef MT6589
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#endif

#include <mach/mt_boot_common.h>  //add by smith
#include <mach/mt_pm_ldo.h>       //add by smith
#include <mach/mt_boot.h>     
#include <mach/eint.h>    //add by smith
#include "mtk_gslX68X.h"

#define MTK_LCM_PHYSICAL_ROTATION 0  //add by smith
extern struct tpd_device *tpd;
/*
extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
extern void mt_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En, kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),kal_bool auto_umask);
*/

static struct gsl_ts_data *ddata = NULL;

static int boot_mode = NORMAL_BOOT;

#define GSL_DEV_NAME "gsl1680"

#define I2C_TRANS_SPEED 400	//100 khz or 400 khz
#define TPD_REG_BASE 0x00

#define TPD_KEY_COUNT           3
#define TPD_KEYS                {KEY_MENU,KEY_HOMEPAGE,KEY_BACK}
#define TPD_KEYS_DIM            {{80,1030,120,80},{240,1030,120,80},{460,1030,120,80}}
#define ANDROID_4

static struct tpd_bootloader_data_t g_bootloader_data;
static const struct i2c_device_id gsl_device_id[] = {{TPD_DEVICE,0},{}};
static unsigned short force[] = {0,0x80,I2C_CLIENT_END,I2C_CLIENT_END};
static const unsigned short * const forces[] = { force, NULL };
#ifdef ANDROID_4 
static struct i2c_board_info __initdata i2c_tpd = { I2C_BOARD_INFO("mtk-tpd", (0x80>>1))};
#else
static struct i2c_client_address_data addr_data = { .forces = forces, };
#endif

static volatile int gsl_halt_flag = 0;
static struct mutex gsl_i2c_lock;

#ifdef TPD_PROXIMITY
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
static u8 tpd_proximity_flag = 0; //flag whether start alps
static u8 tpd_proximity_detect = 1;//0-->close ; 1--> far away
static struct wake_lock ps_lock;
static u8 gsl_psensor_data[8]={0};
#endif

#ifdef GSL_TIMER
#undef TPD_PROC_DEBUG
#define GSL_TIMER_CHECK_CIRCLE        200
static struct delayed_work gsl_timer_check_work;
static struct workqueue_struct *gsl_timer_workqueue = NULL;
static u32 gsl_timer_data = 0;
static volatile int gsl_timer_flag = 0;  // 0:first test  1:second test  2:doing gsl_load_fw
#endif

#ifdef TPD_PROC_DEBUG
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
static struct proc_dir_entry *gsl_config_proc = NULL;
#define GSL_CONFIG_PROC_FILE "gsl_config"
#define CONFIG_LEN 31
static char gsl_read[CONFIG_LEN];
static u8 gsl_data_proc[8] = {0};
static u8 gsl_proc_flag = 0;
#endif

#ifdef GSL_COMPATIBLE_CHIP
static int gsl_compatible_flag = 0;
#endif

#ifdef GSL_THREAD_EINT
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static struct task_struct *thread = NULL;
static int tpd_flag = 0;
#endif


#define print_info(fmt, args...)   \
		do{                              \
		    printk("[tp-gsl][%s]"fmt,__func__, ##args);     \
		}while(0)
#ifdef GSL_DEBUG 
#define print_info(fmt, args...)   \
		do{                              \
		    printk("[tp-gsl][%s]"fmt,__func__, ##args);     \
		}while(0)
#else
//#define print_info(fmt, args...)   //closed by smith
#endif

#ifdef TPD_HAVE_BUTTON
extern void tpd_button(unsigned int x, unsigned int y, unsigned int down);
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

/*****************************************************************************
Prototype    	: gsl_read_interface
Description  	: gsl chip tranfer function
Input        	: struct i2c_client *client
u8 reg
u8 *buf
u32 num
Output		: None
Return Value 	: static

 *****************************************************************************/
static int gsl_read_interface(struct i2c_client *client,
        u8 reg, u8 *buf, u32 num)
{
	int err = 0;
	int i;
	u8 temp = reg;
	mutex_lock(&gsl_i2c_lock);
	if(temp < 0x80)
	{
		if(temp==0x7c){
			temp =0x78;
			i2c_master_send(client,&temp,1);	
			err = i2c_master_recv(client,&buf[0],4);
			temp = 0x7c;
		}else{
			i2c_master_send(client,&temp,1);	
			err = i2c_master_recv(client,&buf[0],4);
		}

	}
	for(i=0;i<num;)
	{	
		temp = temp + i;
		i2c_master_send(client,&temp,1);
		if((i+8)<num)
			err = i2c_master_recv(client,(buf+i),8);
		else
			err = i2c_master_recv(client,(buf+i),(num-i));
		i+=8;
	}
	mutex_unlock(&gsl_i2c_lock);

	return err;
}

/*****************************************************************************
Prototype    : gsl_write_interface
Description  : gsl chip tranfer function
Input        : struct i2c_client *client
const u8 reg
u8 *buf
u32 num
Output       : None
Return Value : static

 *****************************************************************************/
static int gsl_write_interface(struct i2c_client *client,
        const u8 reg, u8 *buf, u32 num)
{
	struct i2c_msg xfer_msg[1] = {0};
	int err;
	u8 tmp_buf[num + 1];

	tmp_buf[0] = reg;
	memcpy(tmp_buf + 1, buf, num);

	xfer_msg[0].addr = client->addr;
	xfer_msg[0].len = num + 1;
	xfer_msg[0].flags = client->flags & I2C_M_TEN;
	xfer_msg[0].buf = tmp_buf;
	xfer_msg[0].timing = I2C_TRANS_SPEED;
	mutex_lock(&gsl_i2c_lock);

	err = i2c_transfer(client->adapter, xfer_msg, 1);
	mutex_unlock(&gsl_i2c_lock);

	
	return err;
}


/*****************************************************************************
Prototype    : gsl_start_core
Description  : touchscreen chip work
Input        : struct i2c_client *client
Output       : None
Return Value : static

 *****************************************************************************/
static void gsl_start_core(struct i2c_client *client)
{
	u8 buf[4] = {0};
#if 0 
	buf[0]=0;
	buf[1]=0x10;
	buf[2]=0xfe;
	buf[3]=0x1;
	gsl_write_interface(client,0xf0,buf,4);
	buf[0]=0xf;
	buf[1]=0;
	buf[2]=0;
	buf[3]=0;
	gsl_write_interface(client,0x4,buf,4);
	msleep(20);
#endif
	buf[0]=0;
	gsl_write_interface(client,0xe0,buf,4);
#ifdef GSL_ALG_ID
	gsl_DataInit(gsl_config_data_id);
#endif
}

/*****************************************************************************
Prototype    : gsl_reset_core
Description  : touchscreen chip soft reset
Input        : struct i2c_client *client
Output       : None
Return Value : static

 *****************************************************************************/
static void gsl_reset_core(struct i2c_client *client)
{
	u8 buf[4] = {0x00};
	
	buf[0] = 0x88;
	gsl_write_interface(client,0xe0,buf,4);
	msleep(5);

	buf[0] = 0x04;
	gsl_write_interface(client,0xe4,buf,4);
	msleep(5);
	
	buf[0] = 0;
	gsl_write_interface(client,0xbc,buf,4);
	msleep(5);
}

static void gsl_clear_reg(struct i2c_client *client)
{
	u8 buf[4]={0};
	//clear reg
	buf[0]=0x88;
	gsl_write_interface(client,0xe0,buf,4);
	msleep(20);
	buf[0]=0x1;
	gsl_write_interface(client,0x80,buf,4);
	msleep(5);
	buf[0]=0x4;
	gsl_write_interface(client,0xe4,buf,4);
	msleep(5);
	buf[0]=0x0;
	gsl_write_interface(client,0xe0,buf,4);
	msleep(20);
	//clear reg
}

/*****************************************************************************
Prototype    : gsl_load_fw
Description  : gsl chip load the firmware throught I2C
Input        : struct i2c_client *client
Output       : None
Return Value : static

 *****************************************************************************/
#if 0
#define DMA_TRANS_LEN 0x20
static void gsl_load_fw(struct i2c_client *client,const struct fw_data *GSL_DOWNLOAD_DATA,int data_len)
{
	u8 buf[DMA_TRANS_LEN*4] = {0};
	u8 send_flag = 1;
	u8 addr=0;
	u32 source_line = 0;
	u32 source_len = data_len;//ARRAY_SIZE(GSL_DOWNLOAD_DATA);

	print_info("=============gsl_load_fw start==============\n");

	for (source_line = 0; source_line < source_len; source_line++) 
	{
		/* init page trans, set the page val */
		if (GSL_PAGE_REG == GSL_DOWNLOAD_DATA[source_line].offset)
		{
			memcpy(buf,&GSL_DOWNLOAD_DATA[source_line].val,4);	
			gsl_write_interface(client, GSL_PAGE_REG, buf, 4);
			send_flag = 1;
		}
		else 
		{
			if (1 == send_flag % (DMA_TRANS_LEN < 0x20 ? DMA_TRANS_LEN : 0x20))
	    			addr = (u8)GSL_DOWNLOAD_DATA[source_line].offset;

			memcpy((buf+send_flag*4 -4),&GSL_DOWNLOAD_DATA[source_line].val,4);	

			if (0 == send_flag % (DMA_TRANS_LEN < 0x20 ? DMA_TRANS_LEN : 0x20)) 
			{
	    		gsl_write_interface(client, addr, buf, DMA_TRANS_LEN * 4);
				send_flag = 0;
			}

			send_flag++;
		}
	}

	print_info("=============gsl_load_fw end==============\n");

}
#else 
static void gsl_load_fw(struct i2c_client *client,const struct fw_data *GSL_DOWNLOAD_DATA,int data_len)
{
	u8 buf[4] = {0};
	//u8 send_flag = 1;
	u8 addr=0;
	u32 source_line = 0;
	u32 source_len = data_len;//ARRAY_SIZE(GSL_DOWNLOAD_DATA);

	print_info("=============gsl_load_fw start==============\n");

	for (source_line = 0; source_line < source_len; source_line++) 
	{
		/* init page trans, set the page val */
    	addr = (u8)GSL_DOWNLOAD_DATA[source_line].offset;
		memcpy(buf,&GSL_DOWNLOAD_DATA[source_line].val,4);
    	gsl_write_interface(client, addr, buf, 4);	
	}
}
#endif

static void gsl_sw_init(struct i2c_client *client)
{
	int temp;
	static volatile int gsl_sw_flag=0;
	if(1==gsl_sw_flag)
		return;
	gsl_sw_flag=1;
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	msleep(20);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(20);

	gsl_clear_reg(client);
	gsl_reset_core(client);

	temp = ARRAY_SIZE(GSLX68X_FW);
	gsl_load_fw(client,GSLX68X_FW,temp);

	gsl_start_core(client);
	gsl_sw_flag=0;
}

/*****************************************************************************
Prototype    : check_mem_data
Description  : check mem data second to deal with power off
Input        : struct i2c_client *client
Output       : None
Return Value : static

 *****************************************************************************/
static void check_mem_data(struct i2c_client *client)
{
	char read_buf[4] = {0};
	gsl_read_interface(client, 0xb0, read_buf, 4);

	print_info("[gsl1680][%s] addr = 0xb0; read_buf = %02x%02x%02x%02x\n",
		__func__, read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
	if (read_buf[3] != 0x5a || read_buf[2] != 0x5a || read_buf[1] != 0x5a || read_buf[0] != 0x5a)
	{
		gsl_sw_init(client);
		//gsl_init_chip(client);
	}
}


#ifdef TPD_PROC_DEBUG
static int char_to_int(char ch)
{
	if(ch>='0' && ch<='9')
		return (ch-'0');
	else
		return (ch-'a'+10);
}

static int gsl_config_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *ptr = page;
	//char temp_data[4] = {0};
	char temp_data[5] = {0};
	//int i;
	unsigned int tmp=0;
	if('v'==gsl_read[0]&&'s'==gsl_read[1])
	{
#ifdef GSL_ALG_ID
		tmp=gsl_version_id();
#else 
		tmp=0x20121215;
#endif
		ptr += sprintf(ptr,"version:%x\n",tmp);
	}
	else if('r'==gsl_read[0]&&'e'==gsl_read[1])
	{
		if('i'==gsl_read[3])
		{
#ifdef GSL_ALG_ID 
			tmp=(gsl_data_proc[5]<<8) | gsl_data_proc[4];
			ptr +=sprintf(ptr,"gsl_config_data_id[%d] = ",tmp);
			if(tmp>=0&&tmp<ARRAY_SIZE(gsl_config_data_id))
				ptr +=sprintf(ptr,"%d\n",gsl_config_data_id[tmp]); 
#endif
		}
		else 
		{
			gsl_write_interface(ddata->client,0xf0,&gsl_data_proc[4],4);
			gsl_read_interface(ddata->client,gsl_data_proc[0],temp_data,4);
			ptr +=sprintf(ptr,"offset : {0x%02x,0x",gsl_data_proc[0]);
			ptr +=sprintf(ptr,"%02x",temp_data[3]);
			ptr +=sprintf(ptr,"%02x",temp_data[2]);
			ptr +=sprintf(ptr,"%02x",temp_data[1]);
			ptr +=sprintf(ptr,"%02x};\n",temp_data[0]);
		}
	}
	*eof = 1;
	return (ptr - page);
}
static int gsl_config_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
	u8 buf[8] = {0};
	char temp_buf[CONFIG_LEN];
	char *path_buf;
	int tmp = 0;
	int tmp1 = 0;
	print_info("[tp-gsl][%s] \n",__func__);
	if(count > 512)
	{
		print_info("size not match [%d:%ld]\n", CONFIG_LEN, count);
        	return -EFAULT;
	}
	path_buf=kzalloc(count,GFP_KERNEL);
	if(!path_buf)
	{
		printk("alloc path_buf memory error \n");
		return -1;
	}	
	if(copy_from_user(path_buf, buffer, count))
	{
		print_info("copy from user fail\n");
		goto exit_write_proc_out;
	}
	memcpy(temp_buf,path_buf,(count<CONFIG_LEN?count:CONFIG_LEN));
	print_info("[tp-gsl][%s][%s]\n",__func__,temp_buf);
	
	buf[3]=char_to_int(temp_buf[14])<<4 | char_to_int(temp_buf[15]);	
	buf[2]=char_to_int(temp_buf[16])<<4 | char_to_int(temp_buf[17]);
	buf[1]=char_to_int(temp_buf[18])<<4 | char_to_int(temp_buf[19]);
	buf[0]=char_to_int(temp_buf[20])<<4 | char_to_int(temp_buf[21]);
	
	buf[7]=char_to_int(temp_buf[5])<<4 | char_to_int(temp_buf[6]);
	buf[6]=char_to_int(temp_buf[7])<<4 | char_to_int(temp_buf[8]);
	buf[5]=char_to_int(temp_buf[9])<<4 | char_to_int(temp_buf[10]);
	buf[4]=char_to_int(temp_buf[11])<<4 | char_to_int(temp_buf[12]);
	if('v'==temp_buf[0]&& 's'==temp_buf[1])//version //vs
	{
		memcpy(gsl_read,temp_buf,4);
		printk("gsl version\n");
	}
	else if('s'==temp_buf[0]&& 't'==temp_buf[1])//start //st
	{
		gsl_proc_flag = 1;
		gsl_reset_core(ddata->client);
	}
	else if('e'==temp_buf[0]&&'n'==temp_buf[1])//end //en
	{
		msleep(20);
		gsl_reset_core(ddata->client);
		gsl_start_core(ddata->client);
		gsl_proc_flag = 0;
	}
	else if('r'==temp_buf[0]&&'e'==temp_buf[1])//read buf //
	{
		memcpy(gsl_read,temp_buf,4);
		memcpy(gsl_data_proc,buf,8);
	}
	else if('w'==temp_buf[0]&&'r'==temp_buf[1])//write buf
	{
		gsl_write_interface(ddata->client,buf[4],buf,4);
	}
	
#ifdef GSL_ALG_ID
	else if('i'==temp_buf[0]&&'d'==temp_buf[1])//write id config //
	{
		tmp1=(buf[7]<<24)|(buf[6]<<16)|(buf[5]<<8)|buf[4];
		tmp=(buf[3]<<24)|(buf[2]<<16)|(buf[1]<<8)|buf[0];
		if(tmp1>=0 && tmp1<ARRAY_SIZE(gsl_config_data_id))
		{
			gsl_config_data_id[tmp1] = tmp;
		}
	}
#endif
exit_write_proc_out:
	kfree(path_buf);
	return count;
}
#endif

#ifdef GSL_TIMER
static void gsl_timer_check_func(struct work_struct *work)
{	
	u8 buf[4] = {0};
	u32 tmp;
	int i,flag=0;
	static int timer_count;
	if(gsl_halt_flag == 1){
		return;
	}
	//buf[0] = 0x9f;
	//gsl_write_interface(ddata->client, GSL_PAGE_REG, buf, 4);
	gsl_read_interface(ddata->client, 0xb4, buf, 4);
	tmp = (buf[3]<<24)|(buf[2]<<16)|(buf[1]<<8)|(buf[0]);

	print_info("[pre] 0xb4 = %x \n",gsl_timer_data);
	print_info("[cur] 0xb4 = %x \n",tmp);
	print_info("gsl_timer_flag=%d\n",gsl_timer_flag);
	if(0 == gsl_timer_flag)
	{
		if(tmp==gsl_timer_data)
		{
			gsl_timer_flag = 1;
			if(0==gsl_halt_flag)
			{
				queue_delayed_work(gsl_timer_workqueue, &gsl_timer_check_work, 25);
			}
		}
		else
		{
			for(i=0;i<5;i++){
				gsl_read_interface(ddata->client,0xbc,buf,4);
				if(buf[0]==0&&buf[1]==0&&buf[2]==0&&buf[3]==0)
				{
					flag = 1;
					break;
				}
				flag =0;
			}
			if(flag == 0){
				gsl_reset_core(ddata->client);
				gsl_start_core(ddata->client);
			}
			gsl_timer_flag = 0;
			timer_count = 0;
			if(0 == gsl_halt_flag)
			{
				queue_delayed_work(gsl_timer_workqueue, 
					&gsl_timer_check_work, GSL_TIMER_CHECK_CIRCLE);
			}
		}
	}
	else if(1==gsl_timer_flag){
		if(tmp==gsl_timer_data)
		{
			if(0==gsl_halt_flag)
			{
				timer_count++;
				gsl_timer_flag = 2;
				gsl_sw_init(ddata->client);
				gsl_timer_flag = 1;
			}
			if(0 == gsl_halt_flag && timer_count < 20)
			{
				queue_delayed_work(gsl_timer_workqueue, 
					&gsl_timer_check_work, GSL_TIMER_CHECK_CIRCLE);
			}
		}
		else{
			timer_count = 0;
			if(0 == gsl_halt_flag && timer_count < 20)
			{
				queue_delayed_work(gsl_timer_workqueue, 
					&gsl_timer_check_work, GSL_TIMER_CHECK_CIRCLE);
			}
		}
		gsl_timer_flag = 0;
	}
	gsl_timer_data = tmp;
}
#endif

#ifdef TPD_PROXIMITY
static void gsl_gain_psensor_data(struct i2c_client *client)
{
	u8 buf[4]={0};
	/**************************/
	buf[0]=0x3;
	gsl_write_interface(client,0xf0,buf,4);
	gsl_read_interface(client,0,&gsl_psensor_data[0],4);
	/**************************/
	
	buf[0]=0x4;
	gsl_write_interface(client,0xf0,buf,4);
	gsl_read_interface(client,0,&gsl_psensor_data[4],4);
	/**************************/
}

static int tpd_get_ps_value(void)
{
	return tpd_proximity_detect;
}
static int tpd_enable_ps(int enable)
{
	u8 buf[4];
	if (enable) {
		wake_lock(&ps_lock);
		buf[3] = 0x00;
		buf[2] = 0x00;
		buf[1] = 0x00;
		buf[0] = 0x3;
		gsl_write_interface(ddata->client, 0xf0, buf, 4);
		buf[3] = 0x5a;
		buf[2] = 0x5a;
		buf[1] = 0x5a;
		buf[0] = 0x5a;
		gsl_write_interface(ddata->client, 0, buf, 4);

		buf[3] = 0x00;
		buf[2] = 0x00;
		buf[1] = 0x00;
		buf[0] = 0x4;
		gsl_write_interface(ddata->client, 0xf0, buf, 4);
		buf[3] = 0x0;
		buf[2] = 0x0;
		buf[1] = 0x0;
		buf[0] = 0x2;
		gsl_write_interface(ddata->client, 0, buf, 4);
		
		tpd_proximity_flag = 1;
		//add alps of function
		printk("tpd-ps function is on\n");
	} else {
		tpd_proximity_flag = 0;
		wake_unlock(&ps_lock);
		buf[3] = 0x00;
		buf[2] = 0x00;
		buf[1] = 0x00;
		buf[0] = 0x3;
		gsl_write_interface(ddata->client, 0xf0, buf, 4);
		buf[3] = gsl_psensor_data[3];
		buf[2] = gsl_psensor_data[2];
		buf[1] = gsl_psensor_data[1];
		buf[0] = gsl_psensor_data[0];
		gsl_write_interface(ddata->client, 0, buf, 4);

		buf[3] = 0x00;
		buf[2] = 0x00;
		buf[1] = 0x00;
		buf[0] = 0x4;
		gsl_write_interface(ddata->client, 0xf0, buf, 4);
		buf[3] = gsl_psensor_data[7];
		buf[2] = gsl_psensor_data[6];
		buf[1] = gsl_psensor_data[5];
		buf[0] = gsl_psensor_data[4];
		gsl_write_interface(ddata->client, 0, buf, 4);
		printk("tpd-ps function is off\n");
	}
	return 0;
}

int tpd_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
        void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data *sensor_data;

	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				printk("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				printk("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value)
				{
					if((tpd_enable_ps(1) != 0))
					{
						printk("enable ps fail: %d\n", err);
						return -1;
					}
				//					set_bit(CMC_BIT_PS, &obj->enable);
				}
				else
				{
					if((tpd_enable_ps(0) != 0))
					{
						printk("disable ps fail: %d\n", err);
						return -1;
					}
				//					clear_bit(CMC_BIT_PS, &obj->enable);
				}
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				printk("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;

				sensor_data->values[0] = tpd_get_ps_value();
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			}
			break;

		default:
			printk("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;

}
#endif



/*****************************************************************************
Prototype    : gsl_report_point
Description  : gsl1680 report touch event
Input        : union gsl_touch_info *ti
Output       : None
Return Value : static

 *****************************************************************************/
//static void gsl_report_point(union gsl_touch_info *ti)
static void gsl_report_point(struct gsl_touch_info *ti)
{
    int tmp = 0;
	static int gsl_up_flag = 0; //prevent more up event
    print_info("gsl_report_point %d \n", ti->finger_num);
	printk("smith-gsl-2 x is %d y is %d\n",ti->x[tmp],ti->y[tmp]);

    if (unlikely(ti->finger_num == 0)) 
    {
    	if(gsl_up_flag == 0)
			return;
    	gsl_up_flag = 0;
        input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
        input_report_key(tpd->dev, BTN_TOUCH, 0);
        input_mt_sync(tpd->dev);
	    if (FACTORY_BOOT == get_boot_mode()|| 
			RECOVERY_BOOT == get_boot_mode())
		{   

			tpd_button(ti->x[tmp], ti->y[tmp], 0);

		}
	} 
	else 
	{
		gsl_up_flag = 1;
		for (tmp = 0; ti->finger_num > tmp; tmp++) 
		{
			print_info("[gsl1680](x[%d],y[%d]) = (%d,%d);\n", 
				ti->id[tmp], ti->id[tmp], ti->x[tmp], ti->y[tmp]);
			input_report_key(tpd->dev, BTN_TOUCH, 1);
			input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 1);

			if(boot_mode!=NORMAL_BOOT && (MTK_LCM_PHYSICAL_ROTATION == 270 
				|| MTK_LCM_PHYSICAL_ROTATION == 90) )
			{
				
				int temp;
				temp = ti->y[tmp];
				ti->y[tmp] = ti->x[tmp];
				ti->x[tmp] = TPD_RES_X-temp;
			
				
			}

			if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
			{ 
				tpd_button(ti->x[tmp], ti->y[tmp], 1);  
			}
		//	printk("smith-gsl x is %d,y is %d\n",ti->x[tmp],ti->y[tmp]);
			printk("smith-270\n");
			/*
		       int temp;
			temp = ti->y[tmp];
			ti->y[tmp]= ti->x[tmp];
			ti->x[tmp]= temp;
			ti->x[tmp]= 600-ti->x[tmp];
			*/
			
			printk("smith-gsl-last x is %d,y is %d\n",ti->x[tmp],ti->y[tmp]);
		//	printk("TPD_RES_X is %d,TPD_RES_Y is %d\n",TPD_RES_X,TPD_RES_Y);
			
		//	ti->y[tmp]=TPD_RES_Y-ti->y[tmp];
			input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, ti->id[tmp] - 1);
			input_report_abs(tpd->dev, ABS_MT_POSITION_X, ti->x[tmp]);
			input_report_abs(tpd->dev, ABS_MT_POSITION_Y, ti->y[tmp]);

			input_mt_sync(tpd->dev);
		}
	}
	input_sync(tpd->dev);
}

/*****************************************************************************
Prototype    : gsl_report_work
Description  : to deal interrupt throught workqueue
Input        : struct work_struct *work
Output       : None
Return Value : static

 *****************************************************************************/
static void gsl_report_work(void)
{

	u8 buf[4] = {0};
	u8 i = 0;
	u16 ret = 0;
	u16 tmp = 0;
	struct gsl_touch_info cinfo={0};
	struct gsl_touch_info* test=&cinfo;  //add by smith
	u8 tmp_buf[44] ={0};
	print_info("enter gsl_report_work\n");
#ifdef TPD_PROXIMITY
	int err;
	hwm_sensor_data sensor_data;
    /*added by bernard*/
	if (tpd_proximity_flag == 1)
	{
	
		gsl_read_interface(ddata->client,0xac,buf,4);
		if (buf[0] == 1 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0)
		{
			tpd_proximity_detect = 0;
			//sensor_data.values[0] = 0;
		}
		else
		{
			tpd_proximity_detect = 1;
			//sensor_data.values[0] = 1;
		}
		//get raw data
		print_info(" ps change\n");
		//map and store data to hwm_sensor_data
		sensor_data.values[0] = tpd_get_ps_value();
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
		//let up layer to know
		if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
		{
			print_info("call hwmsen_get_interrupt_data fail = %d\n", err);
		}
	}
	/*end of added*/
#endif

	
#ifdef GSL_TIMER 
	if(2==gsl_timer_flag){
		return;
	}
#endif

#ifdef TPD_PROC_DEBUG
	if(gsl_proc_flag == 1){
		return;
	}
#endif	
#ifdef GSL_IRQ_CHECK
	gsl_read_interface(ddata->client,0xbc,buf,4);
	if(0x80==buf[3]&&0==buf[2]&&0==buf[1]&&0==buf[0])
	{
		gsl_reset_core(ddata->client);
		gsl_start_core(ddata->client);
		msleep(20);
		check_mem_data(ddata->client);
		goto gsl_report_work_out;
	}
#endif

 	gsl_read_interface(ddata->client, 0x80, tmp_buf, 8);
	if(tmp_buf[0]>=2&&tmp_buf[0]<=10)
		gsl_read_interface(ddata->client, 0x88, &tmp_buf[8], (tmp_buf[0]*4-4));
	cinfo.finger_num = tmp_buf[0] & 0x0f;
	print_info("tp-gsl  finger_num = %d\n",cinfo.finger_num);
	for(tmp=0;tmp<(cinfo.finger_num>10?10:cinfo.finger_num);tmp++)
	{
		cinfo.id[tmp] = tmp_buf[tmp*4+7] >> 4;
		cinfo.y[tmp] = (tmp_buf[tmp*4+4] | ((tmp_buf[tmp*4+5])<<8));
		cinfo.x[tmp] = (tmp_buf[tmp*4+6] | ((tmp_buf[tmp*4+7] & 0x0f)<<8));
		print_info("smith-gsl-original  x = %d y = %d \n",cinfo.x[tmp],cinfo.y[tmp]);
     
		
	}
#ifdef GSL_ALG_ID
	int tmp1 = 0;
	cinfo.finger_num = (tmp_buf[3]<<24)|(tmp_buf[2]<<16)|(tmp_buf[1]<<8)|(tmp_buf[0]);
	gsl_alg_id_main(&cinfo);
	tmp1=gsl_mask_tiaoping();
	print_info("[tp-gsl] tmp1=%x\n",tmp1);
	if(tmp1>0&&tmp1<0xffffffff)
	{
		buf[0]=0xa;
		buf[1]=0;
		buf[2]=0;
		buf[3]=0;
		gsl_write_interface(ddata->client,0xf0,buf,4);
		buf[0]=(u8)(tmp1 & 0xff);
		buf[1]=(u8)((tmp1>>8) & 0xff);
		buf[2]=(u8)((tmp1>>16) & 0xff);
		buf[3]=(u8)((tmp1>>24) & 0xff);
		printk("tmp1=%08x,buf[0]=%02x,buf[1]=%02x,buf[2]=%02x,buf[3]=%02x\n",
			tmp1,buf[0],buf[1],buf[2],buf[3]);
		gsl_write_interface(ddata->client,0x8,buf,4);
	}
#endif
	if(MTK_LCM_PHYSICAL_ROTATION == 270 || MTK_LCM_PHYSICAL_ROTATION == 90)
	{
		print_info("MTK_LCM_PHYSICAL_ROTATION = %d\n",MTK_LCM_PHYSICAL_ROTATION);
		for(i = 0;i < 5 ;i++)
		{                                //closed by smith
			ret = cinfo.x[i];
			cinfo.x[i] = cinfo.y[i];
			cinfo.y[i] = ret;
			
		}
	}

	printk("smith-gsl x is %d, y is %d\n",test->x[tmp],test->y[tmp]);
	gsl_report_point(&cinfo);
	
gsl_report_work_out:
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

}


#ifdef GSL_THREAD_EINT
static int touch_event_handler(void *unused)
{
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	sched_setscheduler(current, SCHED_RR, &param);
	do
	{
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);
		tpd_flag = 0;
		TPD_DEBUG_SET_TIME;
		set_current_state(TASK_RUNNING);
		gsl_report_work();
	} while (!kthread_should_stop());	
	return 0;
}

#endif

/*****************************************************************************
Prototype    : tpd_eint_interrupt_handler
Description  : gsl1680 ISR
Input        : None
Output       : None
Return Value : static

 *****************************************************************************/
static int tpd_eint_interrupt_handler(void)
{

	print_info("[gsl1680] TPD interrupt has been triggered\n");
	printk("smith-8127 interrupt is triggered\n");

	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#ifdef GSL_THREAD_EINT
	tpd_flag=1; 
    wake_up_interruptible(&waiter);
#else
	if (!work_pending(&ddata->work)) {
		queue_work(ddata->wq, &ddata->work);
	}
#endif
}

/*****************************************************************************
Prototype    : gsl_hw_init
Description  : gsl1680 set gpio
Input        : None
Output       : None
Return Value : static

 *****************************************************************************/
static void gsl_hw_init(void)
{
	//power on
#ifdef MT6573
	mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
#endif
#ifdef MT6575
	hwPowerOn(mt_POWER_LDO_VGP2, VOL_2800, "TP");
#endif
#ifdef MT6577
	hwPowerOn(mt_POWER_LDO_VGP, VOL_2800, "TP"); 
#endif	

#ifdef MT6589
	hwPowerOn(mt_POWER_LDO_VGP4, VOL_2800, "TP");
#endif

#ifdef MT8389
	hwPowerOn(mt_POWER_LDO_VGP4, VOL_2800, "TP");
#endif	
	printk("smith-touchpanel:start to poweron\n");
  hwPowerOn(MT6323_POWER_LDO_VGP2,VOL_2800,"TP");

	/* reset ctp gsl1680 */
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	msleep(20);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	/* set interrupt work mode */
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
	msleep(100);
}

/*****************************************************************************
Prototype    : gsl_sw_init
Description  : gsl1680 load firmware
Input        : struct i2c_client *client
Output       : int
Return Value : static

 *****************************************************************************/


#ifdef GSL_COMPATIBLE_CHIP
static int gsl_compatible_id(struct i2c_client *client)
{
	int i,err;
	u8 buf[4]={0};
	for(i=0;i<5;i++)
	{
		err=gsl_read_interface(client,0xfc,buf,4);
		if(err>0)
		{
			printk("[tp-gsl] i2c read 0xfc = 0x%02x%02x%02x%02x\n",
				buf[3],buf[2],buf[1],buf[0]);
			break;
		}
	}
	return (err<0?-1:0);
}
#endif

/*****************************************************************************
Prototype    : gsl_probe
Description  : setup gsl1680 driver
Input        : struct i2c_client *client
const struct i2c_device_id *id
Output       : None
Return Value : static

 *****************************************************************************/
//static int __devinit gsl_probe(struct i2c_client *client, const struct i2c_device_id *id)   //closed by smith
static int  gsl_probe(struct i2c_client *client, const struct i2c_device_id *id)      //add by smith
{
	int i,err;
	unsigned char tp_data[4];
#ifdef TPD_PROXIMITY
	struct hwmsen_object obj_ps;
#endif

	print_info();

	ddata = kzalloc(sizeof(struct gsl_ts_data), GFP_KERNEL);
	if (!ddata) {
		print_info("alloc ddata memory error\n");
		return -ENOMEM;
	}
	mutex_init(&gsl_i2c_lock);
	ddata->client = client;
	print_info("ddata->client->addr = 0x%x \n",ddata->client->addr);
	gsl_hw_init();

	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

	i2c_set_clientdata(ddata->client, ddata);

#ifdef GSL_COMPATIBLE_CHIP
	err = gsl_compatible_id(ddata->client);
	if(err<0)
		goto  err_malloc;
#endif
	gsl_sw_init(ddata->client);
	msleep(20);
	check_mem_data(ddata->client);

#ifdef GSL_THREAD_EINT
	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread)) {
		//err = PTR_ERR(thread);
		TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %ld\n", PTR_ERR(thread));
	}
#else
	INIT_WORK(&ddata->work, gsl_report_work);
	ddata->wq = create_singlethread_workqueue(GSL_DEV_NAME);
	if (!(ddata->wq)) 
	{
		print_info("can't create workqueue\n");
	}
#endif

//	mt_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);   //closed by smith
//	mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);  //closed by smith
#if 1
//	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM,
//		CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY,
//		tpd_eint_interrupt_handler, 1);
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_FALLING, tpd_eint_interrupt_handler, 1); //add by smith
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);               //add by smith
	msleep(50);                                              //add by smith
#else
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM,
		CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_POLARITY_HIGH,
		tpd_eint_interrupt_handler, 1);
#endif
#ifdef GSL_TIMER
	INIT_DELAYED_WORK(&gsl_timer_check_work, gsl_timer_check_func);
	gsl_timer_workqueue = create_workqueue("gsl_esd_check");
	queue_delayed_work(gsl_timer_workqueue, &gsl_timer_check_work, GSL_TIMER_CHECK_CIRCLE);
#endif
#ifdef TPD_PROC_DEBUG
	gsl_config_proc = create_proc_entry(GSL_CONFIG_PROC_FILE, 0666, NULL);
	if (gsl_config_proc == NULL)
	{
		print_info("create_proc_entry %s failed\n", GSL_CONFIG_PROC_FILE);
	}
	else
	{
		gsl_config_proc->read_proc = gsl_config_read_proc;
		gsl_config_proc->write_proc = gsl_config_write_proc;
	}
	gsl_proc_flag = 0;
#endif


#ifdef GSL_ALG_ID
	gsl_DataInit(gsl_config_data_id);
#endif
#ifdef TPD_PROXIMITY
	//obj_ps.self = gsl1680p_obj;
	//	obj_ps.self = cm3623_obj;
	obj_ps.polling = 0;//interrupt mode
	//obj_ps.polling = 1;//need to confirm what mode is!!!
	obj_ps.sensor_operate = tpd_ps_operate//gsl1680p_ps_operate;
	if((err = hwmsen_attach(ID_PROXIMITY, &obj_ps)))
	{
		printk("attach fail = %d\n", err);
	}
	
	gsl_gain_psensor_data(ddata->client);
	wake_lock_init(&ps_lock, WAKE_LOCK_SUSPEND, "ps wakelock");
#endif


	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	
	tpd_load_status = 1;

	return 0;

//err_xfer:
	//   kfree(ddata->ti);
err_malloc:
	if (ddata)
		kfree(ddata);

	return err;
}

/*****************************************************************************
Prototype    : gsl_remove
Description  : remove gsl1680 driver
Input        : struct i2c_client *client
Output       : int
Return Value : static

 *****************************************************************************/
//static int __devexit gsl_remove(struct i2c_client *client)  //closed by smith
static int  gsl_remove(struct i2c_client *client)      //add by smith
{
	print_info("[gsl1680] TPD removed\n");
	return 0;
}

/*****************************************************************************
Prototype    : gsl_detect
Description  : gsl1680 driver local setup without board file
Input        : struct i2c_client *client
int kind
struct i2c_board_info *info
Output       : int
Return Value : static

 *****************************************************************************/
static int gsl_detect (struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	int error;

	print_info("%s, %d\n", __FUNCTION__, __LINE__);
	strcpy(info->type, TPD_DEVICE);
	return 0;
}

static struct i2c_driver gsl_i2c_driver = {
    .driver = {
		.name = TPD_DEVICE,
		.owner = THIS_MODULE,
    },
	.probe = gsl_probe,
//	.remove = __devexit_p(gsl_remove), //closed by smith
	.remove = gsl_remove,   //add by smith
	.id_table = gsl_device_id,
	.detect = gsl_detect,
#ifndef ANDROID_4
	.address_data = &addr_data,
#endif
};

/*****************************************************************************
Prototype    : gsl_local_init
Description  : setup gsl1680 driver
Input        : None
Output       : None
Return Value : static

 *****************************************************************************/
static int gsl_local_init(void)
{
	int ret;
	print_info();
	boot_mode = get_boot_mode();
	print_info("boot_mode == %d \n", boot_mode);

	if (boot_mode == SW_REBOOT)
	boot_mode = NORMAL_BOOT;

#ifdef TPD_HAVE_BUTTON
	print_info("TPD_HAVE_BUTTON\n");
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);
#endif

	ret = i2c_add_driver(&gsl_i2c_driver);

	if (ret < 0) {
		print_info("unable to i2c_add_driver\n");
		return -ENODEV;
	}

	if (tpd_load_status == 0) 
	{
		print_info("tpd_load_status == 0, gsl_probe failed\n");
		i2c_del_driver(&gsl_i2c_driver);
		return -ENODEV;
	}

	/* define in tpd_debug.h */
	tpd_type_cap = 1;
	print_info("end %s, %d\n", __FUNCTION__, __LINE__);
	return 0;
}

/*****************************************************************************
Prototype    : gsl_suspend
Description  : gsl chip power manage, device goto sleep
Input        : struct i2c_client *client
Output       : int
Return Value : static

 *****************************************************************************/
static void gsl_suspend(struct i2c_client *client)
{
	int tmp;
	print_info();
	
#ifdef TPD_PROXIMITY
    if (tpd_proximity_flag == 1)
    {
        return 0;
    }
#endif
	gsl_halt_flag = 1;
	//version info
	printk("[tp-gsl]the last time of debug:%x\n",TPD_DEBUG_TIME);
#ifdef GSL_ALG_ID
	tmp = gsl_version_id();	
	printk("[tp-gsl]the version of alg_id:%x\n",tmp);
#endif
	
	//version info
	
#ifdef TPD_PROC_DEBUG
	if(gsl_proc_flag == 1){
		return;
	}
#endif
#ifdef GSL_TIMER	
	cancel_delayed_work_sync(&gsl_timer_check_work);
	if(2==gsl_timer_flag){
		return;
	}
#endif


	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
//	gsl_reset_core(ddata->client);
//	msleep(20);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
}

/*****************************************************************************
Prototype    : gsl_suspend
Description  : gsl chip power manage, wake up device
Input        : struct i2c_client *client
Output       : int
Return Value : static

 *****************************************************************************/
static void gsl_resume(struct i2c_client *client)
{
    print_info();

#ifdef TPD_PROXIMITY
    if (tpd_proximity_flag == 1)
    {
        tpd_enable_ps(1);
        return;
    }
#endif

#ifdef GSL_TIMER
	if(2==gsl_timer_flag)
	{
		gsl_halt_flag=0;
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		return;
	}
#endif

#ifdef TPD_PROC_DEBUG
	if(gsl_proc_flag == 1){
		return;
	}
#endif


	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(20);
	gsl_reset_core(ddata->client);
	gsl_start_core(ddata->client);
	msleep(20);
	check_mem_data(ddata->client);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#ifdef GSL_TIMER
	queue_delayed_work(gsl_timer_workqueue, &gsl_timer_check_work, GSL_TIMER_CHECK_CIRCLE);
	gsl_timer_flag = 0;
#endif
	gsl_halt_flag = 0;

}


static struct tpd_driver_t gsl_driver = {
	.tpd_device_name = GSL_DEV_NAME,
	.tpd_local_init = gsl_local_init,
	.suspend = gsl_suspend,
	.resume = gsl_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
 	.tpd_have_button = 0,
#endif
};

/*****************************************************************************
Prototype    : gsl_driver_init
Description  : driver module entry
Input        : None
Output       : int
Return Value : static

 *****************************************************************************/
static int __init gsl_driver_init(void)
{
	int ret;

	print_info("[%s]I2C Touchscreen Driver (Built %s @ %s)\n",__func__, __DATE__, __TIME__);
	print_info();
#ifdef ANDROID_4
	i2c_register_board_info(1, &i2c_tpd, 1);
#endif
	if(ret = tpd_driver_add(&gsl_driver) < 0)
		print_info("gsl_driver init error, return num is %d \n", ret);

	return ret;
}


/*****************************************************************************
Prototype    : gsl_driver_exit
Description  : driver module exit
Input        : None
Output       : None
Return Value : static

 *****************************************************************************/
static void __exit gsl_driver_exit(void)
{
	print_info();
	tpd_driver_remove(&gsl_driver);
}

module_init(gsl_driver_init);
module_exit(gsl_driver_exit);

