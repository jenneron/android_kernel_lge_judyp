/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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


#include <linux/thermal.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/platform_device.h>

#include "qti_virtual_sensor.h"

static const struct virtual_sensor_data qti_virtual_sensors[] = {
	{
		.virt_zone_name = "gpu-virt-max-step",
		.num_sensors = 2,
		.sensor_names = {"gpu0-usr",
				"gpu1-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "silv-virt-max-step",
		.num_sensors = 4,
		.sensor_names = {"cpu0-silver-usr",
				"cpu1-silver-usr",
				"cpu2-silver-usr",
				"cpu3-silver-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "gold-virt-max-step",
		.num_sensors = 4,
		.sensor_names = {"cpu0-gold-usr",
				"cpu1-gold-usr",
				"cpu2-gold-usr",
				"cpu3-gold-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "hexa-silv-max-step",
		.num_sensors = 6,
		.sensor_names = {"cpu0-silver-usr",
				"cpu1-silver-usr",
				"cpu2-silver-usr",
				"cpu3-silver-usr",
				"cpu4-silver-usr",
				"cpu5-silver-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "dual-gold-max-step",
		.num_sensors = 2,
		.sensor_names = {"cpu0-gold-usr",
				"cpu1-gold-usr"},
		.logic = VIRT_MAXIMUM,
	},
#if defined(CONFIG_MACH_SDM845_JUDYLN)
	/* -0.15*xo + 0.87*quiet + 6.85 */
	{
		.virt_zone_name = "vts-virt-therm",
		.num_sensors = 2,
		.sensor_names = {"xo-therm-adc",
				"quiet-therm-adc"},
		.coefficient_ct = 2,
		.coefficients = {-15, 87},
		.avg_offset = 685000,
		.avg_denominator = 100,
		.logic = VIRT_WEIGHTED_AVG,
	},
#elif defined(CONFIG_MACH_SDM845_JUDYP)
	/* -0.15*xo + 1.04*quiet + 6.7 */
	{
		.virt_zone_name = "vts-virt-therm",
		.num_sensors = 2,
		.sensor_names = {"xo-therm-adc",
				"quiet-therm-adc"},
		.coefficient_ct = 2,
		.coefficients = {-15, 104},
		.avg_offset = 670000,
		.avg_denominator = 100,
		.logic = VIRT_WEIGHTED_AVG,
	},
#elif defined(CONFIG_MACH_SDM845_JUDYPN)
	{
		.virt_zone_name = "vts-virt-therm",
		.num_sensors = 2,
		.sensor_names = {"xo-therm-adc",
				"quiet-therm-adc"},
		.coefficient_ct = 2,
		.coefficients = {0, 1},
		.avg_offset = 0,
		.avg_denominator = 1,
		.logic = VIRT_WEIGHTED_AVG,
	},
#else
	{
		.virt_zone_name = "vts-virt-therm",
		.num_sensors = 2,
		.sensor_names = {"xo-therm-adc",
				"quiet-therm-adc"},
		.coefficient_ct = 2,
		.coefficients = {0, 1},
		.avg_offset = 0,
		.avg_denominator = 1,
		.logic = VIRT_WEIGHTED_AVG,
	},
#endif
};

int qti_virtual_sensor_register(struct device *dev)
{
	int sens_ct = 0;
	static int idx;
	struct thermal_zone_device *tz;

	sens_ct = ARRAY_SIZE(qti_virtual_sensors);
	for (; idx < sens_ct; idx++) {
		tz = devm_thermal_of_virtual_sensor_register(dev,
				&qti_virtual_sensors[idx]);
		if (IS_ERR(tz))
			dev_dbg(dev, "sensor:%d register error:%ld\n",
					idx, PTR_ERR(tz));
		else
			dev_dbg(dev, "sensor:%d registered\n", idx);
	}

	return 0;
}
EXPORT_SYMBOL(qti_virtual_sensor_register);
