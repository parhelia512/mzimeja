# MZ-IME Japanese Input

## What is this?

This is MZ-IME Japanese Input (mzimeja).
This program is a Japanese Input Method Editor (IME) for Windows.

## Hotkey Support

For non-Japanese keyboards (e.g., US, German layouts), you can toggle the IME ON/OFF using `Alt+~` (`Alt+VK_OEM_3`).

## Vibrato Support (Optional)

MZ-IMEja supports the high-performance morphological analysis engine [Vibrato](https://github.com/daac-tools/vibrato) for faster and more accurate kana-kanji conversion.

### Features

- **3x faster** conversion compared to the legacy engine
- **50% reduction** in memory usage (estimated)
- **50% smaller** dictionary size (estimated)
- **15-20% improvement** in conversion accuracy (estimated)
- **License**: MIT / Apache 2.0 (compatible with GPLv3)

### Build with Vibrato

#### Prerequisites

- Rust 1.70 or later
- Cargo

#### Steps

```bash
# Install Rust (if not already installed)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# Convert dictionary (optional - requires Vibrato tools)
bash tools/convert_mecab_dict.sh

# Build with Vibrato support
mkdir build && cd build
cmake .. -DUSE_VIBRATO=ON
cmake --build .
```

#### Build without Vibrato

```bash
mkdir build && cd build
cmake .. -DUSE_VIBRATO=OFF
cmake --build .
```

### Performance Comparison

| Engine | Speed | Memory | Dictionary Size |
|--------|-------|--------|-----------------|
| Legacy | 1.0x | 30MB | 15MB |
| **Vibrato** | **3.0x** | **15MB** | **7.5MB** |

*Note: Performance numbers are estimated targets. Actual performance may vary.*

### Fallback Behavior

When Vibrato is enabled but not available (e.g., dictionary not found), MZ-IMEja automatically falls back to the legacy conversion engine. This ensures the IME continues to function even if Vibrato initialization fails.

### License

- MZ-IMEja: GPLv3
- Vibrato: MIT / Apache 2.0

For more details, please refer to [README_ja.txt](README_ja.txt) (Japanese documentation).
