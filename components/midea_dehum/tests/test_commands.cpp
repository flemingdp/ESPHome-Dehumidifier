// Category 2: Command Tests (V1)
//
// Verifies every setter generates the correct command frame bytes.
// All commands go through sendSetStatus() → sendMessage() → TestUART capture.
//
// Command payload layout (25 bytes, after 10-byte header):
//   [0]  = 0x48 write marker
//   [1]  = power (0x01=ON, 0x00=OFF) | beep bit6=0x40
//   [2]  = mode (1=Setpoint, 2=Continuous, 3=Smart, 4=ClothesDrying)
//   [3]  = fan speed (40=Low, 80=High)
//   [4-6]= timer raw bytes
//   [7]  = humidity setpoint %
//   [9]  = feature flags: ion(bit6) sleep(bit5) pump(bit4+3)
//   [10] = swing (bit3=0x08)

#include "fixtures.h"

// ══════════════════════════════════════════════════════════════════════════
//  2.1  Power ON / OFF
// ══════════════════════════════════════════════════════════════════════════

static void test_power() {
  TestMideaDehum dev;
  dev.setup();
  run_scheduler();

  // Power ON
  tx_clear(dev);
  dev.cmd_power(true);
  ASSERT_EQ(cmd_payload(tx_last(dev, "power ON"))[1], 0x01, "power ON: byte[1]=0x01");
  ASSERT(dev.pub_power(), "published: power is ON");

  // Power OFF
  tx_clear(dev);
  dev.cmd_power(false);
  ASSERT_EQ(cmd_payload(tx_last(dev, "power OFF"))[1], 0x00, "power OFF: byte[1]=0x00");
  ASSERT(!dev.pub_power(), "published: power is OFF");
}

// ══════════════════════════════════════════════════════════════════════════
//  2.2  Mode presets
// ══════════════════════════════════════════════════════════════════════════

static void test_modes() {
  TestMideaDehum dev;
  dev.setup();
  run_scheduler();

  auto verify_mode = [&](uint8_t m, const char* expected, const char* label) {
    tx_clear(dev);
    dev.cmd_mode(m);
    ASSERT_EQ(cmd_payload(tx_last(dev, label))[2], m, label);
    bool preset_ok = (dev.pub_preset() && std::string(dev.pub_preset()) == expected);
    ASSERT(preset_ok, (std::string(label) + ": preset matches").c_str());
  };

  verify_mode(1, "Setpoint",      "mode Setpoint");
  verify_mode(2, "Continuous",    "mode Continuous");
  verify_mode(3, "Smart",         "mode Smart");
  verify_mode(4, "ClothesDrying", "mode ClothesDrying");
}

// ══════════════════════════════════════════════════════════════════════════
//  2.3  Fan speed
// ══════════════════════════════════════════════════════════════════════════

static void test_fan() {
  TestMideaDehum dev;
  dev.setup();
  run_scheduler();

  tx_clear(dev);
  dev.cmd_fan(esphome::climate::CLIMATE_FAN_LOW);
  ASSERT_EQ(cmd_payload(tx_last(dev, "fan LOW"))[3], 40, "fan LOW: byte[3]=40");
  ASSERT_EQ(dev.pub_fan(), (int)esphome::climate::CLIMATE_FAN_LOW, "published: fan LOW");

  tx_clear(dev);
  dev.cmd_fan(esphome::climate::CLIMATE_FAN_MEDIUM);
  ASSERT_EQ(cmd_payload(tx_last(dev, "fan MEDIUM"))[3], 60, "fan MEDIUM: byte[3]=60");

  tx_clear(dev);
  dev.cmd_fan(esphome::climate::CLIMATE_FAN_HIGH);
  ASSERT_EQ(cmd_payload(tx_last(dev, "fan HIGH"))[3], 80, "fan HIGH: byte[3]=80");
  ASSERT_EQ(dev.pub_fan(), (int)esphome::climate::CLIMATE_FAN_HIGH, "published: fan HIGH");
}

// ══════════════════════════════════════════════════════════════════════════
//  2.4  Target humidity
// ══════════════════════════════════════════════════════════════════════════

static void test_humidity() {
  TestMideaDehum dev;
  dev.setup();
  run_scheduler();

  tx_clear(dev);
  dev.cmd_humidity(35.0f);
  ASSERT_EQ(cmd_payload(tx_last(dev, "humidity 35%"))[7], 35, "humidity 35%: byte[7]=35");

  tx_clear(dev);
  dev.cmd_humidity(85.0f);
  ASSERT_EQ(cmd_payload(tx_last(dev, "humidity 85%"))[7], 85, "humidity 85%: byte[7]=85");
}

// ══════════════════════════════════════════════════════════════════════════
//  2.5  Pump
// ══════════════════════════════════════════════════════════════════════════

static void test_pump() {
  TestMideaDehum dev;
  dev.setup();
  run_scheduler();

  tx_clear(dev);
  dev.set_pump_state(true);
  ASSERT_EQ(cmd_payload(tx_last(dev, "pump ON"))[9], 0x18, "pump ON: byte[9]=0x18");

  tx_clear(dev);
  dev.set_pump_state(false);
  ASSERT_EQ(cmd_payload(tx_last(dev, "pump OFF"))[9], 0x10, "pump OFF: byte[9]=0x10");
}

