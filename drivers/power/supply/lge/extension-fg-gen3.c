/*
 * CAUTION! :
 * 	This file will be included at the end of "qpnp-fg-gen3.c".
 * 	So "qpnp-fg-gen3.c" should be touched before you start to build.
 * 	If not, your work will not be applied to the built image
 * 	because the build system doesn't care the update time of this file.
 */

#include <linux/thermal.h>
#include "veneer-primitives.h"

#define FAKE_DISABLED -1
static struct _fake {
	int temperature;
	int capacity;
	int uvoltage;
} fake = {
	.temperature = FAKE_DISABLED,
	.capacity = FAKE_DISABLED,
	.uvoltage = FAKE_DISABLED,
};

#define FGGEN3_INITVAL -1
static struct _fggen3 {
/* Capacity */
	int capacity_rescaled;
	int capacity_monotonic;
	int capacity_chargecnt;
	int capacity_learned;
/* v/i ADCs */
	int battery_inow;
	int battery_vnow;
	int battery_vpredict;
	int battery_ocv;
	int input_vusb;
	int input_iusb;
	int input_vwlc;
	int input_iwlc;
	int input_aicl;
/* Temperature */
	int temp_compensated;
	int temp_thermister;
	int temp_vts;
/* Impedance */
	int impedance_esr;
	int impedance_rslow;
/* Misc */
	int misc_cycle;
	int misc_batid;
/* SoCs */
	int misc_battsoc;
	int misc_ccsocsw;
	int misc_ccsoc;

} fggen3 = {
/* Capacity    */ FGGEN3_INITVAL, FGGEN3_INITVAL, FGGEN3_INITVAL, FGGEN3_INITVAL,
/* v/i ADCs    */ FGGEN3_INITVAL, FGGEN3_INITVAL, FGGEN3_INITVAL, FGGEN3_INITVAL, FGGEN3_INITVAL, FGGEN3_INITVAL, FGGEN3_INITVAL, FGGEN3_INITVAL, FGGEN3_INITVAL,
/* Temperature */ FGGEN3_INITVAL, FGGEN3_INITVAL, -1000,
/* impedance   */ FGGEN3_INITVAL, FGGEN3_INITVAL,
/* Misc        */ FGGEN3_INITVAL, FGGEN3_INITVAL,
/* SoCs        */ FGGEN3_INITVAL, FGGEN3_INITVAL, FGGEN3_INITVAL,
};

#define RESCALE_INITVAL -1
static struct _rescale {
	// For SoC rescaling, .rawsoc(0~255) is updated ONLY ON
	// 'fg_delta_msoc_irq_handler' and it is rescaled to 0~100
	int	criteria;	// 0 ~ 255
	int	rawsoc;		// 0 ~ 255
	int	result;		// 0 ~ 100
} rescale = {
	RESCALE_INITVAL, RESCALE_INITVAL, RESCALE_INITVAL,
};


///////////////////////////////////////////////////////////////////////////////

