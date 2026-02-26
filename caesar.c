#include <stdlib.h>
#include "caesar.h"

static char current_key = 0;

void cezare_key(char new_key) {
    current_key = new_key;
}

void cezare_enc(void* in_data, void* out_data, int data_len) {
    if (in_data == 0 || out_data == 0 || data_len == 0) {
        return;
    }
    
    unsigned char* reader = (unsigned char*)in_data;
    unsigned char* writer = (unsigned char*)out_data;
    unsigned char* end = reader + data_len;
    
    while (reader < end) {
        *writer = *reader ^ current_key;
        reader++;
        writer++;
    }
}