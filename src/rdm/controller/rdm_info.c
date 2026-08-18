#include "include/rdm_info.h"

#include "dmx/include/driver.h"
#include "dmx/include/service.h"
#include "rdm/controller/include/utils.h"
#include "rdm/include/driver.h"
#include "rdm/include/uid.h"

size_t rdm_send_get_supported_parameters(dmx_port_t dmx_num,
                                         const rdm_uid_t *dest_uid,
                                         rdm_sub_device_t sub_device,
                                         uint16_t *pids, size_t size,
                                         rdm_ack_t *ack) {
  DMX_CHECK(dmx_num < DMX_NUM_MAX, 0, "dmx_num error");
  DMX_CHECK(dest_uid != NULL, 0, "dest_uid is null");
  DMX_CHECK(!rdm_uid_is_broadcast(dest_uid), 0, "dest_uid error");
  DMX_CHECK(sub_device < RDM_SUB_DEVICE_MAX, 0, "sub_device error");
  DMX_CHECK(pids != NULL, 0, "pids is null");
  DMX_CHECK(dmx_driver_is_installed(dmx_num), 0, "driver is not installed");

  const rdm_request_t request = {.dest_uid = dest_uid,
                                 .sub_device = sub_device,
                                 .cc = RDM_CC_GET_COMMAND,
                                 .pid = RDM_PID_SUPPORTED_PARAMETERS};

  const char *format = "w";
  return rdm_send_request(dmx_num, &request, format, pids, size, ack);
}

size_t rdm_send_get_parameter_description(
    dmx_port_t dmx_num, const rdm_uid_t *dest_uid,
    rdm_sub_device_t sub_device, uint16_t pid,
    rdm_parameter_description_t *parameter_description, rdm_ack_t *ack) {
  DMX_CHECK(dmx_num < DMX_NUM_MAX, 0, "dmx_num error");
  DMX_CHECK(dest_uid != NULL, 0, "dest_uid is null");
  DMX_CHECK(!rdm_uid_is_broadcast(dest_uid), 0, "dest_uid error");
  DMX_CHECK(sub_device < RDM_SUB_DEVICE_MAX, 0, "sub_device error");
  DMX_CHECK(pid >= RDM_PID_MANUFACTURER_SPECIFIC_BEGIN &&
            pid <= RDM_PID_MANUFACTURER_SPECIFIC_END, 0, "pid error");
  DMX_CHECK(parameter_description != NULL, 0,
            "parameter_description is null");
  DMX_CHECK(dmx_driver_is_installed(dmx_num), 0, "driver is not installed");

  const rdm_request_t request = {.dest_uid = dest_uid,
                                 .sub_device = sub_device,
                                 .cc = RDM_CC_GET_COMMAND,
                                 .pid = RDM_PID_PARAMETER_DESCRIPTION,
                                 .format = "w$",
                                 .pd = &pid,
                                 .pdl = sizeof(pid)};

  const char *format = "wbbbx00bbddda";
  return rdm_send_request(dmx_num, &request, format, parameter_description,
                          sizeof(*parameter_description), ack);
}

size_t rdm_send_get_parameter_value(dmx_port_t dmx_num,
                                    const rdm_uid_t *dest_uid,
                                    rdm_sub_device_t sub_device,
                                    rdm_pid_t pid, const char *format,
                                    void *pd, size_t size, rdm_ack_t *ack) {
  DMX_CHECK(dmx_num < DMX_NUM_MAX, 0, "dmx_num error");
  DMX_CHECK(dest_uid != NULL, 0, "dest_uid is null");
  DMX_CHECK(!rdm_uid_is_broadcast(dest_uid), 0, "dest_uid error");
  DMX_CHECK(sub_device < RDM_SUB_DEVICE_MAX, 0, "sub_device error");
  DMX_CHECK(pid >= RDM_PID_MANUFACTURER_SPECIFIC_BEGIN &&
            pid <= RDM_PID_MANUFACTURER_SPECIFIC_END, 0, "pid error");
  DMX_CHECK(format != NULL && pd != NULL, 0, "format or pd is null");
  DMX_CHECK(dmx_driver_is_installed(dmx_num), 0, "driver is not installed");

  const rdm_request_t request = {.dest_uid = dest_uid,
                                 .sub_device = sub_device,
                                 .cc = RDM_CC_GET_COMMAND,
                                 .pid = pid};

  return rdm_send_request(dmx_num, &request, format, pd, size, ack);
}

bool rdm_send_set_parameter_value(dmx_port_t dmx_num,
                                  const rdm_uid_t *dest_uid,
                                  rdm_sub_device_t sub_device, rdm_pid_t pid,
                                  const char *format, const void *pd,
                                  size_t size, rdm_ack_t *ack) {
  DMX_CHECK(dmx_num < DMX_NUM_MAX, 0, "dmx_num error");
  DMX_CHECK(dest_uid != NULL, 0, "dest_uid is null");
  DMX_CHECK(sub_device < RDM_SUB_DEVICE_MAX || sub_device == RDM_SUB_DEVICE_ALL,
            0, "sub_device error");
  DMX_CHECK(pid >= RDM_PID_MANUFACTURER_SPECIFIC_BEGIN &&
            pid <= RDM_PID_MANUFACTURER_SPECIFIC_END, 0, "pid error");
  DMX_CHECK(format != NULL && pd != NULL, 0, "format or pd is null");
  DMX_CHECK(dmx_driver_is_installed(dmx_num), 0, "driver is not installed");

  const rdm_request_t request = {.dest_uid = dest_uid,
                                 .sub_device = sub_device,
                                 .cc = RDM_CC_SET_COMMAND,
                                 .pid = pid,
                                 .format = format,
                                 .pd = pd,
                                 .pdl = size};

  return rdm_send_request(dmx_num, &request, NULL, NULL, 0, ack);
}
