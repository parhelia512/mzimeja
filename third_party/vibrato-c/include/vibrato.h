#ifndef VIBRATO_H
#define VIBRATO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct VibratoTokenizer VibratoTokenizer;

typedef struct {
    char* surface;
    char* feature;
    size_t start;
    size_t end;
} VibratoToken;

VibratoTokenizer* vibrato_tokenizer_load(const char* dict_path);
void vibrato_tokenizer_free(VibratoTokenizer* tokenizer);
int vibrato_tokenize(VibratoTokenizer* tokenizer, const char* text, VibratoToken** tokens, size_t* num_tokens);
void vibrato_tokens_free(VibratoToken* tokens, size_t num_tokens);

#ifdef __cplusplus
}
#endif

#endif
