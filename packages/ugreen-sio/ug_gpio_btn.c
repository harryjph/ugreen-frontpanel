/*
 *  Button Hotplug driver
 *
 *  Copyright (C) 2008-2010 Gabor Juhos <juhosg@openwrt.org>
 *
 *  Based on the diag.c - GPIO interface driver for Broadcom boards
 *    Copyright (C) 2006 Mike Baker <mbm@openwrt.org>,
 *    Copyright (C) 2006-2007 Felix Fietkau <nbd@nbd.name>
 *    Copyright (C) 2008 Andy Boyett <agb@openwrt.org>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/input.h>

#include <linux/workqueue.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/kobject.h>
#include <linux/timer.h>
#include <linux/dmi.h>
//#include <misc/ugreen_product.h>
#include <linux/dmi.h>
#include <linux/string.h>

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#undef pr_fmt
#define pr_fmt(fmt) "ug btn: " fmt

#define INTERVAL	50   /*50ms*/
static char *product = NULL;
extern u64 uevent_next_seqnum(void);

static struct timer_list x86btn_timer;
static struct input_dev *ug_key;

struct btn_info{
	unsigned int code;
	const char *name;
	int shift;
	int seen;
	u32 phyaddr;
	void __iomem *base;
	int prestate;
	int active;
	int last_report;
};

static struct btn_info btnSatate[2];

static void btn_monitor(struct timer_list *t)
{
	int val;
	int i;
        //struct btn_hba *priv = container_of(work, struct btn_hba, monitor_work.work);
	//struct btn_info *btn_dev = priv->btn_dev;

	for(i=0; i < sizeof(btnSatate)/sizeof(btnSatate[0]); i++){
		if(btnSatate[i].base != NULL){
			val = ((__raw_readl(btnSatate[i].base) >>  btnSatate[i].shift) & 0x01) ;
			if(val == btnSatate[i].active){
				if(btnSatate[i].seen < 1200){ // < 10min
					btnSatate[i].seen++;
					if((KEY_F24 ==  btnSatate[i].code) ){ // 50ms * 100 = 5000ms   长按到5S后自动触发
						//pr_info("---%s,btn:%d,val:%d, resetkey  \n", __func__,i, val);
						if(btnSatate[i].seen == 100){
						}
						else if( btnSatate[i].seen == 260){
							input_report_key(ug_key, KEY_F24, 1);
							input_sync(ug_key);
							btnSatate[i].last_report = 1;
							pr_info("%s,rport F24 btn:%d,val:%d, \n", __func__,i, val);
						}								
						//input_report_key(ug_key, KEY_F24, 1);
						//input_sync(ug_key);
						

					}
					else if((KEY_F20 ==  btnSatate[i].code)){		/* KEY_F20: powerkey */
						if((btnSatate[i].seen == 1 && strcmp(product, "DXP2800 GT")) /* 其他机型: 收到单片机信号后自动触发 */
							|| (btnSatate[i].seen == 100 && !strcmp(product, "DXP2800 GT"))){ /* DXP2800 GT: seen=100 为5秒 */
								pr_info("%s,btn:%d,val:%d, seen=%d,powerkey  \n", __func__,i, val,btnSatate[i].seen);
								input_report_key(ug_key, KEY_F20, 1);
								input_sync(ug_key);
								btnSatate[i].last_report = 1;
						}
					}
				}
			}else{
				if(KEY_F24 ==  btnSatate[i].code ){
					if(btnSatate[i].seen > 260) {
								input_report_key(ug_key, KEY_F24, 0);
								input_sync(ug_key);
								btnSatate[i].last_report = 0;
								pr_info("%s,rport F24 btn:%d,val:%d, \n", __func__,i, val);
					}

				}
				else {		/* KEY_F20: powerkey */
					if(btnSatate[i].seen >= 100 && val != btnSatate[i].prestate){
								pr_info("%s,btn:%d,val:%d, \n", __func__,i, val);
								input_report_key(ug_key, btnSatate[i].code, 0);		/* powerkey seen >= 100, report 0 */
								input_sync(ug_key);
								btnSatate[i].last_report = 0;
					}
				}
				btnSatate[i].seen = 0;
			}
			btnSatate[i].prestate = val;
		}
	}
	mod_timer(&x86btn_timer, jiffies + msecs_to_jiffies(INTERVAL));
}

