#include "xiaomi_frame_stat.h"

struct frame_stat fm_stat = { .enabled = true };

ssize_t smart_fps_value_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
			g_panel->mi_cfg.smart_fps_value);
}

void frame_stat_notify(int data)
{
	struct dsi_display *display = NULL;
	struct mipi_dsi_host *host = NULL;

	if (g_panel)
		host = g_panel->host;

	if (host)
		display = container_of(host, struct dsi_display, host);

	if (!display) {
		pr_err("%s: invalid param\n", __func__);
		return;
	}

	g_panel->mi_cfg.smart_fps_value = data;

	sysfs_notify(&display->drm_conn->kdev->kobj, NULL,
			"smart_fps_value");
	pr_debug("%s: fps = %d\n", __func__,
			g_panel->mi_cfg.smart_fps_value);

	fm_stat.skip_count = 0;
	fm_stat.last_fps = data;
}

void calc_fps(u64 duration, int input_event)
{
	ktime_t current_time_us;
	u64 fps, diff_us, diff, curr_fps, idle_fps;

	if (!g_panel->mi_cfg.smart_fps_support || !fm_stat.enabled)
		return;

	if (input_event) {
		/* 0xff tells userspace that an input event restored max FPS. */
		frame_stat_notify(0xff);
		fm_stat.last_fps = g_panel->mi_cfg.smart_fps_max_framerate;
		fm_stat.skip_once = true;
		pr_debug("%s: input event restore fps\n", __func__);
		goto exit;
	}

	idle_fps = g_panel->mi_cfg.idle_fps ?
			(u64)g_panel->mi_cfg.idle_fps : IDLE_FPS;
	current_time_us = ktime_get();
	if (fm_stat.idle_status) {
		if (fm_stat.last_fps != idle_fps) {
			if (g_panel->panel_mode == DSI_OP_CMD_MODE)
				/* Video panels notify SDM directly. */
				frame_stat_notify((int)idle_fps);

			fm_stat.last_fps = idle_fps;
			pr_debug("%s: exit fps calc due to idle mode\n",
					__func__);
		}
		goto exit;
	}

	if (!fm_stat.start) {
		fm_stat.last_sampled_time_us = current_time_us;
		fm_stat.start = true;
	}
	diff_us = (u64)ktime_us_delta(current_time_us,
			fm_stat.last_sampled_time_us);

	fm_stat.frame_count++;

	if (fm_stat.last_frame_commit_time_us > 0) {
		diff = (u64)ktime_us_delta(current_time_us,
				fm_stat.last_frame_commit_time_us);
		fm_stat.last_frame_commit_time_us = current_time_us;
		if (diff > LONG_FRAME_INTERVAL) {
			fm_stat.skip_count++;
			pr_debug("%s: long frame interval[%lld ms], count[%lld]\n",
					__func__, diff / NANO_TO_MICRO,
					fm_stat.skip_count);
			if (fm_stat.skip_count > LONG_INTERVAL_FRAME_COUNT) {
				frame_stat_notify((int)idle_fps);
				fm_stat.last_fps = idle_fps;
			}
			goto exit;
		} else {
			fm_stat.skip_count = 0;
		}
	}

	if (diff_us >= FPS_PERIOD_1_SEC) {
		if (fm_stat.skip_once) {
			fm_stat.skip_once = false;
			goto exit;
		}

		/* Multiply by ten to preserve one fractional FPS digit. */
		fps = fm_stat.frame_count * FPS_PERIOD_1_SEC * 10;
		do_div(fps, diff_us);
		curr_fps = (unsigned int)fps / 10;
		if (curr_fps != fm_stat.last_fps)
			frame_stat_notify(curr_fps);

		pr_debug("%s: FPS:%d.%d max_frame=%lld(us) max_fence=%lld(us)\n",
				__func__, (unsigned int)fps / 10,
				(unsigned int)fps % 10,
				fm_stat.max_frame_duration / NANO_TO_MICRO,
				fm_stat.max_input_fence_duration / NANO_TO_MICRO);
		goto exit;
	}

	fm_stat.delta_commit_duration = duration;
	if (fm_stat.max_frame_duration < fm_stat.delta_commit_duration)
		fm_stat.max_frame_duration = fm_stat.delta_commit_duration;

	fm_stat.delta_input_duration = fm_stat.input_fence_duration;
	if (fm_stat.max_input_fence_duration < fm_stat.delta_input_duration)
		fm_stat.max_input_fence_duration = fm_stat.delta_input_duration;

	fm_stat.last_frame_commit_time_us = current_time_us;
	return;

exit:
	fm_stat.last_sampled_time_us = current_time_us;
	fm_stat.frame_count = 0;
	fm_stat.start = false;
	fm_stat.max_frame_duration = 0;
	fm_stat.max_input_fence_duration = 0;
	fm_stat.last_frame_commit_time_us = current_time_us;
}

void frame_stat_collector(u64 duration, enum stat_item item)
{
	ktime_t now = ktime_get();

	switch (item) {
	case COMMIT_START_TS:
		fm_stat.commit_start_ts = now;
		pr_debug("%s: commit start ts = %lld\n", __func__,
				fm_stat.commit_start_ts);
		break;
	case GET_INPUT_FENCE_TS:
		fm_stat.get_input_fence_ts = now;
		pr_debug("%s: input fence ts = %lld, duration = %lld\n",
				__func__, fm_stat.get_input_fence_ts, duration);
		fm_stat.input_fence_duration = duration;
		break;
	case VBLANK_TS:
		fm_stat.commit_start_ts = now;
		pr_debug("vblank ts = %lld\n", fm_stat.get_input_fence_ts);
		break;
	case RETIRE_FENCE_TS:
		fm_stat.retire_fence_ts = now;
		pr_debug("%s: retire fence ts = %lld\n", __func__,
				fm_stat.retire_fence_ts);
		break;
	case COMMIT_END_TS:
		fm_stat.commit_end_ts = now;
		if (fm_stat.input_fence_duration > duration / 10)
			pr_debug("%s: long input fence wait may miss a frame\n",
					__func__);
		calc_fps(duration, 0);
		break;
	default:
		break;
	}
}
