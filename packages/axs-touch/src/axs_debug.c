/*
 * AXS touchscreen driver.
 *
 * Copyright (c) 2020-2021 AiXieSheng Technology. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "axs_core.h"
static struct mutex s_device_mutex;
static int s_rd_len=0;
#if AXS_DEBUG_LOG_EN
u8 g_log_en = 1;
#endif

static int hex_to_int(const char *hex_buf, int size)
{
    int i;
    int base = 1;
    int value = 0;
    char single;

    for (i = size - 1; i >= 0; i--)
    {
        single = hex_buf[i];

        if ((single >= '0') && (single <= '9'))
        {
            value += (single - '0') * base;
        }
        else if ((single >= 'a') && (single <= 'z'))
        {
            value += (single - 'a' + 10) * base;
        }
        else if ((single >= 'A') && (single <= 'Z'))
        {
            value += (single - 'A' + 10) * base;
        }
        else
        {
            return -EINVAL;
        }

        base *= 16;
    }

    return value;
}


static u8 hex_to_u8(const char *hex_buf, int size)
{
    return (u8)hex_to_int(hex_buf, size);
}

/************************************************************************
* Name: axs_tprwreg_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t axs_tprwreg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int total_count=0;
    int count=0;

    int i;
    //char str_temp[20];
    mutex_lock(&s_device_mutex);
    for (i=0; i<s_rd_len; i++)
    {
        //snprintf(str_temp,20,"0x%2x,",g_axs_data->debug_rx_buf[i]);
        count = snprintf(buf+total_count, (PAGE_SIZE-total_count), "0x%2x,",g_axs_data->debug_rx_buf[i]);
        total_count  += count;
    }
    count = snprintf(buf+total_count, (PAGE_SIZE-total_count), "\n");
    total_count  += count;
    mutex_unlock(&s_device_mutex);
    s_rd_len = 0;
    return total_count;
}

/************************************************************************
* Name: axs_tprwreg_store
* Brief:  read/write register
* Input: device, device attribute, char buf, char count
* Output: print register value
* Return: char count
reg write:{'w',w_len,w_buf...}
sample1:'w010a'; write 1byte, cmd_type 0x0a
sample2:'w071331415107080a'; write 7byte{0x13,0x31,0x41,0x51,0x07,0x08,0x0a}, cmd_type 0x07 means write sfr,sfr[0x08]=0x0a

reg read: {'r',w_len,r_len,w_buf...}
sample2:'r010209'; write 1byte, cmd_type 0x09; read 2 byte for firmware id

cmd_type:
0x13,0x31,0x41,0x51,0x05: write sfr reg
0x13,0x31,0x41,0x51,0x06: read sfr reg
0x13,0x31,0x41,0x51,0x07: write scan reg
0x13,0x31,0x41,0x51,0x08: read scan reg
*************************************************************************/