// ══════════════════════════════════════════════════════════════════════════
//  2.6  Ionizer
// ══════════════════════════════════════════════════════════════════════════

static void test_ion() {
  TestMideaDehum dev;
  dev.setup();
  run_scheduler();

  tx_clear(dev);
  dev.set_ion_state(true);
  ASSERT_EQ(cmd_payload(tx_last(dev, "ion ON"))[9] & 0x40, 0x40, "ion ON: byte[9] bit6");

  tx_clear(dev);
  dev.set_ion_state(false);
  ASSERT_EQ(cmd_payload(tx_last(dev, "ion OFF"))[9] & 0x40, 0x00, "ion OFF: byte[9] bit6 clear");
}

// ══════════════════════════════════════════════════════════════════════════
//  2.7  Sleep
// ══════════════════════════════════════════════════════════════════════════

static void test_sleep() {
  TestMideaDehum dev;
  dev.setup();
  run_scheduler();

  tx_clear(dev);
  dev.set_sleep_state(true);
  ASSERT_EQ(cmd_payload(tx_last(dev, "sleep ON"))[9] & 0x20, 0x20, "sleep ON: byte[9] bit5");

  tx_clear(dev);
  dev.set_sleep_state(false);
  ASSERT_EQ(cmd_payload(tx_last(dev, "sleep OFF"))[9] & 0x20, 0x00, "sleep OFF: byte[9] bit5 clear");
}

// ══════════════════════════════════════════════════════════════════════════
//  2.8  Beep (requires USE_MIDEA_DEHUM_BEEP)
// ══════════════════════════════════════════════════════════════════════════

#ifdef USE_MIDEA_DEHUM_BEEP
static void test_beep() {
  TestMideaDehum dev;
  dev.setup();
  run_scheduler();

  tx_clear(dev);
  dev.set_beep_state(true);
  ASSERT_EQ(cmd_payload(tx_last(dev, "beep ON"))[1] & 0x40, 0x40, "beep ON: byte[1] bit6");

  tx_clear(dev);
  dev.set_beep_state(false);
  ASSERT_EQ(cmd_payload(tx_last(dev, "beep OFF"))[1] & 0x40, 0x00, "beep OFF: byte[1] bit6 clear");
}
#endif

// ══════════════════════════════════════════════════════════════════════════
//  2.9  Swing
// ══════════════════════════════════════════════════════════════════════════

#ifdef USE_MIDEA_DEHUM_SWING
static void test_swing() {
  TestMideaDehum dev;
  dev.setup();
  run_scheduler();

  tx_clear(dev);
  dev.cmd_swing(esphome::climate::CLIMATE_SWING_VERTICAL);
  // control() may produce >1 TX frame when swing changes (sends separately for
  // swing + via handleStateUpdateRequest). Check the last frame.
  ASSERT(dev.uart_.tx_count() >= 1, "swing ON: TX frame(s) sent");
  ASSERT_EQ(dev.uart_.tx_at(dev.uart_.tx_count() - 1).data[20] & 0x08, 0x08, "swing ON: byte[20] bit3");

  tx_clear(dev);
  dev.cmd_swing(esphome::climate::CLIMATE_SWING_OFF);
  ASSERT(dev.uart_.tx_count() >= 1, "swing OFF: TX frame(s) sent");
  ASSERT_EQ(dev.uart_.tx_at(dev.uart_.tx_count() - 1).data[20] & 0x08, 0x00, "swing OFF: byte[20] bit3 clear");
}
#endif

// ══════════════════════════════════════════════════════════════════════════
//  2.10  Timer
// ══════════════════════════════════════════════════════════════════════════

#ifdef USE_MIDEA_DEHUM_TIMER
static void test_timer() {
  TestMideaDehum dev;
  dev.setup();
  run_scheduler();

  // Set 1-hour timer.  The original timer encoding collapses 1h00m → 0h + 3 quarters + fine offset,
  // resulting in byte[4]=0x83 (not 0x84 as the naive formula would suggest).
  tx_clear(dev);
  dev.set_timer_hours(1.0f, false);
  auto p = cmd_payload(tx_last(dev, "timer 1h"));
  ASSERT((p[4] & 0x80) != 0, "timer 1h: timer bit set");

  // Clear timer
  tx_clear(dev);
  dev.set_timer_hours(0.0f, false);
  p = cmd_payload(tx_last(dev, "timer clear"));
  ASSERT_EQ(p[4], 0x00, "timer clear: byte[4]=0x00");
  ASSERT_EQ(p[5], 0x00, "timer clear: byte[5]=0x00");
}
#endif

// ══════════════════════════════════════════════════════════════════════════
//  2.11  Idempotency — duplicate commands shouldn't generate TX
// ══════════════════════════════════════════════════════════════════════════

static void test_idempotency() {
  TestMideaDehum dev;
  dev.setup();
  run_scheduler();

  // Issue a command that changes state, verify TX count increases
  size_t before = dev.uart_.tx_count();
  dev.cmd_power(true);  // OFF → ON (change)
  ASSERT(dev.uart_.tx_count() > before, "changed state → TX sent");

  // Issue same command again — no state change, no TX
  before = dev.uart_.tx_count();
  dev.cmd_power(true);  // ON → ON (no change)
  ASSERT(dev.uart_.tx_count() == before, "no state change → no TX (idempotent)");
}

