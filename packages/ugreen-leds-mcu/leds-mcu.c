/*
 *   Author:swing.chen@ugreen.com
 *   date:2024/8/6
 *   描述:
 *       由于iDX601x的通讯协议稍微改变，但原来的LED驱动代码改动地方.
 *       源码使用方法：
 *       1. 添加板级信息
 *       2. 添加点灯接口
 *       3. 设置板级初始化
 */

#include <linux/cdev.h>
#include <linux/container_of.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gfp_types.h>
#include <linux/iio/types.h>
#include <linux/kernel.h>
#include <linux/kstrtox.h>
#include <linux/mod_devicetable.h>
#include <linux/notifier.h>
#include <linux/printk.h>
#include <linux/reboot.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/jiffies.h>
#include <linux/dmi.h>

// #define iDX601x_MCU_DEBUG

#define CONFIG_UG_FEAT_LED_VERSION          "0.1"
#define CONFIG_UG_FEAT_PRODUCT_NAME_MAX     24

#define LED_I2C_ADDRESS 0x3a
#define LED_NAME_LENGTH_MAX     24
#define LED_BRIGHTNESS_MAX		255

#define LED_MODE_LIGHT      0x01
#define LED_MODE_COLOR      0x02
#define LED_MODE_SWTICH     0x03
#define LED_MODE_BLINK      0x04
#define LED_MODE_BREATH     0x05

#define LED_STATUS_OFF      0x00
#define LED_STATUS_NO       0x01
#define LED_STATUS_BLINK    0x02
#define LED_STATUS_BREATH   0x03

#define ACTION_REBOOT		1
#define ACTION_POWEROFF		3

#define MCU_CHIP_ID		0x5A
#define MCU_FW_VER		0x5D
#define MCU_RECEIVE_FLAGS       0x80
#define DXP2800_LED_COUNT       4
#define DXP6800_LED_COUNT       8   
#define IDX6011_LED_COUNT       8
#define IDX6011PRO_LED_COUNT    9
#define DXP4800_GT_LED_COUNT    6 

struct ugreen_led_device ug_led_client;
enum UG_PRODUCT{
	PRODUCT_DXP2800,
	PRODUCT_DXP4800,
	PRODUCT_DXP4800_Plus,
	PRODUCT_DXP6800,
	PRODUCT_DXP6800S,
	PRODUCT_DXP8800,
	PRODUCT_IDX6011,
	PRODUCT_IDX6011_pro,
	PRODUCT_IDX6012,
	PRODUCT_DXP4800_GT,
};
const u8 PRODUCT_NAME[][CONFIG_UG_FEAT_PRODUCT_NAME_MAX] = {
	"DXP2800",
	"DXP4800",
	"DXP4800 Plus",
	"DXP6800",
	"DXP6800S",
	"DXP8800",
	"iDX6011",
	"iDX6011 Pro",
	"iDX6012",
	"DXP4800 GT",
	};
const u8 dxp2800_led_name[DXP2800_LED_COUNT][LED_NAME_LENGTH_MAX] = 
{
    "power","network_stat","disk1","disk2"
};

const u8 iDX6011_led_name[IDX6011_LED_COUNT][LED_NAME_LENGTH_MAX] = 
{
	"power","network_stat","disk1","disk2",
	"disk3","disk4","disk5","disk6"
};

const u8 iDX6011_pro_led_name[IDX6011PRO_LED_COUNT][LED_NAME_LENGTH_MAX] = 
{
	"power","network_stat","network_stat2","disk1","disk2",
	"disk3","disk4","disk5","disk6"
};

const u8 dxp6800_led_name[DXP6800_LED_COUNT][LED_NAME_LENGTH_MAX] = 
{
    "power","network_stat","disk1","disk2",
    "disk3","disk4","disk5","disk6"
};

const u8 dxp4800_gt_led_name[DXP4800_GT_LED_COUNT][LED_NAME_LENGTH_MAX] = 
{
	"power","network_stat","disk1","disk2",
	"disk3","disk4"
};

