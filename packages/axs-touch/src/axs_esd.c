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

void axs_reset_and_lcd_init(void)
{
	AXS_DEBUG("axs_reset_and_lcd_init!\n");
//#if 0
//	g_axs_reset_flag = 1;
//	g_axs_data->esd_firmware_enable = 0;
//#else
	axs_lcd_off();
#if AXS_DOWNLOAD_APP_EN
	if (!axs_download_init()) //include axs_lcd_init()
	{
		AXS_ERROR("axs_download_init fail!\n");
	}
#else
	axs_irq_disable();
	axs_reset_proc(20);
	msleep(100); // delay 100ms after reset
	axs_lcd_init(); //init lcd (11,29)
	msleep(50); //read/write tp after 50ms of lcd init
	axs_irq_enable();	
#endif
	g_axs_data->esd_firmware_enable = 0;
//#endif
}
#if AXS_ESD_CHECK_EN	
#define ESD_CHECK_DELAY_TIME              500 /*unit:ms*/

bool axs_esd_firmware_enable(void)
{
    int ret = 0;
	u8 write_cmd[13] = {0xb5,0xab,0x5a,0xa5,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0xff,0x00};  // max write len: 22-12=10
    write_cmd[4] = 0;
    write_cmd[5] = 2;
    write_cmd[11]=0x21;
    write_cmd[12] = 0;
	ret = axs_write_bytes(write_cmd,13);
    if (ret<0)
    {
        AXS_DEBUG("axs_write_bytes fail");
        return false;
    }
	return true;
}

bool axs_esd_error(int exception_code)
{
	bool ret = false;
    switch (exception_code)
    {
        case RAWDATA_EXCEPTION:
		case SCAN_EXCEPTION:
		case ESD_EXCEPTION:
			ret = true;
			break;
		default:
			ret = false;
			break;
    }
	return ret;
}

static void esd_check_func(struct work_struct *work)
{
    if (g_axs_data->fw_loading ||(!g_axs_data->esd_host_enable)||g_axs_reset_flag)
    {
        AXS_DEBUG("skip esd_check_func,fw_loading:%d,esd_host_enable:%d",g_axs_data->fw_loading,g_axs_data->esd_host_enable);
    }
	else
	{
		if(0==g_axs_data->esd_firmware_enable)
		{
			/*enable firmware esd after 1s of reset*/
			if(g_axs_data->esd_firmware_enable_delay>=2)
			{
				if(axs_esd_firmware_enable())
				{
					AXS_DEBUG("axs_esd_firmware_enable success");
					g_axs_data->esd_firmware_enable = 1;
					g_axs_data->tp_no_touch_500ms_count = 0;
					g_axs_data->esd_firmware_enable_fail_count=0;
				}
				else
				{
					g_axs_data->esd_firmware_enable_fail_count++;
					if(g_axs_data->esd_firmware_enable_fail_count>=3)
					{
						//clear count
						g_axs_data->tp_no_touch_500ms_count = 0;
						g_axs_data->esd_firmware_enable_fail_count = 0;
						axs_reset_and_lcd_init();
						axs_release_all_finger();
					}
				}
			}
			else
			{
				g_axs_data->esd_firmware_enable_delay++;
			}
		}
		else
		{
			g_axs_data->esd_firmware_enable_delay = 0;
			g_axs_data->esd_firmware_enable_fail_count=0;
		    g_axs_data->tp_no_touch_500ms_count += 1;
		    AXS_DEBUG("esd_check_func,500ms_count:%d",g_axs_data->tp_no_touch_500ms_count);
			if (g_axs_data->tp_no_touch_500ms_count>=3)/*no report interrupt for 1.5s */
			{
				//clear count
				g_axs_data->tp_no_touch_500ms_count = 0;
				axs_reset_and_lcd_init();
				axs_release_all_finger();
			}
		}
	}
    queue_delayed_work(g_axs_data->ts_workqueue, &g_axs_data->esd_check_work,
                       msecs_to_jiffies(ESD_CHECK_DELAY_TIME));
}

int axs_esd_check_init(struct axs_ts_data *ts_data)
{
    AXS_FUNC_ENTER();

    if (ts_data->ts_workqueue)
    {
        INIT_DELAYED_WORK(&ts_data->esd_check_work, esd_check_func);
    }
    else
    {
        AXS_ERROR("ts_workqueue is NULL");
        return -EINVAL;
    }
	
	g_axs_data->esd_firmware_enable = 0;
	g_axs_data->esd_firmware_enable_delay = 0;
	g_axs_data->esd_firmware_enable_fail_count=0;
    g_axs_data->esd_host_enable = 1;

    queue_delayed_work(g_axs_data->ts_workqueue, &g_axs_data->esd_check_work,
                       msecs_to_jiffies(ESD_CHECK_DELAY_TIME*2));

    AXS_FUNC_EXIT();
    return 0;
}

int axs_esd_check_suspend(void)
{
    AXS_FUNC_ENTER();
    cancel_delayed_work(&g_axs_data->esd_check_work);
	g_axs_data->tp_no_touch_500ms_count = 0;
    AXS_FUNC_EXIT();
    return 0;
}

int axs_esd_check_resume( void )
{
    AXS_FUNC_ENTER();
	g_axs_data->tp_no_touch_500ms_count = 0;
	g_axs_data->esd_firmware_enable = 0;	
    queue_delayed_work(g_axs_data->ts_workqueue, &g_axs_data->esd_check_work,
                           msecs_to_jiffies(ESD_CHECK_DELAY_TIME*2));
    AXS_FUNC_EXIT();
    return 0;
}
#endif