static int calculate_battery_temperature(/* @Nonnull */ struct fg_chip *chip) {
	#define				tcomp_count 25
	static struct tcomp_entry {
		int	temp_cell;
		int	temp_bias;
	}				tcomp_buffer [tcomp_count];

	static struct device_node*	tcomp_dtroot;
	static struct tcomp_entry*	tcomp_table;
	static int			tcomp_icoeff = 0;

	int i = 0;
	// bias  : 1st compensation by predefined diffs
	// icomp : 2nd compensation by (i^2 * k)
	int battemp_bias = 0;
	int battemp_icomp = 0;
	// Prepare default value to return on error */
	int battemp_cell = 0;
	fg_get_battery_temp(chip, &battemp_cell);

	if (!tcomp_dtroot) {
		#define DNODE_TEMPCOMP_ROOT "lge-battery-supplement"

		tcomp_dtroot = of_find_node_by_name(NULL, DNODE_TEMPCOMP_ROOT);
		pr_info("%s to find " DNODE_TEMPCOMP_ROOT "\n", tcomp_dtroot ? "Success" : "Failed");
		if (!tcomp_dtroot)
			return battemp_cell;
	}

/* Finding tcomp_table and tcomp_icoeff */
	if (tcomp_dtroot && tcomp_table == NULL) {
		#define DNODE_TEMPCOMP_OFFSET "tempcomp-offset"
		int dtarray_count = tcomp_count * 2;
		u32 dtarray_data [tcomp_count * 2];
		if (of_property_read_u32_array(tcomp_dtroot, DNODE_TEMPCOMP_OFFSET,
			dtarray_data, dtarray_count) >= 0) {
			for (i=0; i<dtarray_count; ++i) {
				struct tcomp_entry* saving_entry = &tcomp_buffer[i/2];
				int* saving_member = (i%2) ? &saving_entry->temp_bias : &saving_entry->temp_cell;
				*saving_member = dtarray_data [i];
			}
			/* Logging for verification */
			for (i=0; i<tcomp_count; ++i)
				pr_debug("Index = %02d : %4d - %4d\n",
					i, tcomp_buffer[i].temp_cell, tcomp_buffer[i].temp_bias);
			tcomp_table = tcomp_buffer;
		}
		else {
			pr_info(DNODE_TEMPCOMP_OFFSET " is not found, Skip to compensation\n");
			return battemp_cell;
		}
	}
	if (tcomp_dtroot && tcomp_icoeff == 0 && chip->bp.batt_type_str) {
		#define DNODE_TEMPCOMP_ICOEFF "tempcomp-icoeff"
		struct device_node* tcomp_override = of_find_node_by_name(tcomp_dtroot, chip->bp.batt_type_str);
		of_property_read_u32(tcomp_dtroot, DNODE_TEMPCOMP_ICOEFF, &tcomp_icoeff);
		/* Override it if needed */
		if (tcomp_override && of_property_read_u32(tcomp_override,
			DNODE_TEMPCOMP_ICOEFF, &tcomp_icoeff) >= 0)
			pr_info("ICOEFF is overridden to %d for %s\n", tcomp_icoeff, chip->bp.batt_type_str);
		else
			pr_info("ICOEFF is set to %d by default\n", tcomp_icoeff);
	}

/* Compensating battemp_bias */
	for (i=0; i<tcomp_count; ++i) {
		if (battemp_cell < tcomp_table[i].temp_cell)
			break;
	}
	if (i == 0)
		battemp_bias =
			tcomp_table[0].temp_bias;
	else if (i == tcomp_count)
		battemp_bias =
			tcomp_table[tcomp_count-1].temp_bias;
	else
		battemp_bias = ((tcomp_table[i].temp_bias - tcomp_table[i-1].temp_bias) * (battemp_cell - tcomp_table[i-1].temp_cell)
			/ (tcomp_table[i].temp_cell - tcomp_table[i-1].temp_cell)) + tcomp_table[i-1].temp_bias;

/* Compensating battemp_icomp */
	if (chip->batt_psy) {
		int ichg = 0;
		union power_supply_propval val = { 0, };
		if (!power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_STATUS, &val)
			&& val.intval == POWER_SUPPLY_STATUS_CHARGING
			&& !power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val)
			&& val.intval < 0)
			ichg = ((val.intval) / 1000);

		battemp_icomp = ichg * ichg * tcomp_icoeff / 10000000;

		pr_debug("Temperature : %d = (%d)(cell) + (%d)(bias) - %d(icomp), icoeff = %d, ichg = %d\n",
			battemp_cell + battemp_bias - battemp_icomp,
			battemp_cell, battemp_bias, battemp_icomp,
			tcomp_icoeff, ichg);

		return battemp_cell + battemp_bias - battemp_icomp;
	}
	else {
		pr_info("Charger is not available, returning %d(=%d+%d) as batt temp\n",
			battemp_cell + battemp_bias, battemp_cell, battemp_bias);
		return battemp_cell + battemp_bias;
	}
}