static u8 led_color_table[5][3] = {
  {255, 255, 255},  /* white  180 */   
  {220, 40,  00},   /* oringe  255*/
  {255, 00,  00},    /*   */
  {00,  255, 00},    /*   */
  {00,  00, 255}    /*   */
};

/* Debug 的预留接口，方便日后规范打印 */
#define CONFIG_UG_FEAT_PRINT(fmt, ...)  \
    printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)

struct ugreen_led_device {
	struct i2c_client *i2c_client;
	struct led_classdev *cdev;
	struct mutex mutex; //互斥锁 
	struct led_status *status;
	int led_count;      //led灯数量
	const unsigned char *product;
	int (*i2c_write_led)(struct i2c_client *client,u8 addrs,u8 status,
		u8 data1,u8 data2,u8 data3,u8 data4);
	int (*i2c_read_led)(struct i2c_client *client,struct led_status *status);
};
struct led_status {
	struct work_struct work;
	unsigned long delay_work_time;
	unsigned int oneshort_cnt;
	const unsigned char *name;
	unsigned char trige_name[48];
	unsigned char led_addr;
	unsigned char mode;
	u32 status_data;
	unsigned short int light; /* 亮度 */
	unsigned char rgb[3]; //rgb[0]:red rgb[1]:green [2]:blue
	unsigned short int time;//呼吸灯或者闪烁的时间
	unsigned short int time_light;//呼吸状态或者闪烁状态的亮度和亮度,2 Byte
};

static int ug_notifier_call(struct notifier_block *nb,unsigned long action, void *data){
    struct ugreen_led_device *ug_dev = &ug_led_client;
    unsigned char shutdown_ligh[3] = {0x0a,0x55,0xaa};

    if (!ug_dev) {
		CONFIG_UG_FEAT_PRINT("ug_dev is null.\n");
		return 0;
    }

    /* 重启 action = 1 ;poweroff = 3 */
    if (ACTION_REBOOT == action) {
        if (!strcmp(ug_dev->product,PRODUCT_NAME[PRODUCT_IDX6011]) || !strcmp(ug_dev->product, PRODUCT_NAME[PRODUCT_DXP2800]) ||
            !strcmp(ug_dev->product,PRODUCT_NAME[PRODUCT_IDX6012]) || !strcmp(ug_dev->product, PRODUCT_NAME[PRODUCT_DXP6800]) ||
            !strcmp(ug_dev->product,PRODUCT_NAME[PRODUCT_DXP6800S]) ||
            !strcmp(ug_dev->product,PRODUCT_NAME[PRODUCT_IDX6011_pro]) || !strcmp(ug_dev->product,PRODUCT_NAME[PRODUCT_DXP4800_GT])) {

            i2c_smbus_write_block_data(ug_dev->i2c_client,0x0a,
                sizeof(shutdown_ligh) - 1,shutdown_ligh + 1);    /*关机执行后，开启跑马灯 */
        }
    }

    return 0;
}

static struct notifier_block ug_led_notifier = {
    .notifier_call = ug_notifier_call
};
/*
	author:Swing.Chen
	date:2024/7/31
	函数说明： 
	static int i2c_smbus_write_mcu(struct i2c_client *client,char select,char status,
								char data1,char dat2,char data3,char data4)
	@client:i2c_client
	@select: select light
	@status: mode_flags
	@data(1 ~ 4 Byte): big endian
	@return: write success count byte;
 */
