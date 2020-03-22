/* Copyright (c) 2012-2015, 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/qpnp/pwm.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>

#define HVU_LOG printk(KERN_INFO "leds-hvu: %s %d %d", __func__, __LINE__, parsed_leds);

#define DUTY_PCTS_SCALING_MIN		3
#define DUTY_PCTS_SCALING_MAX		100

#define LED_TRIGGER_DEFAULT		"none"

#define RGB_LED_SRC_SEL(base)		(base + 0x45)
#define RGB_LED_EN_CTL(base)		(base + 0x46)
#define RGB_LED_ATC_CTL(base)		(base + 0x47)

#define RGB_MAX_LEVEL			LED_FULL
#define RGB_LED_ENABLE_RED		0x80
#define RGB_LED_ENABLE_GREEN		0x40
#define RGB_LED_ENABLE_BLUE		0x20
#define RGB_LED_SOURCE_VPH_PWR		0x01
#define RGB_LED_ENABLE_MASK		0xE0
#define RGB_LED_SRC_MASK		0x03
#define QPNP_LED_PWM_FLAGS	(PM_PWM_LUT_LOOP | PM_PWM_LUT_RAMP_UP)
#define QPNP_LUT_RAMP_STEP_DEFAULT	255
#define	PWM_LUT_MAX_SIZE		63
#define	PWM_GPLED_LUT_MAX_SIZE		31
#define RGB_LED_DISABLE			0x00

/**
 * enum qpnp_leds - QPNP supported led ids
 * @QPNP_ID_WLED - White led backlight
 */
enum qpnp_leds {
	QPNP_ID_WLED = 0,
	QPNP_ID_FLASH1_LED0,
	QPNP_ID_FLASH1_LED1,
	QPNP_ID_RGB_RED,
	QPNP_ID_RGB_GREEN,
	QPNP_ID_RGB_BLUE,
	QPNP_ID_LED_MPP,
	QPNP_ID_KPDBL,
	QPNP_ID_LED_GPIO,
	QPNP_ID_MAX,
};

#define QPNP_ID_TO_RGB_IDX(id) (id - QPNP_ID_RGB_RED)

enum led_mode {
	PWM_MODE = 0,
	LPG_MODE,
	MANUAL_MODE,
};

static u8 rgb_pwm_debug_regs[] = {
	0x45, 0x46, 0x47,
};

/**
 *  pwm_config_data - pwm configuration data
 *  @lut_params - lut parameters to be used by pwm driver
 *  @pwm_device - pwm device
 *  @pwm_period_us - period for pwm, in us
 *  @mode - mode the led operates in
 *  @old_duty_pcts - storage for duty pcts that may need to be reused
 *  @default_mode - default mode of LED as set in device tree
 *  @use_blink - use blink sysfs entry
 *  @blinking - device is currently blinking w/LPG mode
 */
struct pwm_config_data {
	struct lut_params	lut_params;
	struct pwm_device	*pwm_dev;
	u32			pwm_period_us;
	struct pwm_duty_cycles	*duty_cycles;
	int	*old_duty_pcts;
	int	*scaled_duty_pcts;
	int	duty_pcts_scaling;
	u8	mode;
	u8	default_mode;
	bool	pwm_enabled;
	bool use_blink;
	bool blinking;
};

/**
 *  rgb_config_data - rgb configuration data
 *  @pwm_cfg - device pwm configuration
 *  @enable - bits to enable led
 */
struct rgb_config_data {
	struct pwm_config_data	*pwm_cfg;
	u8	enable;
};


/**
 * struct qpnp_led_data - internal led data structure
 * @led_classdev - led class device
 * @delayed_work - delayed work for turning off the LED
 * @workqueue - dedicated workqueue to handle concurrency
 * @work - workqueue for led
 * @id - led index
 * @base_reg - base register given in device tree
 * @lock - to protect the transactions
 * @reg - cached value of led register
 * @num_leds - number of leds in the module
 * @max_current - maximum current supported by LED
 * @default_on - true: default state max, false, default state 0
 * @turn_off_delay_ms - number of msec before turning off the LED
 */
struct qpnp_led_data {
	struct led_classdev		cdev;
	struct platform_device		*pdev;
	struct regmap			*regmap;
	struct delayed_work		dwork;
	struct workqueue_struct		*workqueue;
	struct work_struct		work;
	int				id;
	u16				base;
	u8				reg;
	u8				num_leds;
	struct mutex			lock;
	struct rgb_config_data		*rgb_cfg;
	int				max_current;
	bool				default_on;
	bool				in_order_command_processing;
	int				turn_off_delay_ms;
};
/**
 * struct rgb_sync - rgb led synchrnize structure
 */
struct rgb_sync {
	struct led_classdev	cdev;
	struct platform_device	*pdev;
	struct qpnp_led_data	*led_data[3];
};

static int qpnp_led_masked_write(struct qpnp_led_data *led, u16 addr, u8 mask, u8 val)
{
	int rc;

	rc = regmap_update_bits(led->regmap, addr, mask, val);
	if (rc)
		dev_err(&led->pdev->dev, "Unable to regmap_update_bits to addr=%x, rc(%d)\n", addr, rc);
	return rc;
}

static void qpnp_dump_regs(struct qpnp_led_data *led, u8 regs[], u8 array_size)
{
	int i;
	u8 val;

	pr_debug("===== %s LED register dump start =====\n", led->cdev.name);
	for (i = 0; i < array_size; i++) {
		regmap_bulk_read(led->regmap, led->base + regs[i], &val,
				 sizeof(val));
		pr_debug("%s: 0x%x = 0x%x\n", led->cdev.name,
					led->base + regs[i], val);
	}
	pr_debug("===== %s LED register dump end =====\n", led->cdev.name);
}

