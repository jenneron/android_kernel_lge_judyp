/**********************************************************
 *
 *  this source expands the qnovo driver function for LGE
 *
 **********************************************************/

#define CHG_PROBATION_VOTER				"CHG_PROBATION_VOTER"

#define DC_VOLTAGE_MV_BPP           	5000000
#define QNI_DIAG_CURRENT	           	500000

#define QNI_STEP_MIN_FCC            	800000  	// 80mA
#define CHG_PROBATION_MIN_FV        	4380000 	// 4.38V
#define CHG_PROBATION_MAX_FCC       	950000  	// 950mA
#define CHG_PROBATION_ENTER_FCC     	1000000 	// 1A

enum {
	PT_E1 = 0,
	PT_E2,
	PT_E7,
	PT_MAX
};

enum {
	QPS_NOT_READY = -1,		/* before reading from device tree				*/
	QPS_DISABLED = 0,		/* qni probation is disabled by device tree		*/
	QPS_ENABLED,			/* qni probation is enabled by device tree		*/
	QPS_QNI_READY,			/* qni daemon ready 							*/
	QPS_PRE_DEFINED_WLC,	/* pre defined qni probation enabled for WLC	*/
	QPS_PRE_DEFINED_USB,	/* pre defined qni probation enabled for USB	*/
	QPS_RUNTIME_READY,		/* run-time qni probation ready					*/
	QPS_IN_RUNTIME,			/* run-time qni probation enabled				*/
	QPS_IN_CV_MODE,			/* enter cv mode								*/
	QPS_MAX
};

static char qps_str[QPS_MAX+1][20] = {
	"NOT_READY",
	"DISABLED",
	"ENABLED",
	"QNI_READY",
	"PRE_DEFINED_WLC",
	"PRE_DEFINED_USB",
	"RUNTIME_READY",
	"IN_RUNTIME",
	"IN_CV_MODE"
};

struct _extension_qnovo {
	struct qnovo *me;

	int set_fcc;
	int locked_fv;
	int locked_fcc;

	int old_error_sts2;
	bool is_qnovo_ready;
	int qni_probation_status;

	bool cancel_ptrain_restart_work;

	struct power_supply	*main_psy;
	struct power_supply	*qnovo_psy;
	struct power_supply	*wireless_psy;

	unsigned int boot_time;
	struct delayed_work	qni_debounce_work;
	struct delayed_work	rt_monitor_fcc_work;
} ext_qnovo;

struct _extension_qnovo_dt_props {
	bool	enable_qni_debounce;
	bool	enable_qni_probation;

	int		qni_step_min_fcc;
	int		qni_probation_min_fv;
	int		qni_probation_max_fcc;
	int		qni_probation_enter_fcc;
} ext_dt;

struct _qnovo_config {
	int pe_cnt;
	int val[BATTSOC+1];
	int pt_cnt[PT_MAX];
} qnovo_config;

/* set/get qni probation status(qps) */
static int set_qps(int status)
{
	int fix_status = status;
	switch (status) {
		case QPS_NOT_READY:
		case QPS_DISABLED:
		case QPS_ENABLED:
			break;

		case QPS_QNI_READY:
			if (ext_qnovo.is_qnovo_ready)
				fix_status = status;
			else if (status > QPS_DISABLED)
				fix_status = QPS_ENABLED;
			else
				fix_status = QPS_NOT_READY;
			break;

		case QPS_PRE_DEFINED_WLC:
		case QPS_PRE_DEFINED_USB:
		case QPS_RUNTIME_READY:
		case QPS_IN_RUNTIME:
		default:
			break;
	}

	if (ext_qnovo.qni_probation_status != fix_status)
		pr_info("[QNI-PROB] set_qps: %s\n", qps_str[fix_status+1]);

	ext_qnovo.qni_probation_status = fix_status;
	return fix_status;
}

static int get_qps(void)
{
	return ext_qnovo.qni_probation_status;
}

/****************************
 *   Extension of QNI Log   *
 ****************************/
