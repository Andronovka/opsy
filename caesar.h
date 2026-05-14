#ifndef CAESAR_H
#define CAESAR_H

void cezare_key(char key);
void cezare_enc(void* src, void* dst, int len);

void cezare_cleanup(void);
void* get_protected_key_ptr(void);
void test_memory_violation(void);

#endif