static int qpnp_led_get_pwm_cfg(struct qpnp_led_data *led,
				struct pwm_config_data **pwm_cfg)
{
	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		*pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev, "Invalid LED id type for duty pcts\n");
		return -EINVAL;
	}

	return 0;
}

static int qpnp_rgb_set(struct qpnp_led_data *led)
{
	int rc;
	int duty_us, duty_ns, period_us;

	int level = led->cdev.brightness;
	struct pwm_config_data *pwm_cfg;

	if (qpnp_led_get_pwm_cfg(led, &pwm_cfg) == 0) {
		if ((pwm_cfg->duty_pcts_scaling >= DUTY_PCTS_SCALING_MIN) &&
		    (pwm_cfg->duty_pcts_scaling <= DUTY_PCTS_SCALING_MAX))
			level = DIV_ROUND_CLOSEST(
				(level * pwm_cfg->duty_pcts_scaling), 100);
	}

	if (level) {
		if (!led->rgb_cfg->pwm_cfg->blinking)
			led->rgb_cfg->pwm_cfg->mode =
				led->rgb_cfg->pwm_cfg->default_mode;
		if (led->rgb_cfg->pwm_cfg->mode == PWM_MODE) {
			rc = pwm_change_mode(led->rgb_cfg->pwm_cfg->pwm_dev,
					PM_PWM_MODE_PWM);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"Failed to set PWM mode, rc = %d\n",
					rc);
				return rc;
			}
		}
		period_us = led->rgb_cfg->pwm_cfg->pwm_period_us;
		if (period_us > INT_MAX / NSEC_PER_USEC) {
			duty_us = (period_us * level) /
				LED_FULL;
			rc = pwm_config_us(
				led->rgb_cfg->pwm_cfg->pwm_dev,
				duty_us,
				period_us);
		} else {
			duty_ns = ((period_us * NSEC_PER_USEC) /
				LED_FULL) * level;
			rc = pwm_config(
				led->rgb_cfg->pwm_cfg->pwm_dev,
				duty_ns,
				period_us * NSEC_PER_USEC);
		}
		if (rc < 0) {
			dev_err(&led->pdev->dev,
				"pwm config failed\n");
			return rc;
		}
		rc = qpnp_led_masked_write(led,
			RGB_LED_EN_CTL(led->base),
			led->rgb_cfg->enable, led->rgb_cfg->enable);
		if (rc) {
			dev_err(&led->pdev->dev,
				"Failed to write led enable reg\n");
			return rc;
		}
		if (!led->rgb_cfg->pwm_cfg->pwm_enabled) {
			pwm_enable(led->rgb_cfg->pwm_cfg->pwm_dev);
			led->rgb_cfg->pwm_cfg->pwm_enabled = 1;
		}
	} else {
		led->rgb_cfg->pwm_cfg->mode =
			led->rgb_cfg->pwm_cfg->default_mode;
		if (led->rgb_cfg->pwm_cfg->pwm_enabled) {
			pwm_disable(led->rgb_cfg->pwm_cfg->pwm_dev);
			led->rgb_cfg->pwm_cfg->pwm_enabled = 0;
		}
		rc = qpnp_led_masked_write(led,
			RGB_LED_EN_CTL(led->base),
			led->rgb_cfg->enable, RGB_LED_DISABLE);
		if (rc) {
			dev_err(&led->pdev->dev,
				"Failed to write led enable reg\n");
			return rc;
		}
	}

	led->rgb_cfg->pwm_cfg->blinking = false;
	qpnp_dump_regs(led, rgb_pwm_debug_regs, ARRAY_SIZE(rgb_pwm_debug_regs));

	return 0;
}


static void qpnp_led_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct qpnp_led_data *led;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);
	if (value < LED_OFF) {
		dev_err(&led->pdev->dev, "Invalid brightness value\n");
		return;
	}

	if (value > led->cdev.max_brightness)
		value = led->cdev.max_brightness;

	led->cdev.brightness = value;
	if (led->in_order_command_processing)
		queue_work(led->workqueue, &led->work);
	else
		schedule_work(&led->work);
}


static int qpnp_pwm_init(struct pwm_config_data *pwm_cfg,
					struct platform_device *pdev,
					const char *name)
{
	int rc, start_idx, idx_len, lut_max_size;
	int *apply_pcts;

	if (pwm_cfg->pwm_dev) {
		if (pwm_cfg->mode == LPG_MODE) {
			start_idx =
			pwm_cfg->duty_cycles->start_idx;
			idx_len =
			pwm_cfg->duty_cycles->num_duty_pcts;

			apply_pcts = pwm_cfg->duty_cycles->duty_pcts;

			if (strnstr(name, "kpdbl", sizeof("kpdbl")))
				lut_max_size = PWM_GPLED_LUT_MAX_SIZE;
			else
				lut_max_size = PWM_LUT_MAX_SIZE;

			if (idx_len >= lut_max_size && start_idx) {
				dev_err(&pdev->dev,
					"Wrong LUT size or index\n");
				return -EINVAL;
			}

			if ((start_idx + idx_len) > lut_max_size) {
				dev_err(&pdev->dev, "Exceed LUT limit\n");
				return -EINVAL;
			}

			if ((pwm_cfg->duty_pcts_scaling >=
					DUTY_PCTS_SCALING_MIN) &&
			    (pwm_cfg->duty_pcts_scaling <=
					DUTY_PCTS_SCALING_MAX))
				apply_pcts =
					pwm_cfg->scaled_duty_pcts;

			rc = pwm_lut_config(pwm_cfg->pwm_dev,
				pwm_cfg->pwm_period_us,
				apply_pcts,
				pwm_cfg->lut_params);
			if (rc < 0) {
				dev_err(&pdev->dev, "Failed to configure pwm LUT\n");
				return rc;
			}
			rc = pwm_change_mode(pwm_cfg->pwm_dev, PM_PWM_MODE_LPG);
			if (rc < 0) {
				dev_err(&pdev->dev, "Failed to set LPG mode\n");
				return rc;
			}
		}
	} else {
		dev_err(&pdev->dev, "Invalid PWM device\n");
		return -EINVAL;
	}

	return 0;
}


