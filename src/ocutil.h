uint16_t oi16(uint16_t i);
uint32_t oi32(uint32_t i);
uint64_t oi64(uint64_t i);
bool test_tag_field(uint8_t *line, size_t n, bool upcase, bool *on_field, bool *upcase_applied);
size_t fill_buffer(void *buf, size_t left, size_t buflen, FILE *fp);
bool test_non_opus(ogg_page *og);
void write_page(ogg_page *og, FILE *fp);
void set_pageno(ogg_page *og, int no);
void set_granulepos(ogg_page *og, int64_t pos);

#if _POSIX_C_SOURCE < 200809L
size_t strnlen(char const *src, size_t n);
#endif
