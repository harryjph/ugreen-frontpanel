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

#ifndef _LINUX_AXS_CONFIG_H_
#define _LINUX_AXS_CONFIG_H_


#define AXS_I2C_SLAVE_ADDR          0x3B

#define AXS_MT_PROTOCOL_B_EN        1

#define AXS_REPORT_PRESSURE_EN      1

#define AXS_GESTURE_EN                  0

//#define HAVE_TOUCH_KEY

#define AXS_DEBUG_LOG_EN                0
#define AXS_POWER_SOURCE_CUST_EN        0

#define AXS_DOWNLOAD_APP_EN             0

#define AXS_AUTO_UPGRADE_EN             0
#define AXS_UPGRADE_CHECK_VERSION       1

#define AXS_BUS_IIC                     1
#define AXS_BUS_SPI                     0

/*开启手势时，(AXS_GESTURE_EN宏开关时),AXS_DEBUG_SYSFS_EN必须要打开*/
#define AXS_DEBUG_SYSFS_EN              1
//#if AXS_DEBUG_SYSFS_EN
#define AXS_FW_MAX_FILE_NAME_LENGTH     128
//#endif
#define AXS_DEBUG_PROCFS_EN             1

#define AXS_CMD_MAX_WRITE_BUF_LEN       1024
#define AXS_CMD_MAX_READ_BUF_LEN        1024
#endif /* _LINUX_AXS_CONFIG_H_ */

/*ESD CHECK ENABLE*/
#define AXS_ESD_CHECK_EN                0
#define AXS_PEN_EVENT_CHECK_EN          0
#define AXS_POWERUP_CHECK_EN            0
#define AXS_INTERRUPT_THREAD            1
