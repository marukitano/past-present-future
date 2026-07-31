#include "effects.h"

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


/*
 * Cubic Ease-Out, intern im Bereich 0 bis 1000.
 */
static int32_t ease_out_cubic(int32_t progress) {
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
          / (PROGRESS_SCALE * PROGRESS_SCALE);
}


static int16_t slide_bounce_offset(
    AnimationProgress progress,
    int16_t travel_distance,
    uint8_t bounce_distance
) {
  int32_t normalized =
      ((int32_t)progress * PROGRESS_SCALE)
      / ANIMATION_NORMALIZED_MAX;

  normalized = clamp_int32(
      normalized,
      0,
      PROGRESS_SCALE
  );

  /*
   * Phase 1:
   * Von 0 bis etwas über das Ziel hinausschießen.
   */
  if (normalized <= FIRST_PHASE_END) {
    const int32_t local_progress =
        normalized * PROGRESS_SCALE
        / FIRST_PHASE_END;

    const int32_t eased =
        ease_out_cubic(local_progress);

    const int32_t overshoot_target =
        travel_distance + bounce_distance;

    return (int16_t)(
        overshoot_target
        * eased
        / PROGRESS_SCALE
    );
  }

  /*
   * Phase 2:
   * Vom Overshoot weich zum eigentlichen Ziel zurück.
   */
  const int32_t local_progress =
      (normalized - FIRST_PHASE_END)
      * PROGRESS_SCALE
      / (PROGRESS_SCALE - FIRST_PHASE_END);

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
  /*
   * Noch nicht verwendet, aber bereits Teil der Schnittstelle.
   * Der spätere Pixelwellen-Effekt bekommt pro Pixelzeile
   * einen anderen Fortschritt.
   */
  (void)pixel_row;

  if (!settings) {
    return 0;
  }

  switch (
      (PpfAnimationEffect)settings->animation_effect
  ) {
    case PPF_EFFECT_SLIDE_BOUNCE:
      return slide_bounce_offset(
          progress,
          travel_distance,
          settings->bounce_distance_px
      );

    case PPF_EFFECT_NONE:
    default:
      return 0;
  }
}
