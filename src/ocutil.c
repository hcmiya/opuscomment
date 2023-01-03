#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "opuscomment.h"

static bool test_tag_field_keepcase(uint8_t *line, size_t n, bool *on_field)
{
    size_t i;
    bool valid = true;
    for (i = 0; i < n && line[i] != 0x3d; i++)
    {
        if (!(line[i] >= 0x20 && line[i] <= 0x7d))
        {
            valid = false;
        }
    }
    if (i < n) *on_field = false;
    return valid;
}
bool test_tag_field(uint8_t *line, size_t n, bool upcase, bool *on_field, bool *upcase_applied_)
{
    // フィールドの使用文字チェック・大文字化
    if (!upcase)
    {
        return test_tag_field_keepcase(line, n, on_field);
    }

    bool dummy_, *upcase_applied;
    upcase_applied = upcase_applied_ ? upcase_applied_ : &dummy_;

    size_t i;
    bool valid = true;
    for (i = 0; i < n && line[i] != 0x3d; i++)
    {
        if (!(line[i] >= 0x20 && line[i] <= 0x7d))
        {
            valid = false;
        }
        if (line[i] >= 0x61 && line[i] <= 0x7a)
        {
            line[i] -= 32;
            *upcase_applied = true;
        }
    }
    if (i < n) *on_field = false;
    return valid;
}

size_t fill_buffer(void *buf, size_t left, size_t buflen, FILE *fp)
{
    size_t filllen = left > buflen ? buflen : left;
    size_t readlen = fread(buf, 1, filllen, fp);
    return filllen;
}

bool test_non_opus(ogg_page *og)
{
    if (ogg_page_serialno(og) == opus_sno)
    {
        if (ogg_page_bos(og))
        {
            opuserror(err_opus_bad_stream);
        }
        if (opst != PAGE_SOUND && ogg_page_pageno(og) != opus_idx)
        {
            opuserror(err_opus_discontinuous, ogg_page_pageno(og), opus_idx);
        }
        return true;
    }
//  have_multi_streams = true;

    int pno = ogg_page_pageno(og);
    if (pno == 0)
    {
        if (leave_header_packets)
        {
            opuserror(err_opus_bad_stream);
        }
        if (!ogg_page_bos(og) || ogg_page_eos(og) || ogg_page_granulepos(og) != 0)
        {
            opuserror(err_opus_bad_stream);
        }
    }
    else if (pno == 1)
    {
        // 複数論理ストリームの先頭は全て0で始まるため、何かが1ページ目を始めたら今後0ページ目は来ないはずである。
        leave_header_packets = true;
    }
    else
    {
        if (!leave_header_packets)
        {
            // 他で1ページ目が始まってないのに2ページ目以上が来た場合
            opuserror(err_opus_bad_stream);
        }
    }

    if (pno && ogg_page_bos(og))
    {
        // Opusが続いているストリーム途中でBOSが来るはずがない
        opuserror(err_opus_bad_stream);
    }
    return false;
}

void write_page(ogg_page *og, FILE *fp)
{
    size_t ret;
    ret = fwrite(og->header, 1, og->header_len, fp);
    if (ret != og->header_len)
    {
        oserror();
    }
    ret = fwrite(og->body, 1, og->body_len, fp);
    if (ret != og->body_len)
    {
        oserror();
    }
}

void set_pageno(ogg_page *og, int no)
{
    *(uint32_t*)&og->header[18] = oi32(no);
}

void set_granulepos(ogg_page *og, int64_t pos)
{
    *(int64_t*)&og->header[6] = oi64(pos);
}

#if _POSIX_C_SOURCE < 200809L
size_t strnlen(char const *src, size_t n)
{
    char const *endp = src + n, *p = src;
    while (p < endp && *p) p++;
    return p - src;
}
#endif
