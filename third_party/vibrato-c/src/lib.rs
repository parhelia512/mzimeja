use std::ffi::{CStr, CString};
use std::fs::File;
use std::os::raw::c_char;
use std::ptr;
use vibrato::{Dictionary, Tokenizer};

pub struct VibratoTokenizer {
    tokenizer: Tokenizer,
}

#[repr(C)]
pub struct VibratoToken {
    surface: *mut c_char,
    feature: *mut c_char,
    start: usize,
    end: usize,
}

#[no_mangle]
pub extern "C" fn vibrato_tokenizer_load(dict_path: *const c_char) -> *mut VibratoTokenizer {
    if dict_path.is_null() {
        return ptr::null_mut();
    }
    
    let path = unsafe {
        match CStr::from_ptr(dict_path).to_str() {
            Ok(s) => s,
            Err(_) => return ptr::null_mut(),
        }
    };
    
    // Open the dictionary file
    let file = match File::open(path) {
        Ok(f) => f,
        Err(_) => return ptr::null_mut(),
    };
    
    // Read the dictionary
    let dict = match Dictionary::read(file) {
        Ok(d) => d,
        Err(_) => return ptr::null_mut(),
    };
    
    let tokenizer = Tokenizer::new(dict);
    
    Box::into_raw(Box::new(VibratoTokenizer { tokenizer }))
}

#[no_mangle]
pub extern "C" fn vibrato_tokenizer_free(tokenizer: *mut VibratoTokenizer) {
    if !tokenizer.is_null() {
        unsafe {
            drop(Box::from_raw(tokenizer));
        }
    }
}

#[no_mangle]
pub extern "C" fn vibrato_tokenize(
    tokenizer: *mut VibratoTokenizer,
    text: *const c_char,
    tokens: *mut *mut VibratoToken,
    num_tokens: *mut usize,
) -> i32 {
    if tokenizer.is_null() || text.is_null() || tokens.is_null() || num_tokens.is_null() {
        return -1;
    }
    
    let tokenizer = unsafe { &mut *tokenizer };
    let text_str = unsafe {
        match CStr::from_ptr(text).to_str() {
            Ok(s) => s,
            Err(_) => return -1,
        }
    };
    
    let mut worker = tokenizer.tokenizer.new_worker();
    worker.reset_sentence(text_str);
    worker.tokenize();
    
    let result_tokens: Vec<VibratoToken> = worker
        .token_iter()
        .map(|token| {
            // Handle potential null bytes in surface and feature strings
            let surface = match CString::new(token.surface()) {
                Ok(s) => s.into_raw(),
                Err(_) => CString::new("").unwrap().into_raw(), // Fallback for null bytes
            };
            let feature = match CString::new(token.feature()) {
                Ok(s) => s.into_raw(),
                Err(_) => CString::new("").unwrap().into_raw(), // Fallback for null bytes
            };
            VibratoToken {
                surface,
                feature,
                start: token.range_char().start,
                end: token.range_char().end,
            }
        })
        .collect();
    
    unsafe {
        *num_tokens = result_tokens.len();
        *tokens = Box::into_raw(result_tokens.into_boxed_slice()) as *mut VibratoToken;
    }
    
    0
}

#[no_mangle]
pub extern "C" fn vibrato_tokens_free(tokens: *mut VibratoToken, num_tokens: usize) {
    if tokens.is_null() {
        return;
    }
    
    unsafe {
        let token_slice = std::slice::from_raw_parts_mut(tokens, num_tokens);
        for token in token_slice {
            if !token.surface.is_null() {
                drop(CString::from_raw(token.surface));
            }
            if !token.feature.is_null() {
                drop(CString::from_raw(token.feature));
            }
        }
        drop(Box::from_raw(std::slice::from_raw_parts_mut(tokens, num_tokens)));
    }
}