static ssize_t axs_tprwreg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    //ssize_t num_read_chars = 0;
    int ret = 0;

    //int retval;
    int i =0;
    u8 op_type=0; // 0: read ;  1: write;
    size_t wt_len=0;
    size_t rd_len=0;
    //u8 wt_buf[AXS_CMD_MAX_BUF_LEN+4] = {0x13, 0x31, 0x41, 0x51};
    //u8 rd_buf[AXS_CMD_MAX_BUF_LEN];
    AXS_DEBUG("axs_tprwreg_store");

    mutex_lock(&s_device_mutex);
    if (count<3)
    {
        AXS_ERROR("please input more than 5 chars\n");
        goto error_return;
    }
    if ((!g_axs_data->debug_tx_buf)||(!g_axs_data->debug_rx_buf))
    {
        AXS_ERROR("axs_tprwreg_store  i2c buff == null");
        goto error_return;
    }

    if (buf[0]=='w')
    {
        op_type = 0;

    }
    else if (buf[0]=='r')
    {
        op_type = 1;
    }
    else
    {
        AXS_ERROR("buf[0] should be w or r\n");
        goto error_return;
    }

    if (1 == op_type) // read
    {
        if (count<5)
        {
            AXS_ERROR("please input more than 5 chars when read reg\n");
            goto error_return;
        }
        wt_len = hex_to_u8(&buf[1],2);
        rd_len = hex_to_u8(&buf[3],2);
        AXS_DEBUG("wt_len:%d,rd_len:%d",wt_len,rd_len);

        if ((wt_len > AXS_CMD_MAX_WRITE_BUF_LEN)||(rd_len > AXS_CMD_MAX_READ_BUF_LEN))
        {
            AXS_ERROR("axs_tprwreg_store write or read buf len is too large,wt_len:%d,rd_len:%d",wt_len,rd_len);
            goto error_return;
        }

        for (i=0; i<wt_len; i++)
        {
            g_axs_data->debug_tx_buf[i] = hex_to_int(&(buf[5+i*2]), 2); // op_type = read: out cmd begin at g_axs_data->debug_tx_buf[4], in cmd begin at buf[5]
        }

        ret = axs_write_bytes_read_bytes(g_axs_data->debug_tx_buf,wt_len,g_axs_data->debug_rx_buf,rd_len);
        AXS_DEBUG("axs_write_bytes_read_bytes ret=%d, cmd:\n",ret);
        for (i=0; i<wt_len; i++)
        {
            AXS_DEBUG("0x%02x,", g_axs_data->debug_tx_buf[i]);
        }

        AXS_DEBUG("read result:\n");
        for (i=0; i<rd_len; i++)
        {
            AXS_DEBUG("0x%02x,", g_axs_data->debug_rx_buf[i]);
        }
        s_rd_len = rd_len;
    }
    else if (0 == op_type)  // write
    {
        wt_len = hex_to_u8(&buf[1],2);

        if (wt_len > AXS_CMD_MAX_WRITE_BUF_LEN)
        {
            AXS_ERROR("write buf len is too large,wt_len:%d",wt_len);
            goto error_return;
        }

        for (i=0; i<wt_len; i++)
        {
            g_axs_data->debug_tx_buf[i] = hex_to_int(&(buf[3+i*2]), 2); // op_type = write: cmd begin at out g_axs_data->debug_tx_buf[4], cmd begin at in buf[3]
        }

        ret = axs_write_bytes(g_axs_data->debug_tx_buf, wt_len);
        if (ret < 0)
        {
            AXS_ERROR("axs_write_bytes fail, cmd:\n");
            for (i=0; i<wt_len; i++)
            {
                AXS_DEBUG("0x%02x,", g_axs_data->debug_tx_buf[i]);
            }

        }
        s_rd_len = 0;
    }
error_return:
    mutex_unlock(&s_device_mutex);

    return count;
}

//#if AXS_AUTO_UPGRADE_EN
/*
 * axs_upgrade_bin interface
 */
static ssize_t axs_fwupgrade_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return -EPERM;
}

static ssize_t axs_fwupgrade_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    char fwname[AXS_FW_MAX_FILE_NAME_LENGTH];
    if ((count <= 1) || (count >= AXS_FW_MAX_FILE_NAME_LENGTH - 32))
    {
        AXS_DEBUG("fw bin name's length(%d) fail", (int)count);
        return -EINVAL;
    }
    memset(fwname, 0, sizeof(fwname));
    snprintf(fwname, AXS_FW_MAX_FILE_NAME_LENGTH, "%s", buf);
    fwname[count - 1] = '\0';

    AXS_DEBUG("upgrade firmware by sysfs");
    mutex_lock(&s_device_mutex);

    axs_debug_fwupg(fwname);

    mutex_unlock(&s_device_mutex);

    return count;
}
//#endif

//#if AXS_DOWNLOAD_APP_EN
/*
 * axs_upgrade_bin interface
 */
static ssize_t axs_downloadapp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return -EPERM;
}

static ssize_t axs_downloadapp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    char fwname[AXS_FW_MAX_FILE_NAME_LENGTH];
    if ((count <= 1) || (count >= AXS_FW_MAX_FILE_NAME_LENGTH - 32))
    {
        AXS_DEBUG("fw bin name's length(%d) fail", (int)count);
        return -EINVAL;
    }
    memset(fwname, 0, sizeof(fwname));
    snprintf(fwname, AXS_FW_MAX_FILE_NAME_LENGTH, "%s", buf);
    fwname[count - 1] = '\0';

    AXS_DEBUG("upgrade firmware by sysfs");
    mutex_lock(&s_device_mutex);

    axs_debug_download_app(fwname);

    mutex_unlock(&s_device_mutex);

    return count;
}
//#endif

/*
 * axs_driver_version interface
 */

