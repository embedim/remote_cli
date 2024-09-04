#pragma once

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_console.h"

#include "cmd_nvs.h"
#include "cmd_system.h"
#include "cmd_wifi.h"

#include "remote_cli.h"


void  wifi_init_sta();