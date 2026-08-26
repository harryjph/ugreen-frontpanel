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
#if AXS_BUS_IIC
#define I2C_RETRY_NUMBER                    1
#define I2C_BUF_LENGTH                      AXS_CMD_MAX_READ_BUF_LEN /*=max(AXS_CMD_MAX_READ_BUF_LEN,AXS_CMD_MAX_WRITE_BUF_LEN)*/

int axs_write_bytes_read_bytes(u8 *cmd, u16 cmdlen, u8 *data, u16 datalen)
{
    int ret = 0;
    int i = 0;
    struct axs_ts_data *ts_data = g_axs_data;
    struct i2c_client *client = NULL;

    if (!ts_data || !ts_data->client || !cmd || !data
        || (cmdlen >= AXS_CMD_MAX_WRITE_BUF_LEN) || (datalen >= AXS_CMD_MAX_READ_BUF_LEN))
    {
        AXS_ERROR("g_axs_data/client/cmdlen(%d)/data/datalen(%d) is invalid",
                  cmdlen, datalen);
        return -EINVAL;
    }
    client = ts_data->client;

    mutex_lock(&ts_data->bus_mutex);
    for (i = 0; i < I2C_RETRY_NUMBER; i++)
    {
        if (cmd && (cmdlen > 0))
        {
            ret = i2c_master_send(client, (u8 *)cmd, cmdlen);
            if (ret != cmdlen)
            {
                AXS_ERROR("[IIC]: i2c_master_send fail,ret=%d", ret);
                continue;
            }
        }
        ret = i2c_master_recv(client, (u8 *)data, datalen);
        if (ret != datalen)
        {
            AXS_ERROR("[IIC]: i2c_master_recv fail,ret=%d", ret);
            continue;
        }
        break;
    }
    mutex_unlock(&ts_data->bus_mutex);
    return ret;
}

int axs_read_bytes(u8 *rdbuf, u16 rdlen)
{
    int i = 0;
    int ret = 0;
    struct axs_ts_data *ts_data = g_axs_data;
    struct i2c_client *client = NULL;
    if (!ts_data || !ts_data->client || !rdbuf ||(rdlen >= AXS_CMD_MAX_READ_BUF_LEN))
    {
        AXS_ERROR("g_axs_data/client/rdlen(%d) is invalid",rdlen);
        return -EINVAL;
    }
    client = ts_data->client;
    mutex_lock(&ts_data->bus_mutex);
    for (i = 0; i < I2C_RETRY_NUMBER; i++)
    {
        ret = i2c_master_recv(client, (u8 *)rdbuf, rdlen);
        if (ret != rdlen)
        {
            AXS_ERROR("i2c_master_recv fail,ret=%d", ret);
        }
        else
            break;
    }
    mutex_unlock(&ts_data->bus_mutex);
    return ret;
}

int axs_write_bytes(u8 *writebuf, u16 writelen)
{
    int ret = 0;
    int i = 0;
    struct axs_ts_data *ts_data = g_axs_data;
    struct i2c_client *client = NULL;

    if (!ts_data || !ts_data->client || !writebuf || !writelen
        || (writelen >= AXS_CMD_MAX_WRITE_BUF_LEN))
    {
        AXS_ERROR("g_axs_data/client/data/datalen(%d) is invalid", writelen);
        return -EINVAL;
    }
    client = ts_data->client;

    mutex_lock(&ts_data->bus_mutex);
    for (i = 0; i < I2C_RETRY_NUMBER; i++)
    {
        ret = i2c_master_send(client, (u8 *)writebuf, writelen);
        if (ret != writelen)
        {
            AXS_ERROR("i2c_master_send fail,ret=%d", ret);
        }
        else
        {
            break;
        }
    }
    mutex_unlock(&ts_data->bus_mutex);
	if(i == I2C_RETRY_NUMBER)
	{
		return -EINVAL;
	}	
    return ret;
}

int axs_bus_init(struct axs_ts_data *ts_data)
{
    ts_data->bus_tx_buf = kzalloc(AXS_CMD_MAX_WRITE_BUF_LEN, GFP_KERNEL); // kzalloc
    if (NULL == ts_data->bus_tx_buf)
    {
        AXS_ERROR("failed to allocate memory for bus_tx_buf");
        return -ENOMEM;
    }

    ts_data->bus_rx_buf = kzalloc(AXS_CMD_MAX_READ_BUF_LEN, GFP_KERNEL); // kzalloc
    if (NULL == ts_data->bus_rx_buf)
    {
        AXS_ERROR("failed to allocate memory for bus_rx_buf");
        return -ENOMEM;
    }
    return 0;
}

int axs_bus_exit(struct axs_ts_data *ts_data)
{
    if (ts_data)
    {
        kfree_safe(ts_data->bus_tx_buf);
        kfree_safe(ts_data->bus_rx_buf);
    }
    return 0;
}
#endif
