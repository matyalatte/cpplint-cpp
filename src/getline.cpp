#include "getline.h"
#include <stdint.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#define ASCII_MAX 0x7F  // ascii 0x00 ~ 0x7F
#define MULTIBYTE_SEQ_MAX 0xBF  // sequences for multibyte characters 0x80 ~ 0xBF
#define TWO_BYTE_MAX 0xDF  // two-byte characters 0xC0 ~ 0xDF
#define THREE_BYTE_MAX 0xEF  // three-byte characters 0xE0 ~ 0xEF
#define FOUR_BYTE_MAX 0xF7  // four-byte characters 0xF0 ~ 0xF7
// unused codes 0xF8 ~ 0xFF
// #define FIVE_BYTE_MAX 0xFB
// #define SIX_BYTE_MAX 0xFD

#define is_multibyte_seq(c) ((ASCII_MAX < (uint8_t)(c)) && ((uint8_t)(c) <= MULTIBYTE_SEQ_MAX))

#define BAD_RUNE "\xef\xbf\xbd"

size_t GetWidthUTF8(uint32_t utf8_rune);

size_t GetLineWidth(const std::string& line) noexcept {
    // We don't need to consider broken bytes because GetLine() removes them.
    const unsigned char* c = (const unsigned char*)&line[0];
    size_t length = 0;
    while (*c != '\0') {
        size_t rune_size = 0;

        if (*c <= ASCII_MAX) {
            length++;
            c++;
            continue;
        } else if (*c <= TWO_BYTE_MAX) {
            rune_size = 2;  // two-byte
        } else if (*c <= THREE_BYTE_MAX) {
            rune_size = 3;  // three-byte
        } else if (*c <= FOUR_BYTE_MAX) {
            rune_size = 4;  // four-byte
        }

        uint32_t rune = c[0];
        for (size_t i = 1; i < rune_size; i++) {
            rune <<= 8;
            rune += c[i];
        }
        length += GetWidthUTF8(rune);
        c += rune_size;
    }
    return length;
}

// Realloc a string buffer when it requires a larger length
static inline void ResizeBuffer(std::string* buffer, char** buf_p,
                                size_t length, size_t append_size) {
    if (length + append_size > buffer->size()) {
        buffer->resize(buffer->size() * 2);
        *buf_p = buffer->data() + length;
    }
}

std::string GetLine(std::istream& stream, std::string* buffer, int* status) {
    *status = LINE_OK;
    int c = 0;
    unsigned char rune[4];
    char* buf_p = buffer->data();
    size_t length = 0;

    while (c != EOF) {
        size_t rune_size = 0;
        c = stream.get();

        if (c == EOF) {
            *status |= LINE_EOF;
            return std::string(buffer->data(), length);
        } else if (c <= ASCII_MAX) {
            // ascii
            if (c == '\n') {
                // a line found
                return std::string(buffer->data(), length);
            } else if (c == '\0') {
                ResizeBuffer(buffer, &buf_p, length, 3);
                // replace null byte with a bad rune
                memcpy(buf_p, BAD_RUNE, 3);
                buf_p += 3;
                length += 3;
                *status |= LINE_NULL;
            } else {
                // ascii
                ResizeBuffer(buffer, &buf_p, length, 1);
                *buf_p = (unsigned char)c;
                buf_p++;
                length++;
            }
            continue;
        } else if (c <= MULTIBYTE_SEQ_MAX) {
            rune_size = 0;  // bad rune
        } else if (c <= TWO_BYTE_MAX) {
            rune_size = 2;  // two-byte
        } else if (c <= THREE_BYTE_MAX) {
            rune_size = 3;  // three-byte
        } else if (c <= FOUR_BYTE_MAX) {
            rune_size = 4;  // four-byte
        } else {
            rune_size = 0;  // bad rune
        }

        // Read a multi-byte character
        rune[0] = (unsigned char)c;
        for (size_t i = 1; i < rune_size; i++) {
            c = stream.get();
            if (c == EOF) {
                rune_size = 0;
                *status |= LINE_EOF;
                break;
            } else if (is_multibyte_seq(c)) {
                rune[i] = (unsigned char)c;
            } else {
                // bad rune
                rune_size = 0;
                stream.unget();
                break;
            }
        }

        if (rune_size == 0) {
            ResizeBuffer(buffer, &buf_p, length, 3);
            memcpy(buf_p, BAD_RUNE, 3);
            buf_p += 3;
            length += 3;
            *status |= LINE_BAD_RUNE;
        } else {
            ResizeBuffer(buffer, &buf_p, length, 1);
            memcpy(buf_p, rune, rune_size * sizeof(unsigned char));
            buf_p += rune_size;
            length += rune_size;
        }
    }

    return std::string(buffer->data(), length);
}

/* An inclusive range of characters. */
struct uint32_range {
  uint32_t lo;
  uint32_t hi;
};

// utf32 tables are from widechar_width.h
// https://github.com/ridiculousfish/widecharwidth/blob/master/widechar_width.h