static ssize_t pwm_us_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	u32 pwm_us;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret;
	u32 previous_pwm_us;
	struct pwm_config_data *pwm_cfg;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	ret = kstrtou32(buf, 10, &pwm_us);
	if (ret)
		return ret;

	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev, "Invalid LED id type for pwm_us\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	previous_pwm_us = pwm_cfg->pwm_period_us;

	pwm_cfg->pwm_period_us = pwm_us;
	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret) {
		pwm_cfg->pwm_period_us = previous_pwm_us;
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		qpnp_led_set(&led->cdev, led->cdev.brightness);
		dev_err(&led->pdev->dev,
			"Failed to initialize pwm with new pwm_us value\n");
		return ret;
	}
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;
}

static ssize_t pause_lo_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	u32 pause_lo;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret;
	u32 previous_pause_lo;
	struct pwm_config_data *pwm_cfg;

	ret = kstrtou32(buf, 10, &pause_lo);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev,
			"Invalid LED id type for pause lo\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	previous_pause_lo = pwm_cfg->lut_params.lut_pause_lo;

	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	pwm_cfg->lut_params.lut_pause_lo = pause_lo;
	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret) {
		pwm_cfg->lut_params.lut_pause_lo = previous_pause_lo;
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		qpnp_led_set(&led->cdev, led->cdev.brightness);
		dev_err(&led->pdev->dev,
			"Failed to initialize pwm with new pause lo value\n");
		return ret;
	}
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;
}

static ssize_t pause_hi_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	u32 pause_hi;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret;
	u32 previous_pause_hi;
	struct pwm_config_data *pwm_cfg;

	ret = kstrtou32(buf, 10, &pause_hi);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev,
			"Invalid LED id type for pause hi\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	previous_pause_hi = pwm_cfg->lut_params.lut_pause_hi;

	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	pwm_cfg->lut_params.lut_pause_hi = pause_hi;
	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret) {
		pwm_cfg->lut_params.lut_pause_hi = previous_pause_hi;
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		qpnp_led_set(&led->cdev, led->cdev.brightness);
		dev_err(&led->pdev->dev,
			"Failed to initialize pwm with new pause hi value\n");
		return ret;
	}
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;
}

static ssize_t start_idx_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	u32 start_idx;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret;
	u32 previous_start_idx;
	struct pwm_config_data *pwm_cfg;

	ret = kstrtou32(buf, 10, &start_idx);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev,
			"Invalid LED id type for start idx\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	previous_start_idx = pwm_cfg->duty_cycles->start_idx;
	pwm_cfg->duty_cycles->start_idx = start_idx;
	pwm_cfg->lut_params.start_idx = pwm_cfg->duty_cycles->start_idx;
	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret) {
		pwm_cfg->duty_cycles->start_idx = previous_start_idx;
		pwm_cfg->lut_params.start_idx = pwm_cfg->duty_cycles->start_idx;
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		qpnp_led_set(&led->cdev, led->cdev.brightness);
		dev_err(&led->pdev->dev,
			"Failed to initialize pwm with new start idx value\n");
		return ret;
	}
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;
}

static ssize_t ramp_step_ms_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	u32 ramp_step_ms;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret;
	u32 previous_ramp_step_ms;
	struct pwm_config_data *pwm_cfg;

	ret = kstrtou32(buf, 10, &ramp_step_ms);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev,
			"Invalid LED id type for ramp step\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	previous_ramp_step_ms = pwm_cfg->lut_params.ramp_step_ms;

	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	pwm_cfg->lut_params.ramp_step_ms = ramp_step_ms;
	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret) {
		pwm_cfg->lut_params.ramp_step_ms = previous_ramp_step_ms;
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		qpnp_led_set(&led->cdev, led->cdev.brightness);
		dev_err(&led->pdev->dev,
			"Failed to initialize pwm with new ramp step value\n");
		return ret;
	}
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;
}

static ssize_t lut_flags_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	u32 lut_flags;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret;
	u32 previous_lut_flags;
	struct pwm_config_data *pwm_cfg;

	ret = kstrtou32(buf, 10, &lut_flags);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev,
			"Invalid LED id type for lut flags\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	previous_lut_flags = pwm_cfg->lut_params.flags;

	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	pwm_cfg->lut_params.flags = lut_flags;
	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret) {
		pwm_cfg->lut_params.flags = previous_lut_flags;
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		qpnp_led_set(&led->cdev, led->cdev.brightness);
		dev_err(&led->pdev->dev,
			"Failed to initialize pwm with new lut flags value\n");
		return ret;
	}
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;
}

