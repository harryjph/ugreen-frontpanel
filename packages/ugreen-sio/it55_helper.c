#include "it55_helper.h"

#include <linux/printk.h>
#include <linux/module.h>


/* Read EC status */
static u8 ec_read_status(void)
{
	return inb(EC_C_PORT);
}

/* Wait for EC to be ready to receive command */
static int ec_wait_command_ready(void)
{
	unsigned int timeout = EC_TIMEOUT_US;
	u8 status;

	while (timeout > 0) {
		status = ec_read_status();
		if (!(status & EC_S_IBF)) {
			return 0;
		}
		msleep(1);
		timeout -= 1;
	}

	pr_err("EC command timeout, status: 0x%02X\n", status);
	return -ETIMEDOUT;
}

/* Wait for EC to have data ready */
static int ec_wait_data_ready(void)
{
	unsigned int timeout = EC_TIMEOUT_US;
	u8 status;

	while (timeout > 0) {
		status = ec_read_status();
		if (status & EC_S_OBF) {
			return 0;
		}
		msleep(1);
		timeout -= 1;
	}

	pr_err("EC data timeout, status: 0x%02X\n", status);
	return -ETIMEDOUT;
}

/* Send command to EC */
int ec_send_command(u8 command)
{
	int ret;

	ret = ec_wait_command_ready();
	if (ret)
		return ret;

	outb(command, EC_C_PORT);
	pr_debug("EC command sent: 0x%02X\n", command);
	return 0;
}

/* Send data to EC */
int ec_send_data(u8 data)
{
	int ret;

	ret = ec_wait_command_ready();
	if (ret)
		return ret;

	outb(data, EC_D_PORT);
	pr_debug("EC data sent: 0x%02X\n", data);
	return 0;
}

/* Receive data from EC */
int ec_receive_data(u8 *data)
{
	int ret;

	ret = ec_wait_data_ready();
	if (ret)
		return ret;

	*data = inb(EC_D_PORT);
	pr_debug("EC data received: 0x%02X\n", *data);
	return 0;
}

/* Read EC memory */
int ec_read_memory(u8 address, u8 *value)
{
	int ret;

	ret = ec_send_command(EC_C_READ_MEM);
	if (ret)
		return ret;

	ret = ec_send_data(address);
	if (ret)
		return ret;

	return ec_receive_data(value);
}

/* Write EC memory */
int ec_write_memory(u8 address, u8 value)
{
	int ret;

	ret = ec_send_command(EC_C_WRITE_MEM);
	if (ret)
		return ret;

	ret = ec_send_data(address);
	if (ret)
		return ret;

	return ec_send_data(value);
}
