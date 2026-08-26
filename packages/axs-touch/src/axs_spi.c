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

#if AXS_BUS_SPI
int axs_write_bytes_read_bytes_onecs(u8 *cmd, u16 cmdlen, u8 *data, u16 datalen)
{
    int ret = 0;
    struct spi_message msg;
    struct spi_transfer transfer;
    struct axs_ts_data *ts_data = g_axs_data;
    if (!ts_data || !ts_data->spi || !cmd || !data
        || (cmdlen+datalen >= AXS_CMD_MAX_WRITE_BUF_LEN) || (cmdlen+datalen >= AXS_CMD_MAX_READ_BUF_LEN) )
    {
        AXS_ERROR("g_axs_data/client/cmdlen(%d)/data/datalen(%d) is invalid",
                  cmdlen, datalen);
        return -EINVAL;
    }
    memset(ts_data->bus_rx_buf,0,cmdlen+datalen);
    memset(ts_data->bus_tx_buf,0,cmdlen+datalen);
    memset(&transfer, 0, sizeof(transfer));
    memcpy(ts_data->bus_tx_buf,cmd,cmdlen);
    spi_message_init(&msg);
    transfer.tx_buf = ts_data->bus_tx_buf;
    transfer.rx_buf = ts_data->bus_rx_buf;
    transfer.len = cmdlen+datalen;
    spi_message_add_tail(&transfer, &msg);

    mutex_lock(&ts_data->bus_mutex);
    ret = spi_sync(ts_data->spi, &msg);
    if (ret == 0)
    {
        memcpy(data, ts_data->bus_rx_buf+cmdlen, datalen);
    }
    else
    {
        AXS_DEBUG("axs_write_bytes_read_bytes_onecs, fail:%d\n",ret);
    }
    mutex_unlock(&ts_data->bus_mutex);
    return ret;
}


int axs_read_bytes(u8 *rdbuf, u16 rdlen)
{
    int ret = 0;
    struct spi_message msg;
    struct spi_transfer transfer;
    struct axs_ts_data *ts_data = g_axs_data;


    if (!ts_data || !ts_data->spi || !rdbuf || !rdlen
        || (rdlen >= AXS_CMD_MAX_READ_BUF_LEN)||(rdlen >= AXS_CMD_MAX_WRITE_BUF_LEN))
    {
        AXS_ERROR("axs_read_bytes g_axs_data/device/data/datalen(%d) is invalid", rdlen);
        return -EINVAL;
    }
    spi_message_init(&msg);
    memset(&transfer, 0, sizeof(transfer));

    //msg.spi = ts_data->spi;
    memset(ts_data->bus_tx_buf,0,rdlen);
    transfer.tx_buf = ts_data->bus_tx_buf;
    transfer.rx_buf = rdbuf;
    transfer.len = rdlen;
    spi_message_add_tail(&transfer, &msg);

    mutex_lock(&ts_data->bus_mutex);
    ret = spi_sync(ts_data->spi, &msg);
    if (ret!=0)
    {
        AXS_DEBUG("axs_read_bytes, status= %d\n",ret);
    }
    mutex_unlock(&ts_data->bus_mutex);
    return ret;
}

int axs_write_bytes(u8 *writebuf, u16 writelen)
{
    int ret = 0;
    struct spi_message msg;
    struct spi_transfer transfer;
    struct axs_ts_data *ts_data = g_axs_data;


    if (!ts_data || !ts_data->spi || !writebuf || !writelen
        || (writelen >= AXS_CMD_MAX_WRITE_BUF_LEN)||(writelen >= AXS_CMD_MAX_READ_BUF_LEN))
    {
        AXS_ERROR("axs_write_bytes g_axs_data/device/data/datalen(%d) is invalid", writelen);
        return -EINVAL;
    }
    spi_message_init(&msg);
    memset(&transfer, 0, sizeof(transfer));
    memset(ts_data->bus_rx_buf,0,writelen);

    //msg.spi = ts_data->spi;
    transfer.tx_buf = (unsigned char *)writebuf;
    transfer.rx_buf = ts_data->bus_rx_buf;
    transfer.len = writelen;
    spi_message_add_tail(&transfer, &msg);

    mutex_lock(&ts_data->bus_mutex);
    ret = spi_sync(ts_data->spi, &msg);
    if (ret!=0)
    {
        AXS_DEBUG("axs_write_bytes, status= %d\n",ret);
    }
    mutex_unlock(&ts_data->bus_mutex);

    return ret;
}

int axs_write_bytes_read_bytes(u8 *cmd, u16 cmdlen, u8 *data, u16 datalen)
{
    int ret = 0;
    ret = axs_write_bytes(cmd, cmdlen);
    if (ret)
    {
        return ret;
    }
    ret = axs_read_bytes(data, datalen);
    return ret;
}

//为ts_data->bus_tx_buf和ts_data->bus_rx_buf申请内存
int axs_bus_init(struct axs_ts_data *ts_data)
{
    AXS_FUNC_ENTER();

    ts_data->bus_tx_buf = kzalloc(AXS_CMD_MAX_WRITE_BUF_LEN, GFP_KERNEL);
    if (NULL == ts_data->bus_tx_buf)
    {
        AXS_ERROR("failed to allocate memory for bus_tx_buf");
        return -ENOMEM;
    }

    ts_data->bus_rx_buf = kzalloc(AXS_CMD_MAX_READ_BUF_LEN, GFP_KERNEL);
    if (NULL == ts_data->bus_rx_buf)
    {
        AXS_ERROR("failed to allocate memory for bus_rx_buf");
        return -ENOMEM;
    }
    AXS_FUNC_EXIT();
    return 0;
}

int axs_bus_exit(struct axs_ts_data *ts_data)
{
    AXS_FUNC_ENTER();
    if (ts_data)
    {
        kfree_safe(ts_data->bus_tx_buf);
        kfree_safe(ts_data->bus_rx_buf);
    }
    AXS_FUNC_EXIT();
    return 0;
}
#endif