static int duty_pcts_scaling_update(struct qpnp_led_data *led, int value)
{
	struct pwm_config_data *pwm_cfg;
	u32 num_duty_pcts;
	int *cur_pcts;
	ssize_t ret;
	int i = 0;

	ret = qpnp_led_get_pwm_cfg(led, &pwm_cfg);
	if (ret)
		return ret;

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	num_duty_pcts = pwm_cfg->duty_cycles->num_duty_pcts;
	cur_pcts = pwm_cfg->duty_cycles->duty_pcts;

	if (value < 0 || value > DUTY_PCTS_SCALING_MAX) {
		dev_err(&led->pdev->dev,
			"Invalid duty pcts scaling\n");
		return -EINVAL;
	}
	pwm_cfg->duty_pcts_scaling = value;

	for (i = 0; i < pwm_cfg->duty_cycles->num_duty_pcts; i++)
		pwm_cfg->scaled_duty_pcts[i] =
			DIV_ROUND_CLOSEST((cur_pcts[i] * value), 100);

	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}

	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret)
		goto restore;

	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return 0;

restore:
	dev_err(&led->pdev->dev,
		"Failed to initialize pwm with scaled duty pcts value\n");
	pwm_cfg->duty_cycles->num_duty_pcts = num_duty_pcts;
	pwm_cfg->old_duty_pcts = pwm_cfg->duty_cycles->duty_pcts;
	pwm_cfg->duty_cycles->duty_pcts = cur_pcts;
	pwm_cfg->lut_params.idx_len = pwm_cfg->duty_cycles->num_duty_pcts;
	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return ret;
}

static ssize_t duty_pcts_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	int num_duty_pcts = 0;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	char *buffer;
	ssize_t ret;
	int i = 0;
	int max_duty_pcts;
	struct pwm_config_data *pwm_cfg;
	u32 previous_num_duty_pcts;
	int value;
	int *previous_duty_pcts;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		max_duty_pcts = PWM_LUT_MAX_SIZE;
		break;
	default:
		dev_err(&led->pdev->dev,
			"Invalid LED id type for duty pcts\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	buffer = (char *)buf;

	for (i = 0; i < max_duty_pcts; i++) {
		if (buffer == NULL)
			break;
		ret = sscanf((const char *)buffer, "%u,%s", &value, buffer);

		pwm_cfg->old_duty_pcts[i] = value;
		num_duty_pcts++;
		if (ret <= 1)
			break;
	}

	if (num_duty_pcts >= max_duty_pcts) {
		dev_err(&led->pdev->dev,
			"Number of duty pcts given exceeds max (%d)\n",
			max_duty_pcts);
		return -EINVAL;
	}

	previous_num_duty_pcts = pwm_cfg->duty_cycles->num_duty_pcts;
	previous_duty_pcts = pwm_cfg->duty_cycles->duty_pcts;

	pwm_cfg->duty_cycles->num_duty_pcts = num_duty_pcts;
	pwm_cfg->duty_cycles->duty_pcts = pwm_cfg->old_duty_pcts;
	pwm_cfg->old_duty_pcts = previous_duty_pcts;
	pwm_cfg->lut_params.idx_len = pwm_cfg->duty_cycles->num_duty_pcts;

	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}

	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret)
		goto restore;

	qpnp_led_set(&led->cdev, led->cdev.brightness);

	if ((pwm_cfg->duty_pcts_scaling >= DUTY_PCTS_SCALING_MIN) &&
	    (pwm_cfg->duty_pcts_scaling <= DUTY_PCTS_SCALING_MAX))
		duty_pcts_scaling_update(led, pwm_cfg->duty_pcts_scaling);

	return count;

restore:
	dev_err(&led->pdev->dev,
		"Failed to initialize pwm with new duty pcts value\n");
	pwm_cfg->duty_cycles->num_duty_pcts = previous_num_duty_pcts;
	pwm_cfg->old_duty_pcts = pwm_cfg->duty_cycles->duty_pcts;
	pwm_cfg->duty_cycles->duty_pcts = previous_duty_pcts;
	pwm_cfg->lut_params.idx_len = pwm_cfg->duty_cycles->num_duty_pcts;
	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return ret;
}

static ssize_t duty_pcts_scaling_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct qpnp_led_data *led;
	unsigned long value;
	ssize_t ret;

	ret = kstrtoul(buf, 10, &value);
	if (ret)
		return ret;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	ret = duty_pcts_scaling_update(led, (int)value);
	if (ret)
		return ret;

	return count;
}

static void led_blink(struct qpnp_led_data *led,
			struct pwm_config_data *pwm_cfg)
{
	int rc;

	flush_work(&led->work);
	mutex_lock(&led->lock);
	if (pwm_cfg->use_blink) {
		if (led->cdev.brightness) {
			pwm_cfg->blinking = true;
			pwm_cfg->mode = LPG_MODE;
		} else {
			pwm_cfg->blinking = false;
			pwm_cfg->mode = pwm_cfg->default_mode;
		}
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		if (led->id == QPNP_ID_RGB_RED || led->id == QPNP_ID_RGB_GREEN
				|| led->id == QPNP_ID_RGB_BLUE) {
			rc = qpnp_rgb_set(led);
			if (rc < 0)
				dev_err(&led->pdev->dev,
				"RGB set brightness failed (%d)\n", rc);
		}
	}
	mutex_unlock(&led->lock);
}

