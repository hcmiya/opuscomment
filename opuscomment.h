#include <ogg/ogg.h>
#include <wchar.h>

void parse_tags(void);
void validate_tag(wchar_t*);
void add_tag(char*);
void put_tags(void);
void read_page(ogg_sync_state*);
void mainerror(char const*, ...);
void opuserror(char const*, ...);
void oserror(void);
void oserror_fmt(char const*, ...);
void fileerror(char const*);
void move_file(void);

#ifdef NLS
#include <nl_types.h>
#else
#define catgets(catd, set_id, msg_id, s) (s)
#endif

#include "ocutil.h"