static void print_pt_enable_log(int enable)
{
	if (!enable)
		pr_info("[QNI-W] PT_ENABLE=%d\n", enable);
	else if (qnovo_config.val[PE_CTRL_REG] == 0xE7)
		pr_info("[QNI-W] PT_ENABLE=%d(E7 cnt=%d/%d), STS=(0x%X, 0x%X), "
			"FV=%d, FCC=%d, vlim=%d, "
			"Phase1=%d, %d, %d, %d, Phase2=%d, %d, %d, %d, "
			"ppcnt=%d, scnt=%d\n",
				enable,
				++qnovo_config.pt_cnt[PT_E7],
				++qnovo_config.pe_cnt,
				qnovo_config.val[PE_CTRL_REG],
				qnovo_config.val[PE_CTRL2_REG],
				qnovo_config.val[FV_REQUEST]/1000,
				qnovo_config.val[FCC_REQUEST]/1000,
				qnovo_config.val[VLIM1]/1000,
				qnovo_config.val[PREST1],
				qnovo_config.val[PPULS1]/1000,
				qnovo_config.val[NREST1],
				qnovo_config.val[NPULS1],
				qnovo_config.val[PREST2],
				qnovo_config.val[PPULS2]/1000,
				qnovo_config.val[NREST2],
				qnovo_config.val[NPULS2],
				qnovo_config.val[PPCNT],
				qnovo_config.val[SCNT]
		);
	else if (qnovo_config.val[PE_CTRL_REG] == 0xE2)
		pr_info("[QNI-W] PT_ENABLE=%d(E2 cnt=%d/%d), STS=(0x%X, 0x%X), "
			"FV=%d, FCC=%d, vlim=%d, Phase2=%d, %d, %d, %d\n",
				enable,
				++qnovo_config.pt_cnt[PT_E2],
				++qnovo_config.pe_cnt,
				qnovo_config.val[PE_CTRL_REG],
				qnovo_config.val[PE_CTRL2_REG],
				qnovo_config.val[FV_REQUEST]/1000,
				qnovo_config.val[FCC_REQUEST]/1000,
				qnovo_config.val[VLIM2]/1000,
				qnovo_config.val[PREST2],
				qnovo_config.val[PPULS2]/1000,
				qnovo_config.val[NREST2],
				qnovo_config.val[NPULS2]
		);
	else if (qnovo_config.val[PE_CTRL_REG] == 0xE1)
		pr_info("[QNI-W] PT_ENABLE=%d(E1 cnt=%d/%d), STS=(0x%X, 0x%X), "
			"FV=%d, FCC=%d, vlim=%d, Phase1=%d, %d, %d, %d, ppcnt=%d\n",
				enable,
				++qnovo_config.pt_cnt[PT_E1],
				++qnovo_config.pe_cnt,
				qnovo_config.val[PE_CTRL_REG],
				qnovo_config.val[PE_CTRL2_REG],
				qnovo_config.val[FV_REQUEST]/1000,
				qnovo_config.val[FCC_REQUEST]/1000,
				qnovo_config.val[VLIM1]/1000,
				qnovo_config.val[PREST1],
				qnovo_config.val[PPULS1]/1000,
				qnovo_config.val[NREST1],
				qnovo_config.val[NPULS1],
				qnovo_config.val[PPCNT]
		);
}

static void print_cv_mode_log(struct qnovo *chip)
{
	int rc=-1;
	union power_supply_propval pval = {0, };

	if (!is_batt_available(chip))
		return;

	rc = power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
	if (rc < 0) {
		pr_err("Couldn't read battery prop rc = %d\n", rc);
		pval.intval = 4400000;
	}

	if (pval.intval > qnovo_config.val[FV_REQUEST] &&
		qnovo_config.val[PE_CTRL2_REG] == 0x0 &&
		qnovo_config.val[PT_ENABLE] == 0x0 &&
		(qnovo_config.val[ERR_STS2_REG] == 0x0 ||
			qnovo_config.val[ERR_STS2_REG] == 0x10))
		pr_info("[QNI-W] Enter CV Mode: PE_CTRL2=0x%X, FV=%d, FCC=%d\n",
			qnovo_config.val[PE_CTRL2_REG],
			qnovo_config.val[FV_REQUEST]/1000,
			qnovo_config.val[FCC_REQUEST]/1000
		);
}