static ssize_t blink_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	unsigned long blinking;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &blinking);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct qpnp_led_data, cdev);
	led->cdev.brightness = blinking ? led->cdev.max_brightness : 0;

	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		led_blink(led, led->rgb_cfg->pwm_cfg);
		break;
	default:
		dev_err(&led->pdev->dev, "Invalid LED id type for blink\n");
		return -EINVAL;
	}
	return count;
}

static ssize_t on_off_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	printk(KERN_INFO "leds-hvu: %s %d", __func__, __LINE__);
	return 0;
}

static DEVICE_ATTR(on_off, 0664, NULL, on_off_store);
static DEVICE_ATTR(pwm_us, 0664, NULL, pwm_us_store);
static DEVICE_ATTR(pause_lo, 0664, NULL, pause_lo_store);
static DEVICE_ATTR(pause_hi, 0664, NULL, pause_hi_store);
static DEVICE_ATTR(start_idx, 0664, NULL, start_idx_store);
static DEVICE_ATTR(ramp_step_ms, 0664, NULL, ramp_step_ms_store);
static DEVICE_ATTR(lut_flags, 0664, NULL, lut_flags_store);
static DEVICE_ATTR(duty_pcts, 0664, NULL, duty_pcts_store);
static DEVICE_ATTR(duty_pcts_scaling, 0664, NULL, duty_pcts_scaling_store);
static DEVICE_ATTR(blink, 0664, NULL, blink_store);

static struct attribute  *pwm_attrs[] = {
	&dev_attr_pwm_us.attr,
	NULL
};

static struct attribute *lpg_attrs[] = {
	&dev_attr_on_off.attr,
	&dev_attr_pause_lo.attr,
	&dev_attr_pause_hi.attr,
	&dev_attr_start_idx.attr,
	&dev_attr_ramp_step_ms.attr,
	&dev_attr_lut_flags.attr,
	&dev_attr_duty_pcts.attr,
	&dev_attr_duty_pcts_scaling.attr,
	NULL
};

static struct attribute *blink_attrs[] = {
	&dev_attr_blink.attr,
	NULL
};

static const struct attribute_group pwm_attr_group = {
	.attrs = pwm_attrs,
};

static const struct attribute_group lpg_attr_group = {
	.attrs = lpg_attrs,
};

static const struct attribute_group blink_attr_group = {
	.attrs = blink_attrs,
};

static int qpnp_rgb_init(struct qpnp_led_data *led)
{
	int rc;

	rc = qpnp_led_masked_write(led, RGB_LED_SRC_SEL(led->base),
		RGB_LED_SRC_MASK, RGB_LED_SOURCE_VPH_PWR);
	if (rc) {
		dev_err(&led->pdev->dev, "Failed to write led source select register\n");
		return rc;
	}

	rc = qpnp_pwm_init(led->rgb_cfg->pwm_cfg, led->pdev, led->cdev.name);
	if (rc) {
		dev_err(&led->pdev->dev, "Failed to initialize pwm\n");
		return rc;
	}
	/* Initialize led for use in auto trickle charging mode */
	rc = qpnp_led_masked_write(led, RGB_LED_ATC_CTL(led->base),
		led->rgb_cfg->enable, led->rgb_cfg->enable);

	return 0;
}


static enum led_brightness qpnp_led_get(struct led_classdev *led_cdev)
{
	struct qpnp_led_data *led;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	return led->cdev.brightness;
}

static void qpnp_led_turn_off_delayed(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct qpnp_led_data *led
		= container_of(dwork, struct qpnp_led_data, dwork);

	led->cdev.brightness = LED_OFF;
	qpnp_led_set(&led->cdev, led->cdev.brightness);
}

static void qpnp_led_turn_off(struct qpnp_led_data *led)
{
	INIT_DELAYED_WORK(&led->dwork, qpnp_led_turn_off_delayed);
	schedule_delayed_work(&led->dwork, msecs_to_jiffies(led->turn_off_delay_ms));
}

static void __qpnp_led_work(struct qpnp_led_data *led,
				enum led_brightness value)
{
	int rc;
	mutex_lock(&led->lock);

	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		rc = qpnp_rgb_set(led);
		if (rc < 0)
			dev_err(&led->pdev->dev, "RGB set brightness failed (%d)\n", rc);
		break;
	default:
		dev_err(&led->pdev->dev, "Invalid LED(%d)\n", led->id);
		break;
	}

	mutex_unlock(&led->lock);
}

static void qpnp_led_work(struct work_struct *work)
{
	struct qpnp_led_data *led = container_of(work,
					struct qpnp_led_data, work);

	__qpnp_led_work(led, led->cdev.brightness);
}

static int qpnp_led_set_max_brightness(struct qpnp_led_data *led)
{
	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		led->cdev.max_brightness = RGB_MAX_LEVEL;
		break;
	default:
		dev_err(&led->pdev->dev, "Invalid LED(%d)\n", led->id);
		return -EINVAL;
	}

	return 0;
}

static int qpnp_led_initialize(struct qpnp_led_data *led)
{
	int rc = 0;

	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		rc = qpnp_rgb_init(led);
		if (rc)
			dev_err(&led->pdev->dev, "RGB initialize failed(%d)\n", rc);
		break;
	default:
		dev_err(&led->pdev->dev, "Invalid LED(%d)\n", led->id);
		return -EINVAL;
	}

	return rc;
}

static int qpnp_get_common_configs(struct qpnp_led_data *led,
				struct device_node *node)
{
	int rc;
	u32 val;
	const char *temp_string;

	led->cdev.default_trigger = LED_TRIGGER_DEFAULT;
	rc = of_property_read_string(node, "linux,default-trigger",
		&temp_string);
	if (!rc)
		led->cdev.default_trigger = temp_string;
	else if (rc != -EINVAL)
		return rc;

