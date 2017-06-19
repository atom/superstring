// These two functions were taken from the libc++ source code:
// https://github.com/llvm-mirror/libcxx/blob/c3589a8305a317cfa0757bc5f4136a7b93684d23/src/locale.cpp
//
// They are included here because the `codecvt` header is being deprecated:
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0618r0.html
//
// Also, that header is only available in recent versions of libstdc++, so
// relying on them breaks ABI compatibility with older linux systems.

enum transcode_result {
  ok,
  partial,
  error,
};

static
transcode_result
utf16_to_utf8(const uint16_t* frm, const uint16_t* frm_end, const uint16_t*& frm_nxt,
              uint8_t* to, uint8_t* to_end, uint8_t*& to_nxt,
              unsigned long Maxcode = 0x10FFFF)
{
    frm_nxt = frm;
    to_nxt = to;

    for (; frm_nxt < frm_end; ++frm_nxt)
    {
        uint16_t wc1 = *frm_nxt;
        if (wc1 > Maxcode)
            return transcode_result::error;
        if (wc1 < 0x0080)
        {
            if (to_end-to_nxt < 1)
                return transcode_result::partial;
            *to_nxt++ = static_cast<uint8_t>(wc1);
        }
        else if (wc1 < 0x0800)
        {
            if (to_end-to_nxt < 2)
                return transcode_result::partial;
            *to_nxt++ = static_cast<uint8_t>(0xC0 | (wc1 >> 6));
            *to_nxt++ = static_cast<uint8_t>(0x80 | (wc1 & 0x03F));
        }
        else if (wc1 < 0xD800)
        {
            if (to_end-to_nxt < 3)
                return transcode_result::partial;
            *to_nxt++ = static_cast<uint8_t>(0xE0 |  (wc1 >> 12));
            *to_nxt++ = static_cast<uint8_t>(0x80 | ((wc1 & 0x0FC0) >> 6));
            *to_nxt++ = static_cast<uint8_t>(0x80 |  (wc1 & 0x003F));
        }
        else if (wc1 < 0xDC00)
        {
            if (frm_end-frm_nxt < 2)
                return transcode_result::partial;
            uint16_t wc2 = frm_nxt[1];
            if ((wc2 & 0xFC00) != 0xDC00)
                return transcode_result::error;
            if (to_end-to_nxt < 4)
                return transcode_result::partial;
            if (((((wc1 & 0x03C0UL) >> 6) + 1) << 16) +
                ((wc1 & 0x003FUL) << 10) + (wc2 & 0x03FF) > Maxcode)
                return transcode_result::error;
            ++frm_nxt;
            uint8_t z = ((wc1 & 0x03C0) >> 6) + 1;
            *to_nxt++ = static_cast<uint8_t>(0xF0 | (z >> 2));
            *to_nxt++ = static_cast<uint8_t>(0x80 | ((z & 0x03) << 4)     | ((wc1 & 0x003C) >> 2));
            *to_nxt++ = static_cast<uint8_t>(0x80 | ((wc1 & 0x0003) << 4) | ((wc2 & 0x03C0) >> 6));
            *to_nxt++ = static_cast<uint8_t>(0x80 |  (wc2 & 0x003F));
        }
        else if (wc1 < 0xE000)
        {
            return transcode_result::error;
        }
        else
        {
            if (to_end-to_nxt < 3)
                return transcode_result::partial;
            *to_nxt++ = static_cast<uint8_t>(0xE0 |  (wc1 >> 12));
            *to_nxt++ = static_cast<uint8_t>(0x80 | ((wc1 & 0x0FC0) >> 6));
            *to_nxt++ = static_cast<uint8_t>(0x80 |  (wc1 & 0x003F));
        }
    }
    return transcode_result::ok;
}

static
transcode_result
utf8_to_utf16(const uint8_t* frm, const uint8_t* frm_end, const uint8_t*& frm_nxt,
              uint16_t* to, uint16_t* to_end, uint16_t*& to_nxt,
              unsigned long Maxcode = 0x10FFFF)
{
    frm_nxt = frm;
    to_nxt = to;

    for (; frm_nxt < frm_end && to_nxt < to_end; ++to_nxt)
    {
        uint8_t c1 = *frm_nxt;
        if (c1 > Maxcode)
            return transcode_result::error;
        if (c1 < 0x80)
        {
            *to_nxt = static_cast<uint16_t>(c1);
            ++frm_nxt;
        }
        else if (c1 < 0xC2)
        {
            return transcode_result::error;
        }
        else if (c1 < 0xE0)
        {
            if (frm_end-frm_nxt < 2)
                return transcode_result::partial;
            uint8_t c2 = frm_nxt[1];
            if ((c2 & 0xC0) != 0x80)
                return transcode_result::error;
            uint16_t t = static_cast<uint16_t>(((c1 & 0x1F) << 6) | (c2 & 0x3F));
            if (t > Maxcode)
                return transcode_result::error;
            *to_nxt = t;
            frm_nxt += 2;
        }
        else if (c1 < 0xF0)
        {
            if (frm_end-frm_nxt < 3)
                return transcode_result::partial;
            uint8_t c2 = frm_nxt[1];
            uint8_t c3 = frm_nxt[2];
            switch (c1)
            {
            case 0xE0:
                if ((c2 & 0xE0) != 0xA0)
                    return transcode_result::error;
                 break;
            case 0xED:
                if ((c2 & 0xE0) != 0x80)
                    return transcode_result::error;
                 break;
            default:
                if ((c2 & 0xC0) != 0x80)
                    return transcode_result::error;
                 break;
            }
            if ((c3 & 0xC0) != 0x80)
                return transcode_result::error;
            uint16_t t = static_cast<uint16_t>(((c1 & 0x0F) << 12)
                                             | ((c2 & 0x3F) << 6)
                                             |  (c3 & 0x3F));
            if (t > Maxcode)
                return transcode_result::error;
            *to_nxt = t;
            frm_nxt += 3;
        }
        else if (c1 < 0xF5)
        {
            if (frm_end-frm_nxt < 4)
                return transcode_result::partial;
            uint8_t c2 = frm_nxt[1];
            uint8_t c3 = frm_nxt[2];
            uint8_t c4 = frm_nxt[3];
            switch (c1)
            {
            case 0xF0:
                if (!(0x90 <= c2 && c2 <= 0xBF))
                    return transcode_result::error;
                 break;
            case 0xF4:
                if ((c2 & 0xF0) != 0x80)
                    return transcode_result::error;
                 break;
            default:
                if ((c2 & 0xC0) != 0x80)
                    return transcode_result::error;
                 break;
            }
            if ((c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80)
                return transcode_result::error;
            if (to_end-to_nxt < 2)
                return transcode_result::partial;
            if ((((c1 & 7UL) << 18) +
                ((c2 & 0x3FUL) << 12) +
                ((c3 & 0x3FUL) << 6) + (c4 & 0x3F)) > Maxcode)
                return transcode_result::error;
            *to_nxt = static_cast<uint16_t>(
                    0xD800
                  | (((((c1 & 0x07) << 2) | ((c2 & 0x30) >> 4)) - 1) << 6)
                  | ((c2 & 0x0F) << 2)
                  | ((c3 & 0x30) >> 4));
            *++to_nxt = static_cast<uint16_t>(
                    0xDC00
                  | ((c3 & 0x0F) << 6)
                  |  (c4 & 0x3F));
            frm_nxt += 4;
        }
        else
        {
            return transcode_result::error;
        }
    }
    return frm_nxt < frm_end ? transcode_result::partial : transcode_result::ok;
}