static ssize_t axs_driverversion_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int count;
    int ret = 0;
    u8 val[2] = { 0 };
    ret = axs_read_fw_version(&val[0]);
    if (ret < 0)
    {
        AXS_DEBUG("axs_read_fw_version fail");
    }
    mutex_lock(&s_device_mutex);
    count = snprintf(buf, PAGE_SIZE, "driver version = %s firmware version = 0x%x\n",AXS_DRIVER_VERSION, val[0]<<8|val[1]);
    mutex_unlock(&s_device_mutex);
    return count;
}

static ssize_t axs_driverversion_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    return -EPERM;
}

static ssize_t axs_debug_str_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int count;
    int ret;
    u8 write_reg[5] = {0x13,0x31,0x41,0x51,AXS_FREG_DEBUG_LEN_READ}; // read tx,rx
    u8 debug_str_len[2]= {0,0};
    ret = axs_write_bytes_read_bytes(write_reg,5,debug_str_len,2);
    if (ret < 0)
    {
        AXS_ERROR("read debug len fail");
        return -EPERM;
    }
    write_reg[4]=AXS_FREG_DEBUG_STR_READ; // read diff
    ret = axs_write_bytes_read_bytes(write_reg,5,g_axs_data->debug_rx_buf,(debug_str_len[0]<<8)|debug_str_len[1]);
    if (ret < 0)
    {
        AXS_ERROR("read debug str fail");
        return -EPERM;
    }
    mutex_lock(&s_device_mutex);
    count = snprintf(buf,PAGE_SIZE, "%s\n",g_axs_data->debug_rx_buf);
    mutex_unlock(&s_device_mutex);
    return count;
}


static ssize_t axs_debug_str_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    return -EPERM;
}
#if AXS_DEBUG_LOG_EN
static ssize_t axs_log_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int count;

    mutex_lock(&s_device_mutex);
    count = snprintf(buf, PAGE_SIZE,"log en:%d\n",g_log_en);
    mutex_unlock(&s_device_mutex);

    return count;
}


static ssize_t axs_log_en_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    //struct input_dev *input_dev = g_axs_data->input_dev;
    mutex_lock(&s_device_mutex);
    g_log_en = hex_to_u8(&buf[0],2);
    AXS_DEBUG("set log enable:%d",g_log_en);
    mutex_unlock(&s_device_mutex);
    return count;
}
#endif
static ssize_t axs_diff_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int count;
    int total_count=0;
    int i,j;
    int ret;
    u8 write_reg[5] = {0x13,0x31,0x41,0x51,AXS_FREG_TXRX_NUM_READ}; // read tx,rx
    u8 tx_rx_len[2]= {0,0};
    ret = axs_write_bytes_read_bytes(write_reg,5,tx_rx_len,2);
    if (ret < 0)
    {
        AXS_ERROR("read tx rx num fail");
        return -EPERM;
    }
    write_reg[4]=AXS_FREG_DIFF_READ; // read diff
    ret = axs_write_bytes_read_bytes(write_reg,5,g_axs_data->debug_rx_buf,tx_rx_len[0]*tx_rx_len[1]);
    if (ret < 0)
    {
        AXS_ERROR("read rawdata fail");
        return -EPERM;
    }

    mutex_lock(&s_device_mutex);
    for (i=0; i<tx_rx_len[0]; i++) // tx row
    {
        for (j=0; j<tx_rx_len[1]; j++) // rx col
        {
            count = snprintf(buf+total_count, (PAGE_SIZE-total_count), "0x%2x,",g_axs_data->debug_rx_buf[i*tx_rx_len[0]+j]);
            total_count  += count;
        }
        count = snprintf(buf+total_count, (PAGE_SIZE-total_count), "\n");
        total_count  += count;
    }
    mutex_unlock(&s_device_mutex);
    return total_count;
}


