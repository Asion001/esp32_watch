/**
 * @file safe_area.h
 * @brief Safe area constants for round display layout
 *
 * Defines padding distances from display edges to ensure content remains
 * visible on round displays without being cut off at corners.
 */

#ifndef SAFE_AREA_H
#define SAFE_AREA_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Safe area padding for round display
 *
 * These values define the minimum distance from the display edges where
 * content should be placed to avoid being cut off on round displays.
 *
 * For 410x502 AMOLED display:
 * - Top/Bottom: 40px from edge ensures content stays visible on curved edges
 * - Horizontal: 50px from left/right edge for corner clearance
 */

/** Safe area padding from top edge (pixels) */
#define SAFE_AREA_TOP 40

/** Safe area padding from bottom edge (pixels) */
#define SAFE_AREA_BOTTOM 40

/** Safe area padding from left/right edges (pixels) */
#define SAFE_AREA_HORIZONTAL 50

#ifdef __cplusplus
}
#endif

#endif // SAFE_AREA_H
