#include "linux/mod_devicetable.h"
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/gpio/driver.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/dmi.h>
//#include <misc/ugreen_product.h>
#include "it55_helper.h"

#undef pr_fmt
#define pr_fmt(fmt) "gt sataio: " fmt

#define SATA_SWITCH	"sata_sw"
#define BEEPER		"beeper"
#define DELAY_VALUE	 200 //  ms
#define EC_D_PORT        0x62
#define EC_C_PORT        0x66
#define EC_C_READ_MEM    0x80    // Read the EC memory
#define EC_C_WRITE_MEM   0x81    // Write the EC memory

#define EC_SATA_PWR_EN 0xA6		/* for 4800gt */
/* 适配iDX6011 Pro */
#define BEEPER_ADR_6011PRO		0x9e
/*iDX6011*/
#define BEEPER_ADR_6011			0xa5
static struct delayed_work beeper_delay_work;
struct timer_list beeper_timer;
unsigned int timerStatus;
unsigned long delay_off;
unsigned long delay_on;
unsigned long g_count;

static int sata_io_num;
static char * product = NULL;

struct devinfo{
	const char *name;
	int shift; /* bit <<  shift*/
	u32 phyaddr0;
	u32 phyaddr1;
	void __iomem *base0,*base1;
};

struct devinfo beeperInfo = {
	.name="BEEPER",
	.phyaddr0=0XFD6E08B0,
	.shift=0,
	.base0=NULL,
};
struct devinfo sataInfo[8];
struct devinfo sataPwr[8];

static void OemOverRideBeep_open(unsigned int Frequency) {
//	unsigned char    tmp;
	
    outb(0xB6, 0x43 );
    outb(0x44, 0x42 );
    outb(0x09, 0x42);

    outb(inb(0x61)|3,0x61);

}

static void OemOverRideBeep_close(void) {

    // Turn off the speaker
    //
    outb(inb(0x61)&0xfc,0x61);
}

static void ori_OemOverRideBeep_open(unsigned int Frequency) {
    /** THIS IS HW DEPENDENT. PORTING MAY BE REQUIRED. **/
    unsigned short    Divider;
	unsigned char    tmp;
    
    Divider = (unsigned short)((119318200 + Frequency/2)/Frequency);
    //
    // Set up channel 1 timer (used for delays)
    //
	
    outb(0x54, 0x43 );
    outb(0x12, 0x41 );
    //
    // Set up channel 2 timer (used by speaker)
    //
    outb(0xb6, 0x43);
	//printk("----Divider:%#x \n", Divider);
	tmp = (unsigned char)Divider;
		//printk("----tmp:%#x \n", tmp);
    outb(tmp, 0x42);
	tmp = (unsigned char)(Divider>>8);
		//printk("----tmp:%#x \n", tmp);
    outb(tmp,0x42);
    //
    // Turn the speaker on
    //
    outb(inb(0x61)|3,0x61);
    //
    // Delay
    //
    //DelayTime(Duration);
	//msleep(Duration);
    //
    // Turn off the speaker
    //
   // outb(inb(0x61)&0xfc,0x61);
}

static void ori_OemOverRideBeep_close(void) {

    // Turn off the speaker
    //
    outb(inb(0x61)&0xfc,0x61);
}

static int superio_write_byte(u8 value,u8 addr){
	//IDX601x
	//思路
	// 原因： 写入IO空间，但是其他的驱动也有在同个时间点 同时写入，该IO地址为复用功能。
	// 思路：写入后判断再读取该值是否正确，等待写入。
	int EcStatus;
	int i = 0;
	char tmp;
	EcStatus = inb(EC_C_PORT);
	while(i <= 3){
		if((EcStatus & 0x01) != 0)
			break;
		else{
			if(i > 3){
				printk("superio writte failed");
				return -1;
			}
			i++;
		}
	}
	// outb(EC_C_WRITE_MEM,EC_C_PORT);
	// mdelay(1);
	// outb(addr,EC_D_PORT);
	// mdelay(1);
	// outb(value,EC_D_PORT);
	// mdelay(1);

	do{
		outb(EC_C_WRITE_MEM,EC_C_PORT);
		mdelay(1);
		outb(addr,EC_D_PORT);
		mdelay(1);
		outb(value,EC_D_PORT);
		mdelay(1);
		outb(EC_C_READ_MEM,EC_C_PORT);
		mdelay(1);
		outb(addr,EC_D_PORT);
		mdelay(1);
		tmp = inb(EC_D_PORT);
		
		if((addr == BEEPER_ADR_6011 || addr == BEEPER_ADR_6011PRO ) && (tmp == 2) && (value == 1))
			tmp = 1;//写入1，IO空间返回回来的值是2
		
	}while(tmp != value);
	return 0;
}
static u32 get_value(const volatile void __iomem *addr)
{
	return __raw_readl(addr);
}
static void set_value(u32 val,  volatile void __iomem *addr)
{
	__raw_writel(val,addr);
}

