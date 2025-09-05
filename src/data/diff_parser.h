// src/data/diff_parser.h
#ifndef SEE_CODE_DIFF_PARSER_H
#define SEE_CODE_DIFF_PARSER_H

#include "see_code/data/diff_data.h"
#include <stddef.h>

/**
 * @brief Parses a raw git diff buffer into a DiffData structure.
 *
 * This function takes a buffer containing the output of `git diff` and
 * parses it into the internal `DiffData` structure for further processing
 * and rendering.
 *
 * @param data Pointer to the DiffData structure to populate.
 * @param buffer Pointer to the null-terminated string containing the diff.
 * @param buffer_size Size of the buffer in bytes.
 * @return 1 on success, 0 on failure.
 */
int diff_parser_parse(DiffData* data, const char* buffer, size_t buffer_size);

#endif // SEE_CODE_DIFF_PARSER_H