/*
static const struct uint32_range utf32_combining_table[] = {
    {0x00300, 0x0036F},
    {0x00483, 0x00489},
    {0x00591, 0x005BD},
    {0x005BF, 0x005BF},
    {0x005C1, 0x005C2},
    {0x005C4, 0x005C5},
    {0x005C7, 0x005C7},
    {0x00610, 0x0061A},
    {0x0064B, 0x0065F},
    {0x00670, 0x00670},
    {0x006D6, 0x006DC},
    {0x006DF, 0x006E4},
    {0x006E7, 0x006E8},
    {0x006EA, 0x006ED},
    {0x00711, 0x00711},
    {0x00730, 0x0074A},
    {0x007A6, 0x007B0},
    {0x007EB, 0x007F3},
    {0x007FD, 0x007FD},
    {0x00816, 0x00819},
    {0x0081B, 0x00823},
    {0x00825, 0x00827},
    {0x00829, 0x0082D},
    {0x00859, 0x0085B},
    {0x00898, 0x0089F},
    {0x008CA, 0x008E1},
    {0x008E3, 0x00903},
    {0x0093A, 0x0093C},
    {0x0093E, 0x0094F},
    {0x00951, 0x00957},
    {0x00962, 0x00963},
    {0x00981, 0x00983},
    {0x009BC, 0x009BC},
    {0x009BE, 0x009C4},
    {0x009C7, 0x009C8},
    {0x009CB, 0x009CD},
    {0x009D7, 0x009D7},
    {0x009E2, 0x009E3},
    {0x009FE, 0x009FE},
    {0x00A01, 0x00A03},
    {0x00A3C, 0x00A3C},
    {0x00A3E, 0x00A42},
    {0x00A47, 0x00A48},
    {0x00A4B, 0x00A4D},
    {0x00A51, 0x00A51},
    {0x00A70, 0x00A71},
    {0x00A75, 0x00A75},
    {0x00A81, 0x00A83},
    {0x00ABC, 0x00ABC},
    {0x00ABE, 0x00AC5},
    {0x00AC7, 0x00AC9},
    {0x00ACB, 0x00ACD},
    {0x00AE2, 0x00AE3},
    {0x00AFA, 0x00AFF},
    {0x00B01, 0x00B03},
    {0x00B3C, 0x00B3C},
    {0x00B3E, 0x00B44},
    {0x00B47, 0x00B48},
    {0x00B4B, 0x00B4D},
    {0x00B55, 0x00B57},
    {0x00B62, 0x00B63},
    {0x00B82, 0x00B82},
    {0x00BBE, 0x00BC2},
    {0x00BC6, 0x00BC8},
    {0x00BCA, 0x00BCD},
    {0x00BD7, 0x00BD7},
    {0x00C00, 0x00C04},
    {0x00C3C, 0x00C3C},
    {0x00C3E, 0x00C44},
    {0x00C46, 0x00C48},
    {0x00C4A, 0x00C4D},
    {0x00C55, 0x00C56},
    {0x00C62, 0x00C63},
    {0x00C81, 0x00C83},
    {0x00CBC, 0x00CBC},
    {0x00CBE, 0x00CC4},
    {0x00CC6, 0x00CC8},
    {0x00CCA, 0x00CCD},
    {0x00CD5, 0x00CD6},
    {0x00CE2, 0x00CE3},
    {0x00CF3, 0x00CF3},
    {0x00D00, 0x00D03},
    {0x00D3B, 0x00D3C},
    {0x00D3E, 0x00D44},
    {0x00D46, 0x00D48},
    {0x00D4A, 0x00D4D},
    {0x00D57, 0x00D57},
    {0x00D62, 0x00D63},
    {0x00D81, 0x00D83},
    {0x00DCA, 0x00DCA},
    {0x00DCF, 0x00DD4},
    {0x00DD6, 0x00DD6},
    {0x00DD8, 0x00DDF},
    {0x00DF2, 0x00DF3},
    {0x00E31, 0x00E31},
    {0x00E34, 0x00E3A},
    {0x00E47, 0x00E4E},
    {0x00EB1, 0x00EB1},
    {0x00EB4, 0x00EBC},
    {0x00EC8, 0x00ECE},
    {0x00F18, 0x00F19},
    {0x00F35, 0x00F35},
    {0x00F37, 0x00F37},
    {0x00F39, 0x00F39},
    {0x00F3E, 0x00F3F},
    {0x00F71, 0x00F84},
    {0x00F86, 0x00F87},
    {0x00F8D, 0x00F97},
    {0x00F99, 0x00FBC},
    {0x00FC6, 0x00FC6},
    {0x0102B, 0x0103E},
    {0x01056, 0x01059},
    {0x0105E, 0x01060},
    {0x01062, 0x01064},
    {0x01067, 0x0106D},
    {0x01071, 0x01074},
    {0x01082, 0x0108D},
    {0x0108F, 0x0108F},
    {0x0109A, 0x0109D},
    {0x0135D, 0x0135F},
    {0x01712, 0x01715},
    {0x01732, 0x01734},
    {0x01752, 0x01753},
    {0x01772, 0x01773},
    {0x017B4, 0x017D3},
    {0x017DD, 0x017DD},
    {0x0180B, 0x0180D},
    {0x0180F, 0x0180F},
    {0x01885, 0x01886},
    {0x018A9, 0x018A9},
    {0x01920, 0x0192B},
    {0x01930, 0x0193B},
    {0x01A17, 0x01A1B},
    {0x01A55, 0x01A5E},
    {0x01A60, 0x01A7C},
    {0x01A7F, 0x01A7F},
    {0x01AB0, 0x01ACE},
    {0x01B00, 0x01B04},
    {0x01B34, 0x01B44},
    {0x01B6B, 0x01B73},
    {0x01B80, 0x01B82},
    {0x01BA1, 0x01BAD},
    {0x01BE6, 0x01BF3},
    {0x01C24, 0x01C37},
    {0x01CD0, 0x01CD2},
    {0x01CD4, 0x01CE8},
    {0x01CED, 0x01CED},
    {0x01CF4, 0x01CF4},
    {0x01CF7, 0x01CF9},
    {0x01DC0, 0x01DFF},
    {0x020D0, 0x020F0},
    {0x02CEF, 0x02CF1},
    {0x02D7F, 0x02D7F},
    {0x02DE0, 0x02DFF},
    {0x0302A, 0x0302F},
    {0x03099, 0x0309A},
    {0x0A66F, 0x0A672},
    {0x0A674, 0x0A67D},
    {0x0A69E, 0x0A69F},
    {0x0A6F0, 0x0A6F1},
    {0x0A802, 0x0A802},
    {0x0A806, 0x0A806},
    {0x0A80B, 0x0A80B},
    {0x0A823, 0x0A827},
    {0x0A82C, 0x0A82C},
    {0x0A880, 0x0A881},
    {0x0A8B4, 0x0A8C5},
    {0x0A8E0, 0x0A8F1},
    {0x0A8FF, 0x0A8FF},
    {0x0A926, 0x0A92D},
    {0x0A947, 0x0A953},
    {0x0A980, 0x0A983},
    {0x0A9B3, 0x0A9C0},
    {0x0A9E5, 0x0A9E5},
    {0x0AA29, 0x0AA36},
    {0x0AA43, 0x0AA43},
    {0x0AA4C, 0x0AA4D},
    {0x0AA7B, 0x0AA7D},
    {0x0AAB0, 0x0AAB0},
    {0x0AAB2, 0x0AAB4},
    {0x0AAB7, 0x0AAB8},
    {0x0AABE, 0x0AABF},
    {0x0AAC1, 0x0AAC1},
    {0x0AAEB, 0x0AAEF},
    {0x0AAF5, 0x0AAF6},
    {0x0ABE3, 0x0ABEA},
    {0x0ABEC, 0x0ABED},
    {0x0FB1E, 0x0FB1E},
    {0x0FE00, 0x0FE0F},
    {0x0FE20, 0x0FE2F},
    {0x101FD, 0x101FD},
    {0x102E0, 0x102E0},
    {0x10376, 0x1037A},
    {0x10A01, 0x10A03},
    {0x10A05, 0x10A06},
    {0x10A0C, 0x10A0F},
    {0x10A38, 0x10A3A},
    {0x10A3F, 0x10A3F},
    {0x10AE5, 0x10AE6},
    {0x10D24, 0x10D27},
    {0x10EAB, 0x10EAC},
    {0x10EFD, 0x10EFF},
    {0x10F46, 0x10F50},
    {0x10F82, 0x10F85},
    {0x11000, 0x11002},
    {0x11038, 0x11046},
    {0x11070, 0x11070},
    {0x11073, 0x11074},
    {0x1107F, 0x11082},
    {0x110B0, 0x110BA},
    {0x110C2, 0x110C2},
    {0x11100, 0x11102},
    {0x11127, 0x11134},
    {0x11145, 0x11146},
    {0x11173, 0x11173},
    {0x11180, 0x11182},
    {0x111B3, 0x111C0},
    {0x111C9, 0x111CC},
    {0x111CE, 0x111CF},
    {0x1122C, 0x11237},
    {0x1123E, 0x1123E},
    {0x11241, 0x11241},
    {0x112DF, 0x112EA},
    {0x11300, 0x11303},
    {0x1133B, 0x1133C},
    {0x1133E, 0x11344},
    {0x11347, 0x11348},
    {0x1134B, 0x1134D},
    {0x11357, 0x11357},
    {0x11362, 0x11363},
    {0x11366, 0x1136C},
    {0x11370, 0x11374},
    {0x11435, 0x11446},
    {0x1145E, 0x1145E},
    {0x114B0, 0x114C3},
    {0x115AF, 0x115B5},
    {0x115B8, 0x115C0},
    {0x115DC, 0x115DD},
    {0x11630, 0x11640},
    {0x116AB, 0x116B7},
    {0x1171D, 0x1172B},
    {0x1182C, 0x1183A},
    {0x11930, 0x11935},
    {0x11937, 0x11938},
    {0x1193B, 0x1193E},
    {0x11940, 0x11940},
    {0x11942, 0x11943},
    {0x119D1, 0x119D7},
    {0x119DA, 0x119E0},
    {0x119E4, 0x119E4},
    {0x11A01, 0x11A0A},
    {0x11A33, 0x11A39},
    {0x11A3B, 0x11A3E},
    {0x11A47, 0x11A47},
    {0x11A51, 0x11A5B},
    {0x11A8A, 0x11A99},
    {0x11C2F, 0x11C36},
    {0x11C38, 0x11C3F},
    {0x11C92, 0x11CA7},
    {0x11CA9, 0x11CB6},
    {0x11D31, 0x11D36},
    {0x11D3A, 0x11D3A},
    {0x11D3C, 0x11D3D},
    {0x11D3F, 0x11D45},
    {0x11D47, 0x11D47},
    {0x11D8A, 0x11D8E},
    {0x11D90, 0x11D91},
    {0x11D93, 0x11D97},
    {0x11EF3, 0x11EF6},
    {0x11F00, 0x11F01},
    {0x11F03, 0x11F03},
    {0x11F34, 0x11F3A},
    {0x11F3E, 0x11F42},
    {0x13440, 0x13440},
    {0x13447, 0x13455},
    {0x16AF0, 0x16AF4},
    {0x16B30, 0x16B36},
    {0x16F4F, 0x16F4F},
    {0x16F51, 0x16F87},
    {0x16F8F, 0x16F92},
    {0x16FE4, 0x16FE4},
    {0x16FF0, 0x16FF1},
    {0x1BC9D, 0x1BC9E},
    {0x1CF00, 0x1CF2D},
    {0x1CF30, 0x1CF46},
    {0x1D165, 0x1D169},
    {0x1D16D, 0x1D172},
    {0x1D17B, 0x1D182},
    {0x1D185, 0x1D18B},
    {0x1D1AA, 0x1D1AD},
    {0x1D242, 0x1D244},
    {0x1DA00, 0x1DA36},
    {0x1DA3B, 0x1DA6C},
    {0x1DA75, 0x1DA75},
    {0x1DA84, 0x1DA84},
    {0x1DA9B, 0x1DA9F},
    {0x1DAA1, 0x1DAAF},
    {0x1E000, 0x1E006},
    {0x1E008, 0x1E018},
    {0x1E01B, 0x1E021},
    {0x1E023, 0x1E024},
    {0x1E026, 0x1E02A},
    {0x1E08F, 0x1E08F},
    {0x1E130, 0x1E136},
    {0x1E2AE, 0x1E2AE},
    {0x1E2EC, 0x1E2EF},
    {0x1E4EC, 0x1E4EF},
    {0x1E8D0, 0x1E8D6},
    {0x1E944, 0x1E94A},
    {0xE0100, 0xE01EF}
};
*/

