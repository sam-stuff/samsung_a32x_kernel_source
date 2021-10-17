/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#  include <linux/string.h>
#  include <linux/kernel.h>
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#  include <platform/upmu_common.h>
#  include <platform/mt_gpio.h>
#  include <platform/mt_i2c.h>
#  include <platform/mt_pmic.h>
#  include <string.h>
#elif defined(BUILD_UBOOT)
#  include <asm/arch/mt_gpio.h>
#endif

#ifdef BUILD_LK
#  define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#  define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#  define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#  define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#  include <linux/kernel.h>
#  include <linux/module.h>
#  include <linux/fs.h>
#  include <linux/slab.h>
#  include <linux/init.h>
#  include <linux/list.h>
#  include <linux/i2c.h>
#  include <linux/irq.h>
#  include <linux/uaccess.h>
#  include <linux/interrupt.h>
#  include <linux/io.h>
#  include <linux/platform_device.h>
#endif

#define FRAME_WIDTH			(720)
#define FRAME_HEIGHT		(1600)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH		(64500)
#define LCM_PHYSICAL_HEIGHT		(129000)
#define LCM_DENSITY			(480)

#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE		0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#include "disp_dts_gpio.h"

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[230];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 50, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 150, {} },
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0xFF, 1, {0x20}},
	{REGFLAG_DELAY, 1, {}},
	{0xFB, 1, {0x01}},
	{0x01, 1, {0x55}},
	{0x03, 1, {0x55}},
	{0x05, 1, {0x99}},
	{0x07, 1, {0x5F}},
	{0x08, 1, {0xDF}},
	{0x0E, 1, {0x7D}},
	{0x0F, 1, {0x7D}},
	{0x94, 1, {0x40}},
	{0x95, 1, {0xFF}},
	{0x96, 1, {0x13}},
	{0x69, 1, {0x91}},
	{0x1F, 1, {0x02}},
	{0x2F, 1, {0x82}},
	{0xB0, 16,{0x00, 0x08, 0x00, 0x09, 0x00, 0x3B, 0x00, 0x54, 0x00, 0x6B, 0x00, 0x7F, 0x00, 0x91, 0x00, 0xA2}},
	{0xB1, 16,{0x00, 0xB2, 0x00, 0xE7, 0x01, 0x0F, 0x01, 0x4F, 0x01, 0x7F, 0x01, 0xCC, 0x01, 0xF9, 0x01, 0xFB}},
	{0xB2, 16,{0x02, 0x46, 0x02, 0x89, 0x02, 0xB6, 0x02, 0xEB, 0x03, 0x12, 0x03, 0x3D, 0x03, 0x4E, 0x03, 0x5D}},
	{0xB3, 12,{0x03, 0x6F, 0x03, 0x86, 0x03, 0xA3, 0x03, 0xC3, 0x03, 0xDB, 0x03, 0xDF}},
	{0xB4, 16,{0x00, 0x08, 0x00, 0x09, 0x00, 0x3B, 0x00, 0x54, 0x00, 0x6B, 0x00, 0x7F, 0x00, 0x91, 0x00, 0xA2}},
	{0xB5, 16,{0x00, 0xB2, 0x00, 0xE7, 0x01, 0x0F, 0x01, 0x4F, 0x01, 0x7F, 0x01, 0xCC, 0x01, 0xF9, 0x01, 0xFB}},
	{0xB6, 16,{0x02, 0x46, 0x02, 0x89, 0x02, 0xB6, 0x02, 0xEB, 0x03, 0x12, 0x03, 0x3D, 0x03, 0x4E, 0x03, 0x5D}},
	{0xB7, 12,{0x03, 0x6F, 0x03, 0x86, 0x03, 0xA3, 0x03, 0xC3, 0x03, 0xDB, 0x03, 0xDF}},
	{0xB8, 16,{0x00, 0x08, 0x00, 0x09, 0x00, 0x3B, 0x00, 0x54, 0x00, 0x6B, 0x00, 0x7F, 0x00, 0x91, 0x00, 0xA2}},
	{0xB9, 16,{0x00, 0xB2, 0x00, 0xE7, 0x01, 0x0F, 0x01, 0x4F, 0x01, 0x7F, 0x01, 0xCC, 0x01, 0xF9, 0x01, 0xFB}},
	{0xBA, 16,{0x02, 0x46, 0x02, 0x89, 0x02, 0xB6, 0x02, 0xEB, 0x03, 0x12, 0x03, 0x3D, 0x03, 0x4E, 0x03, 0x5D}},
	{0xBB, 12,{0x03, 0x6F, 0x03, 0x86, 0x03, 0xA3, 0x03, 0xC3, 0x03, 0xDB, 0x03, 0xDF}},

	{0xFF, 1, {0x21}},
	{0xFB, 1, {0x01}},
	{0xB0, 16,{0x00, 0x00, 0x00, 0x01, 0x00, 0x33, 0x00, 0x4C, 0x00, 0x63, 0x00, 0x77, 0x00, 0x89, 0x00, 0x9A}},
	{0xB1, 16,{0x00, 0xAA, 0x00, 0xDF, 0x01, 0x07, 0x01, 0x47, 0x01, 0x77, 0x01, 0xC4, 0x02, 0x11, 0x02, 0x13}},
	{0xB2, 16,{0x02, 0x3E, 0x02, 0x81, 0x02, 0xAE, 0x02, 0xE3, 0x03, 0x0A, 0x03, 0x35, 0x03, 0x46, 0x03, 0x55}},
	{0xB3, 12,{0x03, 0x67, 0x03, 0x7E, 0x03, 0x9B, 0x03, 0xBB, 0x03, 0xD3, 0x03, 0xD7}},
	{0xB4, 16,{0x00, 0x00, 0x00, 0x01, 0x00, 0x33, 0x00, 0x4C, 0x00, 0x63, 0x00, 0x77, 0x00, 0x89, 0x00, 0x9A}},
	{0xB5, 16,{0x00, 0xAA, 0x00, 0xDF, 0x01, 0x07, 0x01, 0x47, 0x01, 0x77, 0x01, 0xC4, 0x02, 0x11, 0x02, 0x13}},
	{0xB6, 16,{0x02, 0x3E, 0x02, 0x81, 0x02, 0xAE, 0x02, 0xE3, 0x03, 0x0A, 0x03, 0x35, 0x03, 0x46, 0x03, 0x55}},
	{0xB7, 12,{0x03, 0x67, 0x03, 0x7E, 0x03, 0x9B, 0x03, 0xBB, 0x03, 0xD3, 0x03, 0xD7}},
	{0xB8, 16,{0x00, 0x00, 0x00, 0x01, 0x00, 0x33, 0x00, 0x4C, 0x00, 0x63, 0x00, 0x77, 0x00, 0x89, 0x00, 0x9A}},
	{0xB9, 16,{0x00, 0xAA, 0x00, 0xDF, 0x01, 0x07, 0x01, 0x47, 0x01, 0x77, 0x01, 0xC4, 0x02, 0x11, 0x02, 0x13}},
	{0xBA, 16,{0x02, 0x3E, 0x02, 0x81, 0x02, 0xAE, 0x02, 0xE3, 0x03, 0x0A, 0x03, 0x35, 0x03, 0x46, 0x03, 0x55}},
	{0xBB, 12,{0x03, 0x67, 0x03, 0x7E, 0x03, 0x9B, 0x03, 0xBB, 0x03, 0xD3, 0x03, 0xD7}},

	{0xFF, 1, {0x23}},
	{0xFB, 1, {0x01}},
	{0x00, 1, {0x00}},
	{0x04, 1, {0x00}},
	{0x07, 1, {0x20}},
	{0x08, 1, {0x0F}},
	{0x09, 1, {0x00}},
	{0x12, 1, {0xB4}},
	{0x15, 1, {0xE9}},
	{0x16, 1, {0x0B}},
	{0x19, 1, {0x00}},
	{0x1A, 1, {0x00}},
	{0x1B, 1, {0x08}},
	{0x1C, 1, {0x0A}},
	{0x1D, 1, {0x0C}},
	{0x1E, 1, {0x12}},
	{0x1F, 1, {0x16}},
	{0x20, 1, {0x1A}},
	{0x21, 1, {0x1C}},
	{0x22, 1, {0x20}},
	{0x23, 1, {0x24}},
	{0x24, 1, {0x28}},
	{0x25, 1, {0x2C}},
	{0x26, 1, {0x30}},
	{0x27, 1, {0x38}},
	{0x28, 1, {0x3C}},
	{0x29, 1, {0x10}},
	{0x30, 1, {0xFF}},
	{0x31, 1, {0xFF}},
	{0x32, 1, {0xFE}},
	{0x33, 1, {0xFD}},
	{0x34, 1, {0xFD}},
	{0x35, 1, {0xFC}},
	{0x36, 1, {0xFB}},
	{0x37, 1, {0xF9}},
	{0x38, 1, {0xF7}},
	{0x39, 1, {0xF3}},
	{0x3A, 1, {0xEA}},
	{0x3B, 1, {0xE6}},
	{0x3D, 1, {0xE0}},
	{0x3F, 1, {0xDD}},
	{0x40, 1, {0xDB}},
	{0x41, 1, {0xD9}},
	{0xFF, 1, {0x24}},
	{0xFB, 1, {0x01}},
	{0x93, 1, {0x12}},
	{0x94, 1, {0x0E}},
	{0x00, 1, {0x00}},
	{0x01, 1, {0x1E}},
	{0x02, 1, {0x06}},
	{0x03, 1, {0x21}},
	{0x04, 1, {0x04}},
	{0x05, 1, {0x05}},
	{0x06, 1, {0x1F}},
	{0x07, 1, {0x20}},
	{0x08, 1, {0x0C}},
	{0x09, 1, {0x0D}},
	{0x0A, 1, {0x0E}},
	{0x0B, 1, {0x0F}},
	{0x0C, 1, {0x00}},
	{0x0D, 1, {0x00}},
	{0x0E, 1, {0x00}},
	{0x0F, 1, {0x00}},
	{0x10, 1, {0x00}},
	{0x11, 1, {0x00}},
	{0x12, 1, {0x00}},
	{0x13, 1, {0x00}},
	{0x14, 1, {0x00}},
	{0x15, 1, {0x00}},
	{0x16, 1, {0x00}},
	{0x17, 1, {0x1E}},
	{0x18, 1, {0x07}},
	{0x19, 1, {0x21}},
	{0x1A, 1, {0x04}},
	{0x1B, 1, {0x05}},
	{0x1C, 1, {0x1F}},
	{0x1D, 1, {0x20}},
	{0x1E, 1, {0x0C}},
	{0x1F, 1, {0x0D}},
	{0x20, 1, {0x0E}},
	{0x21, 1, {0x0F}},
	{0x22, 1, {0x00}},
	{0x23, 1, {0x00}},
	{0x24, 1, {0x00}},
	{0x25, 1, {0x00}},
	{0x26, 1, {0x00}},
	{0x27, 1, {0x00}},
	{0x28, 1, {0x00}},
	{0x29, 1, {0x00}},
	{0x2A, 1, {0x00}},
	{0x2B, 1, {0x00}},
	{0x2D, 1, {0x0C}},
	{0x2F, 1, {0x0A}},
	{0x30, 1, {0x08}},
	{0x31, 1, {0x0C}},
	{0x32, 1, {0x0D}},
	{0x33, 1, {0x08}},
	{0x34, 1, {0x0A}},
	{0x35, 1, {0x0D}},
	{0x36, 1, {0x0C}},
	{0x37, 1, {0x44}},
	{0x38, 1, {0x22}},
	{0x39, 1, {0x00}},
	{0x3A, 1, {0x22}},
	{0x3B, 1, {0x8B}},
	{0x3D, 1, {0x54}},
	{0x41, 1, {0x45}},
	{0x42, 1, {0x44}},
	{0x45, 1, {0x44}},
	{0x46, 1, {0x45}},
	{0x47, 1, {0x22}},
	{0x48, 1, {0x22}},
	{0x49, 1, {0x00}},
	{0x4A, 1, {0x22}},	
	{0x4B, 1, {0x8B}},
	{0x4C, 1, {0x54}},
	{0x4D, 1, {0x34}},
	{0x4E, 1, {0x12}},
	{0x4F, 1, {0x00}},
	{0x50, 1, {0x00}},
	{0x51, 1, {0x21}},
	{0x52, 1, {0x43}},
	{0x53, 1, {0x00}},
	{0x54, 1, {0x00}},
	{0x55, 1, {0x47}},
	{0x56, 1, {0x44}},
	{0x58, 1, {0x21}},
	{0x59, 1, {0x00}},
	{0x5A, 1, {0x22}},
	{0x5B, 1, {0x8B}},
	{0x5C, 1, {0x88}},
	{0x5D, 1, {0x08}},
	{0x5E, 1, {0x04}},
	{0x5F, 1, {0x00}},
	{0x88, 1, {0x2E}},
	{0x89, 1, {0x48}},
	{0x8A, 1, {0x88}},
	{0x8D, 1, {0x08}},
	{0x8E, 1, {0x08}},
	{0x60, 1, {0x80}},
	{0x61, 1, {0x90}},
	{0x64, 1, {0x10}},
	{0x80, 1, {0x0B}},
	{0x81, 1, {0x05}},
	{0x82, 1, {0x0B}},
	{0x83, 1, {0x05}},
	{0x84, 1, {0x80}},
	{0x85, 1, {0x01}},
	{0x86, 1, {0x04}},
	{0x91, 1, {0x40}},
	{0x92, 1, {0xAE}},
	{0x98, 1, {0x80}},
	{0xAB, 1, {0x00}},
	{0xAD, 1, {0x00}},
	{0xAF, 1, {0x04}},
	{0xB0, 1, {0x05}},
	{0xB1, 1, {0xAA}},
	{0xFF, 1, {0x25}},
	{0xFB, 1, {0x01}},
	{0x17, 1, {0x82}},
	{0x18, 1, {0x06}},
	{0x19, 1, {0x0F}},
	{0xC0, 1, {0x07}},
	{0xC2, 1, {0x84}},
	{0xC6, 1, {0x92}},
	{0xCA, 1, {0x1C}},
	{0xCB, 1, {0x1C}},
	{0xCC, 1, {0x1C}},
	{0xCD, 1, {0x00}},
	{0xCE, 1, {0x00}},
	{0xD3, 1, {0x11}},
	{0xD4, 1, {0xCC}},
	{0xD5, 1, {0x11}},
	{0xD6, 1, {0x1C}},
	{0xD7, 1, {0x11}},
	{0x0A, 1, {0x82}},
	{0x0B, 1, {0x16}},
	{0x0C, 1, {0x01}},
	{0xFF, 1, {0x26}},
	{0xFB, 1, {0x01}},
	{0x00, 1, {0xA0}},
	{0xFF, 1, {0x27}},
	{0xFB, 1, {0x01}},
	{0x13, 1, {0x00}},
	{0x15, 1, {0xB4}},
	{0x1F, 1, {0x55}},
	{0x26, 1, {0x0F}},
	{0xC0, 1, {0x18}},
	{0xC1, 1, {0xF2}},
	{0xC7, 1, {0x03}},
	{0xC2, 1, {0x00}},
	{0xC3, 1, {0x00}},
	{0xC4, 1, {0xF2}},
	{0xC5, 1, {0x00}},
	{0xC6, 1, {0x00}},
	{0xFF, 1, {0xF0}},
	{0xFB, 1, {0x01}},
	{0x4D, 1, {0x15}},
	{0x4F, 1, {0x00}},
	{0x50, 1, {0x0B}},
	{0xFF, 1, {0xE0}},
	{0xFB, 1, {0x01}},
	{0x25, 1, {0x11}},
	{0x4E, 1, {0x01}},
	{0xFF, 1, {0x10}},
	{0xFB, 1, {0x01}},
	{0xBA, 1, {0x02}},
	{0x53, 1, {0x2C}},
	{0x55, 1, {0x01}},
	{0x68, 2,{0x00, 0x01}},

	{0x11, 0x0, {}},	/* a02_sleep_out */
	{REGFLAG_DELAY, 150, {}},/* wait 4 frame */
	{0x29, 0x0, {} },	/* a02_display_on */
};

