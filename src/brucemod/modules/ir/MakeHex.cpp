#include "MakeHex.h"
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace MakeHex {

// Extended protocol definitions
static const ProtocolDefinition protocolDefinitions[] = {
    { "DAC4", "Frequency=38000\nZero=500,-1000\nOne=500,-500\ndefine X=(D+F)^1\ndefine Y=(1+D+F)^1\nFirst Bit=MSB\nForm=7000,-2800,0:1,D:8,F:8,X:8,500,-60m;7000,-2800,1:1,D:8,F:8,Y:8,500,-60m\n" },
    { "nec1", "Protocol=NEC\nFrequency=38000\nTime Base=564\nOne=1,-3\nZero=1,-1\nPrefix=16,-8\nSuffix=1,-78\nR-Prefix=16,-4\nR-Suffix=1,-174\nDefault S=~D\nForm=*,D:8,S:8,F:8,~F:8,_;*,_\n" },
    { "nec2", "Protocol=NEC2\nFrequency=38000\nTime Base=564\nOne=1,-3\nZero=1,-1\nPrefix=16,-8\nSuffix=1,-78\nDefault S=~D\nForm=;*,D:8,S:8,F:8,~F:8,_\n" },
    { "rc5", "Protocol=RC5\nFrequency=36000\nTime Base=889\nMessage Time=128\nZero=1,-1\nOne=-1,1\nPrefix=1\nFirst Bit=MSB\nForm=;*,~F:1:6,T:1,D:5,F:6\n" },
    { "rc6", "Protocol=RC6\nFrequency=36000\nTime Base=444\nMessage Time=107m\nZero=-1,1\nOne=1,-1\nPrefix=6,-2,1,-1\nFirst Bit=MSB\nForm=;*,M:3,(4*T-2),(2-4*T),D:8,F:8\n" },
    { "sony12", "Protocol=Sony12\nFrequency=40000\nTime Base=600\nZero=1,-1\nOne=2,-1\nPrefix=4,-1\nMessage Time=45m\nForm=;*,f:7,d:5\n" },
    { "sony15", "Protocol=Sony15\nFrequency=40000\nTime Base=600\nZero=1,-1\nOne=2,-1\nPrefix=4,-1\nMessage Time=45m\nForm=;*,f:7,d:8\n" },
    { "sony20", "Protocol=Sony20\nFrequency=40000\nTime Base=600\nZero=1,-1\nOne=2,-1\nPrefix=4,-1\nMessage Time=45m\nForm=;*,f:7,d:5,s:8\n" },
    { "panasonic", "Protocol= Panasonic\nFrequency=37000\nTime Base=432\nZero=1,-1\nOne=1,-3\nPrefix=8,-4\nDefault S=0\nDefine C=D^S^F\nSuffix=1,-173\nForm=;*,2:8,32:8,D:8,S:8,F:8,C:8,_\n" },
    { "samsung32", "Frequency=38000\nTime Base=564\nOne=1,-3\nZero=1,-1\nPrefix=8,-8\nSuffix=1,-78\nForm=;*,D:8,D:8,F:8,~F:8,_\n" },
    { "jvc", "Protocol=JVC\nFrequency=38000\nTime Base=527\nZero=1,-1\nOne=1,-3\nPrefix=16,-8\nSuffix=1,-45\nForm=;D:8,F:8,_\n" },
    { "rca", "Protocol=RCA\nFrequency=56000\nTime Base=500\nZero=1,-2\nOne=2,-1\nPrefix=8,-8\nSuffix=1,-100\nForm=;*,D:4,F:8,~D:4,~F:8,_\n" }
};
static const int protocolCount = sizeof(protocolDefinitions) / sizeof(protocolDefinitions[0]);

// Compile-time bitmask generation
template<size_t N>
struct MaskTable {
    unsigned int values[N];
    constexpr MaskTable() : values{} {
        values[0] = 0;
        for (size_t i = 1; i < N; ++i) {
            values[i] = 2 * values[i - 1] + 1;
        }
    }
};
static constexpr auto m_mask_table = MaskTable<33>();

class IRP {
public:
    IRP();
    ~IRP();
    bool readIrpString(const char* str);
    void generate(int* s, int* r, float* raw, size_t maxSize);
    int m_value[26];
    int m_frequency;

private:
    struct Value {
        double m_val;
        int m_bits;
    };
    unsigned int reverse(unsigned int Number);
    bool match(const char* master);
    void setDigit(int d);
    char* copy();
    void getPair(int* result);
    void parseVal(Value& result, char*& in, int prec = 0);
    void genHex(float number);
    int genHex(const char* Pattern);
    char* m_digits[16];
    int m_timeBase;
    int m_messageTime;
    int m_device[2];
    int m_functions[4];
    char* m_form;
    char* m_prefix;
    char* m_suffix;
    char* m_rPrefix;
    char* m_rSuffix;
    bool m_msb;
    char* m_def[26];
    char m_bufr[1024];
    const char* m_next;
    int m_bitGroup;
    int m_pendingBits;
    double m_cumulative;
    std::vector<float> m_hex;
};

IRP::IRP() 
    : m_frequency(38400), 
      m_timeBase(1), 
      m_messageTime(0), 
      m_form(nullptr), 
      m_prefix(nullptr), 
      m_suffix(nullptr), 
      m_rPrefix(nullptr), 
      m_rSuffix(nullptr), 
      m_msb(false), 
      m_next(nullptr),
      m_bitGroup(2), 
      m_pendingBits(0),
      m_cumulative(0.0) 
{
    memset(m_value, 0, sizeof(m_value));
    memset(m_def, 0, sizeof(m_def));
    memset(m_device, -1, sizeof(m_device));
    memset(m_functions, -1, sizeof(m_functions));
    memset(m_digits, 0, sizeof(m_digits));
    memset(m_bufr, 0, sizeof(m_bufr));
}

IRP::~IRP() {
    for (int i = 0; i < 26; ++i) if (m_def[i]) free(m_def[i]);
    for (int i = 0; i < 16; ++i) if (m_digits[i]) free(m_digits[i]);
    if (m_prefix) free(m_prefix); 
    if (m_suffix) free(m_suffix);
    if (m_rPrefix) free(m_rPrefix); 
    if (m_rSuffix) free(m_rSuffix);
    if (m_form) free(m_form);
}

bool IRP::match(const char* master) {
    int len = strlen(master);
    m_next = m_bufr + len;
    return 0 == memcmp(m_bufr, master, len);
}

void IRP::setDigit(int d) {
    if (m_digits[d]) free(m_digits[d]);
    m_digits[d] = copy();
    while (d >= m_bitGroup) m_bitGroup <<= 1;
}

char* IRP::copy() { return strdup(m_next); }

void IRP::getPair(int* result) {
    for (int nIndex = 0; nIndex < 2; nIndex++) {
        int num = 0;
        for (; *m_next; m_next++) {
            if (*m_next >= '0' && *m_next <= '9') num = num * 10 + *m_next - '0';
            else break;
        }
        result[nIndex] = num;
        if (m_next[0] != '.' || m_next[1] < '0' || m_next[1] > '9') break;
        m_next++;
    }
}

unsigned int IRP::reverse(unsigned int Number) {
    Number = ((Number & 0x55555555) << 1) + ((Number >> 1) & 0x55555555);
    Number = ((Number & 0x33333333) << 2) + ((Number >> 2) & 0x33333333);
    Number = ((Number & 0xF0F0F0F)  << 4) + ((Number >> 4) & 0xF0F0F0F);
    Number = ((Number & 0xFF00FF)   << 8) + ((Number >> 8) & 0xFF00FF);
    return (Number >> 16) + (Number << 16);
}

void IRP::parseVal(Value& result, char*& in, int prec) {
    if (*in >= 'A' && *in <= 'Z') {
        int ndx = *(in++) - 'A';
        const char* in2 = m_def[ndx];
        if (in2) {
            parseVal(result, const_cast<char*&>(in2), 0);
        } else {
            result.m_val = m_value[ndx];
            result.m_bits = 0;
        }
    } else if (*in >= '0' && *in <= '9') {
        result.m_bits = 0; result.m_val = 0.0;
        do { result.m_val = result.m_val * 10 + *(in++) - '0'; } while (*in >= '0' && *in <= '9');
    } else switch (*in) {
        case '-': ++in; parseVal(result, in, 1); result.m_val = -result.m_val; if (result.m_bits > 0) result.m_bits = 0; break;
        case '~': ++in; parseVal(result, in, 1); result.m_val = -(result.m_val + 1); if (result.m_bits > 0) result.m_val = (double)(((int)result.m_val) & m_mask_table.values[result.m_bits]); break;
        case '(': ++in; parseVal(result, in, 0); if (*in == ')') ++in; break;
    }
    if (*in == 'M') { result.m_val *= 1000; result.m_bits = -1; ++in; }
    else if (*in == 'U') { result.m_bits = -1; ++in; }
    for (;;) {
        Value v2;
        if (prec < 2 && *in == '*') { ++in; parseVal(v2, in, 2); result.m_val *= v2.m_val; if (result.m_bits > 0) result.m_bits = 0; continue; }
        if (prec < 1) {
            switch (*in) {
                case '+': ++in; parseVal(v2, in, 1); result.m_val += v2.m_val; if (result.m_bits > 0) result.m_bits = 0; continue;
                case '-': ++in; parseVal(v2, in, 1); result.m_val -= v2.m_val; if (result.m_bits > 0) result.m_bits = 0; continue;
                case '^': ++in; parseVal(v2, in, 1); result.m_val = ((int)result.m_val) ^ ((int)v2.m_val); if (result.m_bits > 0 && (v2.m_bits <= 0 || v2.m_bits > result.m_bits)) result.m_bits = v2.m_bits; continue;
            }
        }
        if (prec < 3 && *in == ':') {
            ++in; parseVal(v2, in, 3); result.m_bits = (int)v2.m_val;
            if (*in == ':') { ++in; parseVal(v2, in, 3); result.m_val = (double)(((int)result.m_val) >> ((int)v2.m_val)); }
            if (result.m_bits < 0) { result.m_bits = -result.m_bits; result.m_val = (double)(reverse((int)result.m_val) >> (32 - result.m_bits)); }
            result.m_val = (double)(((int)result.m_val) & m_mask_table.values[result.m_bits]);
            continue;
        }
        break;
    }
}

bool IRP::readIrpString(const char* str) {
    if (!str) return false;
    char temp[1024]; strncpy(temp, str, 1024);
    char* line = strtok(temp, "\n");
    while (line != NULL && line[0] != 0) {
        strcpy(m_bufr, line);
        char* cur = m_bufr;
        Value val;
        for (char* l2 = cur; *l2 && *l2 != '\'' && *l2 != '\n'; l2++) {
            if (*l2 != ' ' && *l2 != '\t') *(cur++) = toupper(*l2);
        }
        *cur = 0;
        if (match("FREQUENCY=")) { parseVal(val, const_cast<char*&>(m_next)); m_frequency = (int)val.m_val; }
        else if (match("TIMEBASE=")) { parseVal(val, const_cast<char*&>(m_next)); m_timeBase = (int)val.m_val; }
        else if (match("MESSAGETIME=")) { parseVal(val, const_cast<char*&>(m_next)); if (val.m_bits == 0) val.m_val *= m_timeBase; m_messageTime = (int)val.m_val; }
        else if (m_bufr[0] >= '0' && m_bufr[0] <= '9' && m_bufr[1] == '=') { m_next = m_bufr + 2; setDigit(m_bufr[0] - '0'); }
        else if (match("ZERO=")) setDigit(0);
        else if (match("ONE=")) setDigit(1);
        else if (match("PREFIX=")) { if(m_prefix) free(m_prefix); m_prefix = copy(); }
        else if (match("SUFFIX=")) { if(m_suffix) free(m_suffix); m_suffix = copy(); }
        else if (match("FIRSTBIT=MSB")) m_msb = true;
        else if (match("FORM=")) { if(m_form) free(m_form); m_form = copy(); }
        else if (match("DEFINE") || match("DEFAULT")) {
            if (m_next[1] == '=') { m_next += 2; if (m_def[m_next[-2] - 'A']) free(m_def[m_next[-2] - 'A']); m_def[m_next[-2] - 'A'] = copy(); }
        }
        line = strtok(NULL, "\n");
    }
    return true;
}

void IRP::generate(int* s, int* r, float* raw, size_t maxSize) {
    m_hex.clear(); m_cumulative = 0.0; m_pendingBits = (m_msb ? 1 : m_bitGroup);
    int Single = genHex(m_form);
    if (m_cumulative < m_messageTime) genHex((float)(m_cumulative - m_messageTime));
    if (m_hex.size() & 1) genHex(-1.0f);
    if (Single < 0) Single = (int)m_hex.size();
    Single >>= 1;
    
    size_t outSize = m_hex.size();
    if (outSize > maxSize) outSize = maxSize;

    for (size_t nIndex = 0; nIndex < outSize; nIndex++) raw[nIndex] = m_hex[nIndex];
    *s = (Single < (int)(outSize/2)) ? Single : (int)(outSize/2); 
    *r = (int)(outSize / 2) - *s;
}

int IRP::genHex(const char* Pattern) {
    if (!Pattern) return -1;
    int Result = -1;
    if (*Pattern == ';') { Result = 0; Pattern++; }
    while (*Pattern) {
        if (*Pattern == '*') { genHex((float)((Result >= 0 && m_rPrefix) ? (size_t)m_rPrefix : (size_t)m_prefix)); Pattern++; }
        else if (*Pattern == '_') { genHex((float)((Result >= 0 && m_rSuffix) ? (size_t)m_rSuffix : (size_t)m_suffix)); Pattern++; if (m_cumulative < m_messageTime) genHex((float)(m_cumulative - m_messageTime)); }
        else if (*Pattern == '^') { Pattern++; Value val; parseVal(val, const_cast<char*&>(Pattern)); if (val.m_bits == 0) val.m_val *= m_timeBase; if (m_cumulative < val.m_val) genHex((float)(m_cumulative - val.m_val)); }
        else {
            Value val; parseVal(val, const_cast<char*&>(Pattern));
            if (val.m_bits == 0) val.m_val *= m_timeBase;
            if (val.m_bits <= 0) genHex((float)val.m_val);
            else {
                int Number = (int)(val.m_val);
                if (m_msb) Number = (int)(reverse((unsigned int)Number) >> (32 - val.m_bits));
                while (--val.m_bits >= 0) {
                    if (m_msb) { m_pendingBits = (m_pendingBits << 1) + (Number & 1); if (m_pendingBits & m_bitGroup) { genHex((float)(size_t)m_digits[m_pendingBits - m_bitGroup]); m_pendingBits = 1; } }
                    else { m_pendingBits = (m_pendingBits >> 1) + (Number & 1) * m_bitGroup; if (m_pendingBits & 1) { genHex((float)(size_t)m_digits[m_pendingBits >> 1]); m_pendingBits = m_bitGroup; } }
                    Number >>= 1;
                }
            }
        }
        if (*Pattern == ';') { if (m_cumulative < m_messageTime) genHex((float)(m_cumulative - m_messageTime)); if (m_hex.size() & 1) genHex(-1.0f); Result = (int)m_hex.size(); m_cumulative = 0.0; }
        else if (*Pattern != ',') break;
        Pattern++;
    }
    return Result;
}

void IRP::genHex(float number) {
    if (number == 0.0f) return;
    int nHex = (int)m_hex.size();
    if (number > 0) { m_cumulative += number; if (nHex & 1) m_hex[nHex - 1] += number; else m_hex.push_back(number); }
    else if (nHex) { m_cumulative -= number; if (nHex & 1) m_hex.push_back(-number); else m_hex[nHex - 1] -= number; }
}

std::vector<float> encodeRemoteCommand(const RemoteCommand& cmd, const char* protocolString, int& frequency) {
    int p = -1;
    for (int i = 0; i < protocolCount; i++) {
        if (strcasecmp(protocolString, protocolDefinitions[i].name) == 0) { p = i; break; }
    }
    if (p < 0) return {};
    IRP Irp;
    Irp.readIrpString(protocolDefinitions[p].def);
    Irp.m_value['D' - 'A'] = cmd.device;
    Irp.m_value['S' - 'A'] = cmd.subdevice;
    Irp.m_value['F' - 'A'] = cmd.function;
    int s, r; float seq[512]; Irp.generate(&s, &r, seq, 512);
    frequency = Irp.m_frequency;
    return std::vector<float>(seq, seq + 2 * (s + r));
}

}
