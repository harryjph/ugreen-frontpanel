#ifndef EC_DRIVER_H
#define EC_DRIVER_H

#include <linux/types.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ioport.h>
/* EC Port Definitions */
#define EC_D_PORT        0x62
#define EC_C_PORT        0x66

/* EC Status Flags */
#define EC_S_IBF         0x02    // Input buffer is full/empty
#define EC_S_OBF         0x01    // Output buffer is full/empty

/* EC Commands */
#define EC_C_READ_MEM    0x80    // Read the EC memory
#define EC_C_WRITE_MEM   0x81    // Write the EC memory

/* Timeout Definitions */
#define EC_TIMEOUT_US    5  // 5ms timeout

/* EC Core Functions */
int ec_send_command(u8 command);
int ec_send_data(u8 data);
int ec_receive_data(u8 *data);
int ec_read_memory(u8 address, u8 *value);
int ec_write_memory(u8 address, u8 value);

/* EC lock */
/* Request EC I/O ports */
static inline int ec_request_ports(void)
{
	if (!request_muxed_region(EC_C_PORT, 1, "ec_cmd")) {
		pr_err("Failed to request EC command port 0x%02X\n", EC_C_PORT);
		return -EBUSY;
	}

	return 0;
}

/* Release EC I/O ports */
static inline void ec_release_ports(void)
{
	release_region(EC_C_PORT, 1);
}

#endif /* EC_DRIVER_H */