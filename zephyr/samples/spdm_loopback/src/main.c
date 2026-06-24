/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License.
 **/

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "spdm_loopback.h"

/* The two channels making up the loopback link. */
static struct mock_channel req_to_rsp;
static struct mock_channel rsp_to_req;

static struct mock_transport requester_transport;
static struct mock_transport responder_transport;

#define STACK_SIZE 16384

K_THREAD_STACK_DEFINE(responder_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(requester_stack, STACK_SIZE);
static struct k_thread responder_thread;
static struct k_thread requester_thread;

int main(void)
{
    printk("\nlibspdm Zephyr loopback demo\n");
    printk("============================\n");

    mock_channel_init(&req_to_rsp);
    mock_channel_init(&rsp_to_req);

    requester_transport.tx = &req_to_rsp;
    requester_transport.rx = &rsp_to_req;
    responder_transport.tx = &rsp_to_req;
    responder_transport.rx = &req_to_rsp;

    k_thread_create(&responder_thread, responder_stack, STACK_SIZE,
                    responder_thread_main, &responder_transport, NULL, NULL,
                    K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
    k_thread_name_set(&responder_thread, "spdm_rsp");

    k_thread_create(&requester_thread, requester_stack, STACK_SIZE,
                    requester_thread_main, &requester_transport, NULL, NULL,
                    K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
    k_thread_name_set(&requester_thread, "spdm_req");

    /* Wait for requester to finish; responder loops until it times out
     * waiting for the next message. */
    k_thread_join(&requester_thread, K_SECONDS(30));
    printk("\nlibspdm Zephyr loopback demo: main exiting\n");
    return 0;
}
