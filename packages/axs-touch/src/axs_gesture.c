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
#if AXS_GESTURE_EN
#define KEY_GESTURE_UP                          KEY_UP
#define KEY_GESTURE_DOWN                        KEY_DOWN
#define KEY_GESTURE_LEFT                        KEY_LEFT
#define KEY_GESTURE_RIGHT                       KEY_RIGHT
#define KEY_GESTURE_C                           KEY_C
#define KEY_GESTURE_Z                           KEY_Z
#define KEY_GESTURE_M                           KEY_M
#define KEY_GESTURE_O                           KEY_O
#define KEY_GESTURE_S                           KEY_S
#define KEY_GESTURE_V                           KEY_V
#define KEY_GESTURE_W                           KEY_W
#define KEY_GESTURE_E                           KEY_E
#define KEY_GESTURE_U                           KEY_U

typedef enum
{
    GESTURE_NONE        = 0,
    GESTURE_DOUBLE_CLICK = 1,
    GESTURE_UP          = 2,
    GESTURE_DOWN        = 3,
    GESTURE_LEFT        = 4,
    GESTURE_RIGHT       = 5,
    GESTURE_C           = 99,
    GESTURE_Z           = 122,
    GESTURE_M           = 109,
    GESTURE_O           = 111,
    GESTURE_S           = 115,
    GESTURE_V           = 118,
    GESTURE_W           = 119,
    GESTURE_E           = 101
} GESTURE_ID;

bool axs_gesture_init(struct axs_ts_data *ts_data)
{
    struct input_dev *input_dev = ts_data->input_dev;

    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_U);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_UP);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_DOWN);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_LEFT);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_RIGHT);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_O);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_E);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_M);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_W);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_S);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_V);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_Z);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_C);

    __set_bit(KEY_GESTURE_RIGHT, input_dev->keybit);
    __set_bit(KEY_GESTURE_LEFT, input_dev->keybit);
    __set_bit(KEY_GESTURE_UP, input_dev->keybit);
    __set_bit(KEY_GESTURE_DOWN, input_dev->keybit);
    __set_bit(KEY_GESTURE_U, input_dev->keybit);
    __set_bit(KEY_GESTURE_O, input_dev->keybit);
    __set_bit(KEY_GESTURE_E, input_dev->keybit);
    __set_bit(KEY_GESTURE_M, input_dev->keybit);
    __set_bit(KEY_GESTURE_W, input_dev->keybit);
    __set_bit(KEY_GESTURE_S, input_dev->keybit);
    __set_bit(KEY_GESTURE_V, input_dev->keybit);
    __set_bit(KEY_GESTURE_C, input_dev->keybit);
    __set_bit(KEY_GESTURE_Z, input_dev->keybit);

    ts_data->gesture_enable = 0; // default 0; show change this value by sysfs
    return true;
}

void axs_gesture_report(struct input_dev *input_dev, int gesture_id)
{
    int report_key;

    AXS_DEBUG("gesture ID:%d ", gesture_id);
    switch (gesture_id)
    {
        case GESTURE_LEFT:
            report_key = KEY_GESTURE_LEFT;
            break;
        case GESTURE_RIGHT:
            report_key = KEY_GESTURE_RIGHT;
            break;
        case GESTURE_UP:
            report_key = KEY_GESTURE_UP;
            break;
        case GESTURE_DOWN:
            report_key = KEY_GESTURE_DOWN;
            break;
        case GESTURE_DOUBLE_CLICK:
            report_key = KEY_GESTURE_U;
            break;
        case GESTURE_O:
            report_key = KEY_GESTURE_O;
            break;
        case GESTURE_W:
            report_key = KEY_GESTURE_W;
            break;
        case GESTURE_M:
            report_key = KEY_GESTURE_M;
            break;
        case GESTURE_E:
            report_key = KEY_GESTURE_E;
            break;
        case GESTURE_S:
            report_key = KEY_GESTURE_S;
            break;
        case GESTURE_V:
            report_key = KEY_GESTURE_V;
            break;
        case GESTURE_Z:
            report_key = KEY_GESTURE_Z;
            break;
        case  GESTURE_C:
            report_key = KEY_GESTURE_C;
            break;
        default:
            report_key = 0;
            break;
    }
    /* report key*/
    if (report_key)
    {
        AXS_DEBUG("gesture report_key:%d", report_key);
        input_report_key(input_dev, report_key, 1);
        input_sync(input_dev);
        input_report_key(input_dev, report_key, 0);
        input_sync(input_dev);
    }
}

bool axs_read_gesture_enable(u8 *pEnable)
{
    u8 cmd_type[1] = {AXS_REG_GESTURE_READ};

    return axs_read_regs(cmd_type,1,pEnable,1);
}

u16 axs_read_chip_id(void)
{
    u8 cmd_type[1] = {AXS_REG_VERSION_READ};
	u8  read_data[4] = {0,0,0,0};
    axs_read_regs(cmd_type,1,read_data,4);
	return ((read_data[2]<<8)|read_data[3]);
}

bool axs_write_gesture_enable(void)
{
    u8 cmd_type[1] = {AXS_REG_GESTURE_WRITE};
    return axs_write_buf(cmd_type,1);
}
#endif