/* Width 0 combining marks. */
static const struct uint32_range utf8_combining_table[] = {
    {0xCC80, 0xCDAF},
    {0xD283, 0xD289},
    {0xD691, 0xD6BD},
    {0xD6BF, 0xD6BF},
    {0xD781, 0xD782},
    {0xD784, 0xD785},
    {0xD787, 0xD787},
    {0xD890, 0xD89A},
    {0xD98B, 0xD99F},
    {0xD9B0, 0xD9B0},
    {0xDB96, 0xDB9C},
    {0xDB9F, 0xDBA4},
    {0xDBA7, 0xDBA8},
    {0xDBAA, 0xDBAD},
    {0xDC91, 0xDC91},
    {0xDCB0, 0xDD8A},
    {0xDEA6, 0xDEB0},
    {0xDFAB, 0xDFB3},
    {0xDFBD, 0xDFBD},
    {0xE0A096, 0xE0A099},
    {0xE0A09B, 0xE0A0A3},
    {0xE0A0A5, 0xE0A0A7},
    {0xE0A0A9, 0xE0A0AD},
    {0xE0A199, 0xE0A19B},
    {0xE0A298, 0xE0A29F},
    {0xE0A38A, 0xE0A3A1},
    {0xE0A3A3, 0xE0A483},
    {0xE0A4BA, 0xE0A4BC},
    {0xE0A4BE, 0xE0A58F},
    {0xE0A591, 0xE0A597},
    {0xE0A5A2, 0xE0A5A3},
    {0xE0A681, 0xE0A683},
    {0xE0A6BC, 0xE0A6BC},
    {0xE0A6BE, 0xE0A784},
    {0xE0A787, 0xE0A788},
    {0xE0A78B, 0xE0A78D},
    {0xE0A797, 0xE0A797},
    {0xE0A7A2, 0xE0A7A3},
    {0xE0A7BE, 0xE0A7BE},
    {0xE0A881, 0xE0A883},
    {0xE0A8BC, 0xE0A8BC},
    {0xE0A8BE, 0xE0A982},
    {0xE0A987, 0xE0A988},
    {0xE0A98B, 0xE0A98D},
    {0xE0A991, 0xE0A991},
    {0xE0A9B0, 0xE0A9B1},
    {0xE0A9B5, 0xE0A9B5},
    {0xE0AA81, 0xE0AA83},
    {0xE0AABC, 0xE0AABC},
    {0xE0AABE, 0xE0AB85},
    {0xE0AB87, 0xE0AB89},
    {0xE0AB8B, 0xE0AB8D},
    {0xE0ABA2, 0xE0ABA3},
    {0xE0ABBA, 0xE0ABBF},
    {0xE0AC81, 0xE0AC83},
    {0xE0ACBC, 0xE0ACBC},
    {0xE0ACBE, 0xE0AD84},
    {0xE0AD87, 0xE0AD88},
    {0xE0AD8B, 0xE0AD8D},
    {0xE0AD95, 0xE0AD97},
    {0xE0ADA2, 0xE0ADA3},
    {0xE0AE82, 0xE0AE82},
    {0xE0AEBE, 0xE0AF82},
    {0xE0AF86, 0xE0AF88},
    {0xE0AF8A, 0xE0AF8D},
    {0xE0AF97, 0xE0AF97},
    {0xE0B080, 0xE0B084},
    {0xE0B0BC, 0xE0B0BC},
    {0xE0B0BE, 0xE0B184},
    {0xE0B186, 0xE0B188},
    {0xE0B18A, 0xE0B18D},
    {0xE0B195, 0xE0B196},
    {0xE0B1A2, 0xE0B1A3},
    {0xE0B281, 0xE0B283},
    {0xE0B2BC, 0xE0B2BC},
    {0xE0B2BE, 0xE0B384},
    {0xE0B386, 0xE0B388},
    {0xE0B38A, 0xE0B38D},
    {0xE0B395, 0xE0B396},
    {0xE0B3A2, 0xE0B3A3},
    {0xE0B3B3, 0xE0B3B3},
    {0xE0B480, 0xE0B483},
    {0xE0B4BB, 0xE0B4BC},
    {0xE0B4BE, 0xE0B584},
    {0xE0B586, 0xE0B588},
    {0xE0B58A, 0xE0B58D},
    {0xE0B597, 0xE0B597},
    {0xE0B5A2, 0xE0B5A3},
    {0xE0B681, 0xE0B683},
    {0xE0B78A, 0xE0B78A},
    {0xE0B78F, 0xE0B794},
    {0xE0B796, 0xE0B796},
    {0xE0B798, 0xE0B79F},
    {0xE0B7B2, 0xE0B7B3},
    {0xE0B8B1, 0xE0B8B1},
    {0xE0B8B4, 0xE0B8BA},
    {0xE0B987, 0xE0B98E},
    {0xE0BAB1, 0xE0BAB1},
    {0xE0BAB4, 0xE0BABC},
    {0xE0BB88, 0xE0BB8E},
    {0xE0BC98, 0xE0BC99},
    {0xE0BCB5, 0xE0BCB5},
    {0xE0BCB7, 0xE0BCB7},
    {0xE0BCB9, 0xE0BCB9},
    {0xE0BCBE, 0xE0BCBF},
    {0xE0BDB1, 0xE0BE84},
    {0xE0BE86, 0xE0BE87},
    {0xE0BE8D, 0xE0BE97},
    {0xE0BE99, 0xE0BEBC},
    {0xE0BF86, 0xE0BF86},
    {0xE180AB, 0xE180BE},
    {0xE18196, 0xE18199},
    {0xE1819E, 0xE181A0},
    {0xE181A2, 0xE181A4},
    {0xE181A7, 0xE181AD},
    {0xE181B1, 0xE181B4},
    {0xE18282, 0xE1828D},
    {0xE1828F, 0xE1828F},
    {0xE1829A, 0xE1829D},
    {0xE18D9D, 0xE18D9F},
    {0xE19C92, 0xE19C95},
    {0xE19CB2, 0xE19CB4},
    {0xE19D92, 0xE19D93},
    {0xE19DB2, 0xE19DB3},
    {0xE19EB4, 0xE19F93},
    {0xE19F9D, 0xE19F9D},
    {0xE1A08B, 0xE1A08D},
    {0xE1A08F, 0xE1A08F},
    {0xE1A285, 0xE1A286},
    {0xE1A2A9, 0xE1A2A9},
    {0xE1A4A0, 0xE1A4AB},
    {0xE1A4B0, 0xE1A4BB},
    {0xE1A897, 0xE1A89B},
    {0xE1A995, 0xE1A99E},
    {0xE1A9A0, 0xE1A9BC},
    {0xE1A9BF, 0xE1A9BF},
    {0xE1AAB0, 0xE1AB8E},
    {0xE1AC80, 0xE1AC84},
    {0xE1ACB4, 0xE1AD84},
    {0xE1ADAB, 0xE1ADB3},
    {0xE1AE80, 0xE1AE82},
    {0xE1AEA1, 0xE1AEAD},
    {0xE1AFA6, 0xE1AFB3},
    {0xE1B0A4, 0xE1B0B7},
    {0xE1B390, 0xE1B392},
    {0xE1B394, 0xE1B3A8},
    {0xE1B3AD, 0xE1B3AD},
    {0xE1B3B4, 0xE1B3B4},
    {0xE1B3B7, 0xE1B3B9},
    {0xE1B780, 0xE1B7BF},
    {0xE28390, 0xE283B0},
    {0xE2B3AF, 0xE2B3B1},
    {0xE2B5BF, 0xE2B5BF},
    {0xE2B7A0, 0xE2B7BF},
    {0xE380AA, 0xE380AF},
    {0xE38299, 0xE3829A},
    {0xEA99AF, 0xEA99B2},
    {0xEA99B4, 0xEA99BD},
    {0xEA9A9E, 0xEA9A9F},
    {0xEA9BB0, 0xEA9BB1},
    {0xEAA082, 0xEAA082},
    {0xEAA086, 0xEAA086},
    {0xEAA08B, 0xEAA08B},
    {0xEAA0A3, 0xEAA0A7},
    {0xEAA0AC, 0xEAA0AC},
    {0xEAA280, 0xEAA281},
    {0xEAA2B4, 0xEAA385},
    {0xEAA3A0, 0xEAA3B1},
    {0xEAA3BF, 0xEAA3BF},
    {0xEAA4A6, 0xEAA4AD},
    {0xEAA587, 0xEAA593},
    {0xEAA680, 0xEAA683},
    {0xEAA6B3, 0xEAA780},
    {0xEAA7A5, 0xEAA7A5},
    {0xEAA8A9, 0xEAA8B6},
    {0xEAA983, 0xEAA983},
    {0xEAA98C, 0xEAA98D},
    {0xEAA9BB, 0xEAA9BD},
    {0xEAAAB0, 0xEAAAB0},
    {0xEAAAB2, 0xEAAAB4},
    {0xEAAAB7, 0xEAAAB8},
    {0xEAAABE, 0xEAAABF},
    {0xEAAB81, 0xEAAB81},
    {0xEAABAB, 0xEAABAF},
    {0xEAABB5, 0xEAABB6},
    {0xEAAFA3, 0xEAAFAA},
    {0xEAAFAC, 0xEAAFAD},
    {0xEFAC9E, 0xEFAC9E},
    {0xEFB880, 0xEFB88F},
    {0xEFB8A0, 0xEFB8AF},
    {0xF09087BD, 0xF09087BD},
    {0xF0908BA0, 0xF0908BA0},
    {0xF0908DB6, 0xF0908DBA},
    {0xF090A881, 0xF090A883},
    {0xF090A885, 0xF090A886},
    {0xF090A88C, 0xF090A88F},
    {0xF090A8B8, 0xF090A8BA},
    {0xF090A8BF, 0xF090A8BF},
    {0xF090ABA5, 0xF090ABA6},
    {0xF090B4A4, 0xF090B4A7},
    {0xF090BAAB, 0xF090BAAC},
    {0xF090BBBD, 0xF090BBBF},
    {0xF090BD86, 0xF090BD90},
    {0xF090BE82, 0xF090BE85},
    {0xF0918080, 0xF0918082},
    {0xF09180B8, 0xF0918186},
    {0xF09181B0, 0xF09181B0},
    {0xF09181B3, 0xF09181B4},
    {0xF09181BF, 0xF0918282},
    {0xF09182B0, 0xF09182BA},
    {0xF0918382, 0xF0918382},
    {0xF0918480, 0xF0918482},
    {0xF09184A7, 0xF09184B4},
    {0xF0918585, 0xF0918586},
    {0xF09185B3, 0xF09185B3},
    {0xF0918680, 0xF0918682},
    {0xF09186B3, 0xF0918780},
    {0xF0918789, 0xF091878C},
    {0xF091878E, 0xF091878F},
    {0xF09188AC, 0xF09188B7},
    {0xF09188BE, 0xF09188BE},
    {0xF0918981, 0xF0918981},
    {0xF0918B9F, 0xF0918BAA},
    {0xF0918C80, 0xF0918C83},
    {0xF0918CBB, 0xF0918CBC},
    {0xF0918CBE, 0xF0918D84},
    {0xF0918D87, 0xF0918D88},
    {0xF0918D8B, 0xF0918D8D},
    {0xF0918D97, 0xF0918D97},
    {0xF0918DA2, 0xF0918DA3},
    {0xF0918DA6, 0xF0918DAC},
    {0xF0918DB0, 0xF0918DB4},
    {0xF09190B5, 0xF0919186},
    {0xF091919E, 0xF091919E},
    {0xF09192B0, 0xF0919383},
    {0xF09196AF, 0xF09196B5},
    {0xF09196B8, 0xF0919780},
    {0xF091979C, 0xF091979D},
    {0xF09198B0, 0xF0919980},
    {0xF0919AAB, 0xF0919AB7},
    {0xF0919C9D, 0xF0919CAB},
    {0xF091A0AC, 0xF091A0BA},
    {0xF091A4B0, 0xF091A4B5},
    {0xF091A4B7, 0xF091A4B8},
    {0xF091A4BB, 0xF091A4BE},
    {0xF091A580, 0xF091A580},
    {0xF091A582, 0xF091A583},
    {0xF091A791, 0xF091A797},
    {0xF091A79A, 0xF091A7A0},
    {0xF091A7A4, 0xF091A7A4},
    {0xF091A881, 0xF091A88A},
    {0xF091A8B3, 0xF091A8B9},
    {0xF091A8BB, 0xF091A8BE},
    {0xF091A987, 0xF091A987},
    {0xF091A991, 0xF091A99B},
    {0xF091AA8A, 0xF091AA99},
    {0xF091B0AF, 0xF091B0B6},
    {0xF091B0B8, 0xF091B0BF},
    {0xF091B292, 0xF091B2A7},
    {0xF091B2A9, 0xF091B2B6},
    {0xF091B4B1, 0xF091B4B6},
    {0xF091B4BA, 0xF091B4BA},
    {0xF091B4BC, 0xF091B4BD},
    {0xF091B4BF, 0xF091B585},
    {0xF091B587, 0xF091B587},
    {0xF091B68A, 0xF091B68E},
    {0xF091B690, 0xF091B691},
    {0xF091B693, 0xF091B697},
    {0xF091BBB3, 0xF091BBB6},
    {0xF091BC80, 0xF091BC81},
    {0xF091BC83, 0xF091BC83},
    {0xF091BCB4, 0xF091BCBA},
    {0xF091BCBE, 0xF091BD82},
    {0xF0939180, 0xF0939180},
    {0xF0939187, 0xF0939195},
    {0xF096ABB0, 0xF096ABB4},
    {0xF096ACB0, 0xF096ACB6},
    {0xF096BD8F, 0xF096BD8F},
    {0xF096BD91, 0xF096BE87},
    {0xF096BE8F, 0xF096BE92},
    {0xF096BFA4, 0xF096BFA4},
    {0xF096BFB0, 0xF096BFB1},
    {0xF09BB29D, 0xF09BB29E},
    {0xF09CBC80, 0xF09CBCAD},
    {0xF09CBCB0, 0xF09CBD86},
    {0xF09D85A5, 0xF09D85A9},
    {0xF09D85AD, 0xF09D85B2},
    {0xF09D85BB, 0xF09D8682},
    {0xF09D8685, 0xF09D868B},
    {0xF09D86AA, 0xF09D86AD},
    {0xF09D8982, 0xF09D8984},
    {0xF09DA880, 0xF09DA8B6},
    {0xF09DA8BB, 0xF09DA9AC},
    {0xF09DA9B5, 0xF09DA9B5},
    {0xF09DAA84, 0xF09DAA84},
    {0xF09DAA9B, 0xF09DAA9F},
    {0xF09DAAA1, 0xF09DAAAF},
    {0xF09E8080, 0xF09E8086},
    {0xF09E8088, 0xF09E8098},
    {0xF09E809B, 0xF09E80A1},
    {0xF09E80A3, 0xF09E80A4},
    {0xF09E80A6, 0xF09E80AA},
    {0xF09E828F, 0xF09E828F},
    {0xF09E84B0, 0xF09E84B6},
    {0xF09E8AAE, 0xF09E8AAE},
    {0xF09E8BAC, 0xF09E8BAF},
    {0xF09E93AC, 0xF09E93AF},
    {0xF09EA390, 0xF09EA396},
    {0xF09EA584, 0xF09EA58A},
    {0xF3A08480, 0xF3A087AF},
};

