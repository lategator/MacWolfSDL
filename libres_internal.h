// libres internal header
#include <stdio.h>
#include <stdint.h>
#include "res.h"

#define kCompressedResourceTag      0xA89F6572
#define kCompressedResourceFlg0     0x00120901
#define kCompressedResourceFlg1     0x00120801
#define kDCMPInvalidFlags           0xD5DC

#define efail(n) {errno = n; return NULL;}
#define effail(n, m) {errno = n; free(m); return NULL;}
#define eret(n, r) {errno = n; return r;}
#define egoto(n, l) {errno = n; goto l;}

// in-memory structures
struct RFILE {
    FILE            *fp;    // file
    void            *buf;   // memory
    void            *fpriv; // functions
    res_seek_func   seek;   // functions
    res_read_func   read;   // functions
    size_t          size;
    size_t          dataOffset;
    uint16_t        attributes;
    size_t          numTypes;
    struct RmType   *types;
};

struct RmType {
    uint32_t        type;
    size_t          count;
    struct RmResRef *list;
};

struct RmResRef {
    int16_t     ID;
    RFlags      flags;
    uint32_t    size;   // logical size (uncompressed)
    uint32_t    psize;  // physical size
    uint32_t    offset; // offset from data section
    int16_t     dcmp;   // decompressor ID
    char*       name;
};


// in-file structures

struct __attribute__ ((__packed__)) RfHdr {
    // resource fork header
    uint32_t    dataOffset;
    uint32_t    mapOffset;
    uint32_t    dataLength;
    uint32_t    mapLength;
};

struct __attribute__ ((__packed__)) RfMap {
    // resource map
    struct RfHdr    headerCopy;
    uint32_t        rsvNextMapHandle;
    uint16_t        rsvFileRef;
    uint16_t        attributes;
    uint16_t        typeListOffset; // from beginning of map to type list
    uint16_t        nameListOffset; // from beginning of map to name list
    //uint16_t        numTypes;       // types minus one, overlaps RfTypeList
};

struct __attribute__ ((__packed__)) RfTypeEntry {
    // resource type entry
    uint32_t        type;   // resource type
    int16_t         count;  // number of resources minus one
    uint16_t        offset; // offset to ref list from type list 
};

struct __attribute__ ((__packed__)) RfTypeList {
    // resource type list
    int16_t             count;  // minus one
    struct RfTypeEntry  entry[];
};

struct __attribute__ ((__packed__)) RfRefEntry {
    // resource reference entry
    int16_t         ID;
    uint16_t        nameOffset;
    uint8_t         attributes;
    uint8_t         offHi;
    uint16_t        offLo;
    uint32_t        rsvHandle;
};

struct __attribute__ ((__packed__)) RfResEntry {
    // resource data entry
    uint32_t    length;
    char        data[];
};

struct __attribute__ ((__packed__)) RfCmpHdr {
    // compressed resource entry
    uint32_t    tag;
    uint32_t    flags;
    uint32_t    size;   // uncompressed
    union __attribute__ ((__packed__)) {
        struct __attribute__ ((__packed__)) {
            int16_t dcmp;       // decompressor ID
            uint8_t wrkBufFrSz; // working buffer fractional size
            uint8_t expBufSz;   // expansion buffer size
        } v0;
        struct __attribute__ ((__packed__)) {
            uint8_t wrkBufFrSz; // working buffer fractional size
            uint8_t expBufSz;   // expansion buffer size
            int16_t dcmp;       // decompressor ID
        } v1;
    } u;
};

// private functions
void* res_bread (RFILE *rp, void *buf, size_t offset, size_t count);
uint32_t res_szread (RFILE *rp, size_t offset);
RFILE* res_load (RFILE *rp);
int res_ref_compar (const struct RmResRef *, const struct RmResRef *);
int res_type_compar (const struct RmType *, const struct RmType *);
struct RmType * res_type_find (RFILE *rp, uint32_t type);
struct RmResRef * res_ref_find (RFILE *rp, struct RmType *type, int16_t ID);
struct RmResRef * res_ref_find_named (RFILE *rp, struct RmType *type, const char *name);
int res_ref_name_compar (const struct RmResRef * a, const struct RmResRef * b);
void* res_read_raw (RFILE *rp, struct RmResRef *ref, void *buf, size_t start, size_t size, size_t *read, size_t *remain);