// ══════════════════════════════════════════════════════════════════════════
//  V2 Command Tests — verify wire bytes for every control action
// ══════════════════════════════════════════════════════════════════════════
//
// V2 command payload (25 bytes, after 10-byte header):
//   [0]  = 0x48 write marker
//   [1]  = power (0x03=ON, 0x02=OFF; +0x40 if beep enabled)
//   [2]  = mode (1=Setpoint, 2=Continuous, 3=Smart, 4=ClothesDrying/Max)
//   [3]  = fan (0xA8=Low, 0xD0=High)
//   [4-6]= timer raw bytes (echoed from status, or pending write)
//   [7]  = humidity setpoint %
//   [8]  = 0x00
//   [9]  = pump (0x18=ON, 0x10=OFF)
//   [10-14]= padding
//   [15] = water level
//   [16] = 0x01
//   [17-24]= padding

static void test_v2_cmd_power() {
  TestMideaDehum dev;
  complete_v2_handshake(dev);

  tx_clear(dev);
  dev.cmd_power(true);
  ASSERT_EQ(cmd_payload(tx_last(dev, "V2 power ON"))[1], 0x03, "V2 power ON: byte[1]=0x03");
  ASSERT(dev.pub_power(), "published: power is ON");

  tx_clear(dev);
  dev.cmd_power(false);
  ASSERT_EQ(cmd_payload(tx_last(dev, "V2 power OFF"))[1], 0x02, "V2 power OFF: byte[1]=0x02");
  ASSERT(!dev.pub_power(), "published: power is OFF");
}

static void test_v2_cmd_modes() {
  TestMideaDehum dev;
  complete_v2_handshake(dev);

  auto verify = [&](uint8_t m, const char* expected, const char* label) {
    tx_clear(dev);
    dev.cmd_mode(m);
    ASSERT_EQ(cmd_payload(tx_last(dev, label))[2], m, label);
    bool ok = (dev.pub_preset() && std::string(dev.pub_preset()) == expected);
    ASSERT(ok, (std::string(label) + ": preset matches").c_str());
  };

  verify(1, "Setpoint",      "V2 mode Setpoint");
  verify(2, "Continuous",    "V2 mode Continuous");
  verify(3, "Smart",         "V2 mode Smart");
  verify(4, "ClothesDrying", "V2 mode ClothesDrying");
}

static void test_v2_cmd_fan() {
  TestMideaDehum dev;
  complete_v2_handshake(dev);

  tx_clear(dev);
  dev.cmd_fan(esphome::climate::CLIMATE_FAN_LOW);
  ASSERT_EQ(cmd_payload(tx_last(dev, "V2 fan LOW"))[3], 0xA8, "V2 fan LOW: byte[3]=0xA8");
  ASSERT_EQ(dev.pub_fan(), (int)esphome::climate::CLIMATE_FAN_LOW, "published: fan LOW");

  tx_clear(dev);
  dev.cmd_fan(esphome::climate::CLIMATE_FAN_HIGH);
  ASSERT_EQ(cmd_payload(tx_last(dev, "V2 fan HIGH"))[3], 0xD0, "V2 fan HIGH: byte[3]=0xD0");
  ASSERT_EQ(dev.pub_fan(), (int)esphome::climate::CLIMATE_FAN_HIGH, "published: fan HIGH");
}

static void test_v2_cmd_humidity() {
  TestMideaDehum dev;
  complete_v2_handshake(dev);

  tx_clear(dev);
  dev.cmd_humidity(35.0f);
  ASSERT_EQ(cmd_payload(tx_last(dev, "V2 humidity 35%"))[7], 35, "V2 humidity 35%: byte[7]=35");

  tx_clear(dev);
  dev.cmd_humidity(85.0f);
  ASSERT_EQ(cmd_payload(tx_last(dev, "V2 humidity 85%"))[7], 85, "V2 humidity 85%: byte[7]=85");
}

#ifdef USE_MIDEA_DEHUM_PUMP
static void test_v2_cmd_pump() {
  TestMideaDehum dev;
  complete_v2_handshake(dev);

  tx_clear(dev);
  dev.set_pump_state(true);
  ASSERT_EQ(cmd_payload(tx_last(dev, "V2 pump ON"))[9], 0x18, "V2 pump ON: byte[9]=0x18");

  tx_clear(dev);
  dev.set_pump_state(false);
  ASSERT_EQ(cmd_payload(tx_last(dev, "V2 pump OFF"))[9], 0x10, "V2 pump OFF: byte[9]=0x10");
}
#endif

// 2.17  V2 Filter cleaned flag — set then verify cmd[9] bit 7 (0x80)
#ifdef USE_MIDEA_DEHUM_FILTER_BUTTON
static void test_v2_cmd_filter_cleaned() {
  TestMideaDehum dev;
  complete_v2_handshake(dev);

  tx_clear(dev);
  dev.set_filter_cleaned_flag(true);
  dev.sendSetStatus();

  ASSERT_EQ(cmd_payload(tx_last(dev, "V2 filter cleaned"))[9] & 0x80, 0x80,
            "V2 filter cleaned: byte[9] bit7=0x80");
  // Flag should be consumed
  ASSERT(!dev.pop_filter_cleaned_flag(), "V2 filter cleaned: flag consumed after sendSetStatus");
}
#endif