static struct LCM_setting_table
__maybe_unused lcm_deep_sleep_mode_in_setting[] = {
	{0x28, 0x0, {} },
	{REGFLAG_DELAY, 50, {} },
	{0x10, 0x0, {} },
	{REGFLAG_DELAY, 150, {} },
};

static struct LCM_setting_table __maybe_unused lcm_sleep_out_setting[] = {
	{0x11, 0x0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0x0, {} },
	{REGFLAG_DELAY, 50, {} },
};

static struct LCM_setting_table bl_level[] = {
	{0x53, 0x1, {0x24} }, /* SEQ_NT36525B_BRIGHTNESS_MODE */
	{0x51, 0x1, {0x88} }, /* SEQ_NT36525B_BRIGHTNESS */
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

/* i2c control start */
//#define LCM_I2C_ADDR 0x11
//#define LCM_I2C_BUSNUM  1	/* for I2C channel 0 */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"

static struct i2c_client *_lcm_i2c_client;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct _lcm_i2c_dev {
	struct i2c_client *client;
};

static const struct of_device_id _lcm_i2c_of_match[] = {
	{ .compatible = "mediatek,i2c_lcd_bias", },
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = {
	{LCM_I2C_ID_NAME, 0},
	{}
};

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	/* .detect               = _lcm_i2c_detect, */
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},
};

