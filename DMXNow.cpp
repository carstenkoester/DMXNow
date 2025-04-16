#include "DMXNow.h"

esp_now_rate_config_t dmxnow_rate_config = {
  .phymode = WIFI_PHY_MODE_HT20,
  .rate = WIFI_PHY_RATE_MCS1_SGI,
  .ersu = false,
  .dcm = false
};