static void print_pt_result_log(void)
{
	pr_info("[QNI-R] PT=0x%X, ERR=0x%X, TIME=%d, VLIM=%d(diff=%d), "
			"Phase1=%d, %d, Phase2=%d, %d, %d, vmax=%d, snum=%d\n",
				qnovo_config.val[PTRAIN_STS_REG],
				qnovo_config.val[ERR_STS2_REG],
				qnovo_config.val[PTTIME]*2,
				qnovo_config.val[VLIM1]/1000,
				(qnovo_config.val[VLIM1] -
					max(qnovo_config.val[PVOLT1],
						qnovo_config.val[PVOLT2]))/1000,
				qnovo_config.val[PVOLT1]/1000,
				qnovo_config.val[PCUR1]/1000,
				qnovo_config.val[PVOLT2]/1000,
				qnovo_config.val[RVOLT2]/1000,
				qnovo_config.val[PCUR2]/1000,
				qnovo_config.val[VMAX]/1000,
				qnovo_config.val[SNUM]
	);
}

static void reset_qnovo_config(struct qnovo *chip)
{
	qnovo_config.pe_cnt = 0;
	qnovo_config.pt_cnt[0] = 0;
	qnovo_config.pt_cnt[1] = 0;
	qnovo_config.pt_cnt[2] = 0;

	qnovo_config.val[VLIM1] = -1;
	qnovo_config.val[VLIM2] = -1;

	if (get_qps() > QPS_DISABLED) {
		ext_qnovo.set_fcc = 0;
		ext_qnovo.locked_fv = 0;
		ext_qnovo.locked_fcc = 0;

		set_qps(QPS_QNI_READY);
		vote(chip->not_ok_to_qnovo_votable, CHG_PROBATION_VOTER, false, 0);
		pr_info("[QNI-PROB] initialize qni probation.\n");
	}
}

static void set_qnovo_config(struct qnovo *chip, int id, int val, bool is_store)
{
	if (id > BATTSOC) {
		pr_info("[QNI-INFO] Couldn't set id=%d, val=%d\n", id, val);
	}
	else if (id < 0) {
		reset_qnovo_config(chip);
	}
	else {
		if (!ext_qnovo.is_qnovo_ready) {
			ext_qnovo.is_qnovo_ready = true;

			if (get_qps() == QPS_ENABLED) {
				set_qps(QPS_QNI_READY);
				vote(chip->not_ok_to_qnovo_votable,
						CHG_PROBATION_VOTER, false, 0);
				pr_info("[QNI-PROB] ready for qni daemon.\n");
			}
		}

		qnovo_config.val[id] = val;
		if (is_store) {
			switch (id) {
				case QNOVO_ENABLE:
					pr_info("[QNI-W] QNOVO_ENABLE=%d\n", val);
					break;
				case PT_ENABLE:
					print_pt_enable_log(val);
					break;
				case FV_REQUEST:
					print_cv_mode_log(chip);
					break;
			}
		} else {
			switch (id) {
				case OK_TO_QNOVO:
					pr_info("[QNI-R] OK_TO_QNOVO=%d\n", val);
					break;
				case SNUM:
					print_pt_result_log();
					break;
			}
		}
	}
}

/**********************************************************
 *   Extenstion of QNI function - Runtime QNI Probation   *
 **********************************************************/

static bool is_wireless_available(struct qnovo *chip)
{
	if (!ext_qnovo.wireless_psy)
		ext_qnovo.wireless_psy = power_supply_get_by_name("wireless");

	if (!ext_qnovo.wireless_psy)
		return false;

	return true;
}

static bool is_main_available(struct qnovo *chip)
{
	if (!ext_qnovo.main_psy)
		ext_qnovo.main_psy = power_supply_get_by_name("main");

	if (!ext_qnovo.main_psy)
		return false;

	return true;
}

static u8 get_error_sts2(struct qnovo *chip)
{
	u8 val = 0xff;
	int rc = 0;

	rc = qnovo_read(chip, QNOVO_ERROR_STS2, &val, 1);
	if (rc < 0) {
		pr_err("Couldn't read error sts rc = %d\n", rc);
		return 0xff;
	}

	return val;
}