static ssize_t axs_diff_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    return -EPERM;
}
static ssize_t axs_rawdata_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int count;
    int total_count=0;
    int i,j;
    int ret;
    u8 write_reg[5] = {0x13,0x31,0x41,0x51,AXS_FREG_TXRX_NUM_READ}; // read tx,rx
    u8 tx_rx_len[2]= {0,0};
    ret = axs_write_bytes_read_bytes(write_reg,5,tx_rx_len,2);
    if (ret < 0)
    {
        AXS_ERROR("read tx rx num fail");
        return -EPERM;
    }
    write_reg[4]=AXS_FREG_RAWDATA_READ; // read rawdata
    ret = axs_write_bytes_read_bytes(write_reg,5,g_axs_data->debug_rx_buf,tx_rx_len[0]*tx_rx_len[1]);
    if (ret < 0)
    {
        AXS_ERROR("read rawdata fail");
        return -EPERM;
    }

    mutex_lock(&s_device_mutex);
    for (i=0; i<tx_rx_len[0]; i++) // tx row
    {
        for (j=0; j<tx_rx_len[1]; j++) // rx col
        {
            count = snprintf(buf+total_count, (PAGE_SIZE-total_count), "0x%2x,",g_axs_data->debug_rx_buf[i*tx_rx_len[0]+j]);
            total_count  += count;
        }
        count = snprintf(buf+total_count, (PAGE_SIZE-total_count), "\n");
        total_count  += count;
    }
    mutex_unlock(&s_device_mutex);
    return total_count;
}

static ssize_t axs_rawdata_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    return -EPERM;
}

/*
 * axs_hw_reset interface
 */
static ssize_t axs_hw_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    return -EPERM;
}

static ssize_t axs_hw_reset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t count = 0;

    mutex_lock(&s_device_mutex);
    axs_reset_proc(20);
    count = snprintf(buf, PAGE_SIZE, "hw reset!\n");
    mutex_unlock(&s_device_mutex);

    return count;
}

#if AXS_GESTURE_EN
static ssize_t axs_gesture_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    //struct input_dev *input_dev = g_axs_data->input_dev;
    mutex_lock(&s_device_mutex);
    g_axs_data->gesture_enable = hex_to_u8(&buf[0],2);
    axs_write_gesture_enable(g_axs_data->gesture_enable);
    AXS_DEBUG("set gesture enable:%d",g_axs_data->gesture_enable);
    mutex_unlock(&s_device_mutex);
    return count;
}

static ssize_t axs_gesture_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    //struct input_dev *input_dev = g_axs_data->input_dev;
    int count;
    mutex_lock(&s_device_mutex);
    axs_read_gesture_enable(&g_axs_data->gesture_enable);
    count = snprintf(buf, PAGE_SIZE, "get gesture enable: %s\n", g_axs_data->gesture_enable ? "On" : "Off");
    mutex_unlock(&s_device_mutex);
    return count;
}
#endif

#if AXS_ESD_CHECK_EN
static ssize_t axs_esd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    //struct input_dev *input_dev = g_axs_data->input_dev;
    mutex_lock(&s_device_mutex);
    g_axs_data->esd_host_enable = hex_to_u8(&buf[0],2);
    AXS_DEBUG("set esd check enable:%d",g_axs_data->esd_host_enable);
    mutex_unlock(&s_device_mutex);
    return count;
}

static ssize_t axs_esd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    //struct input_dev *input_dev = g_axs_data->input_dev;
    int count;
    mutex_lock(&s_device_mutex);
    count = snprintf(buf, PAGE_SIZE, "get esd check enable: %s\n", g_axs_data->esd_host_enable ? "On" : "Off");
    mutex_unlock(&s_device_mutex);
    return count;
}
#endif

static DEVICE_ATTR(axs_rw_reg, S_IRUGO | S_IWUSR, axs_tprwreg_show, axs_tprwreg_store);
//#if AXS_AUTO_UPGRADE_EN
/*  upgrade from fw bin file   example:echo "*.bin" > axs_upgrade_bin */
static DEVICE_ATTR(axs_upgrade_bin, S_IRUGO | S_IWUSR, axs_fwupgrade_show, axs_fwupgrade_store);
//#endif

//#if AXS_DOWNLOAD_APP_EN
static DEVICE_ATTR(axs_dlapp_bin, S_IRUGO | S_IWUSR, axs_downloadapp_show, axs_downloadapp_store);
//#endif