static void hwinit(void)
{
	int i;
	u32 val;

	for(i=0; i < sata_io_num; i++){
		sataInfo[i].base0 = ioremap(sataInfo[i].phyaddr0, 4);
		if(0 != sataInfo[i].phyaddr1)
			sataInfo[i].base1 = ioremap(sataInfo[i].phyaddr1, 4);
		if(!strcmp(product, "DXP2800 GT")) {
			val = __raw_readl(sataInfo[i].base0);
			val &=   ~(1 << 23);  /* set pin input */
			__raw_writel(val, sataInfo[i].base0);
			val = __raw_readl(sataInfo[i].base1);
			val &=   ~(1 << 23);  /* set pin input */
			__raw_writel(val, sataInfo[i].base1);
		}
	}
}

static void hw_unioremap(void)
{
	int i;
	for(i=0; i < sata_io_num; i++){
		if(sataInfo[i].base0 != NULL){
			iounmap(sataInfo[i].base0);
			sataInfo[i].base0 = NULL;
		}
		if(sataInfo[i].base1 != NULL){
			iounmap(sataInfo[i].base1);
			sataInfo[i].base1 = NULL;
		}
	}
}

static ssize_t sata_satate_read(struct file *file,
					char __user *usr_buf,
					size_t size, loff_t *ppos)
{
	unsigned int val,val1,tmpv,u2_v1,u2_v2;
	unsigned int i;
	unsigned int cnt = 0;
	char tmpbuf[128];

	if (*ppos != 0)
		return 0;
	
	memset(tmpbuf, 0, sizeof(tmpbuf));
	
	for(i=0; i < sata_io_num; i++){
		if(sataInfo[i].base0 != NULL){
			val = (get_value(sataInfo[i].base0)) & (1 << sataInfo[i].shift);
		}
		if(!strcmp(product, "DXP2800 GT")){
			if(sataInfo[i].base1 != NULL){
				val1 = get_value(sataInfo[i].base1);
				//pr_info("read io:%d val1:%#x \n",i, val1);
				val1 &=  (1 << sataInfo[i].shift);
				//pr_info("---read io:%d val:%#x, val1:%#x \n",i,val, val1);
				if( 0 == val &&  val1)
					tmpv = 1;
				else
					tmpv = 0;
				if(0 == i) {
					u2_v1 = tmpv;
				}
				else {
					u2_v2 = tmpv;
				}
				val |= val1;
			}
			val = !val;
		}
		else if(!strcmp(product, "DXP4800 GT")){
			if(sataInfo[i].base1 != NULL){
				val1 = get_value(sataInfo[i].base1);
				val1 &=  (1 << sataInfo[i].shift);
				if( val &&  val1)
					tmpv = 1;
				else
					tmpv = 0;
				if(0 == i) {
					u2_v1 = tmpv;
				}
				else {
					u2_v2 = tmpv;
				}
				if( val && 0 == val1 )
					val = 1;
				else
					val = 0;
			}
		}
		// printk("----i:%d,-addr:0x%p, val%#x \n",i, sataInfo[i].base0, get_value(sataInfo[i].base0));
		cnt += snprintf(tmpbuf + cnt, sizeof(tmpbuf) - cnt, "%s:\t%s\n", sataInfo[i].name, val ? "ON" : "OFF");
	}
    if(!strcmp(product, "DXP2800 GT") || !strcmp(product, "DXP4800 GT")) {
		cnt += snprintf(tmpbuf + cnt, sizeof(tmpbuf) - cnt, "%s:\t%s\n", "U.2_1", u2_v1 ? "ON" : "OFF");
		cnt += snprintf(tmpbuf + cnt, sizeof(tmpbuf) - cnt, "%s:\t%s\n", "U.2_2", u2_v2 ? "ON" : "OFF");
    }

	return simple_read_from_buffer(usr_buf, size, ppos, tmpbuf, cnt);
}
static void beeper_delay_work_func(struct work_struct *work)
{
	if(!strcmp(product, "iDX6012") || !strcmp(product, "iDX6011"))
		superio_write_byte(0,BEEPER_ADR_6011);
	else if(!strcmp(product, "iDX6011 Pro"))
		superio_write_byte(0,BEEPER_ADR_6011PRO);
	else
		OemOverRideBeep_close();
}