/*****************************************************************************
 * Function
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	pr_info("[LCM][I2C] %s\n", __func__);
	pr_info("[LCM][I2C] NT: info==>name=%s addr=0x%x\n",
				client->name, client->addr);
	_lcm_i2c_client = client;

	return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_debug("ERROR!! _lcm_i2c_client is null\n");
	return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	int ret;

	pr_debug("[LCM][I2C] %s\n", __func__);
	ret = i2c_add_driver(&_lcm_i2c_driver);
	if (ret < 0) {
		pr_info("[LCM][I2C] %s fail, device delete\n", __func__);
		i2c_del_driver(&_lcm_i2c_driver);
	} else
		pr_info("[LCM][I2C] %s success\n", __func__);

	return ret;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	i2c_del_driver(&_lcm_i2c_driver);
}

module_init(_lcm_i2c_init);
module_exit(_lcm_i2c_exit);
/* i2c control end */

static void push_table(void *cmdq, struct LCM_setting_table *table,
		       unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;
		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
					 table[i].para_list, force_update);
			break;
		}
	}
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH / 1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT / 1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density = LCM_DENSITY;

	params->dsi.mode = BURST_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = BURST_VDO_MODE;
	LCM_LOGI("%s: lcm_dsi_mode %d\n", __func__, lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_THREE_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */
	params->dsi.data_rate = 639;

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 254;
	params->dsi.vertical_frontporch = 18;
	params->dsi.vertical_frontporch_for_low_power = 84;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

 	params->dsi.horizontal_sync_active = 4;
	params->dsi.horizontal_backporch = 74;
	params->dsi.horizontal_frontporch = 74;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
	params->dsi.cont_clock = 1;
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 277;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x1C;

	/* for ARR 2.0 */
	params->max_refresh_rate = 60;
	params->min_refresh_rate = 45;

}

