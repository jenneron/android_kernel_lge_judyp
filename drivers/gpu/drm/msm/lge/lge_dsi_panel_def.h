#ifndef _H_LGE_DSI_PANEL_DEF_
#define _H_LGE_DSI_PANEL_DEF_
#include "drs/lge_drs_mngr.h"
#include "mplus/lge_mplus.h"

#define NUM_COLOR_MODES  10
#define MAX_BIST_USAGE_TYPE 5

/**
 * enum lge_ddic_dsi_cmd_set_type = LGE DSI command set type
 * @
 */
enum lge_ddic_dsi_cmd_set_type {
	LGE_DDIC_DSI_SET_MPLUS = 0,
	LGE_DDIC_DSI_SET_IMAGE_ENHANCEMENT,
	LGE_DDIC_DSI_SET_BIST_ON,
	LGE_DDIC_DSI_SET_BIST_OFF,
	LGE_DDIC_DSI_SET_WB_DEFAULT,
	LGE_DDIC_DSI_SET_CM_DCI_P3,
	LGE_DDIC_DSI_SET_CM_SRGB,
	LGE_DDIC_DSI_SET_CM_ADOBE,
	LGE_DDIC_DSI_SET_CM_NATIVE,
	LGE_DDIC_DSI_DISP_CTRL_COMMAND_1,
	LGE_DDIC_DSI_DISP_CTRL_COMMAND_2,
	LGE_DDIC_DSI_DISP_DG_COMMAND_DUMMY,
	LGE_DDIC_DSI_BC_DIMMING,
	LGE_DDIC_DSI_BC_DEFAULT_DIMMING,
	LGE_DDIC_DSI_SET_VR_MODE_PRE_ON,
	LGE_DDIC_DSI_SET_VR_MODE_POST_ON,
	LGE_DDIC_DSI_SET_VR_MODE_PRE_OFF,
	LGE_DDIC_DSI_SET_VR_MODE_POST_OFF,
	LGE_DDIC_DSI_SET_LP1,
	LGE_DDIC_DSI_SET_LP2,
	LGE_DDIC_DSI_SET_NOLP,
	LGE_DDIC_DSI_SET_SHARPNESS,
	LGE_DDIC_DSI_DETECT_VERT_LINE_RESTORE,
	LGE_DDIC_DSI_DETECT_BLACK_VERT_LINE,
	LGE_DDIC_DSI_DETECT_WHITE_VERT_LINE,
	LGE_DDIC_DSI_MEM_ERR_DETECT,
	LGE_DDIC_DSI_ESD_DETECT,
	LGE_DDIC_DSI_LINE_DEFECT_DETECT,
	LGE_DDIC_DSI_MPLUS_MODE_SET1,
	LGE_DDIC_DSI_MPLUS_MODE_SET2,
	LGE_DDIC_DSI_MPLUS_MODE_SET3,
	LGE_DDIC_DSI_DISP_SC_COMMAND_DUMMY,
	LGE_DDIC_DSI_CMD_SET_MAX
};

struct lge_ddic_dsi_panel_cmd_set {
	enum lge_ddic_dsi_cmd_set_type type;
	enum dsi_cmd_set_state state;
	u32 count;
	u32 ctrl_idx;
	struct dsi_cmd_desc *cmds;
};

enum lge_panel_lp_state {
	LGE_PANEL_NOLP = 0,
	LGE_PANEL_LP1,
	LGE_PANEL_LP2,
	LGE_PANEL_OFF,
	LGE_PANEL_STATE_MAX,
};

struct lge_panel_pin_seq {
	int gpio;
	u32 level;
	u32 sleep_ms;
};

struct lge_blmap {
	int size;
	int *map;
};

struct lge_rect {
	int x;
	int y;
	int w;
	int h;
};

struct backup_info {
	char owner[32];
	enum dsi_cmd_set_type type;
	char name[32];
	u8 reg;
	int nth_cmds; /* #N command, if duplicated */
	bool is_backup;
};

struct dsi_panel;