static void set_beeper(int dat, unsigned long delay)
{

#if 1
	if(dat == 0){
		if(!strcmp(product, "iDX6012") || !strcmp(product, "iDX6011"))
			superio_write_byte(0,BEEPER_ADR_6011);
		else if(!strcmp(product, "iDX6011 Pro"))
			superio_write_byte(0,BEEPER_ADR_6011PRO);
		else
			OemOverRideBeep_close();
	}else if(dat == 1){
		if(!strcmp(product, "iDX6012") || !strcmp(product, "iDX6011"))
			superio_write_byte(1,BEEPER_ADR_6011);
		else if(!strcmp(product, "iDX6011 Pro"))
			superio_write_byte(1,BEEPER_ADR_6011PRO);
		else
			OemOverRideBeep_open(58732);
	}else if(dat == 2){
		if(!strcmp(product, "iDX6012") || !strcmp(product, "iDX6011"))
			superio_write_byte(1,BEEPER_ADR_6011);
		else if(!strcmp(product, "iDX6011 Pro"))
			superio_write_byte(1,BEEPER_ADR_6011PRO);
		else
			OemOverRideBeep_open(58732);

		schedule_delayed_work(&beeper_delay_work, msecs_to_jiffies(delay));
	}
#endif
}

static void beeperTimerHandle(struct timer_list *t)
{
	static int count;
	if((delay_on < 1) || (delay_off < 1)){
		delay_on = 500;
		delay_off = 2000;
	}
	set_beeper(2,delay_on);
	mod_timer(&beeper_timer, jiffies + msecs_to_jiffies(delay_off + delay_on));
	if(g_count){
		count++;
		if(g_count == count){
			del_timer(&beeper_timer);
			timerStatus = 0;
			count = 0;
		}
	}
}

static ssize_t  beeper_read(struct file *file, char __user *usr_buf, size_t size, loff_t *ppos)
{
	unsigned char val;
	char tmpbuf[128];
	unsigned int cnt = 0;

	if (*ppos != 0)
		return 0;
		
	outb(0x66,0x70);
	val = inb(0x71);
	printk("---open beep is set val %x\n",val);
	if( val & 0x01){
		cnt = snprintf(tmpbuf, sizeof(tmpbuf), "%s\n", "bootoff");
	}
	else {
		cnt = snprintf(tmpbuf, sizeof(tmpbuf), "%s\n", "booton");
	}

	return simple_read_from_buffer(usr_buf, size, ppos, tmpbuf, cnt);
}