/*
static const struct uint32_range utf32_combiningletters_table[] = {
    {0x01160, 0x011FF},
    {0x0D7B0, 0x0D7FF}
};
*/

/* Width 0 combining letters. */
static const struct uint32_range utf8_combiningletters_table[] = {
    {0xE185A0, 0xE187BF},
    {0xED9EB0, 0xED9FBF},
};

/*
static const struct uint32_range utf32_doublewide_table[] = {
    {0x01100, 0x0115F},
    {0x02329, 0x0232A},
    {0x02E80, 0x02E99},
    {0x02E9B, 0x02EF3},
    {0x02F00, 0x02FD5},
    {0x02FF0, 0x02FFB},
    {0x03000, 0x0303E},
    {0x03041, 0x03096},
    {0x03099, 0x030FF},
    {0x03105, 0x0312F},
    {0x03131, 0x0318E},
    {0x03190, 0x031E3},
    {0x031F0, 0x0321E},
    {0x03220, 0x03247},
    {0x03250, 0x04DBF},
    {0x04E00, 0x0A48C},
    {0x0A490, 0x0A4C6},
    {0x0A960, 0x0A97C},
    {0x0AC00, 0x0D7A3},
    {0x0F900, 0x0FAFF},
    {0x0FE10, 0x0FE19},
    {0x0FE30, 0x0FE52},
    {0x0FE54, 0x0FE66},
    {0x0FE68, 0x0FE6B},
    {0x0FF01, 0x0FF60},
    {0x0FFE0, 0x0FFE6},
    {0x16FE0, 0x16FE4},
    {0x16FF0, 0x16FF1},
    {0x17000, 0x187F7},
    {0x18800, 0x18CD5},
    {0x18D00, 0x18D08},
    {0x1AFF0, 0x1AFF3},
    {0x1AFF5, 0x1AFFB},
    {0x1AFFD, 0x1AFFE},
    {0x1B000, 0x1B122},
    {0x1B132, 0x1B132},
    {0x1B150, 0x1B152},
    {0x1B155, 0x1B155},
    {0x1B164, 0x1B167},
    {0x1B170, 0x1B2FB},
    {0x1F200, 0x1F200},
    {0x1F202, 0x1F202},
    {0x1F210, 0x1F219},
    {0x1F21B, 0x1F22E},
    {0x1F230, 0x1F231},
    {0x1F237, 0x1F237},
    {0x1F23B, 0x1F23B},
    {0x1F240, 0x1F248},
    {0x1F260, 0x1F265},
    {0x1F57A, 0x1F57A},
    {0x1F5A4, 0x1F5A4},
    {0x1F6D1, 0x1F6D2},
    {0x1F6D5, 0x1F6D7},
    {0x1F6DC, 0x1F6DF},
    {0x1F6F4, 0x1F6FC},
    {0x1F7E0, 0x1F7EB},
    {0x1F7F0, 0x1F7F0},
    {0x1F90C, 0x1F90F},
    {0x1F919, 0x1F93A},
    {0x1F93C, 0x1F945},
    {0x1F947, 0x1F97F},
    {0x1F985, 0x1F9BF},
    {0x1F9C1, 0x1F9FF},
    {0x1FA70, 0x1FA7C},
    {0x1FA80, 0x1FA88},
    {0x1FA90, 0x1FABD},
    {0x1FABF, 0x1FAC5},
    {0x1FACE, 0x1FADB},
    {0x1FAE0, 0x1FAE8},
    {0x1FAF0, 0x1FAF8},
    {0x20000, 0x2FFFD},
    {0x30000, 0x3FFFD}
};
*/

