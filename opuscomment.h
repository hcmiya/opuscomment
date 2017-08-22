#include <ogg/ogg.h>
#include <stdbool.h>

void parse_tags(void);
void add_tag_from_opt(char const *);
void *put_tags(void*);
void read_page(ogg_sync_state*);
void move_file(void);
void check_tagpacket_length(void);

#ifdef NLS
#include <nl_types.h>
#else
#define catgets(catd, set_id, msg_id, s) (s)
#endif

#include "ocutil.h"
#include "limit.h"
#include "error.h"
