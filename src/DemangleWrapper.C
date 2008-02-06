#include <Base.h>
#include <DemangleWrapper.h>
#include <BinaryFile.h>

#define DMGL_NO_OPTS     0              /* For readability... */
#define DMGL_PARAMS      (1 << 0)       /* Include function args */
#define DMGL_ANSI        (1 << 1)       /* Include const, volatile, etc */
#define DMGL_JAVA        (1 << 2)       /* Demangle as Java rather than C++. */
#define DMGL_VERBOSE     (1 << 3)       /* Include implementation details.  */
#define DMGL_TYPES       (1 << 4)       /* Also try to demangle type encodings.  */
#define DMGL_AUTO        (1 << 8)
#define DMGL_GNU         (1 << 9)
#define DMGL_LUCID       (1 << 10)
#define DMGL_ARM         (1 << 11)
#define DMGL_HP          (1 << 12)       /* For the HP aCC compiler; */
#define DMGL_EDG         (1 << 13)
#define DMGL_GNU_V3      (1 << 14)
#define DMGL_GNAT        (1 << 15)
#define DMGL_STYLE_MASK (DMGL_AUTO|DMGL_GNU|DMGL_LUCID|DMGL_ARM|DMGL_HP|DMGL_EDG|DMGL_GNU_V3|DMGL_JAVA|DMGL_GNAT)


#ifdef USE_DEMANGLERS

extern "C" char* cplus_demangle(const char *mangled, int32_t options);

typedef struct {} *Name;
extern "C" Name* demangle(char*, char**, uint32_t);
extern "C" char* text(Name*);

#endif

void DemangleWrapper::init(){
}


void DemangleWrapper::dest(){
}

const char* __gnu_identifier__ = "GNU";
const char* __xlc_identifier__ = "XLC";
const char* __non_identifier__ = "NON";

char* DemangleWrapper::demangle_combined(char* name,const char** which){
    if(demangled){
        free(demangled);
        demangled = NULL;
    }

#ifdef USE_DEMANGLERS

    if ('\0' == name[0])
        return name;

    char* pre = name;
    while (*name == '.')
        ++name;
    uint32_t pre_len = name - pre;

    char* alloc = NULL;
    char* suf = strchr (name, '@');
    if (suf)
    {
        alloc = (char*)malloc(suf - name + 1);
        memcpy(alloc, name, suf - name);
        alloc[suf - name] = '\0';
        name = alloc;
    }

    if(which) *which = __non_identifier__;

    demangled = cplus_demangle(name,DMGL_PARAMS|DMGL_ANSI);
    if(!demangled){
        char* rest = NULL;
        Name* n = demangle(name,&rest,0x1f);
        if(n){
            demangled = text(n);
            if(which) *which = __xlc_identifier__;
        }
    } else {
        if(which) *which = __gnu_identifier__;
    }

    if(!demangled){
        if (alloc)
            free(alloc);

        return pre;
    }

    if (pre_len || suf)
    {
        uint32_t len;
        uint32_t suf_len;
        char *final;

        if (alloc)
            free(alloc);

        len = strlen (demangled);
        if (!suf)
            suf = demangled + len;
        suf_len = strlen (suf) + 1;
        final = (char*)malloc(pre_len + len + suf_len);

        memcpy (final, pre, pre_len);
        memcpy (final + pre_len, demangled, len);
        memcpy (final + pre_len + len, suf, suf_len);
        free(demangled);
        demangled = final;
    }
    return demangled;
#else
    if(which) *which = __non_identifier__;
    return name;
#endif
}