static const struct uint32_range utf8_doublewide_table[] = {
    {0xE18480, 0xE1859F},
    {0xE28CA9, 0xE28CAA},
    {0xE2BA80, 0xE2BA99},
    {0xE2BA9B, 0xE2BBB3},
    {0xE2BC80, 0xE2BF95},
    {0xE2BFB0, 0xE2BFBB},
    {0xE38080, 0xE380BE},
    {0xE38181, 0xE38296},
    {0xE38299, 0xE383BF},
    {0xE38485, 0xE384AF},
    {0xE384B1, 0xE3868E},
    {0xE38690, 0xE387A3},
    {0xE387B0, 0xE3889E},
    {0xE388A0, 0xE38987},
    {0xE38990, 0xE4B6BF},
    {0xE4B880, 0xEA928C},
    {0xEA9290, 0xEA9386},
    {0xEAA5A0, 0xEAA5BC},
    {0xEAB080, 0xED9EA3},
    {0xEFA480, 0xEFABBF},
    {0xEFB890, 0xEFB899},
    {0xEFB8B0, 0xEFB992},
    {0xEFB994, 0xEFB9A6},
    {0xEFB9A8, 0xEFB9AB},
    {0xEFBC81, 0xEFBDA0},
    {0xEFBFA0, 0xEFBFA6},
    {0xF096BFA0, 0xF096BFA4},
    {0xF096BFB0, 0xF096BFB1},
    {0xF0978080, 0xF0989FB7},
    {0xF098A080, 0xF098B395},
    {0xF098B480, 0xF098B488},
    {0xF09ABFB0, 0xF09ABFB3},
    {0xF09ABFB5, 0xF09ABFBB},
    {0xF09ABFBD, 0xF09ABFBE},
    {0xF09B8080, 0xF09B84A2},
    {0xF09B84B2, 0xF09B84B2},
    {0xF09B8590, 0xF09B8592},
    {0xF09B8595, 0xF09B8595},
    {0xF09B85A4, 0xF09B85A7},
    {0xF09B85B0, 0xF09B8BBB},
    {0xF09F8880, 0xF09F8880},
    {0xF09F8882, 0xF09F8882},
    {0xF09F8890, 0xF09F8899},
    {0xF09F889B, 0xF09F88AE},
    {0xF09F88B0, 0xF09F88B1},
    {0xF09F88B7, 0xF09F88B7},
    {0xF09F88BB, 0xF09F88BB},
    {0xF09F8980, 0xF09F8988},
    {0xF09F89A0, 0xF09F89A5},
    {0xF09F95BA, 0xF09F95BA},
    {0xF09F96A4, 0xF09F96A4},
    {0xF09F9B91, 0xF09F9B92},
    {0xF09F9B95, 0xF09F9B97},
    {0xF09F9B9C, 0xF09F9B9F},
    {0xF09F9BB4, 0xF09F9BBC},
    {0xF09F9FA0, 0xF09F9FAB},
    {0xF09F9FB0, 0xF09F9FB0},
    {0xF09FA48C, 0xF09FA48F},
    {0xF09FA499, 0xF09FA4BA},
    {0xF09FA4BC, 0xF09FA585},
    {0xF09FA587, 0xF09FA5BF},
    {0xF09FA685, 0xF09FA6BF},
    {0xF09FA781, 0xF09FA7BF},
    {0xF09FA9B0, 0xF09FA9BC},
    {0xF09FAA80, 0xF09FAA88},
    {0xF09FAA90, 0xF09FAABD},
    {0xF09FAABF, 0xF09FAB85},
    {0xF09FAB8E, 0xF09FAB9B},
    {0xF09FABA0, 0xF09FABA8},
    {0xF09FABB0, 0xF09FABB8},
    {0xF0A08080, 0xF0AFBFBD},
    {0xF0B08080, 0xF0BFBFBD},
};