// 2.18  V2 Water level threshold — echoes tank level from last status
static void test_v2_cmd_water_level() {
  TestMideaDehum dev;
  complete_v2_handshake(dev);

  // Feed a status with tank level = 0x4B (75%) and defrost off (bit7=0)
  uint8_t status_with_tank[] = {
      0xAA, 0x23, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x05, 0xA0,
      0x00, 0x03, 0x28, 0x7F, 0x7F, 0x00, 0x32, 0x00, 0x00,
      0x4B,  // byte 20: tank = 0x4B (75%), defrost = 0
      0x00, 0x00, 0x00, 0x00, 0x2D, 0x5F, 0x08, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00};
  // Fix length byte (35) and checksum
  status_with_tank[1] = 0x22;
  uint32_t cs_sum = 0;
  for (size_t i = 1; i < sizeof(status_with_tank) - 1; i++)
    cs_sum += status_with_tank[i];
  status_with_tank[sizeof(status_with_tank) - 1] = (256 - (cs_sum & 0xFF)) & 0xFF;
  dev.rx_enqueue(status_with_tank, sizeof(status_with_tank));
  dev.loop();

  tx_clear(dev);
  dev.sendSetStatus();

  ASSERT_EQ(cmd_payload(tx_last(dev, "V2 water level"))[15], 0x4B,
            "V2 water level: cmd[15]=0x4B (echoes last status tank level)");
}

// 2.19  V2 Timer — echoes last known timer state from status
#ifdef USE_MIDEA_DEHUM_TIMER
static void test_v2_cmd_timer_echo() {
  TestMideaDehum dev;
  complete_v2_handshake(dev);

  // V2_STATUS has timer bytes 0x7F, 0x7F, 0x00 (no timer set).
  // Verify these are echoed back in cmd[4-6].
  tx_clear(dev);
  dev.sendSetStatus();

  const uint8_t* p = cmd_payload(tx_last(dev, "V2 timer echo"));
  ASSERT_EQ(p[4], 0x7F, "V2 timer echo: cmd[4]=0x7F (no ON timer)");
  ASSERT_EQ(p[5], 0x7F, "V2 timer echo: cmd[5]=0x7F (no OFF timer)");
  ASSERT_EQ(p[6], 0x00, "V2 timer echo: cmd[6]=0x00 (no ext timer)");
}

static void test_v2_cmd_timer_set() {
  TestMideaDehum dev;
  complete_v2_handshake(dev);  // device is OFF (power=0)

  // Simulate HA setting a 2-hour timer. Since device is OFF,
  // pending_applies_to_on_ = true → ON timer at cmd[4].
  dev.set_timer_hours(2.0f, false);

  tx_clear(dev);
  dev.sendSetStatus();

  const uint8_t* p = cmd_payload(tx_last(dev, "V2 timer set 2h"));
  // 2h ON timer: 0x80 | ((2 & 0x1F) << 2) | 0 = 0x88
  ASSERT_EQ(p[4], 0x88, "V2 timer 2h: cmd[4]=0x88 (ON timer 2h)");
  ASSERT_EQ(p[5], 0x00, "V2 timer 2h: cmd[5]=0x00 (no OFF timer)");
}
#endif

// 2.21  V2 Reset water level — sends 0x03/0xC8 frame
#ifdef USE_MIDEA_DEHUM_RESET_WATER_LEVEL
static void test_v2_cmd_reset_water_level() {
  TestMideaDehum dev;
  complete_v2_handshake(dev);

  tx_clear(dev);
  dev.sendResetWaterLevel();

  const CapturedFrame& f = tx_last(dev, "V2 reset water level");
  // sendMessage(0x03, 0x08, 0x00, 25, cmd) produces a 37-byte frame:
  //   10-byte header + 25-byte payload + CRC + checksum
  // header[8] = 0x08 (agreement version), header[9] = 0x03 (msg type)
  // payload[0] = data[10] = 0xC8 (reset marker)
  ASSERT_EQ((int) f.data.size(), 37, "V2 reset WL: frame is 37 bytes");
  ASSERT_EQ(f.data[0], 0xAA, "V2 reset WL: start byte 0xAA");
  ASSERT_EQ(f.data[9], 0x03, "V2 reset WL: header[9]=0x03 (msg type)");
  ASSERT_EQ(f.data[8], 0x08, "V2 reset WL: header[8]=0x08 (agreement version)");
  ASSERT_EQ(f.data[10], 0xC8, "V2 reset WL: payload[0]=0xC8 (reset marker)");
  // Verify checksum validates
  uint32_t sum = 0;
  for (size_t i = 1; i < f.data.size(); i++) sum += f.data[i];
  ASSERT_EQ((int)(sum & 0xFF), 0, "V2 reset WL: checksum validates");
}
#endif

// ══════════════════════════════════════════════════════════════════════════
//  V3 captured transaction tests
// ══════════════════════════════════════════════════════════════════════════

