#include <assert.h>
#include <signal.h>

#ifdef DO_MEMDEBUG
#include <mncommon/memdebug.h>
MEMDEBUG_DECLARE(mnamqp_testfoo);
#endif

#define TRRET_DEBUG
#include <mncommon/dumpm.h>

#include <mnamqp_private.h>
#include <mnthr.h>

#include "diag.h"

#include "unittest.h"

#ifndef NDEBUG
const char *_malloc_options = "AJ";
#endif

static amqp_conn_t *conn = NULL;

static int
_shutdown(UNUSED int argc, UNUSED void **argv)
{
    amqp_conn_close(conn, 0);
    amqp_conn_post_close(conn);
    amqp_conn_destroy(&conn);
    mnamqp_fini();
    //mnthr_fini();
#ifdef DO_MEMDEBUG
    memdebug_print_stats();
#endif
    exit(0);
    return 0;
}


static void
myterm(UNUSED int sig)
{
    (void)MNTHR_SPAWN("shutdown_thread", _shutdown);
}

static int
mypub(UNUSED int argc, void **argv)
{
    UNUSED amqp_channel_t *chan;

    assert(argc == 1);
    chan = argv[0];

    while (1) {
        char buf[1024];

        mnthr_sleep(15000);
        snprintf(buf, sizeof(buf), "data %ld", mnthr_get_now_nsec());
        TRACEC("%s", buf);
        //if (amqp_channel_publish(chan,
        //                         "",
        //                         "qwe",
        //                         0,
        //                         NULL,
        //                         NULL,
        //                         buf,
        //                         strlen(buf)) != 0) {
        //    break;
        //}
    }
    CTRACE("Exiting mypub...");
    return 0;
}


static int
my_content_cb(UNUSED amqp_frame_t *method,
              amqp_frame_t *header,
              char *data,
              UNUSED void *udata)
{
    TRACE("---");
    if (data != NULL) {
        D8(data, header->payload.header->body_size);
        free(data);
    }
    TRACE("---");
    return 0;
}


static void
declare_queue_cb1(UNUSED amqp_channel_t *chan,
                  amqp_frame_t *fr,
                  UNUSED void *udata)
{
    amqp_queue_declare_t *m;
    //amqp_value_t *args;

    m = (amqp_queue_declare_t *)fr->payload.params;
    //args = amqp_value_new(AMQP_TTABLE);
    //init_table(&args->value.t);
    table_add_i32(&m->arguments, "x-expires", 3600000);
    table_add_lstr(&m->arguments, "x-ha-policy", bytes_new_from_str("all"));
}


static int
run(UNUSED int argc, UNUSED void **argv)
{
    int res;
    amqp_channel_t *chan;
    amqp_consumer_t *cons;

    res = 0;
    conn = amqp_conn_new("localhost",
                         5672,
                         "guest",
                         "guest",
                         "/",
                         0,
                         0,
                         0,
                         0);

    if (amqp_conn_open(conn) != 0) {
        res = 1;
        goto err;
    }

    if (amqp_conn_run(conn) != 0) {
        res = 1;
        goto err;
    }

    if ((chan = amqp_create_channel(conn)) == NULL) {
        res = 1;
        goto err;
    }

    if (amqp_channel_confirm(chan, 0) != 0) {
        res = 1;
        goto err;
    }

    //if (amqp_channel_declare_exchange(chan, "qwe", "direct", DECLARE_EXCHANGE_FPASSIVE) != 0) {
    //    res = 1;
    //    goto err;
    //}

    //if (amqp_channel_declare_exchange(chan, "qwe", "direct", 0) != 0) {
    //    res = 1;
    //    goto err;
    //}

    //if (amqp_channel_delete_exchange(chan, "qwe", 0) != 0) {
    //    res = 1;
    //    goto err;
    //}

    //if (amqp_channel_declare_queue(chan, "qwe", DECLARE_QUEUE_FEXCLUSIVE) != 0) {
    //    res = 1;
    //    goto err;
    //}

    if (amqp_channel_declare_queue_ex(chan,
                                      "qwe",
                                      DECLARE_QUEUE_FEXCLUSIVE,
                                      declare_queue_cb1,
                                      NULL,
                                      NULL,
                                      NULL) != 0) {
        res = 1;
        goto err;
    }

    if ((cons = amqp_channel_create_consumer(chan,
                                             "qwe",
                                             NULL,
                                             CONSUME_FNOACK & 0)) == NULL) {
        res = 1;
        goto err;
    }

    (void)MNTHR_SPAWN("pub", mypub, chan);

    amqp_consumer_handle_content(cons, my_content_cb, NULL, NULL);

    if (amqp_close_consumer(cons)) {
    }

    if (amqp_close_channel(chan) != 0) {
        res = 1;
        goto err;
    }

end:
    amqp_conn_close(conn, 0);
    amqp_conn_post_close(conn);
    amqp_conn_destroy(&conn);
    return res;

err:
    TR(res);
    goto end;
}


static void
test1(void)
{

    mnthr_init();
    mnamqp_init();

    (void)MNTHR_SPAWN("run", run);

    mnthr_loop();

    mnamqp_fini();
    mnthr_fini();
}


int
main(void)
{
#ifdef DO_MEMDEBUG
    MEMDEBUG_REGISTER(array);
    MEMDEBUG_REGISTER(bytes);
    MEMDEBUG_REGISTER(bytestream);
    MEMDEBUG_REGISTER(mnamqp);
    MEMDEBUG_REGISTER(mnamqp_wire);
    MEMDEBUG_REGISTER(mnamqp_frame);
    MEMDEBUG_REGISTER(mnamqp_spec);
    MEMDEBUG_REGISTER(mnamqp_rpc);
    MEMDEBUG_REGISTER(mnamqp_testfoo);
#endif

    if (signal(SIGINT, myterm) == SIG_ERR) {
        return 1;
    }
    if (signal(SIGTERM, myterm) == SIG_ERR) {
        return 1;
    }
    test1();
#ifdef DO_MEMDEBUG
    //memdebug_print_stats_oneline();
    memdebug_print_stats();
#endif
    return 0;
}