static int i2c_iDX601x_write_led(struct i2c_client *client,u8 select,u8 status,
								u8 data1,u8 data2,u8 data3,u8 data4)
{
	int ret;
	int crc_cnt = 0;
	int i;
    int send_cnt = 0;
	u8 data[12]={0x00,0xa0,0x01,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00};
	
	data[0] = select;
	data[5] = status;
	data[6] = data1;data[7] = data2;data[8] = data3;data[9] = data4;
	for (i = 1;i < 10 ;i ++)
		crc_cnt += data[i];
	data[10] = (char)((crc_cnt & 0xff00) >> 8);
	data[11] = (char)(crc_cnt & 0xff);

	send_cnt = 0;
	while (send_cnt < 3) {
        
		ret = i2c_smbus_write_block_data(client,data[0],11,data +1);
        if (ret < 0)
            send_cnt += 1;
        else {
            #ifdef iDX601x_MCU_DEBUG
            CONFIG_UG_FEAT_PRINT("I2C SEND CALL:i2c addr:0x%x,crc_cnt:0x%x",client->addr,crc_cnt);
            CONFIG_UG_FEAT_PRINT("I2C SEND CALL BUFFER:"
            "0x%.02x 0x%.02x 0x%.02x 0x%.02x 0x%.02x 0x%.02x 0x%.02x 0x%.02x 0x%.02x 0x%.02x 0x%.02x 0x%.02x"
                ,data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]
                ,data[8],data[9],data[10],data[11]);
            #endif
            break;
        }
        msleep(5);
    }  
    if (send_cnt >= 3) {
        /* CONFIG_UG_FEAT_PRINT("I2C SEND DATA FAILED"); */
        return  -EBUSY;
    } 
    send_cnt = 0;
    while (send_cnt < 3) {
        ret = i2c_smbus_read_byte_data(client,MCU_RECEIVE_FLAGS);
        /* CONFIG_UG_FEAT_PRINT("i2c read status:%d",ret); */
        if(ret != 1)
            send_cnt += 1;
        else
            break;
        msleep(5);
    }
    if (send_cnt >= 3) {
        /* CONFIG_UG_FEAT_PRINT("I2C READ STATUS SUCCESS,BUT READ STATUS FAILED"); */
        return -EBUSY;
    }
    return ret;
}

static int i2c_iDX601x_read_led(struct i2c_client *client,struct led_status *status)
{
	unsigned char data[11];
	int ret = 0;
	int i;
	int crc_cnt = 0;
    int send_cnt = 0;
	
	memset(data,0,sizeof(data));
    while (send_cnt < 3) {
        ret = i2c_smbus_read_i2c_block_data(client,status->led_addr + 0x81,11,data);    
        if(ret != sizeof(data))
            send_cnt += 1;
	    else
            break;
        msleep(5);
    }
    if (send_cnt >= 3) {
        /* CONFIG_UG_FEAT_PRINT("I2C READ DATA FILED"); */
        return -EBUSY;
    }
	
    send_cnt = 0;
	while (send_cnt < 3) {
        ret = i2c_smbus_read_byte_data(client,MCU_RECEIVE_FLAGS);
        if(ret != 1)
            send_cnt += 1;
        else
            break;
        msleep(5);
    }
    if(send_cnt >= 3){
        // CONFIG_UG_FEAT_PRINT("I2C READ STATUS FAILE");
        return -EBUSY;
    }
#ifdef iDX601x_MCU_DEBUG
	CONFIG_UG_FEAT_PRINT("I2C READ BUFFER FOR 0x%x 0x%x:"
		"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
		client->addr,status->led_addr + 0x81,
		data[0],
		data[1],data[2],
		data[3],data[4],
		data[5],data[6],data[7],data[8],
		data[9],data[10]);
#endif
	for (i = 0; i < sizeof(data) - 2; i++)
		crc_cnt += data[i];
	if (crc_cnt != (data[9] <<8 | data[10])) {
		// CONFIG_UG_FEAT_PRINT("I2C READ FAILED:crc check error,should crc:0x%x,But read crc:%x",crc_cnt,data[9] << 8 | data[10]);
		return -EBUSY;
	}
	// status->mode = data[0];
	// if(status->mode == LED_MODE_BLINK|| )
	status->light = data[1];
	status->status_data = data[2] << 16 | data[3] << 8 | data[4];
	status->rgb[0] = data[2];
	status->rgb[1] = data[3];
	status->rgb[2] = data[4];
	status->time = data[5] << 8 | data[6];
	status->time_light = data[7] << 8 | data[8];
	return ret;
}

