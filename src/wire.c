//#include <arpa/inet.h>
#include <string.h>

#ifdef DO_MEMDEBUG
#include <mncommon/memdebug.h>
MEMDEBUG_DECLARE(mnamqp_wire);
#endif

#include <mncommon/hash.h>
#include <mncommon/bytestream.h>
#include <mncommon/bytes.h>
//#define TRRET_DEBUG
//#define TRRET_DEBUG_VERBOSE
#include <mncommon/dumpm.h>
#include <mncommon/util.h>

#include <mnamqp_private.h>

#include "diag.h"


/*
 * field types
 */
static amqp_type_t *amqp_type_by_tag(uint8_t);
static amqp_type_t field_types[256];



/*
 * basic types
 */

/*
 * octet
 */
void
pack_octet(mnbytestream_t *bs, uint8_t v)
{
    (void)bytestream_cat(bs, sizeof(uint8_t), (char *)&v);
}


ssize_t
unpack_octet(mnbytestream_t *bs, void *fd, uint8_t *v)
{
    while (SAVAIL(bs) < (ssize_t)sizeof(uint8_t)) {
        if (bytestream_consume_data(bs, fd) != 0) {
            TRRET(UNPACK_ECONSUME);
        }
    }
    *v = *SPDATA(bs);
    SINCR(bs);
    return 1;
}


/*
 * short
 */
void
pack_short(mnbytestream_t *bs, uint16_t v)
{
    union {
        uint16_t i;
        char c[sizeof(uint16_t)];
    } u;

    u.i = htobe16(v);
    (void)bytestream_cat(bs, sizeof(uint16_t), u.c);
}


ssize_t
unpack_short(mnbytestream_t *bs, void *fd, uint16_t *v)
{
    union {
        char *c;
        uint16_t *i;
    } u;

    while (SAVAIL(bs) < (ssize_t)sizeof(uint16_t)) {
        if (bytestream_consume_data(bs, fd) != 0) {
            TRRET(UNPACK_ECONSUME);
        }
    }
    u.c = SPDATA(bs);
    *v = be16toh(*u.i);
    SADVANCEPOS(bs, sizeof(uint16_t));
    return sizeof(uint16_t);
}


/*
 * long
 */
void
pack_long(mnbytestream_t *bs, uint32_t v)
{
    union {
        uint32_t i;
        char c[sizeof(uint32_t)];
    } u;

    u.i = htobe32(v);
    (void)bytestream_cat(bs, sizeof(uint32_t), u.c);
}


ssize_t
unpack_long(mnbytestream_t *bs, void *fd, uint32_t *v)
{
    union {
        char *c;
        uint32_t *i;
    } u;

    while (SAVAIL(bs) < (ssize_t)sizeof(uint32_t)) {
        if (bytestream_consume_data(bs, fd) != 0) {
            TRRET(UNPACK_ECONSUME);
        }
    }
    u.c = SPDATA(bs);
    *v = be32toh(*u.i);
    SADVANCEPOS(bs, sizeof(uint32_t));
    return sizeof(uint32_t);
}


/*
 * longlong
 */
void
pack_longlong(mnbytestream_t *bs, uint64_t v)
{
    union {
        uint64_t i;
        char c[sizeof(uint64_t)];
    } u;

    u.i = htobe64(v);
    (void)bytestream_cat(bs, sizeof(uint64_t), u.c);
}


ssize_t
unpack_longlong(mnbytestream_t *bs, void *fd, uint64_t *v)
{
    union {
        char *c;
        uint64_t *i;
    } u;

    while (SAVAIL(bs) < (ssize_t)sizeof(uint64_t)) {
        if (bytestream_consume_data(bs, fd) != 0) {
            TRRET(UNPACK_ECONSUME);
        }
    }
    u.c = SPDATA(bs);
    *v = be64toh(*u.i);
    SADVANCEPOS(bs, sizeof(uint64_t));
    return sizeof(uint64_t);
}


/*
 * float
 */
void
pack_float(mnbytestream_t *bs, float v)
{
    union {
        float f;
        char c[sizeof(uint64_t)];
    } u;

    u.f = v;
    (void)bytestream_cat(bs, sizeof(float), u.c);
}


