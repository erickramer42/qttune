#ifndef QTTUNE_SIGNALS_H
#define QTTUNE_SIGNALS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Signal data type enumeration (append-only evolution)
 * 
 * Values below 256 are reserved for future standard types.
 * New types added at the end; existing values never change meaning.
 */
typedef enum {
    QTTUNE_SIGNAL_TYPE_UINT8   = 0,
    QTTUNE_SIGNAL_TYPE_UINT16  = 1,
    QTTUNE_SIGNAL_TYPE_UINT32  = 2,
    QTTUNE_SIGNAL_TYPE_INT8    = 3,
    QTTUNE_SIGNAL_TYPE_INT16   = 4,
    QTTUNE_SIGNAL_TYPE_INT32   = 5,
    QTTUNE_SIGNAL_TYPE_FLOAT32 = 6,
    QTTUNE_SIGNAL_TYPE_STRING  = 7,  /* Reserved for future use */
    
    /* Manufacturer-specific types start at 256 */
    QTTUNE_SIGNAL_TYPE_MANUF_START = 256
} QttuneSignalType;

/**
 * @brief POD structure defining a single signal mapping
 * 
 * Each signal maps bytes from a CAN frame to a display value using
 * linear scaling: display_value = (raw_value * scale) + offset
 * 
 * Layout constraints:
 * - sizeof(QttuneSignalDef) must be power-of-two aligned
 * - No pointers, no C++ classes, no variable-length arrays
 * - String names are fixed-size buffers (not dynamically allocated)
 */
typedef struct {
    uint16_t id;                  /* Unique signal identifier (e.g., RPM, Speed) */
    char name[32];                /* Human-readable name, null-terminated */
    QttuneSignalType type;        /* Data type of the decoded value */
    
    /* Byte extraction parameters */
    uint8_t start_byte;           /* Starting byte index in frame payload (0-based) */
    uint8_t num_bytes;            /* Number of bytes to extract (1-4 for numeric types) */
    uint8_t bit_offset;           /* Bit offset within start_byte (0-7) */
    uint8_t bit_length;           /* Total bits to extract (8*num_bytes for byte-aligned) */
    
    /* Scaling parameters for linear decode: value = (raw * scale) + offset */
    float scale;                  /* Multiplicative factor (e.g., 0.1 for RPM/10) */
    float offset;                 /* Additive offset (e.g., -40 for temperature) */
    
    /* Display formatting hints */
    char unit[16];                /* Unit string (e.g., "RPM", "km/h", "°C") */
    uint8_t decimal_places;       /* Decimal precision for display (0-3) */
    
    /* Reserved for future extension (must be zero-initialized) */
    uint32_t flags;               /* Bitfield: 0x0 = none, 0x1 = mandatory, etc. */
    uint32_t reserved[3];         /* Padding for ABI stability */
} QttuneSignalDef;

/**
 * @brief Container for a signal definition set (manufacturer/plugin scoped)
 */
typedef struct {
    uint32_t version;             /* Schema version for this set */
    uint32_t count;               /* Number of signals in the array */
    const QttuneSignalDef* signals; /* Pointer to array (owned by provider) */
} QttuneSignalSet;

/**
 * @brief Decode raw frame bytes to a display value using a signal definition
 * 
 * @param signal Pointer to signal definition (must not be NULL)
 * @param frame_payload Pointer to CAN frame payload (must have at least start_byte + num_bytes)
 * @param payload_len Length of frame_payload in bytes
 * @param out_value Output parameter for decoded display value
 * 
 * @return 0 on success, non-zero error code on failure
 * 
 * Error codes:
 *   1 - NULL pointer argument
 *   2 - Payload bounds violation (start_byte + num_bytes > payload_len)
 *   3 - Invalid bit_length or bit_offset combination
 *   4 - Unsupported signal type
 */
int qttune_decode_signal(const QttuneSignalDef* signal,
                         const uint8_t* frame_payload,
                         uint8_t payload_len,
                         float* out_value);

/**
 * @brief Validate a signal definition for ABI compliance
 * 
 * Checks:
 * - num_bytes in range [1, 4] for numeric types
 * - bit_length <= 8 * num_bytes
 * - bit_offset + bit_length <= 8
 * - name/unit strings are null-terminated
 * 
 * @param signal Signal definition to validate
 * @return 0 if valid, non-zero error code otherwise
 */
int qttune_validate_signal(const QttuneSignalDef* signal);

#ifdef __cplusplus
}
#endif

#endif /* QTTUNE_SIGNALS_H */