static void hwinit(void)
{
	int i;
	int val;
	for(i=0; i < sizeof(btnSatate)/sizeof(btnSatate[0]); i++){
		btnSatate[i].base = ioremap(btnSatate[i].phyaddr, 4);
		if(!strcmp(product, "DXP2800 GT")){
			val = __raw_readl(btnSatate[i].base);
			val &=   ~(1 << 23);  /* set pin input */
			__raw_writel(val, btnSatate[i].base);
		}
	}
}

static void hw_unioremap(void)
{
	int i;
	for(i=0; i < sizeof(btnSatate)/sizeof(btnSatate[0]); i++){
		if(btnSatate[i].base != NULL){
			iounmap(btnSatate[i].base);
			btnSatate[i].base = NULL;
		}
	}
}

static int ug_gpiokey_probe(struct platform_device *pdev)
{
	int err;
	hwinit();
	timer_setup(&x86btn_timer, btn_monitor, 0);
	x86btn_timer.expires = jiffies + msecs_to_jiffies(INTERVAL);
	add_timer(&x86btn_timer);
	
	
	ug_key = devm_input_allocate_device(&pdev->dev);
	if (!ug_key) {
		dev_err(&pdev->dev, "Can't allocate power button\n");
		return -ENOMEM;
	}
// #define KEY_F20         192   power
// #define KEY_F24         194	 reset
	ug_key->name = "ug gpiokey";
	ug_key->phys = "ug_gpiokey/input0";
	ug_key->id.bustype = BUS_HOST;
	ug_key->dev.parent = &pdev->dev;

	input_set_capability(ug_key, EV_KEY, KEY_F20);
	input_set_capability(ug_key, EV_KEY, KEY_F24);
	input_set_drvdata(ug_key, pdev);

	err = input_register_device(ug_key);
	if (err) {
		dev_err(&pdev->dev, "Can't register power button: %d\n", err);
		return err;
	}

	platform_set_drvdata(pdev, ug_key);

	return 0;
}

static void ug_gpiokey_remove(struct platform_device *){
	hw_unioremap();
	del_timer(&x86btn_timer);
	input_unregister_device(ug_key);
	input_free_device(ug_key);
	
	return;
}

static void ug_gpiokey_device_release(struct device *dev){
	pr_info("%s \n", __func__);
	return;
}

static struct platform_driver ug_gpiokey_driver = {
	.probe	= ug_gpiokey_probe,
	.driver	= {
		.name = "ug-gpiokey",
	},
	.remove = ug_gpiokey_remove,
};

static struct platform_device ug_gpiokey_device = {
    .name   = "ug-gpiokey",
    .dev = {
          .release = ug_gpiokey_device_release,
   }

};