ssize_t
unpack_float(mnbytestream_t *bs, void *fd, float *v)
{
    union {
        char *c;
        float *f;
    } u;

    while (SAVAIL(bs) < (ssize_t)sizeof(float)) {
        if (bytestream_consume_data(bs, fd) != 0) {
            TRRET(UNPACK_ECONSUME);
        }
    }
    u.c = SPDATA(bs);
    *v = *u.f;
    SADVANCEPOS(bs, sizeof(float));
    return sizeof(float);
}


/*
 * double
 */
void
pack_double(mnbytestream_t *bs, double v)
{
    union {
        double d;
        char c[sizeof(uint64_t)];
    } u;

    u.d = v;
    (void)bytestream_cat(bs, sizeof(double), u.c);
}


ssize_t
unpack_double(mnbytestream_t *bs, void *fd, double *v)
{
    union {
        char *c;
        double *d;
    } u;

    while (SAVAIL(bs) < (ssize_t)sizeof(double)) {
        if (bytestream_consume_data(bs, fd) != 0) {
            TRRET(UNPACK_ECONSUME);
        }
    }
    u.c = SPDATA(bs);
    *v = *u.d;
    SADVANCEPOS(bs, sizeof(double));
    return sizeof(double);
}


/*
 * shortstr
 */
void
pack_shortstr(mnbytestream_t *bs, mnbytes_t *s)
{
    union {
        uint8_t sz;
        char c;
    } u;

    /*
     * discard terminating zero, not to be counted in AMQP
     */
    u.sz = (uint8_t)s->sz - 1;
    (void)bytestream_cat(bs, sizeof(uint8_t), &u.c);
    (void)bytestream_cat(bs, u.sz, BCDATA(s));
}


ssize_t
unpack_shortstr(mnbytestream_t *bs, void *fd, mnbytes_t **v)
{
    uint8_t sz;

    if (unpack_octet(bs, fd, &sz) < 0) {
        TRRET(UNPACK_ECONSUME);
    }

    /*
     * reserve for terminating zero, not to be counted in AMQP
     */
    *v = bytes_new(sz + 1);

    while (SAVAIL(bs) < (ssize_t)sz) {
        if (bytestream_consume_data(bs, fd) != 0) {
            TRRET(UNPACK_ECONSUME);
        }
    }

    memcpy(BDATA(*v), SPDATA(bs), sz);
    BDATA(*v)[sz] = '\0';
    SADVANCEPOS(bs, sz);
    BYTES_INCREF(*v);
    return sizeof(uint8_t) + sz;
}


/*
 * longstr
 */
void
pack_longstr(mnbytestream_t *bs, mnbytes_t *s)
{
    union {
        uint32_t sz;
        char c;
    } u;

    u.sz = htobe32((uint32_t)s->sz - 1);
    /*
     * discard terminating zero, not to be counted in AMQP
     */
    (void)bytestream_cat(bs, sizeof(uint32_t), &u.c);
    (void)bytestream_cat(bs, s->sz - 1, BCDATA(s));
}


ssize_t
unpack_longstr(mnbytestream_t *bs, void *fd, mnbytes_t **v)
{
    uint32_t sz;

    if (unpack_long(bs, fd, &sz) < 0) {
        TRRET(UNPACK_ECONSUME);
    }

    /*
     * reserve for terminating zero, not to be counted in AMQP
     */
    *v = bytes_new(sz + 1);

    while (SAVAIL(bs) < (ssize_t)sz) {
        if (bytestream_consume_data(bs, fd) != 0) {
            TRRET(UNPACK_ECONSUME);
        }
    }

    memcpy(BDATA(*v), SPDATA(bs), sz);
    BDATA(*v)[sz] = '\0';
    SADVANCEPOS(bs, sz);
    BYTES_INCREF(*v);
    return sizeof(uint32_t) + sz;
}



/*
 * field value
 */
static void
pack_field_value(mnbytestream_t *bs, amqp_value_t *v)
{
    pack_octet(bs, v->ty->tag);
    v->ty->enc(v, bs);
}