static void leds_brightness_set(struct led_classdev *cdev, enum led_brightness brightness)
{
	struct ugreen_led_device *ug_dev = &ug_led_client;
	struct led_status *status;
	int i = 0;
	
	// 添加边界检查
	if (!ug_dev || !ug_dev->status || !cdev) {
		return;
	}
	
	// 查找对应的 LED 设备
	for (i = 0; i < ug_dev->led_count; i++)
        if (cdev == (ug_dev->cdev + i))
            break;
    
    // 边界检查
    if (i >= ug_dev->led_count) {
    	return;
    }
    
    status = ug_dev->status + i;
    
    if (brightness == 1) {
        status->light = LED_FULL;
        brightness = status->light;
    }else
        status->light = brightness;
    // mutex_unlock(&ug_dev->mutex);
    schedule_work(&status->work);
}

static int leds_blink_set(struct led_classdev *cdev,unsigned long *delay_on,unsigned long *delay_off)
{
    struct ugreen_led_device *ug_dev = &ug_led_client;
    struct led_status *status;
    u8 tmp_data[4];
    int i = 0;
    int ret;
	
    if (cdev->trigger == NULL || cdev->trigger->name == NULL)
        return -EINVAL;
    mutex_lock(&ug_dev->mutex);
    memset(tmp_data,0,sizeof(tmp_data));
    for (i = 0;i < ug_dev->led_count;i++)
        if(cdev == (ug_dev->cdev + i))
            break;
    

    status = ug_dev->status + i;
    if (!strcmp(cdev->trigger->name,"normal")) {
        status->mode = LED_MODE_LIGHT;
        status->status_data = 0;
        *delay_off = 0;
        *delay_on = 0;
        if (ug_dev->i2c_write_led(ug_dev->i2c_client,status->led_addr,
            LED_MODE_SWTICH,
            0xff,0,0,0) < 0)
            goto FAILED;
        //变成normal后，关闭呼吸灯功能或者
    } else if (!strcmp(cdev->trigger->name,"breath")) {
        status->mode = LED_MODE_BREATH;
        if(*delay_off < 500)
            *delay_off = 500;
        if(*delay_on < 1000)
            *delay_on = 1000;
        status->status_data = (*delay_on + *delay_off) << 16;
        status->status_data |= *delay_on;
        cdev->brightness = status->light;
    } else if(!strcmp(cdev->trigger->name,"timer2")){
        status->mode = LED_MODE_BLINK;
        if(*delay_on == 0 || *delay_off ==0){
            status->status_data = 0;
        }else{
            status->status_data = (*delay_on + *delay_off) << 16;
            status->status_data |= *delay_off;
        }

     } 

    tmp_data[0] = (u8)((status->status_data & 0xff000000) >> 24);
    tmp_data[1] = (u8)((status->status_data & 0xff0000) >> 16);
    tmp_data[2] = (u8)((status->status_data & 0xff00) >> 8);
    tmp_data[3] = (u8)(status->status_data & 0xff);
#ifdef iDX601x_MCU_DEBUG
    CONFIG_UG_FEAT_PRINT("dev name:%s trigger name :%s delay_on:%lu delay_off:%lu" " data:%d all_time:%d low_time:%d",
        cdev->name,cdev->trigger->name,*delay_on,*delay_off,status->status_data,
        status->status_data >> 16,status->status_data &0xffff);
#endif
    ret =  ug_dev->i2c_write_led(ug_dev->i2c_client,status->led_addr,
            status->mode,
            tmp_data[0],tmp_data[1],tmp_data[2],tmp_data[3]); 
    if (ret < 0) {
        goto FAILED;  
    }   
    // }
    mutex_unlock(&ug_dev->mutex);
    return 0;
    FAILED:
    mutex_unlock(&ug_dev->mutex);
    return -EBUSY;
}
static ssize_t color_set(struct device *dev, 
        struct device_attribute *attr, const char *buf, size_t size)
{
    struct ugreen_led_device *ug_dev = &ug_led_client;
    struct led_status *status;
    struct led_classdev *cdev;
    int i;
    unsigned long ret;
	