#define QNI_DEBOUNCE_LIMIT_TIME		600000
#define QNI_DEBOUNCE_LIMIT_SOC		(30*255/100)
#define QNI_DEBOUNCE_LIMIT_ESR		220000
#define QNI_DEBOUNCE_LIMIT_THERM	200
static bool is_needed_qni_debounce(struct qnovo *chip)
{
	union power_supply_propval pval = {0};
	extern bool unified_bootmode_chargerlogo(void);

	bool is_chargerlogo = unified_bootmode_chargerlogo();
	unsigned int time_after_boot = jiffies_to_msecs(jiffies-ext_qnovo.boot_time);
	int msoc = 0, esr = 0, batt_vts = 0;

	if (is_fg_available(chip)) {
		power_supply_get_property(chip->bms_psy,
					POWER_SUPPLY_PROP_CAPACITY_RAW, &pval);
		msoc = pval.intval;

		power_supply_get_property(chip->bms_psy,
					POWER_SUPPLY_PROP_ESR_COUNT, &pval);
		esr = pval.intval;

		power_supply_get_property(chip->bms_psy,
					POWER_SUPPLY_PROP_TEMP, &pval);
		batt_vts = pval.intval;
	}

	pr_info("[QNI-INFO] qni debounce condition: chargerlogo=%d, "
			"time_after_boot=%u, msoc=%d(x10), esr=%d, therm=%d\n",
			is_chargerlogo, time_after_boot, msoc*1000/255, esr, batt_vts);

	if (is_chargerlogo &&
		time_after_boot < 600000 && msoc < (30*255)/100)
		return true;

	return false;
}

static int enter_qni_probation(struct qnovo *chip)
{
	union power_supply_propval pval = {0};

	if (!(get_qps() == QPS_PRE_DEFINED_WLC ||
	      get_qps() == QPS_PRE_DEFINED_USB || get_qps() == QPS_IN_RUNTIME)) {
		pr_info("[QNI-PROB] >>> enter qni probation ERROR: sts=%d, qps=%s\n",
			get_client_vote(chip->not_ok_to_qnovo_votable, CHG_PROBATION_VOTER),
			qps_str[get_qps()+1]);
		return -EINVAL;
	}

	vote(chip->not_ok_to_qnovo_votable, CHG_PROBATION_VOTER, true, 0);

	if (is_fg_available(chip))
		power_supply_set_property(chip->bms_psy,
			POWER_SUPPLY_PROP_CHARGE_QNOVO_ENABLE, &pval);

	pr_info("[QNI-PROB] >>> enter qni probation OK!!\n");
	return 0;
}

static int release_qni_probation(struct qnovo *chip)
{
	if (!is_usb_available(chip) || !is_dc_available(chip))
		return -EINVAL;

	/* if both usb and dc isn't connected = discharging.
		it releases run-time rt probation ready flag */
	if (!chip->usb_present && !chip->dc_present) {
		if (get_qps() == QPS_QNI_READY &&
			!get_client_vote(chip->not_ok_to_qnovo_votable,
					CHG_PROBATION_VOTER))
			return 0;

		pr_info("[QNI-PROB] no input source, force set qni ready state.\n");
		set_qps(QPS_QNI_READY);
		goto release;
	} else if (get_qps() == QPS_IN_RUNTIME) {
		pr_info("[QNI-PROB] release run-time qni probation.\n");
		set_qps(QPS_RUNTIME_READY);
		goto release;
	} else if (get_qps() == QPS_IN_CV_MODE) {
		pr_info("[QNI-PROB] release qni probation by cv stage\n");
		set_qps(QPS_QNI_READY);
		goto release;
	}

	return 0;

release:
	vote(chip->not_ok_to_qnovo_votable, CHG_PROBATION_VOTER, false, 0);
	return 0;
}

static int monitor_fcc(struct qnovo *chip)
{
	u8 val = 0;
	int rc = -1;
	union power_supply_propval pval = {0, };

	if (!is_batt_available(ext_qnovo.me))
		return -EINVAL;

	rc = qnovo_read(chip, QNOVO_PE_CTRL, &val, 1);
	if (val == 0xE2) {
		pr_info("[QNI-PROB] skip run-time monitor_fcc "
				"due to qni diag stage.\n");
		return -EINVAL;
	}

	/* confirm fcc with get_effective_result */
	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &pval);

	if (pval.intval == ext_qnovo.set_fcc)
		return -EINVAL;

	pr_info("[QNI-PROB] run-time monitor_fcc: now=%d, old=%d, locked=%d\n",
			pval.intval/1000, ext_qnovo.set_fcc/1000,
			ext_qnovo.locked_fcc/1000);

	ext_qnovo.set_fcc = pval.intval;

	return 0;
}