static int calculate_rescaled_soc(struct fg_chip *chip) {
	int msoc_raw = 0, rc = 0;

	rc = fg_get_msoc_raw(chip, &msoc_raw);
	if (rc < 0)
		pr_info("Error in getting msoc_raw, rc=%d\n", rc);

	if (rescale.criteria == RESCALE_INITVAL) {
		struct device_node* battsupp = of_find_node_by_name(NULL,
			"lge-battery-supplement");
		int msoc_rechg = DIV_ROUND_CLOSEST(chip->dt.recharge_soc_thr * FULL_SOC_RAW,
			FULL_CAPACITY);

		if (!battsupp
			|| of_property_read_u32(battsupp, "capacity-raw-full", &rescale.criteria) < 0
			|| msoc_rechg < rescale.criteria) {
				pr_err("Failed to preset check dt, "
					"rechg_msoc=%d, criteria=%d\n", msoc_rechg, rescale.criteria);
				rescale.criteria = FULL_SOC_RAW;
		}
	}

	// Calculate reporting SoC with CAPACITY_RAW
	rescale.result = min(FULL_CAPACITY,
			DIV_ROUND_CLOSEST(msoc_raw * FULL_CAPACITY,
			rescale.criteria));
	// Finally, cache CAPACITY_RAW (but it is not used)
	rescale.rawsoc = msoc_raw;

	return 0;
}

static int extension_battery_cycle(/* @Nonnull */ struct fg_chip *chip) {
	static int bucket_sum_backup = 0;

	int cycle = 0;
	if (chip->cyc_ctr.en) {
		int i, bucket_sum_now = 0;

		mutex_lock(&chip->cyc_ctr.lock);
		for (i=0; i<BUCKET_COUNT; i++)
			bucket_sum_now += chip->cyc_ctr.count[i];
		cycle = bucket_sum_now / BUCKET_COUNT;
		mutex_unlock(&chip->cyc_ctr.lock);

		if (bucket_sum_backup != bucket_sum_now) {
			pr_info("Battery cycle : %d (= %d / %d)\n",
				cycle, bucket_sum_now, BUCKET_COUNT);
			bucket_sum_backup = bucket_sum_now;
		}
	}

	return cycle;
}

static void fggen3_snapshot_print(void) {
	/* To remove TAG in the logs, 'printk' is used */

	printk("PMINFO: [CSV] "
	/* Capacity    */ "cSYS:%d, cMNT:%d.%d, cCHG:%d, cLRN:%d, "
	/* v/i ADCs    */ "iBAT:%d, vBAT:%d, vPRD:%d, vOCV:%d, vUSB:%d, iUSB:%d, vWLC:%d, iWCL:%d, AiCL:%d, "
	/* Temperature */ "tSYS:%d, tORI:%d, tVTS:%d, "
	/* Impedance   */ "rTOT:%d, rESR:%d, rSLW:%d, "
	/* Misc        */ "CYCLE:%d, BATID:%d, "
	/* SoCs        */ "sBATT:%d, sCCSW:%d, sCC:%d\n",

	/* Capacity    */ fggen3.capacity_rescaled,
			  fggen3.capacity_monotonic/10, fggen3.capacity_monotonic%10,
			  fggen3.capacity_chargecnt, fggen3.capacity_learned,
	/* Battery     */ fggen3.battery_inow, fggen3.battery_vnow, fggen3.battery_vpredict, fggen3.battery_ocv,
	/* Input       */ fggen3.input_vusb, fggen3.input_iusb, fggen3.input_vwlc, fggen3.input_iwlc, fggen3.input_aicl,
	/* Temperature */ fggen3.temp_compensated, fggen3.temp_thermister, fggen3.temp_vts,
	/* Impedance   */ fggen3.impedance_esr + fggen3.impedance_rslow,
			  fggen3.impedance_esr, fggen3.impedance_rslow,
	/* Misc        */ fggen3.misc_cycle, fggen3.misc_batid,
	/* SoCs        */ fggen3.misc_battsoc, fggen3.misc_ccsocsw, fggen3.misc_ccsoc);
}

