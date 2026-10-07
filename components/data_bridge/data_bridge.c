#include "data_bridge.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "data_bridge";

static float s_cadence_factor = 1.4f;
static float s_cadence_offset = 120.0f;
static bool s_speed_is_mph = false;

// Fallback distance tracking when treadmill doesn't report odometer
static uint32_t s_fallback_distance_dm;
static int64_t s_last_update_us;

// Last converted speed for UI
static float s_display_speed_kmh;

// Speed derived from odometer, for treadmills that never report instantaneous speed
#define DIST_WINDOW_US      5000000
#define DIST_MIN_SPAN_US    2000000
#define DIST_SAMPLES        8
static struct { int64_t t_us; uint32_t dist_m; } s_dist_samples[DIST_SAMPLES];
static uint8_t s_dist_count;
static bool s_speed_reported;

#define RUNNING_THRESHOLD_KMH 6.0f
#define MPH_TO_KMH 1.60934f

void data_bridge_init(const treadlink_config_t *config)
{
    s_cadence_factor = config->cadence_factor;
    s_cadence_offset = config->cadence_offset;
    s_speed_is_mph = config->speed_is_mph;
    s_fallback_distance_dm = 0;
    s_last_update_us = 0;
    s_display_speed_kmh = 0.0f;
    data_bridge_reset();
    ESP_LOGI(TAG, "Data bridge: factor=%.2f offset=%.1f mph=%d",
             s_cadence_factor, s_cadence_offset, s_speed_is_mph);
}

void data_bridge_update_config(const treadlink_config_t *config)
{
    s_cadence_factor = config->cadence_factor;
    s_cadence_offset = config->cadence_offset;
    s_speed_is_mph = config->speed_is_mph;
    ESP_LOGI(TAG, "Config updated: factor=%.2f offset=%.1f mph=%d",
             s_cadence_factor, s_cadence_offset, s_speed_is_mph);
}

void data_bridge_reset(void)
{
    s_dist_count = 0;
    s_speed_reported = false;
}

// Average speed in km/h over the last few seconds of odometer readings
static float speed_from_distance(uint32_t dist_m)
{
    int64_t now_us = esp_timer_get_time();

    // Odometer reset (new workout): start over
    if (s_dist_count > 0 && dist_m < s_dist_samples[s_dist_count - 1].dist_m) {
        s_dist_count = 0;
    }

    // Drop samples outside the window, keeping room for the new one
    uint8_t keep = 0;
    for (uint8_t i = 0; i < s_dist_count; i++) {
        if (now_us - s_dist_samples[i].t_us <= DIST_WINDOW_US) {
            s_dist_samples[keep++] = s_dist_samples[i];
        }
    }
    if (keep == DIST_SAMPLES) {
        for (uint8_t i = 1; i < keep; i++) s_dist_samples[i - 1] = s_dist_samples[i];
        keep--;
    }
    s_dist_samples[keep].t_us = now_us;
    s_dist_samples[keep].dist_m = dist_m;
    s_dist_count = keep + 1;

    int64_t span_us = now_us - s_dist_samples[0].t_us;
    if (span_us < DIST_MIN_SPAN_US) return 0.0f;

    float mps = (dist_m - s_dist_samples[0].dist_m) / (span_us / 1000000.0f);
    return mps * 3.6f;
}

rsc_data_t data_bridge_convert(const ftms_treadmill_data_t *ftms)
{
    rsc_data_t rsc = {0};

    // Convert speed — apply mph correction if needed
    float speed_kmh = ftms->speed_001kmh / 100.0f;
    if (s_speed_is_mph) {
        speed_kmh *= MPH_TO_KMH;
    }

    // Some treadmills report distance but leave speed at 0. Until a real
    // speed shows up, derive it from the odometer (meters, unaffected by mph).
    if (ftms->has_distance) {
        if (ftms->speed_001kmh > 0) s_speed_reported = true;
        float derived_kmh = speed_from_distance(ftms->total_distance_m);
        if (!s_speed_reported) speed_kmh = derived_kmh;
    }
    s_display_speed_kmh = speed_kmh;

    float speed_mps = speed_kmh / 3.6f;
    rsc.speed_256ths_mps = (uint16_t)(speed_mps * 256.0f);

    // Walking vs running (use corrected speed)
    rsc.is_running = (speed_kmh > RUNNING_THRESHOLD_KMH);

    // Cadence estimation from corrected speed
    if (speed_kmh > 0.1f) {
        float cadence = speed_kmh * s_cadence_factor + s_cadence_offset;
        if (cadence < 0) cadence = 0;
        if (cadence > 254) cadence = 254;
        rsc.cadence_spm = (uint8_t)cadence;
    } else {
        rsc.cadence_spm = 0;
    }

    // Stride length from speed and cadence
    if (rsc.cadence_spm > 0) {
        float stride_m = speed_mps / ((float)rsc.cadence_spm / 60.0f);
        rsc.stride_length_cm = (uint16_t)(stride_m * 100.0f);
    }

    // Distance: prefer treadmill odometer, fall back to speed integration
    if (ftms->has_distance && ftms->total_distance_m > 0) {
        rsc.total_distance_dm = ftms->total_distance_m * 10;
        // Keep fallback in sync so handoff is smooth
        s_fallback_distance_dm = rsc.total_distance_dm;
    } else {
        // Integrate speed over time
        int64_t now_us = esp_timer_get_time();
        if (speed_mps > 0.01f) {
            if (s_last_update_us > 0) {
                float dt_s = (now_us - s_last_update_us) / 1000000.0f;
                if (dt_s > 0.0f && dt_s < 5.0f) {
                    s_fallback_distance_dm += (uint32_t)(speed_mps * dt_s * 10.0f);
                }
            }
            s_last_update_us = now_us;
        } else {
            s_last_update_us = 0; // reset timer when stopped
        }
        rsc.total_distance_dm = s_fallback_distance_dm;
    }

    return rsc;
}

float data_bridge_get_display_speed_kmh(void)
{
    return s_display_speed_kmh;
}

bool data_bridge_is_mph_mode(void)
{
    return s_speed_is_mph;
}
