/**
 * @file element_types.h
 * @brief Element type identifiers used by the UI protocol and renderer.
 */
#ifndef ELEMENT_TYPES_H
#define ELEMENT_TYPES_H

/** Unified element type enum for all display protocol modules. */
typedef enum {
  /** Simple text label. */
  ELEMENT_TEXT = 0,
  /** Menu container with selectable items. */
  ELEMENT_MENU = 1,
  /** Progress bar displaying 0-100%. */
  ELEMENT_PROGRESS = 2,
  /** Status text (e.g., connection state). */
  ELEMENT_STATUS_TEXT = 5,
  /** Boolean toggle/switch. */
  ELEMENT_TOGGLE = 6,
  /** Radio button group (single selection). */
  ELEMENT_RADIO_GROUP = 7,
  /** Inline numeric editor. */
  ELEMENT_NUMBER_EDIT = 8,
  /** Scrollable list of entries. */
  ELEMENT_LIST_VIEW = 9,
  /** Full-screen container/root. */
  ELEMENT_SCREEN = 10,
  /** Standalone numeric value display. */
  ELEMENT_NUMBER = 11,
  /** Barrel selector (indexed choice w/ edit). */
  ELEMENT_BARREL = 12,
  /** Trigger (hook-based version bump). */
  ELEMENT_TRIGGER = 14
} element_type_t;

#endif  // ELEMENT_TYPES_H