/*
struct dmi_system_id {
    int (callback)(const struct dmi_header *, void *);
    const char *ident;   // 用于标识DMI信息的字符串

    // 下面的字段用于匹配DMI信息，根据不同的DMI类型可能有不同的字段
    char *vendor;        // 制造商
    char *product_name;  // 产品名称
    char *version;       // 版本
    char *board_name;    // 主板名称
    char *board_version; // 主板版本
    char *date;          // 制造日期
    char *sku_number;    // SKU号码
    char *family;        // 家族

    void *driver_data;    // 驱动私有数据
};

*/
static void  my_dmi_callback(const struct dmi_header *dm, void *private_data)
{
    // 处理dmidecode -t 1（System Information）的数据
    if (dm->type == 1) {
       // struct dmi_system_id *system_info = (struct dmi_system_id *)dm;
        const struct dmi_system_id *system_info = (const struct dmi_system_id *)private_data;
        product =  dmi_get_system_info(DMI_PRODUCT_NAME);

        // 打印System Information的相关信息
        pr_info("DMI Vendor: %s\n", dmi_get_system_info(DMI_BIOS_VENDOR));
        pr_info("DMI Product Name: %s\n", product);
        pr_info("DMI Version: %s\n", dmi_get_system_info(DMI_BIOS_VERSION));
        // 可以继续打印其他字段

		if( !strcmp(product, "DXP4800") || !strcmp(product, "DXP2800") || !strcmp(product, "DXP2800S") || !strcmp(product, "DXP4800S"))
		{
		    btnSatate[0] = (struct btn_info){
		        .code = KEY_F20,
		        .name = "reboot",
		        .phyaddr = 0xfd6a0980,  // GPP_F16
		        .shift = 1,
		        .active = 1,
		        .base = NULL,
		        .seen = 0,
		        .prestate = 0,
		        .last_report = 0,
		   	 };

	        btnSatate[1] = (struct btn_info){
		        .code = KEY_F24,
		        .name = "reset",
		        .phyaddr = 0xfd6d0b50,  // GPP_D17
		        .shift = 1,
		        .active = 0,
		        .base = NULL,
		        .seen = 0,
		        .prestate = 1,
		        .last_report = 0,
	   		 };			
    		}
		else if( !strncmp(product, "DXP4800 Plus", 12 ) || !strncmp(product, "DXP4800 Pro", 11 ))
		{
		    btnSatate[0] = (struct btn_info){
		        .code = KEY_F20,
		        .name = "reboot",
		        .phyaddr = 0xfd6a0980,  // // DXP4800 Plus GPP_E13
		        .shift = 1,
		        .active = 1,
		        .base = NULL,
		        .seen = 0,
		        .prestate = 0,
		        .last_report = 0,
		   	 };

	        btnSatate[1] = (struct btn_info){
		        .code = KEY_F24,
		        .name = "reset",
		        .phyaddr = 0xfd6d0a10,  // DXP4800 Plus GPP_D17
		        .shift = 1,
		        .active = 0,
		        .base = NULL,
		        .seen = 0,
		        .prestate = 1,
		        .last_report = 0,
	   		 };			
    	}
		
		else if( !strncmp(product, "DXP6800", 7 ) || !strncmp(product, "DXP8800", 7 ) || !strncmp(product, "FORT 6", 6 )){
		    btnSatate[0] = (struct btn_info){
		        .code = KEY_F20,
		        .name = "reboot",
		        .phyaddr = 0xfd6a0b40,  // DXP6800 8800 GPP_E13
		        .shift = 1,
		        .active = 1,
		        .base = NULL,
		        .seen = 0,
		        .prestate = 0,
		        .last_report = 0,
		   	 };

	        btnSatate[1] = (struct btn_info){
		        .code = KEY_F24,
		        .name = "reset",
		        .phyaddr = 0xfd6d0a10,  // //DXP6800 8800 GPP_D17 
		        .shift = 1,
		        .active = 0,
		        .base = NULL,
		        .seen = 0,
		        .prestate = 1,
		        .last_report = 0,
	   		 };			
    	}
		else if(!strncmp(product, "DXP480T Plus",12)){
		   	btnSatate[0] = (struct btn_info){
			   .code = KEY_F20,
			   .name = "reboot",
			   .phyaddr = 0XFD6E0810,  // DXP6800 8800 GPP_E13
			   .shift = 1,
			   .active = 1,
			   .base = NULL,
			   .seen = 0,
			   .prestate = 0,
			   .last_report = 0,
			};
		   
		   btnSatate[1] = (struct btn_info){
			   .code = KEY_F24,
			   .name = "reset",
			   .phyaddr = 0XFD6E0770,  // //DXP6800 8800 GPP_D17 
			   .shift = 1,
			   .active = 0,
			   .base = NULL,
			   .seen = 0,
			   .prestate = 1,
			   .last_report = 0,
			}; 
    	}
		else if(!strncmp(product, "iDX6011",7) || !strncmp(product, "iDX6012",7)){
			printk(KERN_INFO ": %s\n", product);
		   btnSatate[0] = (struct btn_info){		//电源按键接入了ACPI电源管理，无法通过地址读取。
			   .code = KEY_F20,
			   .name = "reboot",
			   .phyaddr = 0xE0D20860,  // DXP6800 8800 GPP_E13
			   .shift = 1,
			   .active = 1,
			   .base = NULL,
			   .seen = 0,
			   .prestate = 0,
			   .last_report = 0,
			};
		   btnSatate[1] = (struct btn_info){
			   .code = KEY_F24,
			   .name = "reset",
			   .phyaddr = 0xE0D508A0,  //  
			   .shift = 1,
			   .active = 0,
			   .base = NULL,
			   .seen = 0,
			   .prestate = 1,
			   .last_report = 0,
			}; 
		}
		else if(!strncmp(product, "DH2600", 6)){
		   btnSatate[0] = (struct btn_info){
			   .code = KEY_F20,
			   .name = "reboot",
			   .phyaddr = 0XFD6D0B20,  //  DH2600
			   .shift = 1,
			   .active = 1,
			   .base = NULL,
			   .seen = 0,
			   .prestate = 0,
			   .last_report = 0,
			};
		   btnSatate[1] = (struct btn_info){
			   .code = KEY_F24,
			   .name = "reset",
			   .phyaddr = 0XFD6E0800,  //DH2600
			   .shift = 1,
			   .active = 0,
			   .base = NULL,
			   .seen = 0,
			   .prestate = 1,
			   .last_report = 0,
			};
		}
		else if(!strncmp(product, "DX4600", 6)|| !strncmp(product, "DX4700", 6)){
		   btnSatate[0] = (struct btn_info){
			   .code = KEY_F20,
			   .name = "reboot",
			   .phyaddr =  0xfd6d0690,  //  dx4600 dx4700
			   .shift = 1,
			   .active = 1,
			   .base = NULL,
			   .seen = 0,
			   .prestate = 0,
			   .last_report = 0,
			};
		   btnSatate[1] = (struct btn_info){
			   .code = KEY_F24,
			   .name = "reset",
			   .phyaddr = 0xfd6d0890,  //dx4600 dx4700
			   .shift = 1,
			   .active = 0,
			   .base = NULL,
			   .seen = 0,
			   .prestate = 1,
			   .last_report = 0,
			};
		}
		else if(!strcmp(product, "DXP4800 GT")){
		   btnSatate[0] = (struct btn_info){
			   .code = KEY_F20,
			   .name = "reboot",
			   .phyaddr =  0xFED81526,	/* pwr key  */
			   .shift = 0,
			   .active = 1,
			   .base = NULL,
			   .seen = 0,
			   .prestate = 0,
			   .last_report = 0,
			};
		   btnSatate[1] = (struct btn_info){
			   .code = KEY_F24,
			   .name = "reset",
			   .phyaddr = 0xFED81522,	/*   */
			   .shift = 0,
			   .active = 0,
			   .base = NULL,
			   .seen = 0,
			   .prestate = 1,
			   .last_report = 0,
			};
		}
		else if(!strcmp(product, "DXP2800 GT")){
		   btnSatate[0] = (struct btn_info){
			   .code = KEY_F20,
			   .name = "reboot",
			   .phyaddr =  0xFED81510,	/* AW10 */
			   .shift = 16,
			   .active = 0,
			   .base = NULL,
			   .seen = 0,
			   .prestate = 1,
			   .last_report = 0,
			};
		   btnSatate[1] = (struct btn_info){
			   .code = KEY_F24,
			   .name = "reset",
			   .phyaddr = 0xFED8150C,	/* AT15 */
			   .shift = 16,
			   .active = 0,
			   .base = NULL,
			   .seen = 0,
			   .prestate = 1,
			   .last_report = 0,
			};
    	}
	}
    return ;  // 继续遍历
}

static int __init ioports_hotplug_init(void)
{
//	if(!ug_check_product_match(DX4600_SERIES, __func__)){
//		return -ENODEV;
//	}	
    int ret;
	dmi_walk(my_dmi_callback, NULL);

    ret = platform_driver_register(&ug_gpiokey_driver);

    if (!ret) {
        ret = platform_device_register(&ug_gpiokey_device);
        if (ret)
            platform_driver_unregister(&ug_gpiokey_driver);
    }
    return ret;

}
module_init(ioports_hotplug_init);

static void __exit ioports_hotplug_exit(void)
{
	
	platform_driver_unregister(&ug_gpiokey_driver);
	platform_device_unregister(&ug_gpiokey_device);

}

module_exit(ioports_hotplug_exit);

MODULE_LICENSE("GPL v2");
