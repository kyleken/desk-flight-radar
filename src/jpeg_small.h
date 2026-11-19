// Small JPEG decoder for aircraft photos.
//
// Decodes baseline and progressive JPEGs straight into a small RGB565 picture
// (cropped to fill it, like CSS object-fit: cover). It keeps only the
// low-frequency coefficients the small picture needs, so a 200x136
// planespotters thumbnail decodes in about 30 KB of working memory.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace jpeg_small {

// out: outW*outH RGB565 pixels (value = r5<<11 | g6<<5 | b5).
// maxWork: the most memory the decoder may allocate.
// Returns false (and a short reason in *err, if given) on anything it can't
// decode; it never reads outside data[0..len).
bool decodeCover(const uint8_t* data, size_t len, uint16_t* out, int outW, int outH, size_t maxWork,
                 const char** err = nullptr);

}  // namespace jpeg_small
