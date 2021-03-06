///////////////////////////////////////////////////////////////////////////////
///
/// \brief Implementation of the TelemetrySender class and localLogs class.
///
///////////////////////////////////////////////////////////////////////////////

#include <iostream>

#include "jiminy/core/telemetry/TelemetryData.h"
#include "jiminy/core/Constants.h"

#include "jiminy/core/telemetry/TelemetrySender.h"


namespace jiminy
{
    TelemetrySender::TelemetrySender(void) :
    objectName_(DEFAULT_OBJECT_NAME),
    telemetryData_(nullptr),
    intBufferPosition_(),
    floatBufferPosition_()
    {
        // Empty.
    }

    template <>
    void TelemetrySender::updateValue<int32_t>(std::string const & fieldNameIn,
                                               int32_t     const & value)
    {
        auto it = intBufferPosition_.find(fieldNameIn);
        if (intBufferPosition_.end() == it)
        {
            std::cout << "Error - TelemetrySender::updateValue - Cannot log the variable: it was never registered as an int32_t before! |" << fieldNameIn.c_str() << "|" << std::endl;
            return;
        }

        // Write the value directly in the buffer holder using the pointer stored in the map.
        *(it->second) = value;
    }

    template <>
    void TelemetrySender::updateValue<float64_t>(std::string const & fieldNameIn,
                                                 float64_t   const & value)
    {
        auto it = floatBufferPosition_.find(fieldNameIn);
        if (floatBufferPosition_.end() == it)
        {
            std::cout << "Error - TelemetrySender::updateValue - Cannot log the variable: it was never registered as a float64_t before! |" << fieldNameIn.c_str() << "|" << std::endl;
            return;
        }

        // Write the value directly in the buffer holder using the pointer stored in the map.
        *(it->second) = static_cast<float32_t>(value);
    }

    void TelemetrySender::updateValue(std::vector<std::string>    const & fieldnames,
                                      Eigen::Ref<vectorN_t const> const & values)
    {
        for (uint32_t i=0; i < values.size(); ++i)
        {
            updateValue(fieldnames[i], values[i]);
        }
    }

    template<>
    hresult_t TelemetrySender::registerVariable<int32_t>(std::string const & fieldNameIn,
                                                         int32_t     const & initialValue)
    {
        int32_t * positionInBuffer = nullptr;
        std::string const fullFieldName = objectName_ + TELEMETRY_DELIMITER + fieldNameIn;

        hresult_t returnCode = telemetryData_->registerVariable(fullFieldName, positionInBuffer);
        if (returnCode == hresult_t::SUCCESS)
        {
            intBufferPosition_[fieldNameIn] = positionInBuffer;
            updateValue(fieldNameIn, initialValue);
        }

        return returnCode;
    }

    template<>
    hresult_t TelemetrySender::registerVariable<float64_t>(std::string const & fieldNameIn,
                                                           float64_t   const & initialValue)
    {
        float32_t * positionInBuffer = nullptr;
        std::string const fullFieldName = objectName_ + TELEMETRY_DELIMITER + fieldNameIn;

        hresult_t returnCode = telemetryData_->registerVariable(fullFieldName, positionInBuffer);
        if (returnCode == hresult_t::SUCCESS)
        {
            floatBufferPosition_[fieldNameIn] = positionInBuffer;
            updateValue(fieldNameIn, initialValue);
        }

        return returnCode;
    }

    hresult_t TelemetrySender::registerVariable(std::vector<std::string> const & fieldnames,
                                                vectorN_t                const & initialValues)
    {
        hresult_t returnCode = hresult_t::SUCCESS;
        for (uint32_t i=0; i < initialValues.size(); ++i)
        {
            if (returnCode == hresult_t::SUCCESS)
            {
                returnCode = registerVariable(fieldnames[i], initialValues[i]);
            }
        }
        return returnCode;
    }

    hresult_t TelemetrySender::registerConstant(std::string const & variableNameIn,
                                                std::string const & valueIn)
    {
        std::string const fullFieldName = objectName_ + TELEMETRY_DELIMITER + variableNameIn;
        return telemetryData_->registerConstant(fullFieldName, valueIn);
    }

    void TelemetrySender::configureObject(std::shared_ptr<TelemetryData> telemetryDataInstance,
                                          std::string const & objectNameIn)
    {
        objectName_ = objectNameIn;
        telemetryData_ = std::move(telemetryDataInstance);
        intBufferPosition_.clear();
        floatBufferPosition_.clear();
    }

    uint32_t TelemetrySender::getLocalNumEntries(void) const
    {
        return static_cast<uint32_t>(floatBufferPosition_.size() + intBufferPosition_.size());
    }

    std::string const & TelemetrySender::getObjectName(void) const
    {
        return objectName_;
    }
} // End of namespace jiminy.
