#ifndef GIGE_REMAP_MAP_H
#define GIGE_REMAP_MAP_H
#include <stddef.h>
#include <stdint.h>
#define GIGE_REMAP_MAX_WORDS (640U*480U)
typedef enum { GIGE_REMAP_EMPTY, GIGE_REMAP_UPLOAD, GIGE_REMAP_VALIDATE,
               GIGE_REMAP_READY, GIGE_REMAP_ERROR } GigeRemapMapState;
typedef struct {
    uint32_t *bank[2];
    size_t capacity;
    GigeRemapMapState state;
    uint32_t rows,cols,uploaded,checked,expected_crc,crc,error;
    uint32_t staging_bank,active_bank;
    uint64_t generation,prepared_generation;
    int reader_may_access;
} GigeRemapMap;
/* All calls use the serialized camera/control context. Buffers are dedicated,
 * nonoverlapping and64-byte aligned. Active-bank protection survives failures.
 * This component never accesses MMIO, enables the reader, or proves quiescence. */
#ifdef __cplusplus
extern "C" {
#endif
int GigeRemapMap_Init(GigeRemapMap*,uint32_t*,uint32_t*,size_t capacity);
int GigeRemapMap_Begin(GigeRemapMap*,uint32_t rows,uint32_t cols,uint32_t crc,int camera_busy);
/* Strict sequential upload; identical earlier words are idempotent retries.
 * Conflicting retries and gaps are rejected without modifying either buffer. */
int GigeRemapMap_Write(GigeRemapMap*,uint32_t index,uint32_t word,int camera_busy);
int GigeRemapMap_Commit(GigeRemapMap*,int camera_busy);
/* Maximum256 words per step, validating encoding/window and CRC32 over LE bytes.
 * Returns0 pending,1 ready,-1 failure. A work bound is not a measured time bound. */
int GigeRemapMap_Step(GigeRemapMap*,unsigned budget);
void GigeRemapMap_Abort(GigeRemapMap*);
/* Call only after proving the old reader cannot issue/read more transactions.
 * Software stop acknowledgement alone is not such proof. */
void GigeRemapMap_ReaderQuiesced(GigeRemapMap*);
/* Two-phase activation: obtain immutable address/token, perform cache clean and
 * barriers, program the quiescent reader, then expose this exact generation.
 * Begin/Abort invalidate prepared tokens. Old banks remain protected until the
 * explicit quiescence acknowledgement, including during a failed upload. */
int GigeRemapMap_Prepare(GigeRemapMap*,uint32_t rows,uint32_t cols,
                        const uint32_t **address,uint64_t *generation);
int GigeRemapMap_ReaderStarted(GigeRemapMap*,uint64_t generation);
#ifdef __cplusplus
}
#endif
#endif