    // unsigned char tmp_buf[16];
    mutex_lock(&ug_dev->mutex);
    // ret = copy_from_user(tmp_buf,buf,size);
    
    // CONFIG_UG_FEAT_PRINT("error:%s ret:%ld",buf,ret);
    ret = 0;
    cdev = ug_dev->cdev;
    for (i = 0; i < ug_dev->led_count; i++) {
        if (dev == (cdev + i)->dev)
            break;
    }
    
    // 添加边界检查
    if (i >= ug_dev->led_count) {
        ret = -EINVAL;
        goto do_return;
    }
    
    status = ug_dev->status + i;
    if (kstrtoul(buf, 10, &ret)) {
        ret = -EINVAL;
        goto do_return;
    }
    // ret = color_str_to_uint(buf, size - 1);
    
    ret -= 1;
    if (ret > 4 || ret < 0) {  // 修正边界检查，led_color_table 只有5个元素 (0-4)
        ret = -EINVAL;
        goto do_return;
    }
    status->rgb[0] = led_color_table[ret][0];
    status->rgb[1] = led_color_table[ret][1];
    status->rgb[2] = led_color_table[ret][2];

    //buf value:
    //  witer:0 oringe:1 red:2 ugreen:3 blue:4
    // if(tmp < 1)
    //     tmp = 1;
    
    ret = ug_dev->i2c_write_led(ug_dev->i2c_client,status->led_addr,
            LED_MODE_COLOR,
            led_color_table[ret][0],
            led_color_table[ret][1],
            led_color_table[ret][2],0);

do_return:
    mutex_unlock(&ug_dev->mutex);
    return size;
}
		
static DEVICE_ATTR(color, 0664, NULL, color_set);

static struct attribute *ugreen_led_attrs[] = {
	&dev_attr_color.attr,
	NULL,
};

ATTRIBUTE_GROUPS(ugreen_led);

static void brightness_work(struct work_struct *work){
    struct led_status *status = container_of(work, struct led_status, work);
    struct ugreen_led_device *ug_dev = &ug_led_client;
    struct led_classdev *cdev = NULL;
    struct led_status *tmp;
    int i;
    
    // 不需要在这里加锁，因为 status 是通过 container_of 获取的，是安全的
    // 而且 i2c_write_led 函数内部应该有自己的保护机制
    tmp = ug_dev->status;
    for (i = 0; i < ug_dev->led_count; i++) {
        if(tmp + i == status){
            cdev = ug_dev->cdev + i;
            break;
        }
    }
    if (status->light == 1)
        status->light = 255;
    if (status->light == 0)
    //    LED_MODE_SWTICH
        ug_dev->i2c_write_led(ug_dev->i2c_client,status->led_addr,LED_MODE_LIGHT,
            status->light,0,0,0);
    else {
        ug_dev->i2c_write_led(ug_dev->i2c_client,status->led_addr,LED_MODE_LIGHT,
            status->light,0,0,0);
    }
}
static int ug_led_cdev_register(struct ugreen_led_device *ug_dev)
{
    int i;
    struct led_classdev *cdev;
	
    cdev = ug_dev->cdev;
    
    for (i = 0; i < ug_dev->led_count; i++) {
        cdev[i].name = ug_dev->status[i].name;
        cdev[i].brightness_set = leds_brightness_set;
        cdev[i].blink_set = leds_blink_set;
        cdev[i].max_brightness = LED_BRIGHTNESS_MAX;
        cdev[i].groups = ugreen_led_groups;

        INIT_WORK(&(ug_dev->status[i].work), brightness_work);
        /* register device */
        int ret = led_classdev_register(&ug_dev->i2c_client->dev, &cdev[i]);
        if (ret < 0) {
            dev_err(&ug_dev->i2c_client->dev, 
                "Failed to register LED %d: %d\n", i, ret);
            return ret;
        }
    }
	
    return 0;
}