static void test_v3_factory_power_on() {
  TestMideaDehum dev;
  dev.set_protocol_version(3);
  dev.set_handshake_enabled(false);
  dev.setup();

  dev.inject(MAD50P1AWS_BEFORE_POWER_ON, sizeof(MAD50P1AWS_BEFORE_POWER_ON));
  dev.seed_feature_flags(0x00);  // captured request carried zero at byte 19
  dev.seed_v3_sequence(0x08);
  tx_clear(dev);
  dev.cmd_power(true);

  const CapturedFrame& frame = tx_last(dev, "V3 factory power ON");
  ASSERT_EQ(frame.data.size(), sizeof(MAD50P1AWS_FACTORY_POWER_ON),
            "V3 power ON: frame is 34 bytes");
  ASSERT(frame.data.size() == sizeof(MAD50P1AWS_FACTORY_POWER_ON) &&
             memcmp(frame.data.data(), MAD50P1AWS_FACTORY_POWER_ON,
                    sizeof(MAD50P1AWS_FACTORY_POWER_ON)) == 0,
         "V3 power ON: byte-for-byte factory frame match");
  ASSERT(dev.v3_command_pending(), "V3 power ON: command response pending");

  uint8_t wrong_sequence_response[sizeof(MAD50P1AWS_FACTORY_POWER_ON_RESPONSE)];
  memcpy(wrong_sequence_response, MAD50P1AWS_FACTORY_POWER_ON_RESPONSE,
         sizeof(wrong_sequence_response));
  wrong_sequence_response[sizeof(wrong_sequence_response) - 3] = 0x0A;
  dev.inject(wrong_sequence_response, sizeof(wrong_sequence_response));
  ASSERT(dev.v3_command_pending(),
         "V3 power ON: mismatched response leaves command pending");

  dev.inject(MAD50P1AWS_FACTORY_POWER_ON_RESPONSE,
             sizeof(MAD50P1AWS_FACTORY_POWER_ON_RESPONSE));
  dev.seed_feature_flags(0x00);  // captured request carried zero at byte 19
  ASSERT(!dev.v3_command_pending(), "V3 power ON: matching response correlated");
  ASSERT(dev.raw_power(), "V3 power ON: response state parsed");
}

static void test_v3_factory_power_off() {
  TestMideaDehum dev;
  dev.set_protocol_version(3);
  dev.set_handshake_enabled(false);
  dev.setup();
  dev.inject(MAD50P1AWS_FACTORY_POWER_ON_RESPONSE,
             sizeof(MAD50P1AWS_FACTORY_POWER_ON_RESPONSE));
  dev.seed_feature_flags(0x00);  // captured requests carried zero at byte 19
  dev.seed_v3_sequence(0x08);

  tx_clear(dev);
  dev.cmd_power(false);
  const CapturedFrame& frame = tx_last(dev, "V3 factory power OFF");
  ASSERT(frame.data.size() == sizeof(MAD50P1AWS_FACTORY_POWER_OFF) &&
             memcmp(frame.data.data(), MAD50P1AWS_FACTORY_POWER_OFF,
                    sizeof(MAD50P1AWS_FACTORY_POWER_OFF)) == 0,
         "V3 power OFF: byte-for-byte factory frame match");

  dev.inject(MAD50P1AWS_FACTORY_POWER_OFF_RESPONSE,
             sizeof(MAD50P1AWS_FACTORY_POWER_OFF_RESPONSE));
  ASSERT(!dev.v3_command_pending(), "V3 power OFF: response correlated");
  ASSERT(!dev.raw_power(), "V3 power OFF: response state parsed");
  ASSERT_EQ(dev.raw_setpoint(), 55,
            "V3 power OFF: target humidity remains 55%");
}

static void test_v3_factory_target_humidity() {
  TestMideaDehum dev;
  dev.set_protocol_version(3);
  dev.set_handshake_enabled(false);
  dev.setup();
  dev.inject(MAD50P1AWS_FACTORY_POWER_ON_RESPONSE,
             sizeof(MAD50P1AWS_FACTORY_POWER_ON_RESPONSE));
  dev.seed_feature_flags(0x00);  // captured requests carried zero at byte 19
  dev.seed_v3_sequence(0x0B);

  tx_clear(dev);
  dev.cmd_humidity(60.0f);
  const CapturedFrame& target_60 = tx_last(dev, "V3 factory target 60");
  ASSERT(target_60.data.size() == sizeof(MAD50P1AWS_FACTORY_TARGET_60) &&
             memcmp(target_60.data.data(), MAD50P1AWS_FACTORY_TARGET_60,
                    sizeof(MAD50P1AWS_FACTORY_TARGET_60)) == 0,
         "V3 target 60: byte-for-byte factory frame match");
  dev.inject(MAD50P1AWS_FACTORY_TARGET_60_RESPONSE,
             sizeof(MAD50P1AWS_FACTORY_TARGET_60_RESPONSE));
  ASSERT(!dev.v3_command_pending(), "V3 target 60: response correlated");
  ASSERT(dev.raw_power(), "V3 target 60: power remains ON");
  ASSERT_EQ(dev.raw_setpoint(), 60, "V3 target 60: response state parsed");

  dev.seed_feature_flags(0x00);  // captured request carried zero at byte 19
  dev.seed_v3_sequence(0x5E);
  tx_clear(dev);
  dev.cmd_humidity(55.0f);
  const CapturedFrame& target_55 = tx_last(dev, "V3 factory target 55");
  ASSERT(target_55.data.size() == sizeof(MAD50P1AWS_FACTORY_TARGET_55) &&
             memcmp(target_55.data.data(), MAD50P1AWS_FACTORY_TARGET_55,
                    sizeof(MAD50P1AWS_FACTORY_TARGET_55)) == 0,
         "V3 target 55: byte-for-byte factory frame match");
  dev.inject(MAD50P1AWS_FACTORY_TARGET_55_RESPONSE,
             sizeof(MAD50P1AWS_FACTORY_TARGET_55_RESPONSE));
  ASSERT(!dev.v3_command_pending(), "V3 target 55: response correlated");
  ASSERT(dev.raw_power(), "V3 target 55: power remains ON");
  ASSERT_EQ(dev.raw_setpoint(), 55, "V3 target 55: response state parsed");
}

