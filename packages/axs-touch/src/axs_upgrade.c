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
#define I2C_CMD_INTERVAL 1
#define AXS_APP_BIN_VER_OFFSET          0x5035
#define AXS_MAX_FW_LEN                  (128 * 1024)
#define AXS_UPGRADE_FILE                "firmware/firmware_flash.i"

#define SYSFS_FW_UPGRAGE_NAME			"firmware_flash.bin"


void axs_reset_and_lcd_init(void);

struct upgrade_func
{
    u32 fwveroff;
    bool (*upgrade)(u8 *, u32);
};

struct axs_upgrade
{
    //struct axs_ts_data *ts_data;
    struct upgrade_func *func;
    //struct upgrade_setting_nf *setting_nf;
    //int module_id;
    u8 *fw;
    u32 fw_length;
};


static struct axs_upgrade *fwupgrade=NULL;

#if AXS_AUTO_UPGRADE_EN
static const u8 fw_file[] =
{
#include AXS_UPGRADE_FILE
};
static bool axs_fwupg_get_fw_file(struct axs_upgrade *upg)
{
    AXS_DEBUG("get upgrade fw file");
    if (!upg)
    {
        AXS_ERROR("upg is null");
        return false;
    }


    upg->fw = (u8 *)fw_file;
    upg->fw_length = sizeof(fw_file);

    AXS_DEBUG("upgrade fw file len:%d", upg->fw_length);
    if ((upg->fw_length > AXS_MAX_FW_LEN)||(upg->fw_length < 1024))
    {
        AXS_ERROR("fw file len(%d) fail!", upg->fw_length);
        return false;
    }
    return true;
}
#endif

#if AXS_UPGRADE_CHECK_VERSION
static bool axs_fwupg_get_ver_in_bin(struct axs_upgrade *upg,u8 *ver)
{

    u32 ver_offset = ((upg->fw[0x0B]<<8) | (upg->fw[0x0C])) + upg->func->fwveroff;
    AXS_DEBUG("axs_fwupg_get_ver_in_bin ver_offset=%x + %x",((upg->fw[0x0B]<<8) | (upg->fw[0x0C])),upg->func->fwveroff);

    if ((!upg) || (!upg->func) || (!upg->fw) || (!ver))
    {
        AXS_ERROR("axs_data/upgrade/func/fw/ver is NULL");
        return false;
    }

    if (upg->fw_length < ver_offset)
    {
        AXS_ERROR("fw len(0x%0x) < fw ver offset(0x%x)",
                  upg->fw_length, ver_offset);
        return false;
    }

    AXS_DEBUG("fw version offset:0x%x", ver_offset);

    *ver = upg->fw[ver_offset];
    return true;
}
#endif

#ifdef FW_SUM_CHECK
static bool axs_fwupg_get_check_sum_in_bin(u8 *fw,u16 *crc_code)
{
	u16 start_app_addr = ((fw[0x0b]<<8) | (fw[0x0c]));
    *crc_code = (u16)((fw[0x5037+start_app_addr]<<8) | (fw[0x5036]+start_app_addr));
    return true;
}

static bool axs_fwupg_get_check_sum_in_tp(u16 *crc_code)
{
	int ret =0;
    u8 cmd_type[1] = {0x33};
	u8 tmp[2];
    ret= axs_read_regs(cmd_type, 1, tmp, 2);
    if (ret < 0)
    {
        AXS_ERROR("read crc fail");
        return false;
    }	
	*crc_code = (u16)((tmp[1]<<8) | (tmp[0]));
	return true;
}
#endif