static int iDX601x_led_init(struct ugreen_led_device *ug_dev)
{
    struct led_status *status;
    int i;
    const char *product = NULL;

    product = dmi_get_system_info(DMI_PRODUCT_NAME);
    if (!product) {
        CONFIG_UG_FEAT_PRINT("Failed to retrieve DMI product name.\n");
        product = "unknown"; // 默认值
    }
    ug_dev->product = product;
    status = ug_dev->status;
    ug_dev->i2c_write_led = i2c_iDX601x_write_led;
    ug_dev->i2c_read_led = i2c_iDX601x_read_led;
    /* 这里未来需要添加多一个判断 */
	if (!strcmp(ug_dev->product, PRODUCT_NAME[PRODUCT_DXP2800]))
		ug_dev->led_count = DXP2800_LED_COUNT;
	else if (!strcmp(ug_dev->product, PRODUCT_NAME[PRODUCT_DXP6800]) || !strcmp(ug_dev->product,PRODUCT_NAME[PRODUCT_DXP6800S]))
		ug_dev->led_count = DXP6800_LED_COUNT;
	else if (!strcmp(ug_dev->product, PRODUCT_NAME[PRODUCT_IDX6011]) || !strcmp(ug_dev->product, PRODUCT_NAME[PRODUCT_IDX6012]))
		ug_dev->led_count = IDX6011_LED_COUNT;
	else if (!strcmp(ug_dev->product, PRODUCT_NAME[PRODUCT_IDX6011_pro]))
		ug_dev->led_count = IDX6011PRO_LED_COUNT;
	else if (!strcmp(ug_dev->product,PRODUCT_NAME[PRODUCT_DXP4800_GT]))
		ug_dev->led_count = DXP4800_GT_LED_COUNT;    
    ug_dev->cdev = devm_kmalloc(&ug_dev->i2c_client->dev,
        sizeof(struct led_classdev) * ug_dev->led_count,GFP_KERNEL);
    if (NULL == ug_dev->cdev) {
        CONFIG_UG_FEAT_PRINT("cdev malloc mem failed");
        return -1;
    }
    ug_dev->status = devm_kmalloc(&ug_dev->i2c_client->dev,
            sizeof(struct led_status) * ug_dev->led_count, GFP_KERNEL);
    if (NULL == ug_dev->status) {
        CONFIG_UG_FEAT_PRINT("status malloc mem failed");
        goto error;
    }
    for (i = 0; i < ug_dev->led_count; i++) {

        if (!strcmp(ug_dev->product, PRODUCT_NAME[PRODUCT_DXP6800]) || !strcmp(ug_dev->product,PRODUCT_NAME[PRODUCT_DXP6800S]))
            ug_dev->status[i].name = dxp6800_led_name[i];
       else if (!strcmp(ug_dev->product, PRODUCT_NAME[PRODUCT_DXP2800]))
            ug_dev->status[i].name = dxp2800_led_name[i];
       else if (!strcmp(ug_dev->product, PRODUCT_NAME[PRODUCT_IDX6011]) || !strcmp(ug_dev->product, PRODUCT_NAME[PRODUCT_IDX6012]))
            ug_dev->status[i].name = iDX6011_led_name[i];
	   else if (!strcmp(ug_dev->product, PRODUCT_NAME[PRODUCT_IDX6011_pro]))
            ug_dev->status[i].name = iDX6011_pro_led_name[i];
	   else if (!strcmp(ug_dev->product,PRODUCT_NAME[PRODUCT_DXP4800_GT]))
            ug_dev->status[i].name = dxp4800_gt_led_name[i];            
        ug_dev->status[i].led_addr = i;
    }
    /* Init */
    //3A 00 0B  A0 01 00 00 04 03 E8 01 F4 02 85
    ug_dev->i2c_write_led(ug_dev->i2c_client,0x00,
        LED_MODE_BLINK,0,0,0,0);
    msleep(50);
    ug_dev->i2c_write_led(ug_dev->i2c_client,0x00,
        LED_MODE_LIGHT,0xff,0,0,0);
    msleep(50);
    ug_dev->i2c_write_led(ug_dev->i2c_client,0x00,
        LED_MODE_COLOR,0xff,0xff,0xff,0);
    msleep(50);
    ug_dev->i2c_write_led(ug_dev->i2c_client,0x00,
        LED_MODE_SWTICH,0xff,0,0,0);
    ug_dev->i2c_write_led(ug_dev->i2c_client,0x00,
        LED_MODE_LIGHT,0xff,0,0,0);
    msleep(50);
	
    return 0;

error:
    devm_kfree(&ug_dev->i2c_client->dev, ug_dev->cdev);
    return -1;
}
static int leds_ugreen_probe(struct i2c_client *client){
	int ret;
	ug_led_client.i2c_client = client;
	i2c_set_clientdata(client,&ug_led_client);
	mutex_init(&ug_led_client.mutex);
	CONFIG_UG_FEAT_PRINT("client addr:%x",ug_led_client.i2c_client->addr);
	/* 通过dmi对平台进行选择，使用哪个产品的接口和灯和灯初始化 */

	ug_led_client.product = dmi_get_system_info(DMI_PRODUCT_NAME);

	if (!strcmp(ug_led_client.product,PRODUCT_NAME[PRODUCT_IDX6011]) || !strcmp(ug_led_client.product, PRODUCT_NAME[PRODUCT_DXP2800])||
	    !strcmp(ug_led_client.product,PRODUCT_NAME[PRODUCT_IDX6012]) || !strcmp(ug_led_client.product, PRODUCT_NAME[PRODUCT_DXP6800])||
            !strcmp(ug_led_client.product,PRODUCT_NAME[PRODUCT_DXP6800S]) ||
	    !strcmp(ug_led_client.product,PRODUCT_NAME[PRODUCT_IDX6011_pro]) || !strcmp(ug_led_client.product,PRODUCT_NAME[PRODUCT_DXP4800_GT])) {
		printk("leds-mcu ug_led_driver_init");	
	}else
		return -1;
	ret = register_reboot_notifier(&ug_led_notifier);
	if(ret < 0){
		printk("ug led driver notifier register failed");
		return -1;
	}
	CONFIG_UG_FEAT_PRINT("leds_ugreen_probe product name is :%s",ug_led_client.product);

	iDX601x_led_init(&ug_led_client);
	//iDX601x_led_init(&ug_led_client);//设置初始化信息
	ug_led_cdev_register(&ug_led_client);//公共函数

	return 0;
}
static void leds_ugreen_remove(struct i2c_client *client)
{
    struct ugreen_led_device *ug_dev = i2c_get_clientdata(client);
    if (!ug_dev)
        return;

    struct led_status *status = ug_dev->status;
    int i;

    /* 先取消所有工作队列，防止新的工作被调度 */
    if (ug_dev->status) {
        for (i = 0; i < ug_dev->led_count; i++) {
            /* 使用cancel_*_sync确保工作队列完全停止 */
            cancel_work_sync(&status[i].work);
        }
    }

    /* 然后注销LED设备 */
    if (ug_dev->cdev) {
        for (i = 0; i < ug_dev->led_count; i++) {
            led_classdev_unregister(&ug_dev->cdev[i]);
        }
    }

    /* 最后释放内存 - 按照分配的反序释放 */
    if (ug_dev->cdev) {
        devm_kfree(&client->dev, ug_dev->cdev);
        ug_dev->cdev = NULL;
    }

    if (ug_dev->status) {
        CONFIG_UG_FEAT_PRINT("devm_kfree, ug_dev->status.\n");
        devm_kfree(&client->dev, ug_dev->status);
        ug_dev->status = NULL;
    }

    mutex_destroy(&ug_dev->mutex);
    return;
}