static void fggen3_snapshot_inputnow(int* vusb, int* iusb, int* vwlc, int* iwlc, int* aicl) {
	struct power_supply* psy_main  = power_supply_get_by_name("main");
	struct power_supply* psy_usb   = power_supply_get_by_name("usb");
	struct power_supply* psy_dc    = power_supply_get_by_name("dc");
	union power_supply_propval val = { 0, };

	*aicl = (psy_main && !power_supply_get_property(psy_main, POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED, &val))
		? val.intval/1000 : FGGEN3_INITVAL;
	*vusb = (psy_usb && !power_supply_get_property(psy_usb, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val))
		? val.intval/1000 : FGGEN3_INITVAL;
	*iusb = (psy_usb && !power_supply_get_property(psy_usb, POWER_SUPPLY_PROP_INPUT_CURRENT_NOW, &val))
		? val.intval/1000 : FGGEN3_INITVAL;
	*vwlc = (psy_dc && !power_supply_get_property(psy_dc, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val))
		? val.intval/1000 : FGGEN3_INITVAL;
	*iwlc = (psy_dc && !power_supply_get_property(psy_dc, POWER_SUPPLY_PROP_INPUT_CURRENT_NOW, &val))
		? val.intval/1000 : FGGEN3_INITVAL;

	if (psy_main)
		power_supply_put(psy_main);
	if (psy_usb)
		power_supply_put(psy_usb);
	if (psy_dc)
		power_supply_put(psy_dc);
}

static void fggen3_snapshot_update(struct power_supply *psy) {
	struct thermal_zone_device*	tzd = thermal_zone_get_zone_by_name("vts-virt-therm");
	struct fg_chip*			fg3 = power_supply_get_drvdata(psy);
	int 				buf = 0;

	if (!tzd || !fg3) {
		// Not ready to log
		return;
	}

/* Capacity */
	fggen3.capacity_rescaled = rescale.result < 0
		? FGGEN3_INITVAL : rescale.result;
	fggen3.capacity_monotonic = !fg_get_msoc_raw(fg3, &buf)
		? buf * 1000 / 255 : FGGEN3_INITVAL;
	fggen3.capacity_chargecnt = !fg_get_charge_counter(fg3, &buf)
		? buf / 1000 : FGGEN3_INITVAL;
	fggen3.capacity_learned
		= (fg3->cl.learned_cc_uah) / 1000;
/* Battery */
	fggen3.battery_inow = !fg_get_battery_current(fg3, &buf)
		? buf / 1000 : FGGEN3_INITVAL;
	fggen3.battery_vnow = !fg_get_battery_voltage(fg3, &buf)
		? buf / 1000 : FGGEN3_INITVAL;
	fggen3.battery_vpredict = !fg_get_sram_prop(fg3, FG_SRAM_VOLTAGE_PRED, &buf)
		? buf / 1000 : FGGEN3_INITVAL;
	fggen3.battery_ocv = !fg_get_sram_prop(fg3, FG_SRAM_OCV, &buf)
		? buf / 1000 : FGGEN3_INITVAL;
/* Input */
	fggen3_snapshot_inputnow(&fggen3.input_vusb, &fggen3.input_iusb,
		&fggen3.input_vwlc, &fggen3.input_iwlc,
		&fggen3.input_aicl);
/* Temperature */
	fggen3.temp_compensated
		= calculate_battery_temperature(fg3);
	fggen3.temp_thermister = !fg_get_battery_temp(fg3, &buf)
		? buf : FGGEN3_INITVAL;
	fggen3.temp_vts = !thermal_zone_get_temp(tzd, &buf)
		? buf / 100 : -1000;
/* Impedance */
	fggen3.impedance_esr = !fg_get_sram_prop(fg3, FG_SRAM_ESR, &buf)
		? buf : FGGEN3_INITVAL;
	fggen3.impedance_rslow = !fg_get_sram_prop(fg3, FG_SRAM_RSLOW, &buf)
		? buf : FGGEN3_INITVAL;
/* Misc */
	fggen3.misc_cycle
		= fg_get_cycle_count(fg3);
	fggen3.misc_batid
		= fg3->batt_id_ohms / 1000;
/* SoCs */
	fggen3.misc_battsoc = !fg_get_sram_prop(fg3, FG_SRAM_BATT_SOC, &buf)
		? ((u32)buf >> 24)*1000/255 : FGGEN3_INITVAL;
	fggen3.misc_ccsocsw = !fg_get_sram_prop(fg3, FG_SRAM_CC_SOC_SW, &buf)
		? div_s64((s64)buf*1000, CC_SOC_30BIT) : FGGEN3_INITVAL;
	fggen3.misc_ccsoc = !fg_get_sram_prop(fg3, FG_SRAM_CC_SOC, &buf)
		? div_s64((s64)buf*1000, CC_SOC_30BIT) : FGGEN3_INITVAL;

	/* logging finally */
	fggen3_snapshot_print();
}

