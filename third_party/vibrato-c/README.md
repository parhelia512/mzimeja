# vibrato-c

C API bindings for the Vibrato morphological analysis library.

## Overview

This library provides C-compatible FFI bindings for [Vibrato](https://github.com/daac-tools/vibrato), a Japanese morphological analyzer written in Rust. It enables C and C++ applications to use Vibrato for tokenizing Japanese text.

## API

### Types

- `VibratoTokenizer`: Opaque handle to a tokenizer instance
- `VibratoToken`: Structure containing token information
  - `char* surface`: Surface form of the token (UTF-8)
  - `char* feature`: Feature string in CSV format (UTF-8)
  - `size_t start`: Start position in the input text (character index)
  - `size_t end`: End position in the input text (character index)

### Functions

#### vibrato_tokenizer_load
```c
VibratoTokenizer* vibrato_tokenizer_load(const char* dict_path);
```
Load a Vibrato dictionary and create a tokenizer.

- **Parameters**: 
  - `dict_path`: Path to the dictionary file (UTF-8 encoded)
- **Returns**: Tokenizer handle on success, NULL on failure

#### vibrato_tokenizer_free
```c
void vibrato_tokenizer_free(VibratoTokenizer* tokenizer);
```
Free a tokenizer instance.

- **Parameters**: 
  - `tokenizer`: Tokenizer handle to free

#### vibrato_tokenize
```c
int vibrato_tokenize(VibratoTokenizer* tokenizer, const char* text, 
                     VibratoToken** tokens, size_t* num_tokens);
```
Tokenize Japanese text.

- **Parameters**:
  - `tokenizer`: Tokenizer handle
  - `text`: Input text (UTF-8 encoded)
  - `tokens`: Pointer to receive array of tokens (output parameter)
  - `num_tokens`: Pointer to receive number of tokens (output parameter)
- **Returns**: 0 on success, -1 on failure

#### vibrato_tokens_free
```c
void vibrato_tokens_free(VibratoToken* tokens, size_t num_tokens);
```
Free memory allocated for tokens.

- **Parameters**:
  - `tokens`: Array of tokens to free
  - `num_tokens`: Number of tokens in the array

## Example Usage

```c
#include <vibrato.h>
#include <stdio.h>

int main() {
    // Load dictionary
    VibratoTokenizer* tokenizer = vibrato_tokenizer_load("path/to/dict.zst");
    if (!tokenizer) {
        fprintf(stderr, "Failed to load dictionary\n");
        return 1;
    }
    
    // Tokenize text
    const char* text = "形態素解析の例です。";
    VibratoToken* tokens = NULL;
    size_t num_tokens = 0;
    
    int result = vibrato_tokenize(tokenizer, text, &tokens, &num_tokens);
    if (result != 0) {
        fprintf(stderr, "Tokenization failed\n");
        vibrato_tokenizer_free(tokenizer);
        return 1;
    }
    
    // Process tokens
    for (size_t i = 0; i < num_tokens; i++) {
        printf("Token: %s [%s]\n", tokens[i].surface, tokens[i].feature);
    }
    
    // Clean up
    vibrato_tokens_free(tokens, num_tokens);
    vibrato_tokenizer_free(tokenizer);
    
    return 0;
}
```

## Building

This library is built as part of the MZ-IME Japanese Input project using Cargo:

```bash
cargo build --release
```

The build produces both a static library (`libvibrato_c.a` or `vibrato_c.lib`) and a dynamic library (`libvibrato_c.so`, `libvibrato_c.dylib`, or `vibrato_c.dll`).

## License

This project follows the licensing of Vibrato:
- MIT OR Apache-2.0

## Dependencies

- [Vibrato](https://github.com/daac-tools/vibrato) v0.5.2
