/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _DRM_INTERNAL_MI_H_
#define _DRM_INTERNAL_MI_H_

#include <drm/drm_connector.h>

int dsi_display_set_disp_param(struct drm_connector *connector,
				u32 param_type);
int dsi_display_get_disp_param(struct drm_connector *connector,
				u32 *param_type);
ssize_t dsi_display_write_mipi_reg(struct drm_connector *connector,
				char *buf);
ssize_t dsi_display_read_mipi_reg(struct drm_connector *connector,
				char *buf);
ssize_t dsi_display_read_oled_pmic_id(struct drm_connector *connector,
				char *buf);
ssize_t dsi_display_read_panel_info(struct drm_connector *connector,
				char *buf);
ssize_t dsi_display_read_wp_info(struct drm_connector *connector,
				char *buf);
ssize_t dsi_display_read_dynamic_fps(struct drm_connector *connector,
				char *buf);
int dsi_display_set_doze_brightness(struct drm_connector *connector,
				int doze_brightness);
ssize_t dsi_display_get_doze_brightness(struct drm_connector *connector,
				char *buf);
ssize_t dsi_display_fod_get(struct drm_connector *connector, char *buf);
int dsi_display_set_thermal_hbm_disabled(struct drm_connector *connector,
				bool thermal_hbm_disabled);
int dsi_display_get_thermal_hbm_disabled(struct drm_connector *connector,
				bool *thermal_hbm_disabled);

#endif
