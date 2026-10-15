#include "rdm/controller/include/utils.h"

#include <string.h>

#include "dmx/include/driver.h"
#include "dmx/include/service.h"
#include "rdm/include/driver.h"
#include "rdm/include/uid.h"
#include "dmx/hal/include/uart.h"
#include "esp_timer.h"

// One request/response exchange - see rdm_send_request() below, which adds
// the ACK_TIMER follow-up on top of this.
#if CONFIG_RDM_DEBUG_TRACE
static rdm_trace_entry_t rdm_trace_ring[DMX_NUM_MAX][RDM_TRACE_LEN];
static uint32_t rdm_trace_seq[DMX_NUM_MAX];

static void rdm_trace_commit(dmx_port_t dmx_num, rdm_trace_entry_t *tr,
                             rdm_trace_outcome_t outcome) {
  dmx_driver_t *const driver = dmx_driver[dmx_num];
  tr->outcome = outcome;
  tr->status = (uint8_t)driver->dmx.status;
  tr->progress = (uint8_t)driver->dmx.progress;
  tr->head = (int16_t)driver->dmx.head;
  tr->is_controller = driver->is_controller;
  tr->rts = (uint8_t)dmx_uart_get_rts(dmx_num);
  memcpy(tr->raw, driver->dmx.data, RDM_TRACE_RAW_LEN);
  tr->seq = ++rdm_trace_seq[dmx_num];
  rdm_trace_ring[dmx_num][tr->seq % RDM_TRACE_LEN] = *tr;
}

size_t rdm_trace_read(dmx_port_t dmx_num, uint32_t since,
                      rdm_trace_entry_t *out, size_t max) {
  if (dmx_num >= DMX_NUM_MAX || out == NULL) {
    return 0;
  }
  const uint32_t last = rdm_trace_seq[dmx_num];
  uint32_t first = since + 1;
  if (last >= RDM_TRACE_LEN && first < last - RDM_TRACE_LEN + 1) {
    first = last - RDM_TRACE_LEN + 1;  // older ones were overwritten
  }
  size_t n = 0;
  for (uint32_t seq = first; seq <= last && n < max; ++seq) {
    const rdm_trace_entry_t *e = &rdm_trace_ring[dmx_num][seq % RDM_TRACE_LEN];
    if (e->seq == seq) {
      out[n++] = *e;
    }
  }
  return n;
}
#else
static inline void rdm_trace_commit(dmx_port_t dmx_num, rdm_trace_entry_t *tr,
                                    rdm_trace_outcome_t outcome) {
  (void)dmx_num;
  (void)tr;
  (void)outcome;
}

size_t rdm_trace_read(dmx_port_t dmx_num, uint32_t since,
                      rdm_trace_entry_t *out, size_t max) {
  (void)dmx_num;
  (void)since;
  (void)out;
  (void)max;
  return 0;
}
#endif