#define KI_COEFF_LOW_DISCHG_DEFAULT	800
#define KI_COEFF_MED_DISCHG_DEFAULT	1500
#define KI_COEFF_HI_DISCHG_DEFAULT	2200
int override_fg_adjust_ki_coeff_dischg(struct fg_chip *chip)
{
	int rc, i, msoc;
	int ki_coeff_low = KI_COEFF_LOW_DISCHG_DEFAULT;
	int ki_coeff_med = KI_COEFF_MED_DISCHG_DEFAULT;
	int ki_coeff_hi = KI_COEFF_HI_DISCHG_DEFAULT;
	static int old_ki_coeff_low = 0;
	static int old_ki_coeff_med = 0;
	static int old_ki_coeff_hi = 0;
	u8 val;

	if (!chip->ki_coeff_dischg_en)
		return 0;

	rc = fg_get_prop_capacity(chip, &msoc);
	if (rc < 0) {
		pr_err("Error in getting capacity, rc=%d\n", rc);
		return rc;
	}

	if (chip->esr_flt_sts == LOW_TEMP &&
		chip->charge_status == POWER_SUPPLY_STATUS_DISCHARGING) {
		for (i = KI_COEFF_SOC_LEVELS - 1; i >= 0; i--) {
			if (msoc < chip->dt.ki_coeff_soc[i]) {
				ki_coeff_low = chip->dt.ki_coeff_low_dischg[i];
				ki_coeff_med = chip->dt.ki_coeff_med_dischg[i];
				ki_coeff_hi = chip->dt.ki_coeff_hi_dischg[i];
			}
		}
	}

	if (old_ki_coeff_low == ki_coeff_low
		&& old_ki_coeff_med == ki_coeff_med
		&& old_ki_coeff_hi == ki_coeff_hi)
		return 0;

	old_ki_coeff_low = ki_coeff_low;
	old_ki_coeff_med = ki_coeff_med;
	old_ki_coeff_hi = ki_coeff_hi;

	fg_encode(chip->sp, FG_SRAM_KI_COEFF_LOW_DISCHG, ki_coeff_low, &val);
	rc = fg_sram_write(chip,
			chip->sp[FG_SRAM_KI_COEFF_LOW_DISCHG].addr_word,
			chip->sp[FG_SRAM_KI_COEFF_LOW_DISCHG].addr_byte, &val,
			chip->sp[FG_SRAM_KI_COEFF_LOW_DISCHG].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ki_coeff_low, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_KI_COEFF_MED_DISCHG, ki_coeff_med, &val);
	rc = fg_sram_write(chip,
			chip->sp[FG_SRAM_KI_COEFF_MED_DISCHG].addr_word,
			chip->sp[FG_SRAM_KI_COEFF_MED_DISCHG].addr_byte, &val,
			chip->sp[FG_SRAM_KI_COEFF_MED_DISCHG].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ki_coeff_med, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_KI_COEFF_HI_DISCHG, ki_coeff_hi, &val);
	rc = fg_sram_write(chip,
			chip->sp[FG_SRAM_KI_COEFF_HI_DISCHG].addr_word,
			chip->sp[FG_SRAM_KI_COEFF_HI_DISCHG].addr_byte, &val,
			chip->sp[FG_SRAM_KI_COEFF_HI_DISCHG].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ki_coeff_hi, rc=%d\n", rc);
		return rc;
	}

	fg_dbg(chip, FG_LGE, "Wrote ki_coeff: [%s: soc=%d, temp=%s] low=%d, med=%d, hi=%d\n",
		(chip->charge_status == POWER_SUPPLY_STATUS_DISCHARGING) ? "DISC": "CHG",
		msoc, (chip->esr_flt_sts == LOW_TEMP) ? "low": "room or relex",
		ki_coeff_low, ki_coeff_med, ki_coeff_hi);

	return 0;
}