struct lge_ddic_ops {
	/* For DISPLAY_AMBIENT */
	int (*store_aod_area)(struct dsi_panel *panel, struct lge_rect rect);
	int (*prepare_aod_cmds)(struct dsi_panel *panel, struct dsi_cmd_desc *cmds, int cmds_count);
	/* For DISPLAY_COLOR_MANAGER */
	void (*lge_bc_dim_set)(struct dsi_panel *panel, u8 bc_dim_en, u8 bc_dim_f_cnt);
	int (*lge_set_therm_dim)(struct dsi_panel *panel, int input);
	void (*lge_get_brightness_dim)(struct dsi_panel *panel);
	void (*lge_set_brightness_dim)(struct dsi_panel *panel, int input);
	void (*lge_set_custom_rgb)(struct dsi_panel *panel, bool send_cmd);
	void (*lge_set_rgb_tune)(struct dsi_panel *panel, bool send_cmd);
	void (*lge_set_screen_tune)(struct dsi_panel *panel);
	void (*lge_display_control_store)(struct dsi_panel *panel, bool send_cmd);
	void (*lge_set_screen_mode)(struct dsi_panel *panel, bool send_cmd);
	void (*lge_send_screen_mode_cmd)(struct dsi_panel *panel, int index);
	void (*lge_set_hdr_hbm_lut)(struct dsi_panel *panel, int input);
	void (*lge_set_hdr_mode)(struct dsi_panel *panel, int input);
	void (*lge_set_acl_mode)(struct dsi_panel *panel, int input);
	void (*lge_set_video_enhancement)(struct dsi_panel *panel, int input);
	int(*vr_enable)(struct dsi_panel *panel);
	int(*vr_disable)(struct dsi_panel *panel);

	int (*bist_ctrl)(struct dsi_panel *panel, bool enable);
	int (*release_bist)(struct dsi_panel *panel);
	int (*hdr_mode_set)(struct dsi_panel *panel, int input);
	void (*sharpness_set)(struct dsi_panel *panel, int mode);

	/*For MPLUS */
	/* Hidden Menu Mplus Set */
	enum lge_mplus_mode (*mplus_hd_get)(struct dsi_panel *panel);
	void (*mplus_hd_set)(struct dsi_panel *panel, int mode);
	 /* High Luminance Mplus Set */
	int (*hl_mode_get)(struct dsi_panel *panel);
	void (*hl_mode_set)(struct dsi_panel *panel, int mode);
	/* Max Brightness Mplus Set */
	enum lge_mplus_mode (*mplus_max_get)(struct dsi_panel *panel);
	void (*mplus_max_set)(struct dsi_panel *panel, int mode);
	/* Advanced Mplus Set */
	enum lge_mplus_mode (*mplus_mode_get)(struct dsi_panel *panel);
	void (*mplus_mode_set)(struct dsi_panel *panel, int mp_mode);
	/* Normal Mplus Set */
	enum lge_mplus_mode (*image_enhance_get)(struct dsi_panel *panel);
	void (*image_enhance_set)(struct dsi_panel *panel, int mode);
	/* Brightness Map Match */
	enum lge_blmap_type (*mp_blmap)(struct dsi_panel *panel);
	enum lge_blmap_type (*mp_blmap_sub)(struct dsi_panel *panel, enum lge_mplus_mode mp_mode);

	int (*get_current_res)(struct dsi_panel *panel);
	void (*get_support_res)(int idx, void* input);
	struct backup_info* (*get_reg_backup_list)(int *cnt);
	int (*set_pps_cmds)(struct dsi_panel *panel, enum dsi_cmd_set_type type);
	int (*unset_pps_cmds)(struct dsi_panel *panel, enum dsi_cmd_set_type type);

	/* For DISPLAY_FACTORY */
	void (*lge_check_vert_black_line)(struct dsi_panel *panel);
	void (*lge_check_vert_white_line)(struct dsi_panel *panel);
	void (*lge_check_vert_line_restore)(struct dsi_panel *panel);

	/* For DISPLAY_ERR_DETECT */
	void (*err_detect_work)(struct work_struct *work);
	irqreturn_t (*err_detect_irq_handler)(int irq, void *data);
	int (*set_err_detect_mask)(struct dsi_panel *panel);
};

struct lge_dsi_color_manager_mode_entry {
	u32 color_manager_mode;
	u32 color_manager_status;
};

struct lge_dsi_panel {
	int *pins;
	int pins_num;

	struct lge_panel_pin_seq *panel_on_seq;
	struct lge_panel_pin_seq *panel_off_seq;