static size_t rdm_send_request_once(dmx_port_t dmx_num,
                                    const rdm_request_t *request,
                                    const char *format, void *pd, size_t size,
                                    rdm_ack_t *ack) {
  assert(dmx_num < DMX_NUM_MAX);
  assert(request != NULL);
  assert(request->dest_uid != NULL);
  assert(request->sub_device < RDM_SUB_DEVICE_MAX ||
         request->sub_device == RDM_SUB_DEVICE_ALL);
  assert(request->pid > 0);
  assert(rdm_cc_is_valid(request->cc) && rdm_cc_is_request(request->cc));
  assert(request->sub_device != RDM_SUB_DEVICE_ALL ||
         request->cc == RDM_CC_SET_COMMAND);
  assert(rdm_format_is_valid(request->format));
  assert(request->format != NULL || request->pd == NULL);
  assert(request->pd != NULL || request->pdl == 0);
  assert(request->pdl < RDM_PD_SIZE_MAX);
  assert(rdm_format_is_valid(format));
  assert(dmx_driver_is_installed(dmx_num));

  dmx_driver_t *const driver = dmx_driver[dmx_num];

  // Every return below reports what happened in *ack - including the early
  // ones, which used to leave it untouched. Callers zero-initialise it, and a
  // zeroed rdm_ack_t reads as RDM_RESPONSE_TYPE_ACK: with the driver stuck
  // (status left at SENDING, so dmx_wait_sent() fails at once) every request
  // "succeeded" without ever going out - discovery found UIDs 0, 1, 2... and
  // GETs to any UID returned an empty ACK.
  if (ack != NULL) {
    ack->err = DMX_ERR_TIMEOUT;
    ack->size = 0;
    ack->src_uid = (rdm_uid_t){0, 0};
    ack->pid = 0;
    ack->type = RDM_RESPONSE_TYPE_NONE;
    ack->message_count = 0;
    ack->pdl = 0;
  }

  // Attempt to take the mutex and wait until the driver is done sending
  if (!xSemaphoreTakeRecursive(driver->mux, portMAX_DELAY)) {
    return 0;
  }
  if (!dmx_wait_sent(dmx_num, dmx_ms_to_ticks(23))) {
    rdm_trace_entry_t busy = {
        .t_ms = (uint32_t)(esp_timer_get_time() / 1000),
        .dest = *request->dest_uid,
        .pid = request->pid,
        .cc = request->cc,
        .tn = rdm_get_transaction_num(dmx_num),
        .size = -1,
    };
    rdm_trace_commit(dmx_num, &busy, RDM_TRACE_SEND_BUSY);
    xSemaphoreGiveRecursive(driver->mux);
    return 0;
  }

  // Construct the header using the default arguments and the caller's arguments
  rdm_header_t header = {
      .message_len = 24 + request->pdl,
      .tn = rdm_get_transaction_num(dmx_num),
      .port_id = dmx_num + 1,
      .message_count = 0,
      .sub_device = request->sub_device,
      .cc = request->cc,
      .pid = request->pid,
      .pdl = request->pdl,
  };
  memcpy(&header.dest_uid, request->dest_uid, sizeof(header.dest_uid));
  memcpy(&header.src_uid, rdm_uid_get(dmx_num), sizeof(header.src_uid));
  const uint8_t request_tn = header.tn;  // header is reused for the response
  rdm_trace_entry_t tr = {
      .t_ms = (uint32_t)(esp_timer_get_time() / 1000),
      .dest = *request->dest_uid,
      .pid = request->pid,
      .cc = request->cc,
      .tn = request_tn,
      .size = -1,
  };

  // Copy the old data in the DMX buffer to a temporary buffer
  uint8_t old_data[257];
  const size_t packet_size = header.message_len + 2; 
  dmx_read(dmx_num, old_data, packet_size);

  // Write and send the RDM request
  rdm_write(dmx_num, &header, request->format, request->pd);
  const int64_t sent_at = esp_timer_get_time();
  if (!dmx_send(dmx_num)) {
    rdm_trace_commit(dmx_num, &tr, RDM_TRACE_SEND_FAILED);
    dmx_write(dmx_num, old_data, packet_size);  // Write old data back
    xSemaphoreGiveRecursive(driver->mux);
    if (ack != NULL) {
      ack->err = DMX_OK;
      ack->size = 0;
      ack->src_uid = (rdm_uid_t){0, 0};
      ack->pid = 0;
      ack->type = RDM_RESPONSE_TYPE_NONE;
      ack->message_count = 0;
      ack->pdl = 0;
    }
    return 0;
  }

  // Return early if no response is expected
  if (rdm_uid_is_broadcast(request->dest_uid) &&
      request->pid != RDM_PID_DISC_UNIQUE_BRANCH) {
    dmx_wait_sent(dmx_num, dmx_ms_to_ticks(23));
    rdm_trace_commit(dmx_num, &tr, RDM_TRACE_BROADCAST);
    dmx_write(dmx_num, old_data, packet_size);  // Write old data back
    xSemaphoreGiveRecursive(driver->mux);
    if (ack != NULL) {
      ack->err = DMX_OK;
      ack->size = 0;
      ack->src_uid = (rdm_uid_t){0, 0};
      ack->pid = 0;
      ack->type = RDM_RESPONSE_TYPE_NONE;
      ack->message_count = 0;
      ack->pdl = 0;
    }
    return 0;
  }

  // Attempt to receive the RDM response
  dmx_packet_t packet;
  dmx_receive(dmx_num, &packet, dmx_ms_to_ticks(23));
  tr.rx_us = (uint32_t)(esp_timer_get_time() - sent_at);
  tr.size = (int16_t)packet.size;
  tr.err = (uint8_t)packet.err;
  if (ack != NULL) {
    ack->err = packet.err;
    ack->size = packet.size;
  }

  // Return early if no response was received
  if (packet.size == 0) {
    rdm_trace_commit(dmx_num, &tr, RDM_TRACE_NO_ANSWER);
    dmx_write(dmx_num, old_data, packet_size);  // Write old data back
    xSemaphoreGiveRecursive(driver->mux);
    if (ack != NULL) {
      ack->src_uid = (rdm_uid_t){0, 0};
      ack->pid = 0;
      ack->type = RDM_RESPONSE_TYPE_NONE;
      ack->message_count = 0;
      ack->pdl = 0;
    }
    return 0;
  }

  // Return early if the response checksum was invalid
  if (!rdm_read_header(dmx_num, &header)) {
    rdm_trace_commit(dmx_num, &tr, RDM_TRACE_BAD_HEADER);
    dmx_write(dmx_num, old_data, packet_size);  // Write old data back
    xSemaphoreGiveRecursive(driver->mux);
    if (ack != NULL) {
      ack->src_uid = (rdm_uid_t){0, 0};
      ack->pid = 0;
      ack->type = RDM_RESPONSE_TYPE_INVALID;
      ack->message_count = 0;
      ack->pdl = 0;
    }
    return 0;
  }

  // A valid checksum doesn't make a packet the answer to this request. A
  // responder that went wrong was seen answering every request with the same
  // ACK, whatever UID it was sent to - taken as genuine, discovery "found"
  // devices at UID 0, 1, 2... and every GET to an absent UID succeeded.
  // ANSI E1.20 6.2: the response comes from the UID the request went to, is
  // addressed to this controller, echoes the request's transaction number and
  // PID, and has its command class + 1. A DISC_UNIQUE_BRANCH response carries
  // none of these (any answer, collisions included, only says "devices in
  // this range"), and the answer to QUEUED_MESSAGE carries the PID of the
  // queued response instead.
  if (request->pid != RDM_PID_DISC_UNIQUE_BRANCH) {
    if (!rdm_uid_is_eq(&header.src_uid, request->dest_uid) ||
        !rdm_uid_is_eq(&header.dest_uid, rdm_uid_get(dmx_num)) ||
        header.cc != request->cc + 1 ||
        (header.pid != request->pid &&
         request->pid != RDM_PID_QUEUED_MESSAGE)) {
      DMX_WARN("RDM response doesn't match the request (asked " UIDSTR
               " pid 0x%04x, answer from " UIDSTR " to " UIDSTR
               " pid 0x%04x cc 0x%02x): ignored",
               UID2STR(*request->dest_uid), request->pid,
               UID2STR(header.src_uid), UID2STR(header.dest_uid), header.pid,
               header.cc);
      rdm_trace_commit(dmx_num, &tr, RDM_TRACE_MISMATCH);
      dmx_write(dmx_num, old_data, packet_size);  // Write old data back
      xSemaphoreGiveRecursive(driver->mux);
      if (ack != NULL) {
        ack->src_uid = (rdm_uid_t){0, 0};
        ack->pid = 0;
        ack->type = RDM_RESPONSE_TYPE_INVALID;
        ack->message_count = 0;
        ack->pdl = 0;
      }
      return 0;
    }
    // A wrong transaction number alone is only reported: some responders
    // don't echo it, and rejecting them would leave those fixtures out of
    // reach altogether. What it guards against - a late answer to an earlier
    // request for the same PID of the same device - is also caught, for the
    // PIDs where it matters (one item of a list: personality, sensor, ...),
    // by the callers checking the item number the answer names.
    if (header.tn != request_tn) {
      DMX_WARN("RDM response from " UIDSTR " pid 0x%04x has transaction "
               "number %u, the request had %u (not ANSI E1.20 conformant): "
               "accepted",
               UID2STR(header.src_uid), header.pid, header.tn, request_tn);
    }
  }

  // Copy the parameter data into the output
  if (header.response_type == RDM_RESPONSE_TYPE_ACK &&
      header.pid != RDM_PID_DISC_UNIQUE_BRANCH) {
    if (pd == NULL) {
      rdm_read_pd(dmx_num, format, pd, size);
    }
  }

  // Copy the results into the ack struct
  if (ack != NULL) {
    memcpy(&ack->src_uid, &header.src_uid, sizeof(rdm_uid_t));
    ack->pid = header.pid;
    if (!rdm_response_type_is_valid(header.response_type)) {
      ack->type = RDM_RESPONSE_TYPE_INVALID;
      ack->pdl = header.pdl;
    } else {
      const char *word_format = "w";
      ack->type = header.response_type;
      if (header.response_type == RDM_RESPONSE_TYPE_ACK_TIMER) {
        // Estimated response time, in units of 100ms (ANSI E1.20 6.3.2).
        uint16_t timer;
        rdm_read_pd(dmx_num, word_format, &timer, sizeof(timer));
        ack->timer = dmx_ms_to_ticks((uint32_t)timer * 100);
      } else if (header.response_type == RDM_RESPONSE_TYPE_NACK_REASON) {
        uint16_t nack_reason;
        rdm_read_pd(dmx_num, word_format, &nack_reason, sizeof(nack_reason));
        ack->nack_reason = nack_reason;
      } else {
        rdm_read_pd(dmx_num, format, pd, size);
        ack->pdl = header.pdl;
      }
    }
    ack->message_count = header.message_count;
  }

  rdm_trace_commit(dmx_num, &tr, RDM_TRACE_ACCEPTED);

  // Write the old data from before the request back into the DMX driver
  dmx_write(dmx_num, old_data, packet_size);

  // Give the mutex back and return the PDL or true on success
  xSemaphoreGiveRecursive(driver->mux);
  if (header.response_type == RDM_RESPONSE_TYPE_ACK) {
    if (header.pdl == 0) {
      return 1;
    } else {
      return header.pdl;
    }
  } else {

    return 0;
  }
}

