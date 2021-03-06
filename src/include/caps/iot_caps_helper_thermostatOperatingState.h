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

#ifndef _IOT_CAPS_HELPER_THERMOSTAT_OPERATING_STATE_
#define _IOT_CAPS_HELPER_THERMOSTAT_OPERATING_STATE_

#include "iot_caps_helper.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CAP_ENUM_THERMOSTATOPERATINGSTATE_THERMOSTATOPERATINGSTATE_VALUE_COOLING,
    CAP_ENUM_THERMOSTATOPERATINGSTATE_THERMOSTATOPERATINGSTATE_VALUE_FAN_ONLY,
    CAP_ENUM_THERMOSTATOPERATINGSTATE_THERMOSTATOPERATINGSTATE_VALUE_HEATING,
    CAP_ENUM_THERMOSTATOPERATINGSTATE_THERMOSTATOPERATINGSTATE_VALUE_IDLE,
    CAP_ENUM_THERMOSTATOPERATINGSTATE_THERMOSTATOPERATINGSTATE_VALUE_PENDING_COOL,
    CAP_ENUM_THERMOSTATOPERATINGSTATE_THERMOSTATOPERATINGSTATE_VALUE_PENDING_HEAT,
    CAP_ENUM_THERMOSTATOPERATINGSTATE_THERMOSTATOPERATINGSTATE_VALUE_VENT_ECONOMIZER,
    CAP_ENUM_THERMOSTATOPERATINGSTATE_THERMOSTATOPERATINGSTATE_VALUE_MAX
};

const static struct iot_caps_thermostatOperatingState {
    const char *id;
    const struct thermostatOperatingState_attr_thermostatOperatingState {
        const char *name;
        const unsigned char property;
        const unsigned char value_type;
        const char *values[CAP_ENUM_THERMOSTATOPERATINGSTATE_THERMOSTATOPERATINGSTATE_VALUE_MAX];
    } attr_thermostatOperatingState;
} caps_helper_thermostatOperatingState = {
    .id = "thermostatOperatingState",
    .attr_thermostatOperatingState = {
        .name = "thermostatOperatingState",
        .property = ATTR_SET_VALUE_REQUIRED,
        .value_type = VALUE_TYPE_STRING,
        .values = {"cooling", "fan only", "heating", "idle", "pending cool", "pending heat", "vent economizer"},
    },
};

#ifdef __cplusplus
}
#endif

#endif /* _IOT_CAPS_HERLPER_THERMOSTAT_OPERATING_STATE_ */