/*
static const struct uint32_range utf32_widened_table[] = {
    {0x0231A, 0x0231B},
    {0x023E9, 0x023EC},
    {0x023F0, 0x023F0},
    {0x023F3, 0x023F3},
    {0x025FD, 0x025FE},
    {0x02614, 0x02615},
    {0x02648, 0x02653},
    {0x0267F, 0x0267F},
    {0x02693, 0x02693},
    {0x026A1, 0x026A1},
    {0x026AA, 0x026AB},
    {0x026BD, 0x026BE},
    {0x026C4, 0x026C5},
    {0x026CE, 0x026CE},
    {0x026D4, 0x026D4},
    {0x026EA, 0x026EA},
    {0x026F2, 0x026F3},
    {0x026F5, 0x026F5},
    {0x026FA, 0x026FA},
    {0x026FD, 0x026FD},
    {0x02705, 0x02705},
    {0x0270A, 0x0270B},
    {0x02728, 0x02728},
    {0x0274C, 0x0274C},
    {0x0274E, 0x0274E},
    {0x02753, 0x02755},
    {0x02757, 0x02757},
    {0x02795, 0x02797},
    {0x027B0, 0x027B0},
    {0x027BF, 0x027BF},
    {0x02B1B, 0x02B1C},
    {0x02B50, 0x02B50},
    {0x02B55, 0x02B55},
    {0x1F004, 0x1F004},
    {0x1F0CF, 0x1F0CF},
    {0x1F18E, 0x1F18E},
    {0x1F191, 0x1F19A},
    {0x1F201, 0x1F201},
    {0x1F21A, 0x1F21A},
    {0x1F22F, 0x1F22F},
    {0x1F232, 0x1F236},
    {0x1F238, 0x1F23A},
    {0x1F250, 0x1F251},
    {0x1F300, 0x1F320},
    {0x1F32D, 0x1F335},
    {0x1F337, 0x1F37C},
    {0x1F37E, 0x1F393},
    {0x1F3A0, 0x1F3CA},
    {0x1F3CF, 0x1F3D3},
    {0x1F3E0, 0x1F3F0},
    {0x1F3F4, 0x1F3F4},
    {0x1F3F8, 0x1F43E},
    {0x1F440, 0x1F440},
    {0x1F442, 0x1F4FC},
    {0x1F4FF, 0x1F53D},
    {0x1F54B, 0x1F54E},
    {0x1F550, 0x1F567},
    {0x1F595, 0x1F596},
    {0x1F5FB, 0x1F64F},
    {0x1F680, 0x1F6C5},
    {0x1F6CC, 0x1F6CC},
    {0x1F6D0, 0x1F6D0},
    {0x1F6EB, 0x1F6EC},
    {0x1F910, 0x1F918},
    {0x1F980, 0x1F984},
    {0x1F9C0, 0x1F9C0}
};
*/

