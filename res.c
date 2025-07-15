/*
 * libres - library for reading Macintosh resource forks
 * Copyright (C) 2008-2009 Jesus A. Alvarez
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
// http://developer.apple.com/documentation/mac/MoreToolbox/MoreToolbox-99.html

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <search.h>
#include <arpa/inet.h>
#include "res.h"
#include "libres_internal.h"

const char * libres_id = "libres 1.0.1 (C)2008-2016 namedfork.net";

RFILE* res_open (const char *path, int mode) {
    if (mode != 0) efail(EINVAL);
    RFILE* rp = malloc(sizeof(RFILE));
    if (rp == NULL) efail(ENOMEM);
    bzero(rp, sizeof(RFILE));

    // open
    rp->fp = fopen(path, "r");
    if (rp->fp == NULL) {
        free(rp);
        return NULL;
    }

    // get size
    errno = 0;
    fseek(rp->fp, 0, SEEK_END);
    rp->size = ftell(rp->fp);
    if (errno) effail(errno, rp);

    rp->buf = NULL;
    return res_load(rp);
}

RFILE* res_open_mem (void *buf, size_t size, int copy) {
    if (buf == NULL) return NULL;
    RFILE* rp = malloc(sizeof(RFILE));
    if (rp == NULL) efail(ENOMEM);
    bzero(rp, sizeof(RFILE));
    rp->size = size;
    if (copy) {
        rp->buf = malloc(size);
        if (rp == NULL) effail(ENOMEM, rp);
        memcpy(rp->buf, buf, size);
    } else rp->buf = buf;

    return res_load(rp);
}

RFILE* res_open_funcs (void *priv, res_seek_func seekf, res_read_func readf) {
    RFILE* rp = malloc(sizeof(RFILE));
    if (rp == NULL) efail(ENOMEM);
    bzero(rp, sizeof(RFILE));
    rp->seek = seekf;
    rp->read = readf;
    rp->fpriv = priv;
    rp->size = rp->seek(priv, 0, SEEK_END);
    return res_load(rp);
}

int res_close (RFILE* rp) {
    if (rp == NULL) eret(EBADF, EOF);
    if (rp->buf) free(rp->buf);
    if (rp->fp) fclose(rp->fp);

    // free list
    if (rp->types) {
        for(int i=0; i < rp->numTypes; i++) {
            struct RmType *t = &rp->types[i];
            if (t->list == NULL) continue;
            for(int j=0; j < t->count; j++) {
                struct RmResRef *r = &t->list[j];
                if (r->name) free(r->name);
            }
            free(t->list);
        }
        free(rp->types);
    }

    free(rp);
    return 0;
}

size_t res_typecount (RFILE *rp) {
    return rp->numTypes;
}

uint32_t* res_types (RFILE *rp, uint32_t *buf, size_t start, size_t size, size_t *read, size_t *remain) {
    if (buf != NULL && size == 0) efail(EINVAL);
    if (size == 0 || start + size > rp->numTypes) size = rp->numTypes - start;
    if (buf == NULL) buf = calloc(size, sizeof(uint32_t));
    if (buf == NULL) efail(ENOMEM);
    if (read) *read = size;
    if (remain) *remain = rp->numTypes - (size + start);

    for(size_t i=0; i < size; i++)
        buf[i] = rp->types[start+i].type;

    return buf;
}

size_t res_count (RFILE *rp, uint32_t type) {
    struct RmType *t = res_type_find(rp, type);
    if (t == NULL) return 0;
    return t->count;
}

ResAttr* res_list (RFILE *rp, uint32_t type, ResAttr *buf, size_t start, size_t size, size_t *read, size_t *remain) {
    struct RmType *t = res_type_find(rp, type);
    if (t == NULL) efail(ENOENT);
    if (buf != NULL && size == 0) efail(EINVAL);
    if (size == 0 || start + size > t->count) size = t->count - start;
    if (buf == NULL) buf = calloc(size, sizeof(ResAttr));
    if (buf == NULL) efail(ENOMEM);
    if (read) *read = size;
    if (remain) *remain = t->count - (size + start);

    for(size_t i=0; i < size; i++) {
        buf[i].index = i;
        buf[i].ID    = t->list[start+i].ID;
        buf[i].flags = t->list[start+i].flags;
        buf[i].size  = t->list[start+i].size;
        buf[i].name  = t->list[start+i].name;
    }

    return buf;
}

ResAttr* res_attr (RFILE *rp, uint32_t type, int16_t ID, ResAttr *buf) {
    struct RmType *t = res_type_find(rp, type);
    if (t == NULL) efail(ENOENT);
    struct RmResRef *ref = res_ref_find(rp, t, ID);
    if (ref == NULL) efail(ENOENT);
    if (buf == NULL) buf = malloc(sizeof(ResAttr));
    if (buf == NULL) efail(ENOMEM);

    buf->index = ref - t->list;
    buf->ID    = ref->ID;
    buf->flags = ref->flags;
    buf->size  = ref->size;
    buf->name  = ref->name;

    return buf;
}

ResAttr* res_attr_named (RFILE *rp, uint32_t type, const char *name, ResAttr *buf) {
    struct RmResRef *ref = res_ref_find_named(rp, res_type_find(rp, type), name);
    if (ref == NULL) efail(ENOENT);
    return res_attr(rp, type, ref->ID, buf);
}

void* res_read (RFILE *rp, uint32_t type, int16_t ID, void *buf, size_t start, size_t size, size_t *read, size_t *remain) {
    struct RmType *t = res_type_find(rp, type);
    if (t == NULL) efail(ENOENT);
    struct RmResRef *ref = res_ref_find(rp, t, ID);
    if (ref == NULL) efail(ENOENT);

    if (ref->flags.fl.compressed) efail(ENOSYS);
    return res_read_raw(rp, ref, buf, start, size, read, remain);
}

void* res_read_named (RFILE *rp, uint32_t type, const char *name, void *buf, size_t start, size_t size, size_t *read, size_t *remain) {
    struct RmResRef *ref = res_ref_find_named(rp, res_type_find(rp, type), name);
    if (ref == NULL) efail(ENOENT);
    return res_read(rp, type, ref->ID, buf, start, size, read, remain);
}

void* res_read_ind (RFILE *rp, uint32_t type, int16_t ind, void *buf, size_t start, size_t size, size_t *read, size_t *remain) {
    struct RmType *t = res_type_find(rp, type);
    if (t == NULL) efail(ENOENT);
    if (ind >= t->count || ind < 0) efail(ENOENT);
    struct RmResRef *ref = &t->list[ind];
    if (ref == NULL) efail(ENOENT);

    if (ref->flags.fl.compressed) efail(ENOSYS);
    return res_read_raw(rp, ref, buf, start, size, read, remain);
}

void res_printdir (RFILE *rp) {
    for(int i=0; i < rp->numTypes; i++) {
        struct RmType *t = &rp->types[i];
        for(int j=0; j < t->count; j++)
        printf("%c%c%c%c %hd (%ub) %s\n", TYPECHARS(t->type), t->list[j].ID, t->list[j].size, t->list[j].name?t->list[j].name:"");
    }
}

void res_printattr (const ResAttr *attr, uint32_t type) {
    if (attr == NULL) return;
    if (type)
    printf("Type: %c%c%c%c\n", TYPECHARS(type));
    printf("ID:   %hd\n", attr->ID);
    printf("Size: %u\n", attr->size);
    if (attr->name)
    printf("Name: %s\n", attr->name);
    printf("Attr: ");
    if (attr->flags.fl.sysRef)      printf("sysRef%s",      ((attr->flags.b&0x7F)?", ":""));
    if (attr->flags.fl.sysHeap)     printf("sysHeap%s",     ((attr->flags.b&0x3F)?", ":""));
    if (attr->flags.fl.purgeable)   printf("purgeable%s",   ((attr->flags.b&0x1F)?", ":""));
    if (attr->flags.fl.locked)      printf("locked%s",      ((attr->flags.b&0x0F)?", ":""));
    if (attr->flags.fl.protected)   printf("protected%s",   ((attr->flags.b&0x07)?", ":""));
    if (attr->flags.fl.preload)     printf("preload%s",     ((attr->flags.b&0x03)?", ":""));
    if (attr->flags.fl.changed)     printf("changed%s",     ((attr->flags.b&0x01)?", ":""));
    if (attr->flags.fl.compressed)  printf("compressed");
    printf("\n\n");
}

#if 0
#pragma mark -
#pragma mark Private Functions
#endif

void* res_bread (RFILE *rp, void *buf, size_t offset, size_t count) {
    if (offset+count > rp->size) efail(EFAULT);
    if (buf == NULL) buf = malloc(count);
    if (buf == NULL) efail(ENOMEM);

    if (rp->buf)
        // memory
        memcpy(buf, rp->buf+offset, count);
    else if (rp->fp) {
        // file pointer
        fseek(rp->fp, offset, SEEK_SET);
        fread(buf, 1, count, rp->fp);
    } else {
        // functions
        rp->seek(rp->fpriv, (long)offset, (int)SEEK_SET);
        rp->read(rp->fpriv, buf, (unsigned long)count);
    }
    return buf;
}

uint32_t res_szread (RFILE *rp, size_t offset) {
    uint32_t r = 0;
    res_bread(rp, &r, offset, sizeof r);
    return ntohl(r);
}

void* res_read_raw (RFILE *rp, struct RmResRef *ref, void *buf, size_t start, size_t size, size_t *read, size_t *remain) {
    if (ref == NULL) efail(ENOENT);
    if (buf != NULL && size == 0) efail(EINVAL);
    if (size == 0 || start + size > ref->psize) size = ref->psize - start;
    size_t rstart = start + ref->offset + rp->dataOffset + 4;
    if (rstart+size > rp->size) efail(EFAULT);
    if (buf == NULL) buf = malloc(ref->psize);
    if (buf == NULL) efail(ENOMEM);
    if (read) *read = size;
    if (remain) *remain = ref->psize - (size + start);

    return res_bread(rp, buf, rstart, size);
}

RFILE* res_load (RFILE *rp) {
    // read header
    struct RfHdr hdr;
    res_bread(rp, &hdr, 0, sizeof(struct RfHdr));
    rp->dataOffset = ntohl(hdr.dataOffset);

    // read map
    struct RfMap *map = res_bread(rp, NULL, (size_t)ntohl(hdr.mapOffset), (size_t)ntohl(hdr.mapLength));
    if (map == NULL) egoto(EINVAL, error);
    rp->attributes = ntohs(map->attributes);
    struct RfTypeList *types = ((void*)map)+ntohs(map->typeListOffset);
    uint8_t *names = ((void*)map)+ntohs(map->nameListOffset);

    // read types
    rp->numTypes = 1+(int16_t)ntohs(types->count);
    rp->types = calloc(rp->numTypes, sizeof(struct RmType));
    if (rp->types == NULL) egoto(ENOMEM, error);
    bzero(rp->types, sizeof(struct RmType) * rp->numTypes);

    for(int i=0; i < rp->numTypes; i++) {
        struct RmType *t = &rp->types[i];
        t->type = ntohl(types->entry[i].type);
        t->count = 1+(size_t)ntohs(types->entry[i].count);
        t->list = calloc(t->count, sizeof(struct RmResRef));
        if (t->list == NULL) egoto(ENOMEM, error);
        bzero(t->list, t->count * sizeof(struct RmResRef));

        // read resource refs & names
        int refsNeedSort = 0;
        struct RfRefEntry *ent = ((void*)types)+ntohs(types->entry[i].offset);
        for(int j=0; j < t->count; j++) {
            t->list[j].ID = ntohs(ent[j].ID);
            if (j && (t->list[j].ID < t->list[j-1].ID)) refsNeedSort = 1;
            t->list[j].flags.b = ent[j].attributes;
            t->list[j].offset = ((ent[j].offHi << 16) | ntohs(ent[j].offLo));
            t->list[j].psize = res_szread(rp, rp->dataOffset+t->list[j].offset);

            uint16_t nameOffset = ntohs(ent[j].nameOffset);
            if (nameOffset == 0xFFFF) t->list[j].name = NULL;
            else {
                t->list[j].name = malloc(names[nameOffset]+1);
                if (t->list[j].name == NULL) egoto(ENOMEM, error);
                t->list[j].name[names[nameOffset]] = '\0';
                memcpy(t->list[j].name, &names[nameOffset+1], names[nameOffset]);
            }

            // find logical size
            if (t->list[j].flags.fl.compressed) {
                struct RfCmpHdr cmpHdr;
                if (res_read_raw(rp, &t->list[j], &cmpHdr, 0, sizeof cmpHdr, NULL, NULL) == NULL)
                    goto notCompressed;
                if (ntohl(cmpHdr.tag) != kCompressedResourceTag)
                    goto notCompressed;
                t->list[j].size = ntohl(cmpHdr.size);

                if (ntohl(cmpHdr.flags) == kCompressedResourceFlg0)
                    t->list[j].dcmp = ntohs(cmpHdr.u.v0.dcmp);
                else if (ntohl(cmpHdr.flags) == kCompressedResourceFlg1)
                    t->list[j].dcmp = ntohs(cmpHdr.u.v1.dcmp);
                else {
                    t->list[j].dcmp = kDCMPInvalidFlags;
                    fprintf(stderr, "libres: %c%c%c%c %hd: unknown compression flags\n", TYPECHARS(t->type), t->list[j].ID);
                }
            }

            // resource not compressed, logical size = physical size
            if (t->list[j].flags.fl.compressed == 0) {
                notCompressed:
                t->list[j].flags.fl.compressed = 0;
                t->list[j].size = t->list[j].psize;
            }
        }

        // keep ref list sorted
        // they are normally sorted by ID already, so it's rarely needed
        if (refsNeedSort) qsort(t->list, t->count, sizeof(struct RmResRef), (int(*)(const void*, const void*))res_ref_compar);
    }

    // keep type list sorted
    // types are sorted alphabetically in files, we need them sorted numerically
    qsort(rp->types, rp->numTypes, sizeof(struct RmType), (int(*)(const void*, const void*))res_type_compar);

    errno = 0;
    free(map);
    return rp;
error:
    free(map);
    res_close(rp);
    return NULL;
}

int res_ref_compar (const struct RmResRef * a, const struct RmResRef * b) {
    return (int)(a->ID - b->ID);
}

int res_type_compar (const struct RmType * a, const struct RmType * b) {
    // types are unsigned, compare manually
    if (a->type == b->type) return 0;
    if (a->type < b->type) return -1;
    return 1;
}

struct RmType * res_type_find (RFILE *rp, uint32_t type) {
    struct RmType key;
    key.type = type;
    return bsearch(&key, rp->types, rp->numTypes, sizeof(struct RmType), (int(*)(const void*, const void*))res_type_compar);
}

struct RmResRef * res_ref_find (RFILE *rp, struct RmType *type, int16_t ID) {
    struct RmResRef key;
    if (type == NULL) return NULL;
    key.ID = ID;
    return bsearch(&key, type->list, type->count, sizeof(struct RmResRef), (int(*)(const void*, const void*))res_ref_compar);
}

struct RmResRef * res_ref_find_named (RFILE *rp, struct RmType *type, const char *name) {
    struct RmResRef key;
    if (type == NULL) return NULL;
    key.name = (char*)name;
    size_t count = type->count;
    return lfind(&key, type->list, &count, sizeof(struct RmResRef), (int(*)(const void*, const void*))res_ref_name_compar);
}

int res_ref_name_compar (const struct RmResRef * a, const struct RmResRef * b) {
    if (a->name == NULL || b->name == NULL) return 1;
    return strcmp(a->name, b->name);
}