int override_fg_parse_ki_coefficient(struct fg_chip *chip, struct device_node *node)
{
	int rc, i, temp;

	rc = of_property_read_u32(node, "qcom,ki-coeff-full-dischg", &temp);
	if (!rc)
		chip->dt.ki_coeff_full_soc_dischg = temp;

	rc = fg_parse_dt_property_u32_array(node, "qcom,ki-coeff-soc-dischg",
		chip->dt.ki_coeff_soc, KI_COEFF_SOC_LEVELS);
	if (rc < 0)
		return rc;

	rc = fg_parse_dt_property_u32_array(node, "qcom,ki-coeff-low-dischg",
		chip->dt.ki_coeff_low_dischg, KI_COEFF_SOC_LEVELS);
	if (rc < 0)
		return rc;

	rc = fg_parse_dt_property_u32_array(node, "qcom,ki-coeff-med-dischg",
		chip->dt.ki_coeff_med_dischg, KI_COEFF_SOC_LEVELS);
	if (rc < 0)
		return rc;

	rc = fg_parse_dt_property_u32_array(node, "qcom,ki-coeff-hi-dischg",
		chip->dt.ki_coeff_hi_dischg, KI_COEFF_SOC_LEVELS);
	if (rc < 0)
		return rc;

	for (i = 0; i < KI_COEFF_SOC_LEVELS; i++) {
		if (chip->dt.ki_coeff_soc[i] < 0 ||
			chip->dt.ki_coeff_soc[i] > FULL_CAPACITY) {
			pr_err("Error in ki_coeff_soc_dischg values\n");
			return -EINVAL;
		}

		if (chip->dt.ki_coeff_low_dischg[i] < 0 ||
			chip->dt.ki_coeff_low_dischg[i] > KI_COEFF_MAX) {
			pr_err("Error in ki_coeff_low_dischg values\n");
			return -EINVAL;
		}

		if (chip->dt.ki_coeff_med_dischg[i] < 0 ||
			chip->dt.ki_coeff_med_dischg[i] > KI_COEFF_MAX) {
			pr_err("Error in ki_coeff_med_dischg values\n");
			return -EINVAL;
		}

		if (chip->dt.ki_coeff_hi_dischg[i] < 0 ||
			chip->dt.ki_coeff_hi_dischg[i] > KI_COEFF_MAX) {
			pr_err("Error in ki_coeff_hi_dischg values\n");
			return -EINVAL;
		}
	}
	chip->ki_coeff_dischg_en = true;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

#define PROPERTY_CONSUMED_WITH_SUCCESS	0
#define PROPERTY_CONSUMED_WITH_FAIL	EINVAL
#define PROPERTY_BYPASS_REASON_NOENTRY	ENOENT
#define PROPERTY_BYPASS_REASON_ONEMORE	EAGAIN

static int workaround_backup_bms_property[POWER_SUPPLY_PROP_BATTERY_TYPE];

static enum power_supply_property extension_bms_appended [] = {
	POWER_SUPPLY_PROP_UPDATE_NOW,
};

static int extension_bms_get_property_pre(struct power_supply *psy,
	enum power_supply_property prop, union power_supply_propval *val) {
	int rc = PROPERTY_CONSUMED_WITH_SUCCESS;

	struct fg_chip* chip = power_supply_get_drvdata(psy);
	struct irq_desc* irq = irq_to_desc(chip->irqs[SOC_UPDATE_IRQ].irq);

	switch (prop) {
	case POWER_SUPPLY_PROP_RESISTANCE:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
	case POWER_SUPPLY_PROP_CHARGE_NOW_RAW:
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
	case POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		if (irq && !irq->depth) {
			val->intval = workaround_backup_bms_property[prop];
			pr_info("[W/A] BBP) SOC_UPDATE_IRQ enabled! (==Reading blocked!) "
				"Skip to read property(%d) and reuse %d for faster access time\n",
				prop, val->intval);
		}
		else {
			pr_debug("[W/A] BBP) Not blocked! Bypass to get prp (%d) from bms psy\n",
				prop);
			rc = -PROPERTY_BYPASS_REASON_ONEMORE;
		}
		break;

	case POWER_SUPPLY_PROP_CAPACITY :
		// Battery fake setting has top priority
		if (fake.capacity != FAKE_DISABLED)
			val->intval = fake.capacity;
		else if (rescale.result != RESCALE_INITVAL)
			val->intval = rescale.result;
		else
			rc = -PROPERTY_BYPASS_REASON_ONEMORE;
		break;

	case POWER_SUPPLY_PROP_TEMP :
		if (fake.temperature == FAKE_DISABLED)
			val->intval = calculate_battery_temperature(chip); // Use compensated temperature
		else
			val->intval = fake.temperature;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW :
		if (fake.uvoltage == FAKE_DISABLED)
			rc = -PROPERTY_BYPASS_REASON_ONEMORE;
		else
			val->intval = fake.uvoltage;
		break;

	case POWER_SUPPLY_PROP_ESR_COUNT : {
		int buf = 0;
		val->intval = !fg_get_sram_prop(chip, FG_SRAM_ESR, &buf)
			? buf : FGGEN3_INITVAL;
	}	break;

	case POWER_SUPPLY_PROP_UPDATE_NOW :
		/* Do nothing and just consume getting */
		val->intval = -1;
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = extension_battery_cycle(chip);
		break;

	default:
		rc = -PROPERTY_BYPASS_REASON_NOENTRY;
		break;
	}

	return rc;
}

static int extension_bms_get_property_post(struct power_supply *psy,
	enum power_supply_property prop, union power_supply_propval *val, int rc) {
	workaround_backup_bms_property[prop] = val->intval;
	return rc;
}

static int extension_bms_set_property_pre(struct power_supply *psy,
	enum power_supply_property prop, const union power_supply_propval *val) {

	int  rc = PROPERTY_CONSUMED_WITH_SUCCESS;
	int* fakeset = NULL;

	switch (prop) {
	case POWER_SUPPLY_PROP_UPDATE_NOW :
		if (val->intval)
			fggen3_snapshot_update(psy);
		break;

	case POWER_SUPPLY_PROP_TEMP :
		fakeset = &fake.temperature;
		break;
	case POWER_SUPPLY_PROP_CAPACITY :
		fakeset = &fake.capacity;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW :
		fakeset = &fake.uvoltage;
		break;

	default:
		rc = -PROPERTY_BYPASS_REASON_NOENTRY;
	}

	if (fakeset && *fakeset != val->intval) {
		struct fg_chip *chip = power_supply_get_drvdata(psy);
		*fakeset = val->intval;

		power_supply_changed(chip->batt_psy);
	}

	return rc;
}

static int extension_bms_set_property_post(struct power_supply *psy,
	enum power_supply_property prop, const union power_supply_propval *val, int rc) {
	return rc;
}

///////////////////////////////////////////////////////////////////////////////
enum power_supply_property* extension_bms_properties(void) {
	static enum power_supply_property extended_properties[ARRAY_SIZE(fg_psy_props) + ARRAY_SIZE(extension_bms_appended)];
	int size_original = ARRAY_SIZE(fg_psy_props);
	int size_appended = ARRAY_SIZE(extension_bms_appended);

	memcpy(extended_properties, fg_psy_props,
		size_original * sizeof(enum power_supply_property));
	memcpy(&extended_properties[size_original], extension_bms_appended,
		size_appended * sizeof(enum power_supply_property));

	veneer_extension_pspoverlap(fg_psy_props, size_original,
		extension_bms_appended, size_appended);

	return extended_properties;
}

size_t extension_bms_num_properties(void) {
	return ARRAY_SIZE(fg_psy_props) + ARRAY_SIZE(extension_bms_appended);
}

int extension_bms_get_property(struct power_supply *psy,
	enum power_supply_property prop, union power_supply_propval *val) {

	int rc = extension_bms_get_property_pre(psy, prop, val);
	if (rc == -PROPERTY_BYPASS_REASON_NOENTRY || rc == -PROPERTY_BYPASS_REASON_ONEMORE)
		rc = fg_psy_get_property(psy, prop, val);
	rc = extension_bms_get_property_post(psy, prop, val, rc);

	return rc;
}

int extension_bms_set_property(struct power_supply *psy,
	enum power_supply_property prop, const union power_supply_propval *val) {

	int rc = extension_bms_set_property_pre(psy, prop, val);
	if (rc == -PROPERTY_BYPASS_REASON_NOENTRY || rc == -PROPERTY_BYPASS_REASON_ONEMORE)
		rc = fg_psy_set_property(psy, prop, val);
	rc = extension_bms_set_property_post(psy, prop, val, rc);

	return rc;
}

int extension_bms_property_is_writeable(struct power_supply *psy,
	enum power_supply_property prop) {
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_UPDATE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CAPACITY_RAW:
		rc = 1;
		break;
	default:
		rc = fg_property_is_writeable(psy, prop);
		break;
	}
	return rc;
}

struct device_node* extension_get_batt_profile(struct device_node* container,
	int resistance_id) {
#define DEFAULT_BATTERY_TYPE	"LGE_NEO_TEMP_3000mah"	// mapped to "qcom,battery-type"
#define DEFAULT_BATTERY_KOHM	120			// mapped to "qcom,batt-id-kohm"
#define UNINODE_BATTERY_VALID	"battery_valid"

       /* Search with resistance_id and
	* Hold the result to an unified node(sysfs) for the fab process
	*/
	char buffer [8] = { '\0', };
	struct device_node* profile = of_batterydata_get_best_profile(container,
		resistance_id, NULL);
	snprintf(buffer, sizeof(buffer), "%d", !!profile);
	unified_nodes_store(UNINODE_BATTERY_VALID, buffer, sizeof(buffer));

	/* If no matching, set it as default */
	if (!profile) {
		profile = of_batterydata_get_best_profile(container,
			DEFAULT_BATTERY_KOHM, DEFAULT_BATTERY_TYPE);
		pr_err("Getting default battery profile(%s): %s\n", DEFAULT_BATTERY_TYPE,
			profile ? "success" : "fail");
	}

	return profile;
}
