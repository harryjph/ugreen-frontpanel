// SPDX-License-Identifier: GPL-2.0-only
/*
 * LED Kernel breath Trigger
 *
 * Copyright 2005-2006 Openedhand Ltd.
 *
 * Author: Richard Purdie <rpurdie@openedhand.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/dmi.h>
//#include <misc/ugreen_product.h>

static ssize_t led_delay_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = led_trigger_get_led(dev);

	return sprintf(buf, "%lu\n", led_cdev->blink_delay_on);
}

static ssize_t led_delay_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = led_trigger_get_led(dev);
	unsigned long state;
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &state);
	if (ret)
		return ret;

	led_blink_set(led_cdev, &state, &led_cdev->blink_delay_off);
	led_cdev->blink_delay_on = state;

	return size;
}

static ssize_t led_delay_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = led_trigger_get_led(dev);

	return sprintf(buf, "%lu\n", led_cdev->blink_delay_off);
}

static ssize_t led_delay_off_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = led_trigger_get_led(dev);
	unsigned long state;
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &state);
	if (ret)
		return ret;

	led_blink_set(led_cdev, &led_cdev->blink_delay_on, &state);
	led_cdev->blink_delay_off = state;

	return size;
}

static DEVICE_ATTR(delay_on, 0644, led_delay_on_show, led_delay_on_store);
static DEVICE_ATTR(delay_off, 0644, led_delay_off_show, led_delay_off_store);

static struct attribute *breath_trig_attrs[] = {
	&dev_attr_delay_on.attr,
	&dev_attr_delay_off.attr,
	NULL
};
ATTRIBUTE_GROUPS(breath_trig);

static void pattern_init(struct led_classdev *led_cdev)
{
	u32 *pattern;
	unsigned int size = 0;

	pattern = led_get_default_pattern(led_cdev, &size);
	if (!pattern)
		return;

	if (size != 2) {
		dev_warn(led_cdev->dev,
			 "Expected 2 but got %u values for delays pattern\n",
			 size);
		goto out;
	}

	led_cdev->blink_delay_on = pattern[0];
	led_cdev->blink_delay_off = pattern[1];
	/* led_blink_set() called by caller */

out:
	kfree(pattern);
}

struct led_trig_data {
	char name[10];
};

static int breath_trig_activate(struct led_classdev *led_cdev)
{
	struct led_trig_data *led_data = kzalloc(sizeof(*led_data), GFP_KERNEL);
	if (!led_data)
		return -ENOMEM;
    memcpy(led_data->name,"breath",6);
	led_set_trigger_data(led_cdev, led_data);
	if (led_cdev->flags & LED_INIT_DEFAULT_TRIGGER) {
		pattern_init(led_cdev);
		/*
		 * Mark as initialized even on pattern_init() error because
		 * any consecutive call to it would produce the same error.
		 */
		led_cdev->flags &= ~LED_INIT_DEFAULT_TRIGGER;
	}

	/*
	 * If "set brightness to 0" is pending in workqueue, we don't
	 * want that to be reordered after blink_set()
	 */
	flush_work(&led_cdev->set_brightness_work);
	msleep(30);
	led_blink_set(led_cdev, &led_cdev->blink_delay_on,
		      &led_cdev->blink_delay_off);

	return 0;
}

static void breath_trig_deactivate(struct led_classdev *led_cdev)
{
	struct led_trig_data *led_data = led_get_trigger_data(led_cdev);

	kfree(led_data);
	/* Stop blinking */
	led_set_brightness(led_cdev, LED_OFF);
}

static struct led_trigger breath_led_trigger = {
	.name     = "breath",
	.activate = breath_trig_activate,
	.deactivate = breath_trig_deactivate,
	.groups = breath_trig_groups,
};
static int __init ledtrig_breath_ht32f52231_init(void)
{
//	if(!ug_check_product_match(DX4600_SERIES, __func__)){
//		return -ENODEV;
//	}
	led_trigger_register(&breath_led_trigger);
	return 0;
}
module_init(ledtrig_breath_ht32f52231_init);

static void __exit ledtrig_breath_ht32f52231_exit(void)
{
 led_trigger_unregister(&breath_led_trigger);
}

module_exit(ledtrig_breath_ht32f52231_exit);

MODULE_AUTHOR("Richard Purdie <rpurdie@openedhand.com>");
MODULE_DESCRIPTION("breath LED trigger");
MODULE_LICENSE("GPL v2");