static bool axs_fwupg_need_upgrade(struct axs_upgrade *upg)
{
#if AXS_UPGRADE_CHECK_VERSION
    bool ret = 0;
    u8 fw_ver_in_bin = 0;
    u8 fw_ver_in_tp = 0;
    ret = axs_fwupg_get_ver_in_bin(upg, &fw_ver_in_bin);
    if (!ret)
    {
        AXS_ERROR("get fw ver in host fail");
        return false;
    }

    ret = axs_fwupg_get_ver_in_tp(&fw_ver_in_tp);
    if (!ret)
    {
        AXS_ERROR("get fw ver in tp fail");
        return false;
    }

    AXS_DEBUG("fw version in tp:%x, host:%x", fw_ver_in_tp, fw_ver_in_bin);
	if ((fw_ver_in_tp != fw_ver_in_bin)&&(fw_ver_in_tp!=0)) /*if (fw_ver_in_tp != fw_ver_in_bin)*/
    {
        return true;
    }
    return false;
#else
    return true;
#endif
}

bool selectFlash(void)
{
    int ret = 0;
    u8 write_buf[] = {0xa5, 0x5a, 0xb5, 0xab, 0x00};
//    axs_reset_level(0);
//    usleep_range(1000, 1100);
//    axs_reset_level(1);
//    usleep_range(2000, 2100);
	g_axs_data->esd_firmware_enable = 0;
    ret = axs_write_bytes(write_buf, sizeof(write_buf));
    usleep_range(1000, 1100);
    if (ret < 0)
    {
        AXS_ERROR("upgrade firmware selectFlash fail");
        //axs_reset_level(1);
        return false;
    }
    return true;
}

bool axs_fwupg_upgrade(struct axs_upgrade *upg,bool debug)
{
    bool ret = false;
    bool upgrade_flag = false;
    int upgrade_count = 0;
    u8 ver = 0;

    AXS_DEBUG("axs_fwupg_upgrade function");
    if ((NULL == upg) || (NULL == upg->func))
    {
        AXS_ERROR("upg/upg->func is null");
        return false;
    }

    if (!debug)
    {
        upgrade_flag = axs_fwupg_need_upgrade(upg);
        AXS_DEBUG("fw upgrade flag:%d", upgrade_flag);

        if (!upgrade_flag)
        {
            AXS_DEBUG("do not need upgrade");
            return false;
        }
    }

    do
    {
        upgrade_count++;
        AXS_DEBUG("upgrade fw app (times:%d) begin", upgrade_count);
        if (!selectFlash())
        {
            AXS_ERROR("axs_fwupg_upgrade selectFlash fail");
            return false;
        }
        if (upg->func->upgrade)
        {
            ret = upg->func->upgrade(upg->fw, upg->fw_length);
            axs_reset_proc(100);
            if (!ret)
            {
                AXS_DEBUG("upgrade fw app (times:%d) fail", upgrade_count);
            }
            else
            {
                axs_fwupg_get_ver_in_tp(&ver);

                AXS_DEBUG("upgrade fw app (times:%d) success, new fw version %02x",upgrade_count,ver);
                break;
            }
        }
        else
        {
            AXS_ERROR("upgrade func/upgrade is null, return immediately");
            ret = false;
            break;
        }
    }
    while (upgrade_count < 1);
	
	axs_reset_and_lcd_init();
    return ret;
}

#if AXS_AUTO_UPGRADE_EN
static void axs_fwupg_work(struct work_struct *work)
{
    struct axs_upgrade *upg = fwupgrade;
    AXS_DEBUG("axs_fwupg_work begin");
    if (!upg || !g_axs_data)
    {
        AXS_ERROR("upg/g_axs_data is null");
        return ;
    }

    g_axs_data->fw_loading = 1;
    //axs_irq_disable();

    /* get fw */
    if (!axs_fwupg_get_fw_file(upg))
    {
        AXS_ERROR("get file fail, can't upgrade");
    }
    else
    {
        /* run upgrade firmware*/
        axs_fwupg_upgrade(upg,false);
    }

    //axs_irq_enable();
    g_axs_data->fw_loading = 0;
    AXS_DEBUG("axs_fwupg_work end");
}
#endif