int lcm_bias_regulator_init(void)
{
	return 0;
}

int lcm_bias_disable(void)
{
	return 0;
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_init_power(void)
{
	pr_info("%s\n", __func__);

	/*HT BL enable*/
	_lcm_i2c_write_bytes(0x0C, 0x2C);
	_lcm_i2c_write_bytes(0x0D, 0x26);
	_lcm_i2c_write_bytes(0x0E, 0x26);
	_lcm_i2c_write_bytes(0x09, 0xBE);
	_lcm_i2c_write_bytes(0x02, 0x6B);
	_lcm_i2c_write_bytes(0x03, 0x0D);
	_lcm_i2c_write_bytes(0x11, 0x74);
	_lcm_i2c_write_bytes(0x04, 0x05);
	_lcm_i2c_write_bytes(0x05, 0xCC);
	_lcm_i2c_write_bytes(0x10, 0x67);
	_lcm_i2c_write_bytes(0x08, 0x13);
}

static void lcm_suspend_power(void)
{
	pr_info("nt36525b : %s\n", __func__);
	SET_RESET_PIN(0);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT0);

	MDELAY(1);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENN);
}

static void lcm_resume_power(void)
{
	pr_info("nt36525b : %s\n", __func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENP);
	SET_RESET_PIN(0);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);

	MDELAY(1);

	lcm_init_power();
}

static void lcm_init(void)
{
	pr_info("nt36525b : %s\n", __func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
	MDELAY(1);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT0);
	MDELAY(10);

	SET_RESET_PIN(1);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
	MDELAY(5);

	push_table(NULL, init_setting_vdo, ARRAY_SIZE(init_setting_vdo), 1);
	pr_info("td4150_fhdp----lm36274----lcm mode = vdo mode :%d----\n",
		 lcm_dsi_mode);
}

static void lcm_suspend(void)
{
	pr_info("nt36525b : %s\n", __func__);
	push_table(NULL, lcm_suspend_setting,
		   ARRAY_SIZE(lcm_suspend_setting), 1);
}

static void lcm_resume(void)
{
	pr_info("nt36525b : %s\n", __func__);
	lcm_init();
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	pr_info("%s,nt36525b_hdp backlight: level = %d\n", __func__, level);

	bl_level[1].para_list[0] = level;

	push_table(handle, bl_level, ARRAY_SIZE(bl_level), 1);
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width,
	unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

struct LCM_DRIVER nt36525b_hdp_dsi_vdo_lm36274_boe_lcm_drv = {
	.name = "nt36525b_hdp_dsi_vdo_lm36274_csot_sharp_lcm_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.update = lcm_update,
};