static int rt_qni_probation(struct qnovo *chip)
{
	int rc = -1;
	union power_supply_propval pval = {0};

	if (!is_main_available(ext_qnovo.me))
		return -EINVAL;

	if (ext_qnovo.set_fcc > ext_dt.qni_probation_enter_fcc) {
		release_qni_probation(chip);
		return 0;
	}

	/* current */
	pval.intval = min(ext_qnovo.set_fcc, ext_dt.qni_probation_max_fcc);
	if (pval.intval != ext_qnovo.set_fcc ) {
		rc = power_supply_set_property(ext_qnovo.main_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
		if (rc < 0) {
			pr_err("Couldn't set prop qnovo_fcc rc = %d\n", rc);
			return -EINVAL;
		}
	}
	ext_qnovo.locked_fcc = pval.intval;

	/* voltage */
	pval.intval = max(qnovo_config.val[VLIM1], ext_dt.qni_probation_min_fv);
	pval.intval = min(chip->fv_uV_request, pval.intval);
	if (pval.intval > 0 && (ext_qnovo.locked_fv != pval.intval)) {
		rc = power_supply_set_property(ext_qnovo.me->batt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_QNOVO, &pval);
		if (rc < 0) {
			pr_err("Couldn't set prop qnovo_fv rc = %d\n", rc);
			return -EINVAL;
		}
	}
	ext_qnovo.locked_fv = pval.intval;

	pr_info("[QNI-PROB] set run-time qni probation: "
			"[fcc] now:%d, max:%d, sel:%d "
			"[fv] now:%d, min:%d, qnovo:%d, sel:%d\n",
				ext_qnovo.set_fcc/1000,
				ext_dt.qni_probation_max_fcc/1000,
				ext_qnovo.locked_fcc/1000,
				qnovo_config.val[VLIM1]/1000,
				ext_dt.qni_probation_min_fv/1000,
				chip->fv_uV_request/1000,
				ext_qnovo.locked_fv/1000
	);

	if (get_qps() == QPS_RUNTIME_READY) {
		set_qps(QPS_IN_RUNTIME);
		enter_qni_probation(ext_qnovo.me);
	}

	return 0;
}

static void rt_monitor_fcc_work(struct work_struct *work)
{
	if (get_qps() == QPS_RUNTIME_READY || get_qps() == QPS_IN_RUNTIME) {
		if (!monitor_fcc(ext_qnovo.me))
			rt_qni_probation(ext_qnovo.me);

		schedule_delayed_work(
				&ext_qnovo.rt_monitor_fcc_work, msecs_to_jiffies(4000));
	}
}

static bool pred_usb_probation(struct qnovo *chip)
{
	union power_supply_propval pval = {0};
	int rc = 0;

	if(is_usb_available(chip)) {
		rc = power_supply_get_property(chip->usb_psy,
						POWER_SUPPLY_PROP_REAL_TYPE, &pval);
		if (rc < 0) {
			pr_err("Couldn't get usb prop rc=%d\n", rc);
			goto out;
		}

		if (pval.intval == POWER_SUPPLY_TYPE_USB ||
			pval.intval == POWER_SUPPLY_TYPE_UNKNOWN ||
			pval.intval == POWER_SUPPLY_TYPE_USB_FLOAT ) {
			pr_info("[QNI-PROB] enter pre-defined qni probation by "
					"SDP or FLOAT or Unknown.\n");
			return true;
		}
	}

out:
	return false;
}

static bool pred_dc_probation(struct qnovo *chip)
{
	union power_supply_propval pval = {0};
	int rc = 0;

	if (is_wireless_available(chip)) {
		rc = power_supply_get_property(ext_qnovo.wireless_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
		if (rc < 0) {
			pr_err("Couldn't get dc prop rc=%d\n", rc);
			goto out;
		}

		if (pval.intval == DC_VOLTAGE_MV_BPP ) {
			pr_info("[QNI-PROB] enter pre-defined qni probation by "
					"wireless-bpp(5V).\n");
			return true;
		}
	}

out:
	return false;
}

/* qni probation main function */
bool pred_qni_probation(struct qnovo *chip, bool is_dc)
{
	bool is_pred_probation = false;

	if (get_qps() < QPS_ENABLED)
		return false;

	/* If not charging, skip qni probation */
	if (get_error_sts2(chip))
		return false;

	/* When input is plugged, confirm qni probation */
	is_pred_probation = is_dc ?
			pred_dc_probation(chip) : pred_usb_probation(chip);

	if (is_pred_probation)
	{
		if (is_dc)
			set_qps(QPS_PRE_DEFINED_WLC);
		else
			set_qps(QPS_PRE_DEFINED_USB);

		enter_qni_probation(chip);
	}
	else {
		if (is_needed_qni_debounce(chip) && ext_dt.enable_qni_debounce) {
			vote(chip->not_ok_to_qnovo_votable, CHG_PROBATION_VOTER, true, 0);
			schedule_delayed_work(&ext_qnovo.qni_debounce_work,
					msecs_to_jiffies(600000-jiffies_to_msecs(jiffies-ext_qnovo.boot_time)));
			pr_info("[QNI-INFO] qni debounced: delay=%d\n",
					600000-jiffies_to_msecs(jiffies-ext_qnovo.boot_time));
		}
		else {
			set_qps(QPS_RUNTIME_READY);
			schedule_delayed_work(&ext_qnovo.rt_monitor_fcc_work,
				msecs_to_jiffies(4000));
			pr_info("[QNI-PROB] ready for run-time qni probation.\n");
		}
	}

	return true;
}

/**************************************************
 *   Override function of QNI original function   *
 **************************************************/
int override_qnovo_update_status(struct qnovo *chip)
{
	u8 val = 0;
	int rc = -1;
	bool hw_ok_to_qnovo;
	union power_supply_propval pval = {0};

	rc = qnovo_read(chip, QNOVO_ERROR_STS2, &val, 1);
	if (rc < 0) {
		pr_err("Couldn't read error sts rc = %d\n", rc);
		hw_ok_to_qnovo = false;
	} else {
		/*
		 * For CV mode keep qnovo enabled, userspace is expected to
		 * disable it after few runs
		 */
		hw_ok_to_qnovo = (val == ERR_CV_MODE || val == 0) ?
			true : false;

		if ((val == ERR_CV_MODE) || (val & ERR_CHARGING_DISABLED)) {
			msleep(50);
			rc = qnovo_read(chip, QNOVO_ERROR_STS2, &val, 1);
			if (rc < 0) {
				pr_err("Couldn't read error sts rc = %d\n", rc);
				hw_ok_to_qnovo = false;
				goto out;
			}
		}

		if (val & ERR_CHARGING_DISABLED) {
			/* if not qni-charging, enable ESR extration bit */
			if (is_fg_available(chip))
				power_supply_set_property(chip->bms_psy,
					POWER_SUPPLY_PROP_CHARGE_QNOVO_ENABLE,
					&pval);

			if (get_qps() > QPS_ENABLED) {
		 		release_qni_probation(chip);
			}
		}
		else {
			if (get_qps() < QPS_PRE_DEFINED_WLC)
				goto out;

			if (val == ERR_CV_MODE) {
				if (get_qps() == QPS_IN_RUNTIME
						&& ext_qnovo.locked_fcc > ext_dt.qni_step_min_fcc) {
					pr_info("[QNI-PROB] cv stage: "
							"step down fcc by 100mA (%d->%dmA)\n",
									ext_qnovo.locked_fcc/1000,
									ext_qnovo.locked_fcc/1000-100);

					pval.intval = ext_qnovo.locked_fcc - 100000;
					rc = power_supply_set_property(chip->batt_psy,
							POWER_SUPPLY_PROP_CURRENT_QNOVO, &pval);
					if (rc < 0) {
						pr_err("Couldn't set prop qnovo_fcc rc = %d\n", rc);
						return -EINVAL;
					}
					ext_qnovo.locked_fcc = pval.intval;
				}
				else if (get_qps() > QPS_PRE_DEFINED_WLC) {
						set_qps(QPS_IN_CV_MODE);
						release_qni_probation(chip);
				}
			}
			else if (val == 0) {
				if (val == ext_qnovo.old_error_sts2)
					goto out;

				if (get_qps() == QPS_RUNTIME_READY)
					schedule_delayed_work(&ext_qnovo.rt_monitor_fcc_work,
						msecs_to_jiffies(4000));
			}
		}
		ext_qnovo.old_error_sts2 = val;
	}

out:
	vote(chip->not_ok_to_qnovo_votable, HW_OK_TO_QNOVO_VOTER,
					!hw_ok_to_qnovo, 0);
	return 0;
}

int override_qnovo_disable_cb(
	struct votable *votable, void *data, int disable, const char *client)
{
	int rc;
	struct qnovo *chip = data;
	union power_supply_propval pval = {0};

	if (!is_batt_available(chip))
		return -EINVAL;

	pval.intval = !disable;
	rc = power_supply_set_property(chip->batt_psy,
				POWER_SUPPLY_PROP_CHARGE_QNOVO_ENABLE,
				&pval);
	if (rc < 0) {
		pr_err("Couldn't set prop qnovo_enable rc = %d\n", rc);
		return -EINVAL;
	}

	/*
	 * fg must be available for enable FG_AVAILABLE_VOTER
	 * won't enable it otherwise
	 */
	if (is_fg_available(chip))
		power_supply_set_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CHARGE_QNOVO_ENABLE,
				&pval);

	vote(chip->pt_dis_votable, QNOVO_OVERALL_VOTER, disable, 0);

	/* When it is disabled by CHG_PROBATION_VOTER, keep qnovo voter
			because qni probation current is controlled by qnovo voter */
	if (disable &&
		(get_qps() == QPS_IN_RUNTIME))
		pr_info("[QNI-PROB] skip qnovo voter for run-time qni probation.\n");
	else
		rc = qnovo_batt_psy_update(chip, disable);

	return rc;
}

irqreturn_t override_handle_ptrain_done(int irq, void *data)
{
	struct qnovo *chip = data;
	union power_supply_propval pval = {0};
	u8 pe = 0;
	u8 sts2 = 0;
	u8 pt_t = 0;
	static int esr_skip_count = 0;

	qnovo_read(chip, QNOVO_PE_CTRL, &pe, 1);
	qnovo_read(chip, QNOVO_ERROR_STS2, &sts2, 1);
	qnovo_read(chip, QNOVO_PTTIME_STS, &pt_t, 1);
	pr_info("[QNI-INFO] qni irq: pe=0x%X, sts2=0x%X, "
			"pt_t=%d, cnt=%d\n", pe, sts2, pt_t*2, esr_skip_count);

	// for reducing the charging time
	//    prevent from ptrain restart work frequently.
	if (sts2 == 0x0 && pt_t == 0) {
		mutex_lock(&chip->write_lock);
		ext_qnovo.cancel_ptrain_restart_work = true;
		mutex_unlock(&chip->write_lock);
	}

	override_qnovo_update_status(chip);

	/*
	 * hw resets pt_en bit once ptrain_done triggers.
	 * vote on behalf of QNI to disable it such that
	 * once QNI enables it, the votable state changes
	 * and the callback that sets it is indeed invoked
	 */
	vote(chip->pt_dis_votable, QNI_PT_VOTER, true, 0);

	// for reducing the charging time
	//    prevent from ESR measurement frequently.
	if (pt_t >= 10 || esr_skip_count > 5 ) {
		esr_skip_count = 0;
		vote(chip->pt_dis_votable, ESR_VOTER, true, 0);

		if (is_fg_available(chip)
			&& !get_client_vote(chip->disable_votable, USER_VOTER)
			&& !get_effective_result(chip->not_ok_to_qnovo_votable))
			power_supply_set_property(chip->bms_psy,
					POWER_SUPPLY_PROP_RESISTANCE, &pval);

		vote(chip->pt_dis_votable, ESR_VOTER, false, 0);
	}
	else
		esr_skip_count++;

	kobject_uevent(&chip->dev->kobj, KOBJ_CHANGE);
	return IRQ_HANDLED;
}

void override_ptrain_restart_work(struct work_struct *work)
{
	struct qnovo *chip = container_of(work,
				struct qnovo, ptrain_restart_work.work);

	u8 pt_t1 = 0, pt_t2 = 0;
	u8 pt_en = 0;
	int rc = 0;
	int delay_cnt = 21;

	rc = qnovo_read(chip, QNOVO_PTRAIN_EN, &pt_en, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read QNOVO_PTRAIN_EN rc = %d\n",
				rc);
		goto clean_up;
	}

	if (!pt_en) {
		pr_info("[QNI-INFO] pt check <<< error!! (pt_en=%u)\n", pt_en);
		rc = qnovo_masked_write(chip, QNOVO_PTRAIN_EN,
				QNOVO_PTRAIN_EN_BIT, QNOVO_PTRAIN_EN_BIT);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't enable pulse train rc=%d\n",
					rc);
			goto clean_up;
		}
		/* sleep 20ms for the pulse trains to restart and settle */
		msleep(20);
	}

	rc = qnovo_read(chip, QNOVO_PTTIME_STS, &pt_t1, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read QNOVO_PTTIME_STS rc = %d\n",
			rc);
		goto clean_up;
	}

	while (--delay_cnt) {
		if (ext_qnovo.cancel_ptrain_restart_work) {
			ext_qnovo.cancel_ptrain_restart_work = false;
			pr_info("[QNI-INFO] PT Check <<< CANCELED!!\n");
			goto clean_up;
		}
		msleep(100);
	}

	rc = qnovo_read(chip, QNOVO_PTTIME_STS, &pt_t2, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read QNOVO_PTTIME_STS rc = %d\n",
			rc);
		goto clean_up;
	}

	if (pt_t1 != pt_t2) {
		pr_info("[QNI-INFO] pt check <<< success!! (t1=%u, t2=%u)\n",
				pt_t1, pt_t2);
		goto clean_up;
	}

	pr_info("[QNI-INFO] pt check <<< error!! (t1=%u, t2=%u)\n",
			pt_t1, pt_t2);

	/* Toggle pt enable to restart pulse train */
	rc = qnovo_masked_write(chip, QNOVO_PTRAIN_EN, QNOVO_PTRAIN_EN_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't disable pulse train rc=%d\n", rc);
		goto clean_up;
	}
	msleep(1000);
	rc = qnovo_masked_write(chip, QNOVO_PTRAIN_EN, QNOVO_PTRAIN_EN_BIT,
				QNOVO_PTRAIN_EN_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't enable pulse train rc=%d\n", rc);
		goto clean_up;
	}