static DEVICE_ATTR(axs_driver_version, S_IRUGO | S_IWUSR, axs_driverversion_show, axs_driverversion_store);
static DEVICE_ATTR(axs_hw_reset, S_IRUGO | S_IWUSR, axs_hw_reset_show, axs_hw_reset_store);
#if AXS_GESTURE_EN
static DEVICE_ATTR (axs_gesture_enable, S_IRUGO | S_IWUSR, axs_gesture_show, axs_gesture_store);
#endif
#if AXS_ESD_CHECK_EN
static DEVICE_ATTR (axs_esd_enable, S_IRUGO | S_IWUSR, axs_esd_show, axs_esd_store);
#endif
static DEVICE_ATTR(axs_driver_diff, S_IRUGO | S_IWUSR, axs_diff_show, axs_diff_store);
static DEVICE_ATTR(axs_driver_rawdata, S_IRUGO | S_IWUSR, axs_rawdata_show, axs_rawdata_store);
static DEVICE_ATTR(axs_driver_debug_str, S_IRUGO | S_IWUSR, axs_debug_str_show, axs_debug_str_store);
#if AXS_DEBUG_LOG_EN
static DEVICE_ATTR(axs_log_en, S_IRUGO | S_IWUSR, axs_log_en_show, axs_log_en_store);
#endif

/*
 /sys/devices/platform/soc/soc:ap-apb/70b00000.spi/spi_master/spi1/spi1.0/axs_rw_reg
 /sys/devices/platform/soc/soc:ap-apb/70b00000.spi/spi_master/spi1/spi1.0/axs_hw_reset
 /sys/devices/platform/soc/soc:ap-apb/70b00000.spi/spi_master/spi1/spi1.0/axs_driver_version
 ......
*/
static struct attribute *axs_attributes[] =
{
    &dev_attr_axs_rw_reg.attr,
//#if AXS_AUTO_UPGRADE_EN
    &dev_attr_axs_upgrade_bin.attr,
//#endif
//#if AXS_DOWNLOAD_APP_EN
    &dev_attr_axs_dlapp_bin.attr,
//#endif
    &dev_attr_axs_driver_version.attr,
    &dev_attr_axs_hw_reset.attr,
#if AXS_GESTURE_EN
    &dev_attr_axs_gesture_enable.attr,
#endif
#if AXS_ESD_CHECK_EN
    &dev_attr_axs_esd_enable.attr,
#endif
    &dev_attr_axs_driver_diff.attr,
    &dev_attr_axs_driver_rawdata.attr,
    &dev_attr_axs_driver_debug_str.attr,
#if AXS_DEBUG_LOG_EN
    &dev_attr_axs_log_en.attr,
#endif
    NULL
};

static struct attribute_group axs_attribute_group =
{
    .attrs = axs_attributes
};

/************************************************************************
* Name: axs_debug_create_sysfs
* Brief: create sysfs interface
* Input:
* Output:
* Return: return 0 if success
***********************************************************************/
bool axs_debug_create_sysfs(struct axs_ts_data *ts_data)
{
    int ret = 0;
#if AXS_BUS_SPI
    struct spi_device *client = ts_data->spi;
#else
    struct i2c_client *client = ts_data->client;
#endif
    ret = sysfs_create_group(&(client->dev.kobj), &axs_attribute_group);
    if (ret)
    {
        AXS_DEBUG("[EX]: sysfs_create_group() failed!!");
        sysfs_remove_group(&(client->dev.kobj), &axs_attribute_group);
        return false;
    }
    else
    {
        AXS_DEBUG("[EX]: sysfs_create_group() succeeded!!");
    }

    ts_data->debug_tx_buf = kzalloc(AXS_CMD_MAX_WRITE_BUF_LEN, GFP_KERNEL); // 4*1024
    if (NULL == ts_data->debug_tx_buf)
    {
        AXS_ERROR("failed to allocate memory for debug_tx_buf");
        return false;
    }

    ts_data->debug_rx_buf = kzalloc(AXS_CMD_MAX_READ_BUF_LEN, GFP_KERNEL); // 4*1024
    if (NULL == ts_data->debug_rx_buf)
    {
        AXS_ERROR("failed to allocate memory for debug_rx_buf");
        return false;
    }

    mutex_init(&s_device_mutex);
    return true;
}
/************************************************************************
* Name: axs_debug_remove_sysfs
* Brief: remove sysfs interface
* Input:
* Output:
* Return:
***********************************************************************/
int axs_debug_remove_sysfs(struct axs_ts_data *ts_data)
{
#if AXS_BUS_SPI
    struct spi_device *client = ts_data->spi;
#else
    struct i2c_client *client = ts_data->client;
#endif
    kfree_safe(ts_data->debug_tx_buf);
    kfree_safe(ts_data->debug_rx_buf);
    sysfs_remove_group(&(client->dev.kobj), &axs_attribute_group);
    mutex_destroy(&s_device_mutex);
    return 0;
}