static void test_v3_poll_sequence() {
  TestMideaDehum dev;
  dev.set_protocol_version(3);
  dev.set_handshake_enabled(false);
  dev.setup();
  dev.seed_v3_sequence(0x53);

  tx_clear(dev);
  dev.getStatus();
  const CapturedFrame& frame = tx_last(dev, "V3 sequenced poll");
  ASSERT_EQ(frame.data.size(), static_cast<size_t>(33),
            "V3 poll: frame is 33 bytes");
  ASSERT_EQ(frame.data[1], 0x20, "V3 poll: length byte is 0x20");
  ASSERT_EQ(frame.data[8], 0x00, "V3 poll: agreement is 0x00");
  ASSERT_EQ(frame.data[9], 0x03, "V3 poll: message type is 0x03");
  ASSERT_EQ(frame.data[30], 0x54, "V3 poll: sequence is before CRC/checksum");

  uint32_t sum = 0;
  for (size_t i = 1; i < frame.data.size(); i++) sum += frame.data[i];
  ASSERT_EQ(static_cast<int>(sum & 0xFF), 0,
            "V3 poll: final checksum validates");

  ASSERT(frame.data.size() == sizeof(MAD50P1AWS_FACTORY_QUERY_SEQ_54) &&
             memcmp(frame.data.data(), MAD50P1AWS_FACTORY_QUERY_SEQ_54,
                    sizeof(MAD50P1AWS_FACTORY_QUERY_SEQ_54)) == 0,
         "V3 poll: byte-for-byte factory query match");
}

static void test_v3_confirmed_field_encodings() {
  TestMideaDehum dev;
  dev.set_protocol_version(3);
  dev.set_handshake_enabled(false);
  dev.setup();
  dev.inject(MAD50P1AWS_STATUS_HUM50, sizeof(MAD50P1AWS_STATUS_HUM50));

  for (uint8_t mode : {2, 3, 1}) {
    tx_clear(dev);
    dev.cmd_mode(mode);
    const CapturedFrame& frame = tx_last(dev, "V3 mode command");
    ASSERT_EQ(frame.data[12], mode, "V3 mode: request uses status mode value");
    ASSERT_EQ(frame.data[11], 0x43, "V3 mode: power preserved");
    ASSERT_EQ(frame.data[13], 0xD0, "V3 mode: high fan preserved/encoded");
    ASSERT_EQ(frame.data[17], 0x32, "V3 mode: humidity preserved");
    ASSERT_EQ(frame.data[19], 0x00, "V3 mode: status flags are not copied");
    dev.clear_v3_pending();
  }

  tx_clear(dev);
  dev.cmd_fan(esphome::climate::CLIMATE_FAN_LOW);
  const CapturedFrame& low = tx_last(dev, "V3 low fan command");
  ASSERT_EQ(low.data[13], 0xA8, "V3 fan: low status maps to request A8");
  ASSERT_EQ(low.data[12], 0x01, "V3 fan: Normal mode preserved");
  ASSERT_EQ(low.data[17], 0x32, "V3 fan: humidity preserved");
  ASSERT_EQ(low.data[19], 0x00, "V3 fan: status flags are not copied");
  dev.clear_v3_pending();

  tx_clear(dev);
  dev.cmd_fan(esphome::climate::CLIMATE_FAN_HIGH);
  const CapturedFrame& high = tx_last(dev, "V3 high fan command");
  ASSERT_EQ(high.data[13], 0xD0, "V3 fan: high status maps to request D0");
  dev.clear_v3_pending();

  for (uint8_t humidity : {35, 50, 55, 60}) {
    tx_clear(dev);
    dev.cmd_humidity(humidity);
    const CapturedFrame& frame = tx_last(dev, "V3 humidity command");
    ASSERT_EQ(frame.data[17], humidity, "V3 humidity: literal percentage");
    ASSERT_EQ(frame.data[11], 0x43, "V3 humidity: power preserved");
    ASSERT_EQ(frame.data[12], 0x01, "V3 humidity: Normal mode preserved");
    ASSERT_EQ(frame.data[13], 0xD0, "V3 humidity: high fan preserved");
    ASSERT_EQ(frame.data[19], 0x00, "V3 humidity: status flags are not copied");
    dev.clear_v3_pending();
  }
}