/* Characters that were widened from width 1 to 2 in Unicode 9. */
static const struct uint32_range utf8_widened_table[] = {
    {0xE28C9A, 0xE28C9B},
    {0xE28FA9, 0xE28FAC},
    {0xE28FB0, 0xE28FB0},
    {0xE28FB3, 0xE28FB3},
    {0xE297BD, 0xE297BE},
    {0xE29894, 0xE29895},
    {0xE29988, 0xE29993},
    {0xE299BF, 0xE299BF},
    {0xE29A93, 0xE29A93},
    {0xE29AA1, 0xE29AA1},
    {0xE29AAA, 0xE29AAB},
    {0xE29ABD, 0xE29ABE},
    {0xE29B84, 0xE29B85},
    {0xE29B8E, 0xE29B8E},
    {0xE29B94, 0xE29B94},
    {0xE29BAA, 0xE29BAA},
    {0xE29BB2, 0xE29BB3},
    {0xE29BB5, 0xE29BB5},
    {0xE29BBA, 0xE29BBA},
    {0xE29BBD, 0xE29BBD},
    {0xE29C85, 0xE29C85},
    {0xE29C8A, 0xE29C8B},
    {0xE29CA8, 0xE29CA8},
    {0xE29D8C, 0xE29D8C},
    {0xE29D8E, 0xE29D8E},
    {0xE29D93, 0xE29D95},
    {0xE29D97, 0xE29D97},
    {0xE29E95, 0xE29E97},
    {0xE29EB0, 0xE29EB0},
    {0xE29EBF, 0xE29EBF},
    {0xE2AC9B, 0xE2AC9C},
    {0xE2AD90, 0xE2AD90},
    {0xE2AD95, 0xE2AD95},
    {0xF09F8084, 0xF09F8084},
    {0xF09F838F, 0xF09F838F},
    {0xF09F868E, 0xF09F868E},
    {0xF09F8691, 0xF09F869A},
    {0xF09F8881, 0xF09F8881},
    {0xF09F889A, 0xF09F889A},
    {0xF09F88AF, 0xF09F88AF},
    {0xF09F88B2, 0xF09F88B6},
    {0xF09F88B8, 0xF09F88BA},
    {0xF09F8990, 0xF09F8991},
    {0xF09F8C80, 0xF09F8CA0},
    {0xF09F8CAD, 0xF09F8CB5},
    {0xF09F8CB7, 0xF09F8DBC},
    {0xF09F8DBE, 0xF09F8E93},
    {0xF09F8EA0, 0xF09F8F8A},
    {0xF09F8F8F, 0xF09F8F93},
    {0xF09F8FA0, 0xF09F8FB0},
    {0xF09F8FB4, 0xF09F8FB4},
    {0xF09F8FB8, 0xF09F90BE},
    {0xF09F9180, 0xF09F9180},
    {0xF09F9182, 0xF09F93BC},
    {0xF09F93BF, 0xF09F94BD},
    {0xF09F958B, 0xF09F958E},
    {0xF09F9590, 0xF09F95A7},
    {0xF09F9695, 0xF09F9696},
    {0xF09F97BB, 0xF09F998F},
    {0xF09F9A80, 0xF09F9B85},
    {0xF09F9B8C, 0xF09F9B8C},
    {0xF09F9B90, 0xF09F9B90},
    {0xF09F9BAB, 0xF09F9BAC},
    {0xF09FA490, 0xF09FA498},
    {0xF09FA680, 0xF09FA684},
    {0xF09FA780, 0xF09FA780},
};

