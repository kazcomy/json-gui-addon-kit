/**
 * @file status_codes.h
 * @brief Common result/status codes to avoid magic numbers in returns.
 */
#ifndef STATUS_CODES_H
#define STATUS_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result codes used by internal command handlers and helpers.
 *
 * Keep numeric values stable to preserve existing protocol mappings.
 */
typedef enum {
  RES_OK = 0,          /**< Success */
  RES_BAD_LEN = -1,    /**< Invalid payload length */
  RES_BAD_STATE = -2,  /**< Operation not allowed in current state */
  RES_UNKNOWN_ID = -3, /**< Unknown or out-of-range element/screen id */
  RES_RANGE = -4,      /**< Value out of range */
  RES_INTERNAL = -5,   /**< Internal error */
  RES_NO_SPACE = -6,   /**< Not enough capacity/space */
  RES_PARSE_FAIL = -10 /**< Parse failure */
} result_t;

#ifdef __cplusplus
}
#endif

#endif /* STATUS_CODES_H */