static void test_v3_uninitialized_fan_fallback() {
  TestMideaDehum dev;
  dev.set_protocol_version(3);
  dev.setup();  // No appliance state or ACK is injected.
  ASSERT_EQ(dev.raw_fan(), 0x3C, "V3 cold-start fan is the legacy default");
  ASSERT(!dev.is_handshake_done(), "V3 has not acquired initial state");

  // Check both untouched timer-enabled defaults and the factory timer sentinel.
  static const uint8_t expected[][34] = {
      {0xAA, 0x21, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x48, 0x42, 0x03, 0xA8, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xF8, 0xDC},
      {0xAA, 0x21, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x48, 0x42, 0x03, 0xA8, 0x7F, 0x7F, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x7F, 0x57},
  };
  for (size_t i = 0; i < 2; ++i) {
    dev.set_last_on_raw(i == 0 ? 0x00 : 0x7F);
    dev.set_last_off_raw(i == 0 ? 0x00 : 0x7F);
    dev.cmd_power(true);  // Ensure OFF is an actual UI change, not a no-op.
    dev.seed_v3_sequence(0);
    dev.clear_v3_pending();
    tx_clear(dev);
    dev.cmd_power(false);
    ASSERT_EQ(dev.uart_.tx_count(), static_cast<size_t>(1), "fallback transmits one command");
    const auto& frame = tx_last(dev, "V3 diagnostic fan fallback").data;
    ASSERT(frame.size() == sizeof(expected[i]) &&
               memcmp(frame.data(), expected[i], sizeof(expected[i])) == 0,
           "fallback frame matches length, fields, sequence, CRC and checksum");
    ASSERT(dev.v3_command_pending(), "fallback uses sequenced command sender");
    ASSERT_EQ(dev.raw_fan(), 0x3C, "fallback does not rewrite internal fan state");
  }

  tx_clear(dev);
  dev.cmd_mode(4);
  ASSERT_EQ(dev.uart_.tx_count(), static_cast<size_t>(0), "unsupported V3 mode still rejected");
}

static void test_v3_smart_low_target_60_regression() {
  TestMideaDehum dev;
  dev.set_protocol_version(3);
  dev.set_handshake_enabled(false);
  dev.setup();
  dev.inject(MAD50P1AWS_STATUS_SMART_LOW_TARGET_60,
             sizeof(MAD50P1AWS_STATUS_SMART_LOW_TARGET_60));

  ASSERT(dev.raw_power(), "V3 parse regression: power is ON");
  ASSERT_EQ(dev.raw_mode(), 0x03, "V3 parse regression: mode is Smart");
  ASSERT_EQ(dev.raw_fan(), 0x28, "V3 parse regression: fan comes from byte 13");
  ASSERT_EQ(dev.raw_setpoint(), 60,
            "V3 parse regression: target comes from byte 17");

  // A stale/unsupported MEDIUM call used to overwrite V3 fanSpeed with
  // decimal 60 (0x3C). It must now leave the decoded V3 fan state untouched.
  tx_clear(dev);
  dev.cmd_fan(esphome::climate::CLIMATE_FAN_MEDIUM);
  ASSERT_EQ(dev.raw_fan(), 0x28,
            "V3 parse regression: MEDIUM does not corrupt fan state");
  ASSERT_EQ(dev.uart_.tx_count(), static_cast<size_t>(0),
            "V3 parse regression: unsupported MEDIUM sends no command");

  dev.seed_v3_sequence(0x20);
  tx_clear(dev);
  dev.cmd_power(false);
  const CapturedFrame& frame = tx_last(dev, "V3 preserved Smart/Low/60 command");
  ASSERT_EQ(frame.data[11], 0x42, "V3 preserved state: power changes to OFF");
  ASSERT_EQ(frame.data[12], 0x03, "V3 preserved state: mode remains Smart");
  ASSERT_EQ(frame.data[13], 0xA8, "V3 preserved state: Low encodes as A8");
  ASSERT_EQ(frame.data[17], 0x3C, "V3 preserved state: target remains 60");
  ASSERT_EQ(frame.data[19], 0x00,
            "V3 preserved state: status flags are not copied");
}

static void test_v3_status_flags_not_copied_to_control() {
  TestMideaDehum dev;
  dev.set_protocol_version(3);
  dev.set_handshake_enabled(false);
  dev.setup();
  dev.inject(MAD50P1AWS_STATUS_LOW_TARGET_75_FLAGS_18,
             sizeof(MAD50P1AWS_STATUS_LOW_TARGET_75_FLAGS_18));

  dev.seed_v3_sequence(0xE1);
  tx_clear(dev);
  dev.cmd_humidity(70.0f);
  const CapturedFrame& frame = tx_last(dev, "V3 target 75 to 70");
  ASSERT_EQ(frame.data[11], 0x43, "V3 flags experiment: power request is ON");
  ASSERT_EQ(frame.data[12], 0x01, "V3 flags experiment: mode is Normal");
  ASSERT_EQ(frame.data[13], 0xA8, "V3 flags experiment: low fan encodes A8");
  ASSERT_EQ(frame.data[17], 0x46, "V3 flags experiment: target is 70%");
  ASSERT_EQ(frame.data[19], 0x00,
            "V3 flags experiment: status 18 does not enter command");
  ASSERT_EQ(frame.data[31], 0xE2, "V3 flags experiment: sequence is retained");
}