clean_up:
	vote(chip->awake_votable, PT_RESTART_VOTER, false, 0);
}

static void qni_debounce_work(struct work_struct *work)
{
	vote(ext_qnovo.me->not_ok_to_qnovo_votable, CHG_PROBATION_VOTER, false, 0);

	set_qps(QPS_RUNTIME_READY);
	schedule_delayed_work(&ext_qnovo.rt_monitor_fcc_work,
		msecs_to_jiffies(4000));
	pr_info("[QNI-PROB] ready for run-time qni probation.\n");
}

static int extension_qnovo_parse_dt(struct qnovo *chip)
{
	struct device_node *node = chip->dev->of_node;

	ext_dt.enable_qni_debounce = 0;
	ext_dt.enable_qni_debounce = of_property_read_bool(node,
			"lge,enable-qni-debounce");

	ext_dt.enable_qni_probation = of_property_read_bool(node,
			"lge,enable-qni-probation");
	ext_qnovo.qni_probation_status = ext_dt.enable_qni_probation ?
					QPS_ENABLED : QPS_DISABLED;

	ext_dt.qni_step_min_fcc = QNI_STEP_MIN_FCC;
	of_property_read_u32(node,
			"lge,qni-step-min-fcc",
			&ext_dt.qni_step_min_fcc);

	ext_dt.qni_probation_min_fv = CHG_PROBATION_MIN_FV;
	of_property_read_u32(node,
			"lge,qni-probation-min-fv",
			&ext_dt.qni_probation_min_fv);

	ext_dt.qni_probation_max_fcc = CHG_PROBATION_MAX_FCC;
	of_property_read_u32(node,
			"lge,qni-probation-max-fcc",
			&ext_dt.qni_probation_max_fcc);

	ext_dt.qni_probation_enter_fcc = CHG_PROBATION_ENTER_FCC;
	of_property_read_u32(node,
			"lge,qni-probation-enter-fcc",
			&ext_dt.qni_probation_enter_fcc);

	pr_info("[QNI-INFO] extension dt: debounce=%d, probation=%d, "
			"step_min=%d, max_fcc=%d, enter_fcc=%d",
				ext_dt.enable_qni_debounce,
				ext_dt.enable_qni_probation,
				ext_dt.qni_step_min_fcc,
				ext_dt.qni_probation_max_fcc,
				ext_dt.qni_probation_enter_fcc);

	return 0;
}

int qnovo_init_psy(struct qnovo *chip)
{
	int rc = 0;

	if (!ext_qnovo.me)
		ext_qnovo.me = chip;

	ext_qnovo.old_error_sts2 = -1;
	ext_qnovo.is_qnovo_ready = false;
	ext_qnovo.qni_probation_status = QPS_NOT_READY;
	ext_qnovo.boot_time = jiffies;

	extension_qnovo_parse_dt(chip);

	INIT_DELAYED_WORK(&ext_qnovo.qni_debounce_work, qni_debounce_work);
	INIT_DELAYED_WORK(&ext_qnovo.rt_monitor_fcc_work, rt_monitor_fcc_work);

	return rc;
}
