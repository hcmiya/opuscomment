#include <ogg/ogg.h>
#include <stdbool.h>
#include <iconv.h>

void add_tag_from_opt(char const *);
void read_page(ogg_sync_state*);
void move_file(void);
iconv_t iconv_new(char const *to, char const *from);

#ifdef NLS
#include <nl_types.h>
#else
#define catgets(catd, set_id, msg_id, s) (s)
#endif

#include "ocutil.h"
#include "limit.h"
#include "error.h"