#if AXS_DEBUG_PROCFS_EN
#define PROC_HW_RESET                      's'
#define PROC_READ_CMD                      'r'
#define PROC_WRITE_CMD                     'w'

#define PROC_NAME                          "axs_debug"

static ssize_t axs_proc_write(struct file *filp, const char __user *buff, size_t count, loff_t *ppos)
{
    int buflen = count;
    int writelen = 0;
    int ret = 0;
    struct axs_ts_data *ts_data = g_axs_data;
    u8 *writebuf = g_axs_data->debug_tx_buf;

    if ((!g_axs_data->debug_tx_buf) || (count == 0) || (count > AXS_CMD_MAX_WRITE_BUF_LEN))
    {
        AXS_ERROR("axs_proc_write count(%d) fail", (int)count);
        return -EINVAL;
    }

    if (copy_from_user(writebuf, buff, count))   // &
    {
        AXS_ERROR("axs_proc_write: copy from user error!!");
        return -EFAULT;
    }

    ts_data->proc_opmode = writebuf[0];

    switch (ts_data->proc_opmode)
    {
        case PROC_HW_RESET:
            AXS_DEBUG("axs_proc_write Reset");
#if AXS_DOWNLOAD_APP_EN
            if (!axs_download_init())
            {
                AXS_ERROR("axs_download_init fail!\n");
            }
#else
            axs_reset_proc(20);
#endif
            break;

        case PROC_READ_CMD:
        case PROC_WRITE_CMD:
            writelen = buflen - 1;
            if ((writelen > AXS_CMD_MAX_WRITE_BUF_LEN)||(writelen ==0))
            {
                AXS_ERROR("axs_proc_write error len:%d",writelen);
                return -EINVAL;
            }

            ret = axs_write_bytes(writebuf + 1, writelen);
            if (ret < 0)
            {
                AXS_ERROR("axs_write_bytes error");
            }
            break;
        default:
            break;
    }

    if (ret < 0)
    {
        return ret;
    }
    else
    {
        return count;
    }
}

static ssize_t axs_proc_read(struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int ret = 0;
    int num_read_chars = 0;
    struct axs_ts_data *ts_data = g_axs_data;

    if ((!g_axs_data->debug_rx_buf)|| (count == 0) || (count > AXS_CMD_MAX_READ_BUF_LEN))
    {
        AXS_ERROR("axs_proc_read count:%d fail", (int)count);
        return -EINVAL;
    }
    switch (ts_data->proc_opmode)
    {
        case PROC_READ_CMD:
            ret = axs_read_bytes(g_axs_data->debug_rx_buf, count);
            if (ret < 0)
            {
                AXS_ERROR("axs_proc_read iic error!!");
                return ret;
            }
            num_read_chars = count;
            break;
        default:
            AXS_ERROR("axs_proc_read cmd error!!");
            return -EINVAL;
            break;
    }
    if (copy_to_user(buff, g_axs_data->debug_rx_buf, num_read_chars))
    {
        AXS_ERROR("axs_proc_read copy to user error!!");
        return -EFAULT;
    }
    return num_read_chars;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops axs_proc_fops =
{
    .proc_read   = axs_proc_read,
    .proc_write  = axs_proc_write,
};
#else
static const struct file_operations axs_proc_fops =
{
    .owner  = THIS_MODULE,
    .read   = axs_proc_read,
    .write  = axs_proc_write,
};
#endif

int axs_create_proc_file(struct axs_ts_data *ts_data)
{
    ts_data->proc_dir = proc_create(PROC_NAME, 0777, NULL, &axs_proc_fops);
    if (NULL == ts_data->proc_dir)
    {
        AXS_ERROR("axs_create_proc_file fail");
        return -ENOMEM;
    }
    else
    {
        AXS_DEBUG("axs_create_proc_file success!");
    }
    return 0;
}

void axs_release_proc_file(struct axs_ts_data *ts_data)
{

    if (ts_data->proc_dir)
    {
        proc_remove(ts_data->proc_dir); /* remove_proc_entry(PROC_NAME,ts_data->proc_dir);*/
        ts_data->proc_dir = NULL;
    }
}

#endif