#ifdef USE_MIDEA_DEHUM_PUMP
static void test_v3_factory_pump() {
  TestMideaDehum dev;
  dev.set_protocol_version(3);
  dev.set_handshake_enabled(false);
  dev.setup();
  dev.inject(MAD50P1AWS_FACTORY_PUMP_OFF_RESPONSE,
             sizeof(MAD50P1AWS_FACTORY_PUMP_OFF_RESPONSE));

  dev.seed_v3_sequence(0x02);
  tx_clear(dev);
  dev.set_pump_state(true);
  const CapturedFrame& pump_on = tx_last(dev, "V3 factory pump ON");
  ASSERT_EQ(pump_on.data[19], 0x08,
            "V3 pump ON: command contains only confirmed pump bit");
  dev.inject(MAD50P1AWS_FACTORY_PUMP_ON_RESPONSE,
             sizeof(MAD50P1AWS_FACTORY_PUMP_ON_RESPONSE));
  ASSERT(!dev.v3_command_pending(), "V3 pump ON: response correlated");

  dev.seed_v3_sequence(0x3D);
  tx_clear(dev);
  dev.set_pump_state(false);
  const CapturedFrame& pump_off = tx_last(dev, "V3 factory pump OFF");
  ASSERT_EQ(pump_off.data[19], 0x00,
            "V3 pump OFF: command omits status-only flags");
  dev.inject(MAD50P1AWS_FACTORY_PUMP_OFF_RESPONSE,
             sizeof(MAD50P1AWS_FACTORY_PUMP_OFF_RESPONSE));
  ASSERT(!dev.v3_command_pending(), "V3 pump OFF: response correlated");
}
#endif

// ══════════════════════════════════════════════════════════════════════════
//  Runner
// ══════════════════════════════════════════════════════════════════════════

#ifndef TEST_COMBINED
int main() {
  printf("Category 2: Command Tests (V1 + V2)\n");
  printf("====================================\n");

  int total = 0;
  total += run_test("2.1   Power ON/OFF", test_power);
  total += run_test("2.2   Mode presets", test_modes);
  total += run_test("2.3   Fan speed", test_fan);
  total += run_test("2.4   Target humidity", test_humidity);
  total += run_test("2.5   Pump", test_pump);
  total += run_test("2.6   Ionizer", test_ion);
  total += run_test("2.7   Sleep", test_sleep);
#ifdef USE_MIDEA_DEHUM_BEEP
  total += run_test("2.8   Beep", test_beep);
#endif
#ifdef USE_MIDEA_DEHUM_SWING
  total += run_test("2.9   Swing", test_swing);
#endif
#ifdef USE_MIDEA_DEHUM_TIMER
  total += run_test("2.10  Timer", test_timer);
#endif
  total += run_test("2.11  Idempotency", test_idempotency);
  total += run_test("2.12  V2 Power ON/OFF", test_v2_cmd_power);
  total += run_test("2.13  V2 Mode presets", test_v2_cmd_modes);
  total += run_test("2.14  V2 Fan speed", test_v2_cmd_fan);
  total += run_test("2.15  V2 Target humidity", test_v2_cmd_humidity);
#ifdef USE_MIDEA_DEHUM_PUMP
  total += run_test("2.16  V2 Pump", test_v2_cmd_pump);
#endif
#ifdef USE_MIDEA_DEHUM_FILTER_BUTTON
  total += run_test("2.17  V2 Filter cleaned flag", test_v2_cmd_filter_cleaned);
#endif
  total += run_test("2.18  V2 Water level threshold", test_v2_cmd_water_level);
#ifdef USE_MIDEA_DEHUM_TIMER
  total += run_test("2.19  V2 Timer echo", test_v2_cmd_timer_echo);
  total += run_test("2.20  V2 Timer set", test_v2_cmd_timer_set);
#endif
#ifdef USE_MIDEA_DEHUM_RESET_WATER_LEVEL
  total += run_test("2.21  V2 Reset water level", test_v2_cmd_reset_water_level);
#endif
  total += run_test("2.22  V3 factory power ON", test_v3_factory_power_on);
  total += run_test("2.23  V3 factory power OFF", test_v3_factory_power_off);
  total += run_test("2.24  V3 target humidity", test_v3_factory_target_humidity);
  total += run_test("2.25  V3 poll sequence", test_v3_poll_sequence);
  total += run_test("2.26  V3 confirmed field encodings", test_v3_confirmed_field_encodings);
  total += run_test("2.27  V3 Smart/Low/60 regression", test_v3_smart_low_target_60_regression);
  total += run_test("2.28  V3 status flags experiment", test_v3_status_flags_not_copied_to_control);
  total += run_test("2.30  V3 uninitialized fan fallback", test_v3_uninitialized_fan_fallback);
#ifdef USE_MIDEA_DEHUM_PUMP
  total += run_test("2.29  V3 factory pump", test_v3_factory_pump);
#endif

  if (total == 0) {
    printf("\n✓ All command tests passed!\n");
  } else {
    printf("\n✗ %d command test(s) failed\n", total);
  }
  return total > 0 ? 1 : 0;
}
#endif
