// SPDX-License-Identifier: GPL-2.0-only
/*
 * LED Kernel Timer Trigger
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
#include <linux/dmi.h>
//#include <misc/ugreen_product.h>

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

static int normal_trig_activate(struct led_classdev *led_cdev)
{
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
	//flush_work(&led_cdev->set_brightness_work);
	led_blink_set(led_cdev, &led_cdev->blink_delay_on,
		      &led_cdev->blink_delay_off);

	return 0;
}

static void normal_trig_deactivate(struct led_classdev *led_cdev)
{
	/* Stop blinking */
	led_set_brightness(led_cdev, LED_OFF);
}

static struct led_trigger normal_led_trigger = {
	.name     = "normal",
	.activate = normal_trig_activate,
	.deactivate = normal_trig_deactivate,
};
static int __init ledtrig_normal_ht32f52231_init(void)
{
//	if(!ug_check_product_match(DX4600_SERIES, __func__)){
//		return -ENODEV;
//	}
	led_trigger_register(&normal_led_trigger);
	return 0;
}
module_init(ledtrig_normal_ht32f52231_init);

static void __exit ledtrig_normal_ht32f52231_exit(void)
{
 led_trigger_unregister(&normal_led_trigger);
}

module_exit(ledtrig_normal_ht32f52231_exit);

MODULE_DESCRIPTION("normaling LED trigger");
MODULE_LICENSE("GPL v2");
