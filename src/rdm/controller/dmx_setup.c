#include "include/dmx_setup.h"

#include "dmx/include/driver.h"
#include "dmx/include/service.h"
#include "rdm/controller/include/utils.h"
#include "rdm/include/driver.h"
#include "rdm/include/uid.h"

size_t rdm_send_get_dmx_start_address(dmx_port_t dmx_num,
                                      const rdm_uid_t *dest_uid,
                                      rdm_sub_device_t sub_device,
                                      uint16_t *dmx_start_address,
                                      rdm_ack_t *ack) {
  DMX_CHECK(dmx_num < DMX_NUM_MAX, 0, "dmx_num error");
  DMX_CHECK(dest_uid != NULL, 0, "dest_uid is null");
  DMX_CHECK(!rdm_uid_is_broadcast(dest_uid), 0, "dest_uid error");
  DMX_CHECK(sub_device < RDM_SUB_DEVICE_MAX, 0, "sub_device error");
  DMX_CHECK(dmx_start_address != NULL, 0, "dmx_start_address is null");
  DMX_CHECK(dmx_driver_is_installed(dmx_num), 0, "driver is not installed");

  const rdm_request_t request = {
    .dest_uid = dest_uid, 
    .sub_device = sub_device,
    .cc = RDM_CC_GET_COMMAND,
    .pid = RDM_PID_DMX_START_ADDRESS,
  };

  const char *format = "w$";
  return rdm_send_request(dmx_num, &request, format, dmx_start_address,
                          sizeof(*dmx_start_address), ack);
}

bool rdm_send_set_dmx_start_address(dmx_port_t dmx_num,
                                    const rdm_uid_t *dest_uid,
                                    rdm_sub_device_t sub_device,
                                    const uint16_t dmx_start_address,
                                    rdm_ack_t *ack) {
  DMX_CHECK(dmx_num < DMX_NUM_MAX, 0, "dmx_num error");
  DMX_CHECK(dest_uid != NULL, 0, "dest_uid is null");
  DMX_CHECK(sub_device < RDM_SUB_DEVICE_MAX || sub_device == RDM_SUB_DEVICE_ALL,
            0, "sub_device error");
  DMX_CHECK(dmx_start_address < 513, 0, "dmx_start_address is invalid");
  DMX_CHECK(dmx_driver_is_installed(dmx_num), 0, "driver is not installed");

  const rdm_request_t request = {.dest_uid = dest_uid,
                                 .sub_device = sub_device,
                                 .cc = RDM_CC_SET_COMMAND,
                                 .pid = RDM_PID_DMX_START_ADDRESS,
                                 .format = "w$",
                                 .pd = &dmx_start_address,
                                 .pdl = sizeof(dmx_start_address)};

  return rdm_send_request(dmx_num, &request, NULL, NULL, 0, ack);
}

size_t rdm_send_get_dmx_personality(dmx_port_t dmx_num,
                                    const rdm_uid_t *dest_uid,
                                    rdm_sub_device_t sub_device,
                                    rdm_dmx_personality_t *personality,
                                    rdm_ack_t *ack) {
  DMX_CHECK(dmx_num < DMX_NUM_MAX, 0, "dmx_num error");
  DMX_CHECK(dest_uid != NULL, 0, "dest_uid is null");
  DMX_CHECK(!rdm_uid_is_broadcast(dest_uid), 0, "dest_uid error");
  DMX_CHECK(sub_device < RDM_SUB_DEVICE_MAX, 0, "sub_device error");
  DMX_CHECK(personality != NULL, 0, "personality is null");
  DMX_CHECK(dmx_driver_is_installed(dmx_num), 0, "driver is not installed");

  const rdm_request_t request = {
    .dest_uid = dest_uid,
    .sub_device = sub_device,
    .cc = RDM_CC_GET_COMMAND,
    .pid = RDM_PID_DMX_PERSONALITY,
  };

  const char *format = "bb$";
  return rdm_send_request(dmx_num, &request, format, personality,
                          sizeof(*personality), ack);
}

size_t rdm_send_get_dmx_personality_description(
    dmx_port_t dmx_num, const rdm_uid_t *dest_uid,
    rdm_sub_device_t sub_device, uint8_t personality_num,
    rdm_dmx_personality_description_t *personality_description,
    rdm_ack_t *ack) {
  DMX_CHECK(dmx_num < DMX_NUM_MAX, 0, "dmx_num error");
  DMX_CHECK(dest_uid != NULL, 0, "dest_uid is null");
  DMX_CHECK(!rdm_uid_is_broadcast(dest_uid), 0, "dest_uid error");
  DMX_CHECK(sub_device < RDM_SUB_DEVICE_MAX, 0, "sub_device error");
  DMX_CHECK(personality_num > 0, 0, "personality_num error");
  DMX_CHECK(personality_description != NULL, 0,
            "personality_description is null");
  DMX_CHECK(dmx_driver_is_installed(dmx_num), 0, "driver is not installed");

  const rdm_request_t request = {.dest_uid = dest_uid,
                                 .sub_device = sub_device,
                                 .cc = RDM_CC_GET_COMMAND,
                                 .pid = RDM_PID_DMX_PERSONALITY_DESCRIPTION,
                                 .format = "b$",
                                 .pd = &personality_num,
                                 .pdl = sizeof(personality_num)};

  const char *format = "bwa";
  return rdm_send_request(dmx_num, &request, format, personality_description,
                          sizeof(*personality_description), ack);
}

bool rdm_send_set_dmx_personality(dmx_port_t dmx_num,
                                  const rdm_uid_t *dest_uid,
                                  rdm_sub_device_t sub_device,
                                  const uint8_t personality_num,
                                  rdm_ack_t *ack) {
  DMX_CHECK(dmx_num < DMX_NUM_MAX, 0, "dmx_num error");
  DMX_CHECK(dest_uid != NULL, 0, "dest_uid is null");
  DMX_CHECK(sub_device < RDM_SUB_DEVICE_MAX || sub_device == RDM_SUB_DEVICE_ALL,
            0, "sub_device error");
  DMX_CHECK(personality_num > 0, 0, "personality_num error");
  DMX_CHECK(dmx_driver_is_installed(dmx_num), 0, "driver is not installed");

  const rdm_request_t request = {.dest_uid = dest_uid,
                                 .sub_device = sub_device,
                                 .cc = RDM_CC_SET_COMMAND,
                                 .pid = RDM_PID_DMX_PERSONALITY,
                                 .format = "b$",
                                 .pd = &personality_num,
                                 .pdl = sizeof(personality_num)};

  return rdm_send_request(dmx_num, &request, NULL, NULL, 0, ack);
}