	bool use_labibb;
	bool reset_after_ddvd;
	bool is_incell;
	bool use_panel_reset_low_before_lp11;

	enum lge_panel_lp_state lp_state;
	enum lge_panel_lp_state panel_state;

	struct lge_blmap *blmap_list;
	int blmap_list_size;
	bool dcs_brightness_be;
	bool use_bist;
	bool update_pps_in_lp;
	bool panel_dead;
	bool panel_dead_pending;
	struct delayed_work panel_dead_work;
	int pps_orig;
	int bist_on;
	const char *bist_usage_type[MAX_BIST_USAGE_TYPE];
	struct mutex bist_lock;
	bool use_drs_mngr;
	bool scaler_trigger_after_res_switched;
	struct lge_drs_mngr drs_mngr;
	bool use_ddic_reg_backup;
	bool ddic_reg_backup_complete;
	bool is_sent_bc_dim_set;
	int hdr_mode;
	int mfts_auto_touch;

	/* For DISPLAY_FACTORY */
	int use_line_detect;

	/*For MPLUS */
	bool use_mplus;
	bool use_hl_mode;
	int mplus_dim_cnt;
	int mplus_dim_delay;
	enum lge_mplus_mode mplus_hd; /* Hidden Menu Mplus Set */
	int hl_mode; /* High Luminance Mplus Set */
	enum lge_mplus_mode mp_max; /* Max Brightness Mplus Set */
	enum lge_mplus_mode adv_mp_mode; /* Advanced Mplus Set */
	enum lge_mplus_mode mp_mode; /* Normal Mplus Set */
	enum lge_mplus_mode cur_mp_mode;
	int screen_tune_status;
	int sc_sat_step;
	int sc_hue_step;
	int sc_sha_step;
	int color_filter;
	bool sharpness_control;

#if IS_ENABLED(CONFIG_LGE_DISPLAY_USE_FSC)
	bool use_u2_fsc;
	int fsc_old;
	int fsc_req;
	int fsc_u3;
	int fsc_u2;
#endif

	/* For DISPLAY_COLOR_MANAGER */
	bool use_color_manager;
	u8 dgc_status;
	int color_manager_status;
	int color_manager_mode;
	bool is_backup;
	int screen_mode;
	struct dsi_panel_cmd_set d65_default_cmds;
	int cm_preset_step;
	int cm_red_step;
	int cm_green_step;
	int cm_blue_step;
	struct lge_dsi_color_manager_mode_entry color_manager_table[NUM_COLOR_MODES];
	u32 color_manager_table_len;
	bool color_manager_default_status;
	struct dsi_panel_cmd_set dg_preset_cmds;
	int sharpness;
	int video_enhancement;
	int hdr_hbm_lut;
	int ve_mode;
	int hbm_mode;
	int acl_mode;
	bool is_cm_reg_backup;
	u8 bc_dim_en;
	u8 bc_dim_f_cnt;
	struct delayed_work bc_dim_work;
	bool use_bc_dimming_work;
	int vr_low_persist_enable;
	int vr_status;

	atomic_t backup_state;
	struct work_struct backup_work;
	struct lge_ddic_ops *ddic_ops;

	/* FOR DISPLAY BACKLIGT CONTROL */
	bool allow_bl_update;
	int bl_lvl_unset;
	int bl_lvl_recovery_unset;

	/* For DISPLAY_AMBIENT */
	bool use_ambient;
	bool allow_bl_update_ex;
	int bl_ex_lvl_unset;
	struct backlight_device *bl_ex_device;
	struct lge_rect aod_area;
	struct mutex pa_changed_lock;
	bool partial_area_vertical_changed;
	bool partial_area_horizontal_changed;
	bool partial_area_height_changed;

	u32 aod_interface;

	/* For DISPLAY_ERR_DETECT */
	bool use_panel_err_detect;
	bool err_detect_crash_enabled;
	bool err_detect_irq_enabled;
	bool is_first_err_mask;
	int err_detect_gpio;
	int err_detect_result;
	int err_detect_mask;
	struct workqueue_struct *err_detect_int_workq;
	struct delayed_work err_detect_int_work;

	struct lge_ddic_dsi_panel_cmd_set lge_cmd_sets[LGE_DDIC_DSI_CMD_SET_MAX];
};

#endif //_H_LGE_DSI_PANEL_DEF_