static ssize_t
unpack_field_value(mnbytestream_t *bs, void *fd, amqp_value_t **v)
{
    amqp_type_t *ty;
    ssize_t res0, res1;
    uint8_t tag;

    if ((res0 = unpack_octet(bs, fd, &tag)) < 0) {
        TRRET(UNPACK_ECONSUME);
    }

    if ((ty = amqp_type_by_tag(tag)) == NULL) {
        TRRET(UNPACK_ETAG);
    }

    assert(ty->tag == tag || ty->enc != NULL);

    if (*v == NULL) {
        if ((*v = malloc(sizeof(amqp_value_t))) == NULL) {
            FAIL("malloc");
        }
    }

    (*v)->ty = ty;

    if ((res1 = ty->dec(*v, bs, fd)) < 0) {
        TRRET(UNPACK_ECONSUME);
    }
    return res0 + res1;
}



/*
 * table
 */
static int
table_item_fini(mnbytes_t *key, amqp_value_t *value)
{
    BYTES_DECREF(&key);
    amqp_value_destroy(&value);
    return 0;
}

static int
pack_table_cb( mnbytes_t *key,
              amqp_value_t *value,
              void *udata)
{
    struct {
        mnbytestream_t *bs;
    } *params = udata;

    pack_shortstr(params->bs, key);
    pack_field_value(params->bs, value);
    return 0;
}


void
pack_table(mnbytestream_t *bs, mnhash_t *v)
{
    struct {
        mnbytestream_t *bs;
    } params;
    off_t seod0, seod1;
    union {
        uint32_t *i;
        char *c;
    } u;

    params.bs = bs;
    seod0 = SEOD(bs);
    pack_long(bs, 0); // placeholder
    seod1 = SEOD(bs);
    (void)hash_traverse(v,
                        (hash_traverser_t)pack_table_cb,
                        &params);
    u.c = SDATA(bs, seod0);
    *u.i = htobe32((uint32_t)SEOD(bs) - seod1);
}


void
init_table(mnhash_t *v)
{
    hash_init(v, 17,
             (hash_hashfn_t)bytes_hash,
             (hash_item_comparator_t)bytes_cmp,
             (hash_item_finalizer_t)table_item_fini);

}