bool flash_is_ready(unsigned int max_times)
{
    int ret = 0;
    int wait_max_times = max_times; // 100
    u8 write_read_buf[20] = { 0xab, 0xb5, 0xa5, 0x5a };
    write_read_buf[4] = 0x00;
    write_read_buf[5] = 0x01;
    write_read_buf[6] = 0x00;
    write_read_buf[7] = 0x01;
    write_read_buf[8] = 0x00;
    write_read_buf[9] = 0x00;
    write_read_buf[10] = 0x00;
    write_read_buf[11] = 0x05;
    write_read_buf[12] = 0xff;
    while (wait_max_times--)
    {
#if AXS_BUS_SPI
        ret = axs_write_bytes_read_bytes_onecs(write_read_buf, 12, &write_read_buf[12],1);
#else
        ret = axs_write_bytes_read_bytes(write_read_buf, 12, &write_read_buf[12],1);
#endif
        if (ret >= 0 && write_read_buf[12] == 0x00)
        {
            break;
        }
        AXS_DEBUG("flash write waiting...");
        usleep_range(1000, 1100);
    }
    if (write_read_buf[12] != 0x00)
    {
        return false;
    }
    return true;
}

bool axs_fwupg_flash_init(void)
{
    int ret = 0;
    u8 write_read_buf[20] = { 0xab, 0xb5, 0xa5, 0x5a };
    AXS_DEBUG("axs_fwupg_flash_init begin");
    write_read_buf[4] = 0x00;
    write_read_buf[5] = 0x01;
    write_read_buf[6] = 0x00;
    write_read_buf[7] = 0x00;
    write_read_buf[8] = 0x00;
    write_read_buf[9] = 0x00;
    write_read_buf[10] = 0x00;
    write_read_buf[11] = 0x06;
    ret = axs_write_bytes(write_read_buf, 12);
    if (ret < 0)
    {
        AXS_ERROR("axs_fwupg_flash_init step 1 fail");
        return false;
    }
    mdelay(I2C_CMD_INTERVAL);

    write_read_buf[4] = 0x00;
    write_read_buf[5] = 0x01;
    write_read_buf[6] = 0x00;
    write_read_buf[7] = 0x01;
    write_read_buf[8] = 0x00;
    write_read_buf[9] = 0x00;
    write_read_buf[10] = 0x00;
    write_read_buf[11] = 0x9f;
#if AXS_BUS_SPI
    ret = axs_write_bytes_read_bytes_onecs(write_read_buf, 12, &write_read_buf[12],1);
#else
    ret = axs_write_bytes_read_bytes(write_read_buf, 12, &write_read_buf[12],1);
#endif
    if (ret < 0)
    {
        AXS_ERROR("axs_fwupg_flash_init step 2 fail");
        return false;
    }
    mdelay(I2C_CMD_INTERVAL);
    write_read_buf[4] = 0x00;
    write_read_buf[5] = 0x01;
    write_read_buf[6] = 0x00;
    write_read_buf[7] = 0x00;
    write_read_buf[8] = 0x00;
    write_read_buf[9] = 0x00;
    write_read_buf[10] = 0x00;
    write_read_buf[11] = 0x06;
    ret = axs_write_bytes(write_read_buf, 12);
    if (ret < 0)
    {
        AXS_ERROR("axs_fwupg_flash_init step 3 fail");
        return false;
    }
    mdelay(I2C_CMD_INTERVAL);
    write_read_buf[4] = 0x00;
    write_read_buf[5] = 0x01;
    write_read_buf[6] = 0x00;
    write_read_buf[7] = 0x01;
    write_read_buf[8] = 0x00;
    write_read_buf[9] = 0x00;
    write_read_buf[10] = 0x00;
    write_read_buf[11] = 0x05;
#if AXS_BUS_SPI
    ret = axs_write_bytes_read_bytes_onecs(write_read_buf, 12, &write_read_buf[12],1);
#else
    ret = axs_write_bytes_read_bytes(write_read_buf, 12, &write_read_buf[12],1);
#endif
    if (ret < 0)
    {
        AXS_ERROR("upgrade init flash step 4 fail");
        return false;
    }
    mdelay(I2C_CMD_INTERVAL);

    write_read_buf[4] = 0x00;
    write_read_buf[5] = 0x02;
    write_read_buf[6] = 0x00;
    write_read_buf[7] = 0x00;
    write_read_buf[8] = 0x00;
    write_read_buf[9] = 0x00;
    write_read_buf[10] = 0x00;
    write_read_buf[11] = 0x01;
    write_read_buf[12] = 0x02;
    ret = axs_write_bytes(write_read_buf, 13);
    if (ret < 0)
    {
        AXS_ERROR("axs_fwupg_flash_init step 5 fail");
        return false;
    }
    mdelay(I2C_CMD_INTERVAL);
    if (!flash_is_ready(2000))
    {
        AXS_ERROR("axs_fwupg_flash_init !flash_is_ready");
        return false;
    }
    AXS_DEBUG("axs_fwupg_flash_init end");
    return true;
}

