/* ***************************************************************************
 *
 * Copyright 2019-2020 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#ifndef _IOT_CAPS_HELPER_SIGNAL_STRENGTH_
#define _IOT_CAPS_HELPER_SIGNAL_STRENGTH_

#include "iot_caps_helper.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CAP_ENUM_SIGNALSTRENGTH_RSSI_UNIT_DBM,
    CAP_ENUM_SIGNALSTRENGTH_RSSI_UNIT_MAX
};

const static struct iot_caps_signalStrength {
    const char *id;
    const struct signalStrength_attr_rssi {
        const char *name;
        const unsigned char property;
        const unsigned char value_type;
        const char *units[CAP_ENUM_SIGNALSTRENGTH_RSSI_UNIT_MAX];
        const double min;
        const double max;
    } attr_rssi;
    const struct signalStrength_attr_lqi {
        const char *name;
        const unsigned char property;
        const unsigned char value_type;
        const int min;
        const int max;
    } attr_lqi;
} caps_helper_signalStrength = {
    .id = "signalStrength",
    .attr_rssi = {
        .name = "rssi",
        .property = ATTR_SET_VALUE_MIN | ATTR_SET_VALUE_MAX | ATTR_SET_VALUE_REQUIRED,
        .value_type = VALUE_TYPE_NUMBER,
        .units = {"dBm"},
        .min = -200,
        .max = 0,
    },
    .attr_lqi = {
        .name = "lqi",
        .property = ATTR_SET_VALUE_MIN | ATTR_SET_VALUE_MAX | ATTR_SET_VALUE_REQUIRED,
        .value_type = VALUE_TYPE_INTEGER,
        .min = 0,
        .max = 255,
    },
};

#ifdef __cplusplus
}
#endif

#endif /* _IOT_CAPS_HERLPER_SIGNAL_STRENGTH_ */