#define TABLE_ADD_DEF(n, ty_, tag, vname)                      \
TABLE_ADD_REF(n, ty_)                                          \
{                                                              \
    mnbytes_t *k;                                                \
    k = bytes_new_from_str(key);                               \
    if (hash_get_item(v, k) != NULL) {                         \
        BYTES_DECREF(&k);                                      \
        return 1;                                              \
    } else {                                                   \
        amqp_value_t *vv;                                      \
                                                               \
        if ((vv = malloc(sizeof(amqp_value_t))) == NULL) {     \
            FAIL("malloc");                                    \
        }                                                      \
        if ((vv->ty = amqp_type_by_tag(tag)) == NULL) {        \
            FAIL("table_add_" #n);                             \
        }                                                      \
        vv->value.vname = val;                                 \
        hash_set_item(v, k, vv);                               \
        BYTES_INCREF(k);                                       \
    }                                                          \
    return 0;                                                  \
}                                                              \

/*
 * quick add some scalars to table
 */
TABLE_ADD_DEF(boolean, char, AMQP_TBOOL, b)
TABLE_ADD_DEF(i8, int8_t, AMQP_TINT8, i8)
TABLE_ADD_DEF(u8, uint8_t, AMQP_TUINT8, u8)
TABLE_ADD_DEF(i16, int16_t, AMQP_TINT16, i16)
TABLE_ADD_DEF(u16, uint16_t, AMQP_TUINT16, u16)
TABLE_ADD_DEF(i32, int32_t, AMQP_TINT32, i32)
TABLE_ADD_DEF(u32, uint32_t, AMQP_TUINT32, u32)
TABLE_ADD_DEF(i64, int64_t, AMQP_TINT64, i64)
TABLE_ADD_DEF(u64, uint64_t, AMQP_TUINT64, u64)
TABLE_ADD_DEF(float, float, AMQP_TFLOAT, f)
TABLE_ADD_DEF(double, double, AMQP_TDOUBLE, d)
// RabbitMQ doesn't like short str?
//TABLE_ADD_DEF(sstr, mnbytes_t *, AMQP_TSSTR, str)
TABLE_ADD_DEF(lstr, mnbytes_t *, AMQP_TLSTR, str)


int
table_add_value(mnhash_t *v, const char *key, amqp_value_t *val)
{
    mnbytes_t *k;

    k = bytes_new_from_str(key);

    if (hash_get_item(v, k) != NULL) {
        BYTES_DECREF(&k);
        return 1;
    } else {
        hash_set_item(v, k, val);
    }
    return 0;
}


amqp_value_t *
table_get_value(mnhash_t *v, mnbytes_t *key)
{
    mnhash_item_t *hit;

    if ((hit = hash_get_item(v, key)) == NULL) {
        return NULL;
    }
    return hit->value;
}


static int
table_str_cb(mnbytes_t *key, amqp_value_t *val, mnbytestream_t *bs)
{
    bytestream_nprintf(bs, 1024, "%s=", BDATA(key));
    switch (val->ty->tag) {
    case AMQP_TBOOL:
        (void)bytestream_nprintf(bs, 1024, "%s ", val->value.b ? "#t" : "#f");
        break;

    case AMQP_TINT8:
        (void)bytestream_nprintf(bs, 1024, "%hhd ", val->value.i8);
        break;

    case AMQP_TUINT8:
        (void)bytestream_nprintf(bs, 1024, "0x%02hhx ", val->value.u8);
        break;

    case AMQP_TINT16:
        (void)bytestream_nprintf(bs, 1024, "%hd ", val->value.i16);
        break;

    case AMQP_TUINT16:
        (void)bytestream_nprintf(bs, 1024, "0x%04hx ", val->value.u16);
        break;

    case AMQP_TINT32:
        (void)bytestream_nprintf(bs, 1024, "%d ", val->value.i32);
        break;

    case AMQP_TUINT32:
        (void)bytestream_nprintf(bs, 1024, "0x%08x ", val->value.u32);
        break;

    case AMQP_TINT64:
        (void)bytestream_nprintf(bs, 1024, "%ld ", val->value.i64);
        break;

    case AMQP_TUINT64:
        (void)bytestream_nprintf(bs, 1024, "0x%016lx ", val->value.u64);
        break;

    case AMQP_TFLOAT:
        (void)bytestream_nprintf(bs, 1024, "%f ", val->value.f);
        break;

    case AMQP_TDOUBLE:
        (void)bytestream_nprintf(bs, 1024, "%lf ", val->value.d);
        break;

    case AMQP_TSSTR:
    case AMQP_TLSTR:
        if (val->value.str != NULL) {
            (void)bytestream_nprintf(bs,
                               1024,
                               "'%s' ",
                               BDATA(val->value.str));
        } else {
            (void)bytestream_nprintf(bs,
                               1024,
                               "%p ",
                               NULL);
        }
        break;

    case AMQP_TTABLE:
        table_str(&val->value.t, bs);
        break;

    default:
        (void)bytestream_nprintf(bs, 1024, "... ");
    }
    return 0;
}

void
table_str(mnhash_t *v, mnbytestream_t *bs)
{
    off_t eod;

    bytestream_cat(bs, 1, "{");
    eod = SEOD(bs);
    hash_traverse(v, (hash_traverser_t)table_str_cb, bs);
    if (eod < SEOD(bs)) {
        SADVANCEEOD(bs, -1);
    }
    bytestream_cat(bs, 2, "} ");
}


ssize_t
unpack_table(mnbytestream_t *bs, void *fd, mnhash_t *v)
{
    uint32_t sz;
    ssize_t nread;

    if (unpack_long(bs, fd, &sz) < 0) {
        TRRET(UNPACK_ECONSUME);
    }

    nread = 0;
    while (nread < sz) {
        ssize_t n;
        mnbytes_t *key;
        amqp_value_t *value;

        key = NULL;
        if ((n = unpack_shortstr(bs, fd, &key)) < 0) {
            BYTES_DECREF(&key);
            TRRET(UNPACK_ECONSUME);
        }
        nread += n;

        value = NULL;
        if ((n = unpack_field_value(bs, fd, &value)) < 0) {
            amqp_value_destroy(&value);
            TRRET(UNPACK_ECONSUME);
        }
        nread += n;

        /* ignore dups */
        if (hash_get_item(v, key) != NULL) {
            BYTES_DECREF(&key);
            amqp_value_destroy(&value);
        } else {
            hash_set_item(v, key, value);
        }
        //if (nread >= sz) {
        //    break;
        //}
    }

    assert(nread == sz);

    return sizeof(uint32_t) + sz;
}


/*
 * array
 */
static int
pack_array_cb(amqp_value_t **v, void *udata)
{
    mnbytestream_t *bs;

    bs = udata;
    assert(*v != NULL);
    pack_field_value(bs, *v);
    return 0;
}

void
pack_array(mnbytestream_t *bs, mnarray_t *v)
{
    pack_long(bs, (uint32_t)v->elnum);
    array_traverse(v, (array_traverser_t)pack_array_cb, bs);
}


static int
array_item_fini(amqp_value_t **v)
{
    amqp_value_destroy(v);
    return 0;
}


ssize_t
unpack_array(mnbytestream_t *bs, void *fd, mnarray_t *v)
{
    uint32_t elnum, i;
    ssize_t nread;

    if ((nread = unpack_long(bs, fd, &elnum)) < 0) {
        TRRET(UNPACK_ECONSUME);
    }

    array_init(v, 0, sizeof(amqp_value_t *),
               NULL,
               (array_finalizer_t)array_item_fini);

    for (i = 0; i < elnum; ++i) {
        ssize_t n;
        amqp_value_t **value;

        if ((value = array_incr(v)) == NULL) {
            FAIL("array_incr");
        }

        *value = NULL;
        if ((n = unpack_field_value(bs, fd, value)) < 0) {
            amqp_value_destroy(value);
            TRRET(UNPACK_ECONSUME);
        }
        nread += n;
    }
    return nread;
}


/*
 * field values
 */

/*
 * boolean
 */
static void
enc_bool(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_octet(bs, v->value.b);
}


static ssize_t
dec_bool(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_octet(bs, fd, (uint8_t *)&v->value.b);
}


/*
 * int8
 */
static void
enc_int8(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_octet(bs, v->value.i8);
}


static ssize_t
dec_int8(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_octet(bs, fd, (uint8_t *)&v->value.i8);
}


/*
 * uint8
 */
static void
enc_uint8(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_octet(bs, v->value.u8);
}


static ssize_t
dec_uint8(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_octet(bs, fd, &v->value.u8);
}


/*
 * int16
 */
static void
enc_int16(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_short(bs, v->value.i16);
}


static ssize_t
dec_int16(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_short(bs, fd, (uint16_t *)&v->value.i16);
}


/*
 * uint16
 */
static void
enc_uint16(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_short(bs, v->value.u16);
}


static ssize_t
dec_uint16(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_short(bs, fd, &v->value.u16);
}


/*
 * int32
 */
static void
enc_int32(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_long(bs, v->value.i32);
}


static ssize_t
dec_int32(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_long(bs, fd, (uint32_t *)&v->value.i32);
}


/*
 * uint32
 */
static void
enc_uint32(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_long(bs, v->value.u32);
}


static ssize_t
dec_uint32(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_long(bs, fd, &v->value.u32);
}


/*
 * int64
 */
static void
enc_int64(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_longlong(bs, v->value.i64);
}


static ssize_t
dec_int64(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_longlong(bs, fd, (uint64_t *)&v->value.i64);
}


/*
 * uint64
 */
static void
enc_uint64(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_longlong(bs, v->value.u64);
}


static ssize_t
dec_uint64(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_longlong(bs, fd, &v->value.u64);
}


/*
 * float
 */
static void
enc_float(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_float(bs, v->value.f);
}


static ssize_t
dec_float(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_float(bs, fd, &v->value.f);
}


/*
 * double
 */
static void
enc_double(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_double(bs, v->value.f);
}


static ssize_t
dec_double(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_double(bs, fd, &v->value.d);
}


/*
 * decimal
 */
static void
enc_decimal(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_octet(bs, v->value.dc.places);
    pack_long(bs, v->value.dc.value);
}


static ssize_t
dec_decimal(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    ssize_t res0, res1;

    if ((res0 = unpack_octet(bs, fd, &v->value.dc.places)) < 0) {
        TRRET(UNPACK_ECONSUME);
    }
    if ((res1 = unpack_long(bs, fd, (uint32_t *)&v->value.dc.value)) < 0) {
        TRRET(UNPACK_ECONSUME);
    }

    return res0 + res1;
}


/*
 * sstr
 */
static void
enc_sstr(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_shortstr(bs, v->value.str);
}


static ssize_t
dec_sstr(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_shortstr(bs, fd, &v->value.str);
}


/*
 * lstr
 */
static void
enc_lstr(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_longstr(bs, v->value.str);
}


static ssize_t
dec_lstr(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_longstr(bs, fd, &v->value.str);
}


static void
kill_str(amqp_value_t *v)
{
    BYTES_DECREF(&v->value.str);
}


/*
 * array
 */
static void
enc_array(UNUSED amqp_value_t *v, UNUSED mnbytestream_t *bs)
{
}


static ssize_t
dec_array(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    return unpack_array(bs, fd, &v->value.a);
}


static void
kill_array(amqp_value_t *v)
{
    array_fini(&v->value.a);
}


/*
 * table
 */
static void
enc_table(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_table(bs, &v->value.t);
}


static ssize_t
dec_table(amqp_value_t *v, mnbytestream_t *bs, void *fd)
{
    init_table(&v->value.t);
    return unpack_table(bs, fd, &v->value.t);
}


static void
kill_table(amqp_value_t *v)
{
    hash_fini(&v->value.t);
}


/*
 * void
 */
static void
enc_void(amqp_value_t *v, mnbytestream_t *bs)
{
    pack_octet(bs, v->ty->tag);
}


static ssize_t
dec_void(UNUSED amqp_value_t *v, UNUSED mnbytestream_t *bs, UNUSED void *fd)
{
    return 0;
}


static struct {
    char tag;
    amqp_encode enc;
    amqp_decode dec;
    amqp_kill kill;
} _typeinfo[] = {
    {AMQP_TBOOL, enc_bool, dec_bool, NULL},
    {AMQP_TINT8, enc_int8, dec_int8, NULL},
    {AMQP_TUINT8, enc_uint8, dec_uint8, NULL},
    {AMQP_TINT16, enc_int16, dec_int16, NULL},
    {AMQP_TUINT16, enc_uint16, dec_uint16, NULL},
    {AMQP_TINT32, enc_int32, dec_int32, NULL},
    {AMQP_TUINT32, enc_uint32, dec_uint32, NULL},
    {AMQP_TINT64, enc_int64, dec_int64, NULL},
    {AMQP_TUINT64, enc_uint64, dec_uint64, NULL},
    {AMQP_TFLOAT, enc_float, dec_float, NULL},
    {AMQP_TDOUBLE, enc_double, dec_double, NULL},
    {AMQP_TDECIMAL, enc_decimal, dec_decimal, NULL},
    {AMQP_TSSTR, enc_sstr, dec_sstr, kill_str},
    {AMQP_TLSTR, enc_lstr, dec_lstr, kill_str},
    {AMQP_TARRAY, enc_array, dec_array, kill_array},
    {AMQP_TTSTAMP, enc_int64, dec_int64, NULL},
    {AMQP_TTABLE, enc_table, dec_table, kill_table},
    {AMQP_TVOID, enc_void, dec_void, NULL},
};


static amqp_type_t *
amqp_type_by_tag(uint8_t tag)
{
    if (field_types[tag].enc == NULL) {
        return NULL;
    }
    return &field_types[tag];
}


amqp_value_t *
amqp_value_new(uint8_t tag)
{
    amqp_value_t *res;

    if ((res = malloc(sizeof(amqp_value_t))) == NULL) {
        FAIL("malloc");
    }
    if ((res->ty = amqp_type_by_tag(tag)) == NULL) {
        FAIL("amqp_table_new");
    }

    return res;
}

void
amqp_value_destroy(amqp_value_t **v)
{
    if (*v != NULL) {
        if ((*v)->ty->kill != NULL) {
            (*v)->ty->kill(*v);
        }
        free(*v);
        *v = NULL;
    }
}


void
mnamqp_init(void)
{
    size_t i;

    for (i = 0; i < countof(_typeinfo); ++i) {
        amqp_type_t *ty;

        ty = field_types + _typeinfo[i].tag;
        ty->tag = _typeinfo[i].tag;
        ty->enc = _typeinfo[i].enc;
        ty->dec = _typeinfo[i].dec;
        ty->kill = _typeinfo[i].kill;
    }
    amqp_spec_init();
}


void
mnamqp_fini(void)
{
    amqp_spec_fini();
}