bool axs_fwupg_flash_erase(void)
{
    int ret = 0;
    u8 erase_cmd_1[]= {0xAB,0xB5,0xA5,0x5A, 0x00, 0x01, 0x00, 0x00,0x00,0x00,0x00,0x06};
    u8 erase_cmd_2[]= {0xAB,0xB5,0xA5,0x5A, 0x00, 0x01, 0x00, 0x00,0x00,0x00,0x00,0xc7};


    AXS_DEBUG("axs_fwupg_flash_erase begin");

    ret = axs_write_bytes(erase_cmd_1, sizeof(erase_cmd_1));
    if (ret < 0)
    {
        AXS_ERROR("axs_fwupg_flash_erase cmd 1 fail");
        return false;
    }
    mdelay(I2C_CMD_INTERVAL);
    ret = axs_write_bytes(erase_cmd_2, sizeof(erase_cmd_2));
    if (ret < 0)
    {
        AXS_ERROR("axs_fwupg_flash_erase cmd 2 fail");
        return false;
    }
    mdelay(I2C_CMD_INTERVAL);
    if (!flash_is_ready(2000))
    {
        AXS_ERROR("axs_fwupg_flash_erase !flash_is_ready()");
        return false;
    }
    AXS_DEBUG("axs_fwupg_flash_erase end");
    return true;
}

bool axs_fwupg_flash_write(u8 *fw_buf, u32 fw_len)
{
    int ret = 0;
    const int packet_len = 256;
    int currentNum = 1;
    int currentSize = 0;
    u32 addr = 0;
    u8 write_buf[300] = { 0xab, 0xb5, 0xa5, 0x5a };
    u8 *pAddr = (u8*)(&addr);
    int totalPackets = fw_len / packet_len;
    if (fw_len % packet_len)
	{
        ++totalPackets;
	}
    AXS_DEBUG("func begin,totalPackets:%x",totalPackets);
    while (currentNum <= totalPackets)
    {
        write_buf[4] = 0x00;
        write_buf[5] = 0x01;
        write_buf[6] = 0x00;
        write_buf[7] = 0x00;
        write_buf[8] = 0x00;
        write_buf[9] = 0x00;
        write_buf[10] = 0x00;
        write_buf[11] = 0x06;
        ret = axs_write_bytes(write_buf, 12);
        if (ret < 0)
        {
            AXS_ERROR("upgrade write firmware step1 fail,writed sector num:%d",currentNum);
            return false;
        }

        currentSize = (currentNum == totalPackets) ? (fw_len - (currentNum - 1) * packet_len) : packet_len;
        addr = (currentNum - 1) * packet_len;
        write_buf[4] = (currentSize + 4)>>8;
        write_buf[5] = (currentSize + 4)&0xff;

        write_buf[6] = 0x00;
        write_buf[7] = 0x00;
        write_buf[8] = 0x00;
        write_buf[9] = 0x00;
        write_buf[10] = 0x00;
        write_buf[11] = 0x02;
        write_buf[12] = *(pAddr + 2);
        write_buf[13] = *(pAddr + 1);
        write_buf[14] = *(pAddr + 0);

        memcpy(&write_buf[15], fw_buf + addr, currentSize);
        ret = axs_write_bytes(write_buf, currentSize+15);
        if (ret<0)
        {
            AXS_ERROR("step2 fail, sector num:%d",currentNum);
            return false;
        }
        if (!flash_is_ready(2000))
        {
            AXS_DEBUG("!flash_is_ready, sector num:%d",currentNum);
            return false;
        }
        AXS_DEBUG("sector num:%d finished",currentNum);
        ++currentNum;
    }

    AXS_DEBUG("func end");
    return true;
}


