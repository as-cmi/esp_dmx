/**
 * @file rdm/controller/include/rdm_info.h
 * @brief This file contains RDM information functions for the RDM
 * controller. The PIDs in RDM information include
 * RDM_PID_SUPPORTED_PARAMETERS and RDM_PID_PARAMETER_DESCRIPTION - these
 * allow a controller to discover and self-describe a responder's
 * manufacturer-specific parameters without any prior vendor-specific
 * knowledge.
 */
#pragma once

#include <stdint.h>

#include "dmx/include/types.h"
#include "rdm/controller.h"
#include "rdm/include/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sends an RDM GET supported parameters request and reads the
 * response, if any.
 *
 * GET supported parameters requests are sent without parameter data. If a
 * response is received, the response parameter data will include an array of
 * PIDs supported by the responding device beyond the minimum required set.
 *
 * @param dmx_num The DMX port number.
 * @param[in] dest_uid A pointer to the UID of the destination.
 * @param sub_device The sub-device number of the destination.
 * @param[out] pids A pointer to an array of PIDs which will be received in
 * the response.
 * @param size The size, in bytes, of the pids array. Used to prevent buffer
 * overflows.
 * @param[out] ack A pointer to an ACK struct which contains information about
 * the response, including information if no response is received.
 * @return The number of bytes that were received in the response parameter
 * data.
 */
size_t rdm_send_get_supported_parameters(dmx_port_t dmx_num,
                                         const rdm_uid_t *dest_uid,
                                         rdm_sub_device_t sub_device,
                                         uint16_t *pids, size_t size,
                                         rdm_ack_t *ack);

/**
 * @brief Sends an RDM GET parameter description request and reads the
 * response, if any.
 *
 * GET parameter description requests are sent with a 16-bit PID, which must
 * be in the manufacturer-specific range (RDM_PID_MANUFACTURER_SPECIFIC_BEGIN
 * through RDM_PID_MANUFACTURER_SPECIFIC_END, inclusive). If a response is
 * received, the response parameter data will self-describe that PID's data
 * type, read/write capability, valid range, and a human-readable name -
 * enough to generate GET and SET commands for it without any prior
 * vendor-specific knowledge.
 *
 * @param dmx_num The DMX port number.
 * @param[in] dest_uid A pointer to the UID of the destination.
 * @param sub_device The sub-device number of the destination.
 * @param pid The manufacturer-specific PID to describe.
 * @param[out] parameter_description A pointer to a parameter which will be
 * received in the response.
 * @param[out] ack A pointer to an ACK struct which contains information about
 * the response, including information if no response is received.
 * @return The number of bytes that were received in the response parameter
 * data.
 */
size_t rdm_send_get_parameter_description(
    dmx_port_t dmx_num, const rdm_uid_t *dest_uid,
    rdm_sub_device_t sub_device, uint16_t pid,
    rdm_parameter_description_t *parameter_description, rdm_ack_t *ack);

/**
 * @brief Sends a generic RDM GET request for a manufacturer-specific PID's
 * current value and reads the response, if any.
 *
 * Unlike the other functions in this file, the wire format of a
 * manufacturer-specific PID's value is not fixed - it must be determined at
 * runtime from a prior rdm_send_get_parameter_description() call's
 * data_type/pdl_size fields. This function only saves the caller from
 * hand-building an rdm_request_t; it applies no interpretation of the format
 * or the received data of its own.
 *
 * @param dmx_num The DMX port number.
 * @param[in] dest_uid A pointer to the UID of the destination.
 * @param sub_device The sub-device number of the destination.
 * @param pid The manufacturer-specific PID whose value is being requested.
 * @param format The response parameter data format string (see the esp_dmx
 * parameter format documentation), chosen by the caller to match the PID's
 * data type.
 * @param[out] pd A pointer to a parameter which will be received in the
 * response, in a shape matching format.
 * @param size The size, in bytes, of pd. Used to prevent buffer overflows.
 * @param[out] ack A pointer to an ACK struct which contains information about
 * the response, including information if no response is received.
 * @return The number of bytes that were received in the response parameter
 * data.
 */
size_t rdm_send_get_parameter_value(dmx_port_t dmx_num,
                                    const rdm_uid_t *dest_uid,
                                    rdm_sub_device_t sub_device,
                                    rdm_pid_t pid, const char *format,
                                    void *pd, size_t size, rdm_ack_t *ack);

/**
 * @brief Sends a generic RDM SET request for a manufacturer-specific PID's
 * value. SET counterpart to rdm_send_get_parameter_value() - same
 * runtime-determined-format rationale, chosen from a prior
 * rdm_send_get_parameter_description() call's data_type/pdl_size fields.
 *
 * @param dmx_num The DMX port number.
 * @param[in] dest_uid A pointer to the UID of the destination.
 * @param sub_device The sub-device number of the destination.
 * @param pid The manufacturer-specific PID whose value is being set.
 * @param format The outgoing parameter data format string, chosen by the
 * caller to match the PID's data type.
 * @param[in] pd A pointer to the value to send, in a shape matching format.
 * @param size The size, in bytes, of pd.
 * @param[out] ack A pointer to an ACK struct which contains information about
 * the response, including information if no response is received.
 * @return true if a properly formatted RDM_RESPONSE_TYPE_ACK was received.
 * @return false if no response was received, was improperly formatted, or an
 * RDM_RESPONSE_TYPE_ACK was not received.
 */
bool rdm_send_set_parameter_value(dmx_port_t dmx_num,
                                  const rdm_uid_t *dest_uid,
                                  rdm_sub_device_t sub_device, rdm_pid_t pid,
                                  const char *format, const void *pd,
                                  size_t size, rdm_ack_t *ack);

#ifdef __cplusplus
}
#endif