	led->default_on = false;
	rc = of_property_read_string(node, "qcom,default-state",
		&temp_string);
	if (!rc) {
		if (strcmp(temp_string, "on") == 0)
			led->default_on = true;
	} else if (rc != -EINVAL)
		return rc;

	led->turn_off_delay_ms = 0;
	rc = of_property_read_u32(node, "qcom,turn-off-delay-ms", &val);
	if (!rc)
		led->turn_off_delay_ms = val;
	else if (rc != -EINVAL)
		return rc;

	return 0;
}

static int qpnp_led_get_mode(const char *mode)
{
	if (strcmp(mode, "manual") == 0)
		return MANUAL_MODE;
	else if (strcmp(mode, "pwm") == 0)
		return PWM_MODE;
	else if (strcmp(mode, "lpg") == 0)
		return LPG_MODE;
	else
		return -EINVAL;
}

static int qpnp_get_config_pwm(struct pwm_config_data *pwm_cfg,
				struct platform_device *pdev,
				struct device_node *node)
{
	struct property *prop;
	int rc, i, lut_max_size;
	u32 val;
	u8 *temp_cfg;
	const char *led_label;

	pwm_cfg->pwm_dev = of_pwm_get(node, NULL);

	if (IS_ERR(pwm_cfg->pwm_dev)) {
		rc = PTR_ERR(pwm_cfg->pwm_dev);
		dev_err(&pdev->dev, "Cannot get PWM device rc:(%d)\n", rc);
		pwm_cfg->pwm_dev = NULL;
		return rc;
	}

	if (pwm_cfg->mode != MANUAL_MODE) {
		rc = of_property_read_u32(node, "qcom,pwm-us", &val);
		if (!rc)
			pwm_cfg->pwm_period_us = val;
		else
			return rc;
	}

	pwm_cfg->use_blink =
		of_property_read_bool(node, "qcom,use-blink");

	if (pwm_cfg->mode == LPG_MODE || pwm_cfg->use_blink) {
		pwm_cfg->duty_cycles =
			devm_kzalloc(&pdev->dev,
			sizeof(struct pwm_duty_cycles), GFP_KERNEL);
		if (!pwm_cfg->duty_cycles) {
			dev_err(&pdev->dev, "Unable to allocate memory\n");
			rc = -ENOMEM;
			goto bad_lpg_params;
		}

		prop = of_find_property(node, "qcom,duty-pcts",
			&pwm_cfg->duty_cycles->num_duty_pcts);
		if (!prop) {
			dev_err(&pdev->dev, "Looking up property node qcom,duty-pcts failed\n");
			rc =  -ENODEV;
			goto bad_lpg_params;
		} else if (!pwm_cfg->duty_cycles->num_duty_pcts) {
			dev_err(&pdev->dev, "Invalid length of duty pcts\n");
			rc =  -EINVAL;
			goto bad_lpg_params;
		}

		rc = of_property_read_string(node, "label", &led_label);

		if (rc < 0) {
			dev_err(&pdev->dev, "Failure reading label, rc = %d\n", rc);
			return rc;
		}

		if (strcmp(led_label, "kpdbl") == 0)
			lut_max_size = PWM_GPLED_LUT_MAX_SIZE;
		else
			lut_max_size = PWM_LUT_MAX_SIZE;

		pwm_cfg->duty_cycles->duty_pcts =
			devm_kzalloc(&pdev->dev,
			sizeof(int) * lut_max_size,
			GFP_KERNEL);
		if (!pwm_cfg->duty_cycles->duty_pcts) {
			dev_err(&pdev->dev, "Unable to allocate memory\n");
			rc = -ENOMEM;
			goto bad_lpg_params;
		}

		pwm_cfg->old_duty_pcts =
			devm_kzalloc(&pdev->dev,
			sizeof(int) * lut_max_size,
			GFP_KERNEL);
		if (!pwm_cfg->old_duty_pcts) {
			dev_err(&pdev->dev, "Unable to allocate memory\n");
			rc = -ENOMEM;
			goto bad_lpg_params;
		}

		pwm_cfg->scaled_duty_pcts =
			devm_kzalloc(&pdev->dev,
			sizeof(int) * lut_max_size,
			GFP_KERNEL);
		if (!pwm_cfg->scaled_duty_pcts) {
			dev_err(&pdev->dev, "Unable to allocate memory\n");
			rc = -ENOMEM;
			goto bad_lpg_params;
		}

		temp_cfg = devm_kzalloc(&pdev->dev,
				pwm_cfg->duty_cycles->num_duty_pcts *
				sizeof(u8), GFP_KERNEL);
		if (!temp_cfg) {
			dev_err(&pdev->dev, "Failed to allocate memory for duty pcts\n");
			rc = -ENOMEM;
			goto bad_lpg_params;
		}

		memcpy(temp_cfg, prop->value,
			pwm_cfg->duty_cycles->num_duty_pcts);

		for (i = 0; i < pwm_cfg->duty_cycles->num_duty_pcts; i++)
			pwm_cfg->duty_cycles->duty_pcts[i] =
				(int) temp_cfg[i];

		rc = of_property_read_u32(node, "qcom,start-idx", &val);
		if (!rc) {
			pwm_cfg->lut_params.start_idx = val;
			pwm_cfg->duty_cycles->start_idx = val;
		} else
			goto bad_lpg_params;

		pwm_cfg->lut_params.lut_pause_hi = 0;
		rc = of_property_read_u32(node, "qcom,pause-hi", &val);
		if (!rc)
			pwm_cfg->lut_params.lut_pause_hi = val;
		else if (rc != -EINVAL)
			goto bad_lpg_params;

		pwm_cfg->lut_params.lut_pause_lo = 0;
		rc = of_property_read_u32(node, "qcom,pause-lo", &val);
		if (!rc)
			pwm_cfg->lut_params.lut_pause_lo = val;
		else if (rc != -EINVAL)
			goto bad_lpg_params;

		pwm_cfg->lut_params.ramp_step_ms =
				QPNP_LUT_RAMP_STEP_DEFAULT;
		rc = of_property_read_u32(node, "qcom,ramp-step-ms", &val);
		if (!rc)
			pwm_cfg->lut_params.ramp_step_ms = val;
		else if (rc != -EINVAL)
			goto bad_lpg_params;

		pwm_cfg->lut_params.flags = QPNP_LED_PWM_FLAGS;
		rc = of_property_read_u32(node, "qcom,lut-flags", &val);
		if (!rc)
			pwm_cfg->lut_params.flags = (u8) val;
		else if (rc != -EINVAL)
			goto bad_lpg_params;

		pwm_cfg->lut_params.idx_len =
			pwm_cfg->duty_cycles->num_duty_pcts;
	}
	return 0;

bad_lpg_params:
	pwm_cfg->use_blink = false;
	if (pwm_cfg->mode == PWM_MODE) {
		dev_err(&pdev->dev, "LPG parameters not set for blink mode, defaulting to PWM mode\n");
		return 0;
	}
	return rc;
};