static ssize_t beeper_write(struct file *file,const char __user * buffer,size_t count, loff_t * ppos)
{
        int result = 0;
        char buf[32] = { '\0' };
	

        if (/*!beeper ||*/ (count > sizeof(buf) - 1)){
                return -EINVAL;
		}
        if (copy_from_user(buf, buffer, count)) {
                result = -EFAULT;
                goto end;
        }
	if(buf[count - 1] == 0x0a){
		buf[count - 1] = '\0';
	}
	if((!strncmp(buf, "booton",6)) || (!strncmp(buf, "BOOTON",6))){
	    outb(0x66,0x70);
    	outb(inb(0x71)&(~0x01),0x71);   /* open boot beep */
		printk("---open boot beep\n");
	}
	else if((!strncmp(buf, "bootoff",7)) || (!strncmp(buf, "BOOTOFF",7))){
    	outb(0x66,0x70);
    	outb(inb(0x71)|0x01,0x71);   /* close boot beep */
		printk("---close boot beep\n");
	}	
	else if((!strncmp(buf, "on",3)) || (!strncmp(buf, "ON",3))){
		del_timer(&beeper_timer);
		timerStatus = 0;
		set_beeper(1, 0);
	}else if((!strncmp(buf, "off",4)) || (!strncmp(buf, "OFF",4))){
		del_timer(&beeper_timer);
		timerStatus = 0;
		set_beeper(0, 0);
	}else if((!strncmp(buf, "one",4)) || (!strncmp(buf, "ONE",4))){
		del_timer(&beeper_timer);
		timerStatus = 0;
		set_beeper(2, DELAY_VALUE);
	}else if(!strncmp(buf, "timer ",6)){
		// timer 2000 500 
        	int fields = 0;
		unsigned long a,b;

		fields = sscanf(buf, "timer %ld %ld", &a, &b);
        	if (fields != 2){
                     result = -EFAULT;
                     goto end;
		}

		if((a < 20) || (a > 99999) ||
   		   (b < 20)  || (b > 99999)){
                     result = -EFAULT;
                     goto end;
		}

		delay_off = a;
		delay_on = b;

		if(0 == timerStatus){
		     beeper_timer.expires = jiffies + msecs_to_jiffies(100);
		     add_timer(&beeper_timer);
		     timerStatus = 1;
		}
	}else if(!strncmp(buf, "rep ",3)){
		// timer 2000 500 
        	int fields2 = 0;
		unsigned long count,c,d;
		

		fields2 = sscanf(buf, "rep %ld %ld %ld", &count,&c, &d);
        	if (fields2 != 3){
                     result = -EFAULT;
                     goto end;
		}

		if((c < 20) || (c > 99999) ||
   		   (d < 20)  || (d > 99999)){
                     result = -EFAULT;
                     goto end;
		}

		delay_off = c;
		delay_on = d;
		g_count = count;

		if(0 == timerStatus){
		     beeper_timer.expires = jiffies + msecs_to_jiffies(100);
		     add_timer(&beeper_timer);
		     timerStatus = 1;
		}
	}else{
		result = -EINVAL;
	}
end:
	if(result){
		return result;
	}
        return count;
}

static int null_proc_open(struct inode *inode, struct file *file)
{
        return single_open(file, NULL, pde_data(inode));
}

static const struct proc_ops sata_proc_fops = {

        .proc_open           = null_proc_open,
        .proc_read           = sata_satate_read,
        .proc_release        = seq_release,
};

static const struct proc_ops beeper_proc_fops = {
        .proc_open           = null_proc_open,
		.proc_write		= beeper_write,
        .proc_release        = seq_release,
		.proc_read           = beeper_read,
};
static struct proc_dir_entry *nas_dir = NULL;

static int procfs_create(void)
{
	struct proc_dir_entry *entry = NULL;
	int ret = 0;

	/* create /proc/nas/ */
	nas_dir = proc_mkdir("nas", NULL);
	if (!nas_dir){
		return -ENODEV;
	}
	/* create /proc/nas/sata_sw */
	entry = proc_create_data(SATA_SWITCH, S_IRUGO, nas_dir, &sata_proc_fops, NULL);
	if (!entry) {
		ret = -ENODEV;
		goto remove_dev_dir;
	}

	/* create /proc/nas/beeper */
	entry = proc_create_data(BEEPER, S_IWUGO, nas_dir, &beeper_proc_fops, NULL);
	if (!entry) {
		ret = -ENODEV;
		goto remove_dev_dir;
	}
done:
	return ret;

remove_dev_dir:
	remove_proc_entry("nas", NULL);
	nas_dir = NULL;
	goto done;
}
static int procfs_remove(void)
{
	remove_proc_entry(SATA_SWITCH, nas_dir);
	remove_proc_entry(BEEPER, nas_dir);
	remove_proc_entry("nas", NULL);
	nas_dir = NULL;
	return 0;

}