// How an ACK_TIMER is followed up: wait what the responder asked for (within
// these bounds - the caller, often the task that also refreshes DMX, is held
// meanwhile) and send the same request again, a few times at most.
#define RDM_ACK_TIMER_MAX_RETRIES 4
#define RDM_ACK_TIMER_WAIT_MIN_MS 50
#define RDM_ACK_TIMER_WAIT_MAX_MS 500

size_t rdm_send_request(dmx_port_t dmx_num, const rdm_request_t *request,
                        const char *format, void *pd, size_t size,
                        rdm_ack_t *ack) {
  rdm_ack_t local_ack = {.type = RDM_RESPONSE_TYPE_NONE};
  if (ack == NULL) {
    ack = &local_ack;
  }

  size_t ret = rdm_send_request_once(dmx_num, request, format, pd, size, ack);

  // ACK_TIMER: the responder has the request but not the answer yet. It was
  // simply reported to the caller, and every caller took it for a failure.
  // Seen where the RDM line is carried over a network: fixtures behind a
  // LumiNode (DMX out) -> Art-Net -> LumiNode (DMX in) chain, this controller
  // on the second node's input. What sits on the wire there cannot fetch the
  // far answer within the RDM response time, so most GETs came back as
  // ACK_TIMER the first time and the data was handed over when the same PID
  // was asked again. Without a follow-up the read was lost, and worse, the
  // prepared answer came out on the *next* request for that PID, whatever it
  // asked for: sensor 1 got sensor 0's definition, one personality another's
  // description. Asking again after the estimated time is meant to get the
  // right answer to the right question - not yet tried against that setup.
  for (int retry = 0; retry < RDM_ACK_TIMER_MAX_RETRIES &&
                      ack->type == RDM_RESPONSE_TYPE_ACK_TIMER;
       ++retry) {
    TickType_t wait = ack->timer;
    const TickType_t wait_min = dmx_ms_to_ticks(RDM_ACK_TIMER_WAIT_MIN_MS);
    const TickType_t wait_max = dmx_ms_to_ticks(RDM_ACK_TIMER_WAIT_MAX_MS);
    if (wait < wait_min) {
      wait = wait_min;
    } else if (wait > wait_max) {
      wait = wait_max;
    }
    vTaskDelay(wait);
    ret = rdm_send_request_once(dmx_num, request, format, pd, size, ack);
  }

  return ret;
}

uint32_t rdm_get_transaction_num(dmx_port_t dmx_num) {
  assert(dmx_num < DMX_NUM_MAX);
  assert(dmx_driver_is_installed(dmx_num));

  uint32_t tn;
  taskENTER_CRITICAL(DMX_SPINLOCK(dmx_num));
  tn = dmx_driver[dmx_num]->rdm.tn;
  taskEXIT_CRITICAL(DMX_SPINLOCK(dmx_num));

  return tn;
}