static int qpnp_get_config_rgb(struct qpnp_led_data *led,
				struct device_node *node)
{
	int rc;
	u8 led_mode;
	const char *mode;

	led->rgb_cfg = devm_kzalloc(&led->pdev->dev,
				sizeof(struct rgb_config_data), GFP_KERNEL);
	if (!led->rgb_cfg)
		return -ENOMEM;

	if (led->id == QPNP_ID_RGB_RED)
		led->rgb_cfg->enable = RGB_LED_ENABLE_RED;
	else if (led->id == QPNP_ID_RGB_GREEN)
		led->rgb_cfg->enable = RGB_LED_ENABLE_GREEN;
	else if (led->id == QPNP_ID_RGB_BLUE)
		led->rgb_cfg->enable = RGB_LED_ENABLE_BLUE;
	else
		return -EINVAL;

	rc = of_property_read_string(node, "qcom,mode", &mode);
	if (!rc) {
		led_mode = qpnp_led_get_mode(mode);
		if ((led_mode == MANUAL_MODE) || (led_mode == -EINVAL)) {
			dev_err(&led->pdev->dev, "Selected mode not supported for rgb\n");
			return -EINVAL;
		}
		led->rgb_cfg->pwm_cfg = devm_kzalloc(&led->pdev->dev,
					sizeof(struct pwm_config_data),
					GFP_KERNEL);
		if (!led->rgb_cfg->pwm_cfg) {
			dev_err(&led->pdev->dev, "Unable to allocate memory\n");
			return -ENOMEM;
		}
		led->rgb_cfg->pwm_cfg->mode = led_mode;
		led->rgb_cfg->pwm_cfg->default_mode = led_mode;
	} else {
		return rc;
	}

	rc = qpnp_get_config_pwm(led->rgb_cfg->pwm_cfg, led->pdev, node);
	if (rc < 0) {
		if (led->rgb_cfg->pwm_cfg->pwm_dev)
			pwm_put(led->rgb_cfg->pwm_cfg->pwm_dev);
		return rc;
	}

	return 0;
}

