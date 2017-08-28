#include <ogg/ogg.h>
#include <stdbool.h>
#include <iconv.h>

void parse_opt_tag(int, char const*);
void pticonv_close(void);
void read_page(ogg_sync_state*);
void move_file(void);
iconv_t iconv_new(char const *to, char const *from);
void open_output_file(void);

#ifdef NLS
#include <nl_types.h>
#else
#define catgets(catd, set_id, msg_id, s) (s)
#endif

#include "ocutil.h"
#include "limit.h"
#include "error.h"

struct rettag_st {
	FILE *tag, *padding;
	long tagbegin;
	size_t num, del;
	bool upcase;
};
struct edit_st {
	FILE *str, *len;
	size_t num;
};
