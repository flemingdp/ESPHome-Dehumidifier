// Protocol V3 - MAD50P1AWS / SK105 implementation.
#ifdef MIDEA_PROTOCOL_V3

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "midea_dehum.h"

namespace esphome {
namespace midea_dehum {

static const char* const TAG = "midea_dehum";

// MAD50P1AWS factory-adapter sequence captured on both UART directions.
static const uint8_t v3_announce[12] = {
    0xAA, 0x0B, 0xFF, 0xF4, 0x00, 0x00, 0x01, 0x00, 0x08, 0x07, 0x00, 0xF2};
static const uint8_t v3_acquiring[31] = {
    0xAA, 0x1E, 0xA1, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDA};
static const uint8_t v3_wifi[31] = {
    0xAA, 0x1E, 0xA1, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x08, 0x0D, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xFF, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x67};
static const uint8_t v3_wifi_ip_1[31] = {
    0xAA, 0x1E, 0xA1, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x08, 0x0D, 0x01, 0x01, 0x04, 0xBA, 0x08, 0xA8,
    0xC0, 0xFF, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x3B};
static const uint8_t v3_wifi_ip_2[31] = {
    0xAA, 0x1E, 0xA1, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x08, 0x0D, 0x01, 0x01, 0x04, 0xBA, 0x08, 0xA8,
    0xC0, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x3A};

static void v3_send_acquiring(MideaDehumComponent* self, uint8_t index);
static void v3_send_initial_network_status(MideaDehumComponent* self);

static void v3_start_handshake(MideaDehumComponent* self) {
  if (self->get_handshake_step() == 0) {
    ESP_LOGI(TAG, "V3 short 07 sent at +%u ms", (unsigned) millis());
    self->write_array(v3_announce, sizeof(v3_announce));
    self->set_handshake_step(1);
    // Diagnostic: schedule once from short 07, without gating on long 07.
    // Step 1 still tracks the pending response; only step 0 starts this burst.
    App.scheduler.set_timeout(self, "v3_acquire_1", 50, [self]() { v3_send_acquiring(self, 1); });
    App.scheduler.set_timeout(self, "v3_acquire_2", 383, [self]() { v3_send_acquiring(self, 2); });
    App.scheduler.set_timeout(self, "v3_acquire_3", 712, [self]() { v3_send_acquiring(self, 3); });
    App.scheduler.set_timeout(self, "v3_acquire_4", 1046, [self]() { v3_send_acquiring(self, 4); });
    App.scheduler.set_timeout(self, "v3_acquire_5", 1388, [self]() { v3_send_acquiring(self, 5); });
    App.scheduler.set_timeout(self, "v3_acquire_6", 1708, [self]() { v3_send_acquiring(self, 6); });
    App.scheduler.set_timeout(self, "v3_wifi", 2056,
                              [self]() { v3_send_initial_network_status(self); });
    App.scheduler.set_timeout(self, "v3_long_07_timeout", 1500, [self]() {
      if (self->get_handshake_step() == 1)
        ESP_LOGW(TAG, "V3 long 07 not received / timeout at +%u ms; diagnostic startup continues",
                 (unsigned) millis());
    });
  }
}

static bool v3_is_status_response(uint8_t* data, size_t len) {
  return len == 0x23 && data[0] == 0xAA && data[1] == 0x22 && data[2] == 0xA1 &&
         data[9] == 0x05 && data[10] == 0xA0;
}

// Captured MAD50P1AWS query body, followed on the wire by a transaction ID.
static const uint8_t v3_status_payload[20] = {
    0x41, 0x81, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static void v3_get_status_query(MideaDehumComponent* self) {
  self->sendV3SequencedMessage(0x03, v3_status_payload,
                               sizeof(v3_status_payload), false);
}

static bool v3_encode_mode(uint8_t status_mode, uint8_t* request_mode) {
  if (status_mode < 0x01 || status_mode > 0x03) return false;
  *request_mode = status_mode;
  return true;
}

static bool v3_encode_fan(uint8_t status_fan, uint8_t* request_fan) {
  if (status_fan == 0x28)
    *request_fan = 0xA8;
  else if (status_fan == 0x50)
    *request_fan = 0xD0;
  else
    return false;
  return true;
}

// MAD50P1AWS control body observed in successful factory power ON/OFF and
// target-humidity transactions. Only fields evidenced by those captures are
// populated here.
// Bytes 8..20 remain zero; in particular, V2-only trailing fields are not
// copied into this shorter variant.
static void v3_send_set_status(MideaDehumComponent* self) {
  uint8_t cmd[21];
  memset(cmd, 0, sizeof(cmd));

  const auto& s = self->get_state();
  cmd[0] = 0x48;
  // Confirmed by paired factory captures: 0x43=ON, 0x42=OFF.
  cmd[1] = 0x42 | (s.powerOn ? 0x01 : 0x00);
  ESP_LOGD(TAG, "V3 CONTROL STATE: power=%u mode=%02X fan=%02X target=%02X",
           s.powerOn, s.mode, s.fanSpeed, s.humiditySetpoint);
  if (!v3_encode_mode(s.mode, &cmd[2])) {
    ESP_LOGW(TAG, "V3 control not sent: unsupported mode state %02X", s.mode);
    return;
  }
  if (!v3_encode_fan(s.fanSpeed, &cmd[3])) {
    // TODO: Reconsider this diagnostic fallback once initial V3 state acquisition
    // is reliable. This does not establish a status mapping for 0x3C or others.
    ESP_LOGW(TAG, "V3 unknown/uninitialized fan state %02X; using diagnostic Low request A8",
             s.fanSpeed);
    cmd[3] = 0xA8;
  }
#ifdef USE_MIDEA_DEHUM_TIMER
  // Use the V2 quarter-hour layout; V3 timer writes need device verification.
  if (self->get_timer_write_pending()) {
    const float hours = self->get_pending_timer_hours();
    uint8_t on_raw = 0x7F, off_raw = 0x7F, ext_raw = 0x00;
    if (hours > 0.01f) {
      const uint16_t minutes = static_cast<uint16_t>(hours * 60.0f + 0.5f);
      const uint8_t encoded = 0x80 | ((minutes / 60) << 2) | ((minutes % 60) / 15);
      if (self->get_pending_applies_to_on())
        on_raw = encoded;
      else
        off_raw = encoded;
      ext_raw = 0x0F;
    }
    self->set_last_on_raw(on_raw);
    self->set_last_off_raw(off_raw);
    self->set_last_ext_raw(ext_raw);
    self->clear_timer_write_pending();
  }
  cmd[4] = self->get_last_on_raw();
  cmd[5] = self->get_last_off_raw();
  cmd[6] = self->get_last_ext_raw();
#else
  // The captured factory command echoed the appliance's 0x7F/0x7F timer
  // sentinel even when timer entities were not involved.
  cmd[4] = 0x7F;
  cmd[5] = 0x7F;
#endif
  // Confirmed direct percentage encoding: 0x37=55%, 0x3C=60%.
  cmd[7] = s.humiditySetpoint;

  // Factory pump writes use 0x18 for ON and 0x10 for OFF. Non-pump
  // controls use zero here; do not copy the raw status flags.
  uint8_t command_flags = 0x00;
#ifdef USE_MIDEA_DEHUM_PUMP
  if (self->v3_pump_command_pending())
    command_flags = self->v3_pump_command_on() ? 0x18 : 0x10;
#endif
#ifdef USE_MIDEA_DEHUM_FILTER_BUTTON
  // Filter acknowledgement follows V2; not yet captured on V3 hardware.
  if (self->pop_filter_cleaned_flag()) command_flags |= 0x80;
#endif
  cmd[9] = command_flags;
  ESP_LOGD(TAG, "V3 command flags: status=%02X command=%02X pump=%s",
           self->get_feature_flags(), command_flags,
           self->v3_pump_command_pending() && self->v3_pump_command_on() ? "on" : "off");
  self->clear_v3_pump_command();

  ESP_LOGD(TAG, "V3 CONTROL ENCODED: power=%02X mode=%02X fan=%02X target=%02X timer=%02X/%02X/%02X flags=%02X",
           cmd[1], cmd[2], cmd[3], cmd[7], cmd[4], cmd[5], cmd[6], cmd[9]);
  self->sendV3SequencedMessage(0x02, cmd, sizeof(cmd), true);
}

static void v3_send_acquiring(MideaDehumComponent* self, uint8_t index) {
  ESP_LOGI(TAG, "V3 A0 acquisition %u/6 at +%u ms", index, (unsigned) millis());
  self->write_array(v3_acquiring, sizeof(v3_acquiring));
}

static void v3_send_initial_network_status(MideaDehumComponent* self) {
  ESP_LOGI(TAG, "V3 initial 0D sent at +%u ms", (unsigned) millis());
  self->write_array(v3_wifi, sizeof(v3_wifi));
}

static void v3_send_connected_network_status(MideaDehumComponent* self,
                                             const uint8_t* frame,
                                             uint8_t index) {
  ESP_LOGI(TAG, "V3 connected 0D %u/2 TX at +%u ms", index,
           (unsigned) millis());
  self->write_array(frame, sizeof(v3_wifi_ip_1));
}

static bool v3_on_message(MideaDehumComponent* self, uint8_t* data, size_t len) {
  if (len < 10) return false;

  // Command and poll responses use agreement 03, subtype C8, and echo the
  // request sequence in the byte immediately before CRC8/checksum.
  if (len == 0x23 && data[8] == 0x03 && data[10] == 0xC8 &&
      (data[9] == 0x02 || data[9] == 0x03)) {
    const uint8_t sequence = data[len - 3];
    if (data[9] == 0x02)
      self->matchV3CommandResponse(sequence);
    else
      ESP_LOGD(TAG, "V3 RX poll response: sequence=%02X", sequence);
    self->parseState(data, len);
    return true;
  }

  if (data[9] != 0x07 || self->get_handshake_step() != 1)
    return PROTOCOL_V1.on_message(self, data, len);

  ESP_LOGI(TAG, "V3 long 07 received at +%u ms", (unsigned) millis());
  self->set_appliance_type(data[2]);
  self->set_mcu_protocol_version(data[8]);
  self->set_device_info_known(true);
  self->set_handshake_step(2);

  // Preserve the later connected/IP announcements relative to the long 07.
  App.scheduler.set_timeout(self, "v3_ip_1", 10743, [self]() {
    v3_send_connected_network_status(self, v3_wifi_ip_1, 1);
  });
  App.scheduler.set_timeout(self, "v3_ip_2", 12719, [self]() {
    v3_send_connected_network_status(self, v3_wifi_ip_2, 2);
  });
  return true;
}

const ProtocolVTable PROTOCOL_V3 = {
    .version = 3,
    .start_handshake = v3_start_handshake,
    .is_status_response = v3_is_status_response,
    .on_message = v3_on_message,
    .get_status_query = v3_get_status_query,
    .send_set_status = v3_send_set_status,
    .startup_delay_ms = 2000,
};

}  // namespace midea_dehum
}  // namespace esphome

#endif  // MIDEA_PROTOCOL_V3
