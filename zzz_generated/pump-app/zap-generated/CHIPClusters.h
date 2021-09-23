/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

// THIS FILE IS GENERATED BY ZAP

// Prevent multiple inclusion
#pragma once

#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Commands.h>

#include <controller/CHIPCluster.h>
#include <lib/core/CHIPCallback.h>
#include <lib/support/Span.h>

namespace chip {
namespace Controller {

class DLL_EXPORT FlowMeasurementCluster : public ClusterBase
{
public:
    FlowMeasurementCluster() : ClusterBase(app::Clusters::FlowMeasurement::Id) {}
    ~FlowMeasurementCluster() {}

    // Cluster Attributes
    CHIP_ERROR ReadAttributeMeasuredValue(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback);
    CHIP_ERROR ReadAttributeMinMeasuredValue(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback);
    CHIP_ERROR ReadAttributeMaxMeasuredValue(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback);
    CHIP_ERROR ReadAttributeClusterRevision(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback);
    CHIP_ERROR ConfigureAttributeMeasuredValue(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback,
                                               uint16_t minInterval, uint16_t maxInterval, int16_t change);
    CHIP_ERROR ReportAttributeMeasuredValue(Callback::Cancelable * onReportCallback);
};

class DLL_EXPORT PressureMeasurementCluster : public ClusterBase
{
public:
    PressureMeasurementCluster() : ClusterBase(app::Clusters::PressureMeasurement::Id) {}
    ~PressureMeasurementCluster() {}

    // Cluster Attributes
    CHIP_ERROR ReadAttributeMeasuredValue(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback);
    CHIP_ERROR ReadAttributeMinMeasuredValue(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback);
    CHIP_ERROR ReadAttributeMaxMeasuredValue(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback);
    CHIP_ERROR ReadAttributeClusterRevision(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback);
    CHIP_ERROR ConfigureAttributeMeasuredValue(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback,
                                               uint16_t minInterval, uint16_t maxInterval, int16_t change);
    CHIP_ERROR ReportAttributeMeasuredValue(Callback::Cancelable * onReportCallback);
};

class DLL_EXPORT TemperatureMeasurementCluster : public ClusterBase
{
public:
    TemperatureMeasurementCluster() : ClusterBase(app::Clusters::TemperatureMeasurement::Id) {}
    ~TemperatureMeasurementCluster() {}

    // Cluster Attributes
    CHIP_ERROR ReadAttributeMeasuredValue(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback);
    CHIP_ERROR ReadAttributeMinMeasuredValue(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback);
    CHIP_ERROR ReadAttributeMaxMeasuredValue(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback);
    CHIP_ERROR ReadAttributeClusterRevision(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback);
    CHIP_ERROR ConfigureAttributeMeasuredValue(Callback::Cancelable * onSuccessCallback, Callback::Cancelable * onFailureCallback,
                                               uint16_t minInterval, uint16_t maxInterval, int16_t change);
    CHIP_ERROR ReportAttributeMeasuredValue(Callback::Cancelable * onReportCallback);
};

} // namespace Controller
} // namespace chip