static void  dmi_get_product(void)
{
	product =  dmi_get_system_info(DMI_PRODUCT_NAME);
	printk(KERN_INFO "DMI Product Name: %s SATA&beep init\n", dmi_get_system_info(DMI_PRODUCT_NAME));

	memset(sataInfo, 0, sizeof(sataInfo));
	memset(sataPwr, 0, sizeof(sataPwr));
	
	if(!strcmp(product, "DXP2800") || !strcmp(product, "DXP2800S") || !strcmp(product, "DXP4800") || !strcmp(product, "DXP4800S"))
	{
		if(!strncmp(product, "DXP2800",7))	/* DXP2800,DXP2800S */
			sata_io_num = 2;
		if(!strncmp(product, "DXP4800", 7))	/* DXP4800,DXP4800S */
			sata_io_num = 4;

		sataInfo[0] = (struct devinfo){.name="SATA1", .phyaddr0=0XFD6A0930, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};
		sataInfo[1] = (struct devinfo){.name="SATA2", .phyaddr0=0XFD6A0940, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};
		sataInfo[2] = (struct devinfo){.name="SATA3", .phyaddr0=0XFD6A0950, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};
		sataInfo[3] = (struct devinfo){.name="SATA4", .phyaddr0=0XFD6A0960, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};
	}
	else if( !strncmp(product, "DXP4800 Plus", 12) || !strncmp(product, "DXP4800 Pro", 11))
	{
		
		sata_io_num = 4;
		sataInfo[0] = (struct devinfo){.name="SATA1", .phyaddr0=0XFD6A0930, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};
		sataInfo[1] = (struct devinfo){.name="SATA2", .phyaddr0=0XFD6A0940, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};
		sataInfo[2] = (struct devinfo){.name="SATA3", .phyaddr0=0XFD6A0950, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};
		sataInfo[3] = (struct devinfo){.name="SATA4", .phyaddr0=0XFD6A0960, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};
	}
	else if( !strncmp(product, "DXP6800", 7 ) ||  !strncmp(product, "DXP8800", 7 ) || !strncmp(product, "FORT 6", 6 ))
	{
		if(!strncmp(product, "DXP6800", 7 ) || !strncmp(product, "FORT 6", 6 ))
			sata_io_num = 6;
		if(!strncmp(product, "DXP8800", 7))
			sata_io_num = 8;
		printk(KERN_INFO "DMI Product Name: %s  init\n", dmi_get_system_info(DMI_PRODUCT_NAME));

		sataInfo[0] = (struct devinfo){.name="SATA1", .phyaddr0=0XFD6A0930, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL}; 	//GPP_F11
		sataInfo[1] = (struct devinfo){.name="SATA2", .phyaddr0=0XFD6A0940, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL}; 	//GPP_F12
		sataInfo[2] = (struct devinfo){.name="SATA3", .phyaddr0=0XFD6A0950, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL}; 	//GPP_F13
		sataInfo[3] = (struct devinfo){.name="SATA4", .phyaddr0=0XFD6A0960, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL}; 	//GPP_F14
		sataInfo[4] = (struct devinfo){.name="SATA5", .phyaddr0=0XFD6A0970, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL}; 	//GPP_F15
		sataInfo[5] = (struct devinfo){.name="SATA6", .phyaddr0=0XFD6A0980, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL}; 	//GPP_F16
		sataInfo[6] = (struct devinfo){.name="SATA7", .phyaddr0=0XFD6A0990, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL}; 	//GPP_F17
		sataInfo[7] = (struct devinfo){.name="SATA8", .phyaddr0=0XFD6A09A0, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL}; 	//GPP_F18
	}
	else if(!strncmp(product, "iDX6012", sizeof("iDX6012") - 1) || !strncmp(product, "iDX6011", sizeof("iDX6011") - 1))/*iDX6011，iDX6012, iDX6011 Pro */
	{
		sata_io_num = 6;
		sataInfo[0] = (struct devinfo){.name="SATA1", .phyaddr0=0XE0D30850, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};       /*GPD_F11~F18*/
		sataInfo[1] = (struct devinfo){.name="SATA2", .phyaddr0=0XE0D30860, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};
		sataInfo[2] = (struct devinfo){.name="SATA3", .phyaddr0=0XE0D30870, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};
		sataInfo[3] = (struct devinfo){.name="SATA4", .phyaddr0=0XE0D30880, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};
		sataInfo[4] = (struct devinfo){.name="SATA5", .phyaddr0=0XE0D30890, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};
		sataInfo[5] = (struct devinfo){.name="SATA6", .phyaddr0=0XE0D308A0, .phyaddr1=0, .shift=1, .base0=NULL, .base1=NULL};

	}
	else {
		pr_err("%s no match!\n",product);
	}
    return ;  // 继续遍历
}

static int __init sataio_beep_init(void)
{
	
	dmi_get_product();
	INIT_DELAYED_WORK(&beeper_delay_work, beeper_delay_work_func);
	timerStatus = 0;
	timer_setup(&beeper_timer, beeperTimerHandle, 0);
	procfs_create();
	hwinit();
	return 0;
	
}

static void __exit sataio_beep_exit(void)
{
	hw_unioremap();
	procfs_remove();
	cancel_delayed_work_sync(&beeper_delay_work);
	
	if(timerStatus == 1){
	     del_timer(&beeper_timer);
	}
	printk("Exit\n");
	return;
}

module_init(sataio_beep_init);
module_exit(sataio_beep_exit);

MODULE_LICENSE("GPL");