bool axs_fwupg_flash_read_check(u32 fw_len) // , u8*read_buf
{
    int ret = 0;
    u8 *tmpbuf = NULL;
    const int packet_len = 256;
    int currentNum = 1;
    int currentSize = 0;
    u32 addr = 0;
    u8 write_buf[15] = { 0xab, 0xb5, 0xa5, 0x5a };
    u8 *pAddr = (u8*)(&addr);
    int totalPackets = fw_len / packet_len;
    if (fw_len % packet_len)
	{
        ++totalPackets;
	}
    tmpbuf = kzalloc(packet_len+1, GFP_KERNEL); // alloc 257 byte buff
    if (!tmpbuf)
    {
        AXS_ERROR("tmpbuf alloc fail");
        return false;
    }

    while (currentNum <= totalPackets)
    {
        currentSize = (currentNum == totalPackets) ? (fw_len - (currentNum - 1) * packet_len) : packet_len;
        addr = (currentNum - 1) * packet_len;

        write_buf[4] = 0x00;
        write_buf[5] = 0x04;
        write_buf[6] = (currentSize + 1)>>8;
        write_buf[7] = (currentSize + 1)&0xff;
        write_buf[8] = 0x00;
        write_buf[9] = 0x00;
        write_buf[10] = 0x00;
        write_buf[11] = 0x0b;
        write_buf[12] = *(pAddr + 2);
        write_buf[13] = *(pAddr + 1);
        write_buf[14] = *(pAddr + 0);

#if AXS_BUS_SPI
        ret = axs_write_bytes_read_bytes_onecs(write_buf, 15, tmpbuf,currentSize+1); // currentSize<=packet_len
#else
        ret = axs_write_bytes_read_bytes(write_buf, 15, tmpbuf,currentSize+1); // currentSize<=packet_len
#endif
        if (ret < 0)
        {
            AXS_ERROR("step2 fail, sector num:%d",currentNum);
            kfree_safe(tmpbuf);
            return false;
        }
	    if (memcmp(fwupgrade->fw+addr,&tmpbuf[1],currentSize))
	    {
	        AXS_DEBUG("read firmware is not same with writed firmware");
			kfree_safe(tmpbuf);
	        return false;
	    }
        //memcpy(read_buf + addr, &tmpbuf[1], currentSize);
        ++currentNum;
    }

    kfree_safe(tmpbuf);
    return true;
}

static bool axs_Y15205_upgrade(u8 *fw_buf, u32 fw_len)
{
    //u8 *tmpbuf = NULL;

    AXS_DEBUG("firmware upgrade...");

    if (NULL == fw_buf)
    {
        AXS_ERROR("fw buf is null");
        return false;
    }
    /*
    if ((fw_len !=AXS_FW_LEN)) {
        AXS_ERROR("fw buffer len(%x) fail", fw_len);
        return false;
    }*/
    if (!axs_fwupg_flash_init())
    {
        AXS_ERROR("axs_fwupg_flash_init fail");
        return false;
    }
    if (!axs_fwupg_flash_erase())
    {
        AXS_ERROR("axs_fwupg_flash_erase fail");
        return false;
    }
    // write firmware
    if (!axs_fwupg_flash_write(fw_buf, fw_len))
    {
        AXS_ERROR("axs_fwupg_flash_write fail");
        return false;
    }
    msleep(100);
    // read firmware
    /*tmpbuf = vmalloc(fw_len+10);//, GFP_KERNEL);
	    if (!tmpbuf)
	    {
	        AXS_ERROR("axs_Y15205_upgrade tmpbuf alloc fail");
	        return false;
	    }*/

    if (!axs_fwupg_flash_read_check(fw_len))// , tmpbuf
    {
        AXS_ERROR("axs_fwupg_flash_read_check fail");
        //vfree_safe(tmpbuf);
        return false;
    }
    /*if (memcmp(fw_buf,tmpbuf,fw_len))
    {
        AXS_ERROR("read firmware is not same with write firmware");
		vfree_safe(tmpbuf);
        return false;
    }
    vfree_safe(tmpbuf);*/
    return true;
}

