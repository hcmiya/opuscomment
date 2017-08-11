#include <ogg/ogg.h>

void parse_tags(void);
void validate_tag(wchar_t*);
void add_tag(char*);
void put_tags(void);
void read_page(ogg_sync_state*);
void mainerror(char*, ...);
void opuserror(char*, ...);
void oserror(void);
void move_file(void);
