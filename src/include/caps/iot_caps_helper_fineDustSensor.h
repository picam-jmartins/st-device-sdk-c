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

#ifndef _IOT_CAPS_HELPER_FINE_DUST_SENSOR_
#define _IOT_CAPS_HELPER_FINE_DUST_SENSOR_

#include "iot_caps_helper.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CAP_ENUM_FINEDUSTSENSOR_FINEDUSTLEVEL_UNIT_UG_PER_M3,
    CAP_ENUM_FINEDUSTSENSOR_FINEDUSTLEVEL_UNIT_MAX
};

const static struct iot_caps_fineDustSensor {
    const char *id;
    const struct fineDustSensor_attr_fineDustLevel {
        const char *name;
        const unsigned char property;
        const unsigned char value_type;
        const char *units[CAP_ENUM_FINEDUSTSENSOR_FINEDUSTLEVEL_UNIT_MAX];
        const int min;
    } attr_fineDustLevel;
} caps_helper_fineDustSensor = {
    .id = "fineDustSensor",
    .attr_fineDustLevel = {
        .name = "fineDustLevel",
        .property = ATTR_SET_VALUE_MIN | ATTR_SET_VALUE_REQUIRED,
        .value_type = VALUE_TYPE_INTEGER,
        .units = {"μg/m^3"},
        .min = 0,
    },
};

#ifdef __cplusplus
}
#endif

#endif /* _IOT_CAPS_HERLPER_FINE_DUST_SENSOR_ */
