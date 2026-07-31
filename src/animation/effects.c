#include "effects.h"

#include "../digit_font.h"

#define PROGRESS_SCALE 1000
#define FIRST_PHASE_END 780


static int32_t clamp_int32(
    int32_t value,
    int32_t minimum,
    int32_t maximum
) {
  if (value < minimum) {
    return minimum;
  }

  if (value > maximum) {
    return maximum;
  }

  return value;
}


static int32_t normalize_progress(
    AnimationProgress progress
) {
  return clamp_int32(
      ((int32_t)progress * PROGRESS_SCALE)
          / ANIMATION_NORMALIZED_MAX,
      0,
      PROGRESS_SCALE
  );
}


/*
 * Cubic Ease-Out im Bereich 0 bis 1000.
 */
static int32_t ease_out_cubic(
    int32_t progress
) {
  progress = clamp_int32(
      progress,
      0,
      PROGRESS_SCALE
  );

  const int32_t inverse =
      PROGRESS_SCALE - progress;

  const int32_t inverse_cubed =
      inverse * inverse * inverse;

  return PROGRESS_SCALE
      - inverse_cubed
          / (
            PROGRESS_SCALE
            * PROGRESS_SCALE
          );
}


/*
 * Erzeugt für jede horizontale Pixelzeile einen
 * leicht versetzten lokalen Animationsfortschritt.
 *
 * Die oberste Zeile beginnt bei global 0.
 * Die unterste Zeile beginnt nach max_delay.
 *
 * Alle Zeilen haben trotzdem dieselbe lokale Laufzeit:
 *
 * obere Zeile:
 *   global 0 ... active_duration
 *
 * untere Zeile:
 *   global max_delay ... 1000
 */
static int32_t row_wave_progress(
    int32_t global_progress,
    uint8_t pixel_row,
    uint8_t wave_delay_percent
) {
  if (
      wave_delay_percent == 0
      || PPF_DIGIT_HEIGHT <= 1
  ) {
    return global_progress;
  }

  const int32_t max_delay =
      ((int32_t)wave_delay_percent
          * PROGRESS_SCALE)
      / 100;

  const int32_t row_delay =
      max_delay
      * pixel_row
      / (PPF_DIGIT_HEIGHT - 1);

  const int32_t active_duration =
      PROGRESS_SCALE - max_delay;

  if (active_duration <= 0) {
    return global_progress;
  }

  const int32_t delayed_progress =
      global_progress - row_delay;

  if (delayed_progress <= 0) {
    return 0;
  }

  return clamp_int32(
      delayed_progress
          * PROGRESS_SCALE
          / active_duration,
      0,
      PROGRESS_SCALE
  );
}


/*
 * Slide mit leichtem Überschwingen und Rückfederung.
 *
 * Der Fortschritt wird bereits normalisiert im
 * Bereich 0 bis 1000 übergeben.
 */
static int16_t slide_bounce_offset(
    int32_t normalized_progress,
    int16_t travel_distance,
    uint8_t bounce_distance
) {
  normalized_progress = clamp_int32(
      normalized_progress,
      0,
      PROGRESS_SCALE
  );

  /*
   * Phase 1:
   * Von der Startposition über das Ziel hinausschießen.
   */
  if (
      normalized_progress
          <= FIRST_PHASE_END
  ) {
    const int32_t local_progress =
        normalized_progress
        * PROGRESS_SCALE
        / FIRST_PHASE_END;

    const int32_t eased =
        ease_out_cubic(local_progress);

    const int32_t overshoot_target =
        travel_distance
        + bounce_distance;

    return (int16_t)(
        overshoot_target
        * eased
        / PROGRESS_SCALE
    );
  }

  /*
   * Phase 2:
   * Vom Overshoot weich zum endgültigen Ziel zurück.
   */
  const int32_t local_progress =
      (
        normalized_progress
        - FIRST_PHASE_END
      )
      * PROGRESS_SCALE
      / (
        PROGRESS_SCALE
        - FIRST_PHASE_END
      );

  const int32_t eased =
      ease_out_cubic(local_progress);

  return (int16_t)(
      travel_distance
      + bounce_distance
      - (
          bounce_distance
          * eased
          / PROGRESS_SCALE
        )
  );
}


int16_t ppf_effect_calculate_row_offset(
    const AppSettings *settings,
    AnimationProgress progress,
    uint8_t pixel_row,
    int16_t travel_distance
) {
  if (!settings) {
    return 0;
  }

  const int32_t normalized =
      normalize_progress(progress);

  switch (
      (PpfAnimationEffect)
          settings->animation_effect
  ) {
    case PPF_EFFECT_ROW_WAVE: {
      const int32_t wave_progress =
          row_wave_progress(
              normalized,
              pixel_row,
              app_settings_wave_delay_percent()
          );

      return slide_bounce_offset(
          wave_progress,
          travel_distance,
          app_settings_bounce_distance_px()
      );
    }

    case PPF_EFFECT_SLIDE_BOUNCE:
      return slide_bounce_offset(
          normalized,
          travel_distance,
          app_settings_bounce_distance_px()
      );

    case PPF_EFFECT_NONE:
    default:
      return 0;
  }
}