size_t GetWidthUTF8(uint32_t utf8_rune) {
    for (const uint32_range& range : utf8_combining_table) {
        if (utf8_rune < range.lo)
            break;
        if (range.lo <= utf8_rune && utf8_rune <= range.hi)
            return 0;
    }
    for (const uint32_range& range : utf8_combiningletters_table) {
        if (utf8_rune < range.lo)
            break;
        if (range.lo <= utf8_rune && utf8_rune <= range.hi)
            return 0;
    }
    for (const uint32_range& range : utf8_doublewide_table) {
        if (utf8_rune < range.lo)
            break;
        if (range.lo <= utf8_rune && utf8_rune <= range.hi)
            return 2;
    }
    for (const uint32_range& range : utf8_widened_table) {
        if (utf8_rune < range.lo)
            break;
        if (range.lo <= utf8_rune && utf8_rune <= range.hi)
            return 2;
    }
    return 1;
}

/*
// This function was used to convert utf32 tables to utf8 tables
uint32_t UTF32toUTF8(uint32_t rune) {
	uint8_t b, c, d, e;
    uint32_t n;

	if (rune < 0x80) {  // ASCII bytes represent themselves
        return rune;
	} else if (rune < 0x800) {  // two-byte encoding
		c = (uint8_t) (rune & 0x3F);
		c |= 0x80;
		rune >>= 6;
		b = (uint8_t) (rune & 0x1F);
		b |= 0xC0;
        n = b;
        n <<= 8;
        n += c;
        return n;
	} else if (rune < 0x10000) {  // three-byte encoding
		d = (uint8_t) (rune & 0x3F);
		d |= 0x80;
		rune >>= 6;
		c = (uint8_t) (rune & 0x3F);
		c |= 0x80;
		rune >>= 6;
		b = (uint8_t) (rune & 0x0F);
		b |= 0xE0;
		n = b;
        n <<= 8;
        n += c;
        n <<= 8;
        n += d;
        return n;
	}
	// otherwise use a four-byte encoding
	e = (uint8_t) (rune & 0x3F);
	e |= 0x80;
	rune >>= 6;
	d = (uint8_t) (rune & 0x3F);
	d |= 0x80;
	rune >>= 6;
	c = (uint8_t) (rune & 0x3F);
	c |= 0x80;
	rune >>= 6;
	b = (uint8_t) (rune & 0x07);
	b |= 0xF0;
	n = b;
    n <<= 8;
    n += c;
    n <<= 8;
    n += d;
    n <<= 8;
    n += e;
	return n;
}
*/