static int qpnp_leds_probe(struct platform_device *pdev)
{
	struct qpnp_led_data *led, *led_array;
	unsigned int base;
	struct device_node *node, *temp;
	int rc, i, num_leds = 0, parsed_leds = 0;
	const char *led_label;
	bool regulator_probe = false;
	struct rgb_sync  *rgb_sync = NULL;

	node = pdev->dev.of_node;
	if (node == NULL)
		return -ENODEV;

	temp = NULL;
	while ((temp = of_get_next_child(node, temp)))
		num_leds++;

	if (!num_leds)
		return -ECHILD;

	led_array = devm_kcalloc(&pdev->dev, num_leds, sizeof(*led_array), GFP_KERNEL);
	if (!led_array)
		return -ENOMEM;

	for_each_child_of_node(node, temp) {
		led = &led_array[parsed_leds];
		led->num_leds = num_leds;
		led->regmap = dev_get_regmap(pdev->dev.parent, NULL);
		if (!led->regmap) {
			dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
			return -EINVAL;
		}
		led->pdev = pdev;

		rc = of_property_read_u32(pdev->dev.of_node, "reg", &base);
		if (rc < 0) {
			dev_err(&pdev->dev, "Couldn't find reg in node = %s rc = %d\n",
				pdev->dev.of_node->full_name, rc);
			goto fail_id_check;
		}
		led->base = base;

		rc = of_property_read_string(temp, "label", &led_label);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "Failure reading label, rc = %d\n", rc);
			goto fail_id_check;
		}

		rc = of_property_read_string(temp, "linux,name",
			&led->cdev.name);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "Failure reading led name, rc = %d\n", rc);
			goto fail_id_check;
		}

		rc = of_property_read_u32(temp, "qcom,max-current",
			&led->max_current);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "Failure reading max_current, rc =  %d\n", rc);
			goto fail_id_check;
		}

		rc = of_property_read_u32(temp, "qcom,id", &led->id);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "Failure reading led id, rc =  %d\n", rc);
			goto fail_id_check;
		}

		rc = qpnp_get_common_configs(led, temp);
		if (rc) {
			dev_err(&led->pdev->dev, "Failure reading common led configuration, rc = %d\n", rc);
			goto fail_id_check;
		}

		led->cdev.brightness_set    = qpnp_led_set;
		led->cdev.brightness_get    = qpnp_led_get;

		if (strcmp(led_label, "rgb") == 0) {
			rc = qpnp_get_config_rgb(led, temp);
			if (rc < 0) {
				dev_err(&led->pdev->dev, "Unable to read rgb config data\n");
				goto fail_id_check;
			}
		}
		else {
			dev_err(&led->pdev->dev, "No LED matching label\n");
			rc = -EINVAL;
			goto fail_id_check;
		}

		if (led->id != QPNP_ID_FLASH1_LED0 &&
					led->id != QPNP_ID_FLASH1_LED1)
			mutex_init(&led->lock);

		INIT_WORK(&led->work, qpnp_led_work);

		rc =  qpnp_led_initialize(led);
		if (rc < 0)
			goto fail_id_check;

		rc = qpnp_led_set_max_brightness(led);
		if (rc < 0)
			goto fail_id_check;

		rc = led_classdev_register(&pdev->dev, &led->cdev);
		if (rc) {
			dev_err(&pdev->dev, "unable to register led %d,rc=%d\n", led->id, rc);
			goto fail_id_check;
		}

		if ((led->id == QPNP_ID_RGB_RED) ||
			(led->id == QPNP_ID_RGB_GREEN) ||
			(led->id == QPNP_ID_RGB_BLUE)) {
			if (led->rgb_cfg->pwm_cfg->mode == PWM_MODE) {
				HVU_LOG
				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&pwm_attr_group);
				if (rc)
					goto fail_id_check;
			}
			if (led->rgb_cfg->pwm_cfg->use_blink) {
				HVU_LOG
				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&blink_attr_group);
				if (rc)
					goto fail_id_check;

				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&lpg_attr_group);
				if (rc)
					goto fail_id_check;

			} else if (led->rgb_cfg->pwm_cfg->mode == LPG_MODE) {
				HVU_LOG
				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&lpg_attr_group);
				if (rc)
					goto fail_id_check;
			}
		}

		/* configure default state */
		if (led->default_on) {
			led->cdev.brightness = led->cdev.max_brightness;
			__qpnp_led_work(led, led->cdev.brightness);
			if (led->turn_off_delay_ms > 0)
				qpnp_led_turn_off(led);
		} else
			led->cdev.brightness = LED_OFF;

		parsed_leds++;
	}
	dev_set_drvdata(&pdev->dev, led_array);
	return 0;

fail_id_check:
	for (i = 0; i < parsed_leds; i++) {
		if (led_array[i].id != QPNP_ID_FLASH1_LED0 &&
				led_array[i].id != QPNP_ID_FLASH1_LED1)
			mutex_destroy(&led_array[i].lock);
		if (led_array[i].in_order_command_processing)
			destroy_workqueue(led_array[i].workqueue);
		led_classdev_unregister(&led_array[i].cdev);
	}

	return rc;
}

static int qpnp_leds_remove(struct platform_device *pdev)
{
	struct qpnp_led_data *led_array  = dev_get_drvdata(&pdev->dev);
	int i, parsed_leds = led_array->num_leds;

	for (i = 0; i < parsed_leds; i++) {
		cancel_work_sync(&led_array[i].work);
		if (led_array[i].id != QPNP_ID_FLASH1_LED0 &&
				led_array[i].id != QPNP_ID_FLASH1_LED1)
			mutex_destroy(&led_array[i].lock);

		if (led_array[i].in_order_command_processing)
			destroy_workqueue(led_array[i].workqueue);
		led_classdev_unregister(&led_array[i].cdev);
		switch (led_array[i].id) {
		case QPNP_ID_RGB_RED:
		case QPNP_ID_RGB_GREEN:
		case QPNP_ID_RGB_BLUE:
			if (led_array[i].rgb_cfg->pwm_cfg->mode == PWM_MODE)
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&pwm_attr_group);
			if (led_array[i].rgb_cfg->pwm_cfg->use_blink) {
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&blink_attr_group);
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&lpg_attr_group);
			} else if (led_array[i].rgb_cfg->pwm_cfg->mode
				   == LPG_MODE)
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&lpg_attr_group);
			break;
		default:
			dev_err(&led_array->pdev->dev,
					"Invalid LED(%d)\n",
					led_array[i].id);
			return -EINVAL;
		}
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id spmi_match_table[] = {
	{ .compatible = "qcom,leds-qpnp",},
	{ },
};
#else
#define spmi_match_table NULL
#endif

static struct platform_driver qpnp_leds_driver = {
	.driver		= {
		.name		= "qcom,leds-qpnp",
		.of_match_table	= spmi_match_table,
	},
	.probe		= qpnp_leds_probe,
	.remove		= qpnp_leds_remove,
};

static int __init qpnp_led_init(void)
{
	return platform_driver_register(&qpnp_leds_driver);
}
module_init(qpnp_led_init);

static void __exit qpnp_led_exit(void)
{
	platform_driver_unregister(&qpnp_leds_driver);
}
module_exit(qpnp_led_exit);

MODULE_DESCRIPTION("QPNP LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-qpnp");