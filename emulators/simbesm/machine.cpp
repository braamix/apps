/*
 * The driver.
 *
 * Copyright (c) 2026, Serge Vakulenko
 */
#include "besm6_defs.h"

IoRequest io_request;

void io_post(UNIT *u, t_stat (*serve)(UNIT *u), int32 delay)
{
    io_request.unit  = u;
    io_request.serve = serve;
    io_request.delay = delay;
}

t_stat io_service(void)
{
    UNIT *u                 = io_request.unit;
    t_stat (*serve)(UNIT *) = io_request.serve;
    int32 delay             = io_request.delay;
    t_stat r;

    if (!u)
        return SCPE_OK;
    io_request.unit = NULL;

    r = serve(u);
    if (r != SCPE_OK)
        return r;

    /* Wait for an event from the device. */
    sim_activate(u, delay);
    return SCPE_OK;
}