/*
* static void leds_ugreen_shutdown(struct i2c_client *client){
*     struct ugreen_led_device *ug_dev = i2c_get_clientdata(client);
*     int i;
*     struct led_status *status = ug_dev->status;
*     for(i = 0;i < ug_dev->led_count;i++){
*         led_classdev_unregister(ug_dev->cdev + i);
*         flush_delayed_work(&(status + i)->check_work);
*         flush_work(&(status + i)->work);
*     }
*     // if(!strcmp(ug_dev->product,PRODUCT_NAME[PRODUCT_IDX6011]) || 
*     //     !strcmp(ug_dev->product,PRODUCT_NAME[PRODUCT_IDX6012]) ){
*     //     unsigned char shutdown_ligh[3] = {0x0a,0x55,0xaa};
*     //     i2c_smbus_write_block_data(ug_dev->i2c_client,0x0a,
*     //         sizeof(shutdown_ligh) - 1,shutdown_ligh + 1);    //关机执行后，开启跑马灯
*     // }
*     return;
* }
*/

static int leds_ugreen_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    struct i2c_adapter *adapter = client->adapter;
    u16 chipid;
    u8 vendid;
    const u8 *name;
    int i;

    for(i=0; i<3; i++){
        chipid = i2c_smbus_read_word_data(client,MCU_CHIP_ID);
        //vendid = mcu_read8(client, MCU_FW_VER);
        pr_info("---i2c get chipid:%#x \n",chipid);
        if (chipid == 0xC5B2) {
            name = "leds-ugreen";
            break;
        }
        msleep(100);
    }

    if (chipid == 0xC5B2)
        name = "leds-ugreen";
    else
        return -ENODEV;
	
    vendid = i2c_smbus_read_word_data(client,MCU_FW_VER);
    dev_info(&adapter->dev, "found %s version:v1.0.%d\n", name, vendid);
    strncpy(info->type, name, I2C_NAME_SIZE);
    return 0;
}