struct upgrade_func upgrade_func_15205 =
{
    .fwveroff = AXS_APP_BIN_VER_OFFSET,
    .upgrade = axs_Y15205_upgrade,
};

#if AXS_AUTO_UPGRADE_EN
bool axs_fwupg_init(void)
{
    AXS_DEBUG("function begin");

    if (!g_axs_data || !g_axs_data->ts_workqueue)
    {
        AXS_ERROR("g_axs_data/workqueue is NULL, can't run upgrade function");
        return false;
    }

    if (NULL == fwupgrade)
    {
        fwupgrade = (struct axs_upgrade *)kzalloc(sizeof(*fwupgrade), GFP_KERNEL);
        if (NULL == fwupgrade)
        {
            AXS_ERROR("malloc memory for upgrade fail");
            return false;
        }
        fwupgrade->func = &upgrade_func_15205;
    }

    //fwupgrade->ts_data = ts_data;
    INIT_WORK(&g_axs_data->fwupg_work, axs_fwupg_work);
    queue_work(g_axs_data->ts_workqueue, &g_axs_data->fwupg_work);
    AXS_DEBUG("function end");
    return true;
}
#endif

#if AXS_DEBUG_SYSFS_EN
void axs_debug_fwupg(char *fw_name)
{
    int ret = 0;
    struct axs_upgrade *upg;
	const struct firmware *fw = NULL;

    AXS_DEBUG("function begin");
    if (!g_axs_data || !g_axs_data->ts_workqueue)
    {
        AXS_ERROR("g_axs_data/workqueue is NULL, can't run upgrade function");
        return;
    }

    if (NULL == fwupgrade)
    {
        fwupgrade = (struct axs_upgrade *)kzalloc(sizeof(*fwupgrade), GFP_KERNEL);
        if (NULL == fwupgrade)
        {
            AXS_ERROR("malloc memory for upgrade fail");
            return;
        }
        fwupgrade->func = &upgrade_func_15205;
    }

    upg = fwupgrade;
    g_axs_data->fw_loading = 1;
    axs_irq_disable();

    upg->fw = NULL;
    if (request_firmware(&fw, SYSFS_FW_UPGRAGE_NAME, g_axs_data->dev)) 
    {
		AXS_ERROR("request firmware failed");
        return;
	}

	if (fw) {
		AXS_DEBUG("firmware size = %d",fw->size);
        upg->fw = vmalloc(fw->size);
        if (NULL == upg->fw) {
            AXS_ERROR("fw buffer vmalloc fail");
			release_firmware(fw);
			fw = NULL;
            ret = -ENOMEM;
        } else {
            memcpy(upg->fw, fw->data, fw->size);
            ret = fw->size;
        }
        upg->fw_length = fw->size;
		release_firmware(fw);
		fw = NULL;
	}

	if (ret < 0)
	{
		AXS_ERROR("read fw bin file fail, len:%d", ret);
		return;
	}
	
    AXS_DEBUG("fw bin file len:%x", upg->fw_length);

    if ((ret < 0) || (ret >AXS_MAX_FW_LEN)||(upg->fw_length < 1024))
    {
        AXS_ERROR("read fw bin file fail, len:%d", ret);
        goto err;
    }
    else
    {
        axs_fwupg_upgrade(upg,true);
    }

err:
    if (upg->fw)
    {
        vfree(upg->fw);
        upg->fw = NULL;
        upg->fw_length = 0;
    }
    axs_irq_enable();
    g_axs_data->fw_loading = 0;
    AXS_DEBUG("function end");
}
#endif