static const unsigned short normal_i2c[] = { LED_I2C_ADDRESS, I2C_CLIENT_END };
static const struct i2c_device_id leds_ugreen_id[] = {
	{ "leds-ugreen", 0 },
	{ }
};
static struct i2c_driver leds_ugreen_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver		= {
		.name	= "leds-ugreen",
		.owner	= THIS_MODULE,
	},
	.probe		= leds_ugreen_probe,
	.remove		= leds_ugreen_remove,
	// .shutdown	= leds_ugreen_shutdown,
	.id_table	= leds_ugreen_id,
	//.detect 	= leds_ugreen_detect,
	.address_list   = normal_i2c,
};

static struct i2c_client * client0 = NULL;
static struct i2c_board_info info = {
        I2C_BOARD_INFO("leds-ugreen", LED_I2C_ADDRESS),
};

static int __init ug_led_driver_init(void) {
    struct i2c_adapter *adap;
    int i = 0;
    for (i = 0; i < 15; i++) {
            adap = i2c_get_adapter(i);
            if (!adap) {
                    pr_err("i2c-%d adapter not found\n",i);
                    break;
            }
            if(strncmp("SMBus", adap->name,5) && strncmp("Synopsys", adap->name,8) ) {
                    i2c_put_adapter(adap);
                    pr_info("ignore I2C adapter %d: %s\n", i, adap->name);
                    continue;
            }

            client0 = i2c_new_client_device(adap, &info);
            i2c_put_adapter(adap);
            if (IS_ERR(client0)) {
                    pr_err("i2c-%d creat client failed\n",i);
                    continue;
            }
            if( 0 == leds_ugreen_detect(client0, &info)){
                    pr_info("detect success!\n");
                    break;
            }
            else {
                    pr_info(" i2c-%d detect failed!\n", i);
                    i2c_unregister_device(client0);
                    client0 = NULL;
            }

    }

    return i2c_add_driver(&leds_ugreen_driver);
}
static void __exit ug_led_driver_exit(void) {
    unregister_reboot_notifier(&ug_led_notifier);
    i2c_del_driver(&leds_ugreen_driver);
    if (client0 && !IS_ERR(client0))
        i2c_unregister_device(client0);
}

module_init(ug_led_driver_init);
module_exit(ug_led_driver_exit);

MODULE_AUTHOR("swing.chen@ugreen.com");
MODULE_DESCRIPTION("ugreen_nas_leds driver");
MODULE_LICENSE("GPL");
