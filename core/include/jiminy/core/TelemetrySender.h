///////////////////////////////////////////////////////////////////////////////
///
/// \brief       Contains the telemetry sender interface.
///
/// \details     This file contains the telemetry sender class that will be the parent
///              of all classes that needed to send telemetry data.
///
///////////////////////////////////////////////////////////////////////////////

#ifndef EXO_SIMU_TELEMETRY_CLIENT_CLASS_H
#define EXO_SIMU_TELEMETRY_CLIENT_CLASS_H

#include <string>
#include <unordered_map>

#include "jiminy/core/Types.h"


namespace jiminy
{
    class TelemetryData;

    std::string const DEFAULT_OBJECT_NAME("Uninitialized Object");

    ////////////////////////////////////////////////////////////////////////
    /// \class TelemetrySender
    /// \brief Class to inherit if you want to send telemetry data.
    ////////////////////////////////////////////////////////////////////////
    class TelemetrySender
    {
    public:
        ////////////////////////////////////////////////////////////////////////
        /// \brief      Constructor with arguments.
        ////////////////////////////////////////////////////////////////////////
        explicit TelemetrySender(void);

        ////////////////////////////////////////////////////////////////////////
        /// \brief      Destructor.
        ////////////////////////////////////////////////////////////////////////
        virtual ~TelemetrySender(void);

        ////////////////////////////////////////////////////////////////////////
        /// \brief      Update specified registered variable in the telemetry buffer.
        ///
        /// \param[in]  fieldNameIn  Name of the value to update.
        /// \param[in]  valueIn      Updated value of the variable.
        ////////////////////////////////////////////////////////////////////////
        template <typename T>
        void updateValue(std::string const & fieldNameIn, 
                         T           const & valueIn);

        ////////////////////////////////////////////////////////////////////////
        /// \brief      Register a variable into the telemetry system..
        ///
        /// \details    A variable must be registered to be taken into account by the telemetry system.
        ///
        /// \param[in]  fieldNameIn   Name of the field to record in the telemetry system.
        /// \param[in]  initialValue  Initial value of the newly recored field.
        ////////////////////////////////////////////////////////////////////////
        template<typename T>
        result_t registerNewEntry(std::string const & fieldNameIn, 
                                  T           const & initialValue);

        ////////////////////////////////////////////////////////////////////////
        /// \brief     Configure the object.
        ///
        /// \param[in]  telemetryDataInstance Shared pointer to the telemetry instance
        /// \param[in]  objectNameIn          Name of the object.
        ///
        /// \remarks  Should only be used when default constructor is called for
        ///           later configuration.
        ///           Should be set before registering any entry.
        /// \retval   E_EPERM if object is already configured.
        ///////////////////////////////////////////////////////////////////////
        void configureObject(std::shared_ptr<TelemetryData> const & telemetryDataInstance,
                             std::string                    const & objectNameIn);

        ////////////////////////////////////////////////////////////////////////
        /// \brief     Get the number of registered entries.
        ///
        /// \return    The number of registered entries.
        ///////////////////////////////////////////////////////////////////////
        uint32_t getLocalNumEntries(void) const;

        ////////////////////////////////////////////////////////////////////////
        /// \brief     Get the object name.
        ///
        /// \return    The object name.
        ///////////////////////////////////////////////////////////////////////
        std::string const & getObjectName(void) const;

        ////////////////////////////////////////////////////////////////////////
        /// \brief     Add an invariant header entry in the log file.
        ///
        /// \param[in] invariantNameIn  Name of the invariant.
        /// \param[in] valueIn          Value of the invariant.
        ///
        /// \retval E_REGISTERING_NOT_AVAILABLE if the registering is closed (the telemetry is already started).
        /// \retval E_ALREADY_REGISTERED        if the constant was already registered.
        ///////////////////////////////////////////////////////////////////////
        result_t addConstantEntry(std::string const & invariantNameIn, 
                                  std::string const & valueIn);

    protected:
        std::string objectName_;  ///< Name of the logged object.

    private:
        std::shared_ptr<TelemetryData> telemetryData_;
        /// \brief Associate int32_t variable position to their ID.
        std::unordered_map<std::string, int32_t *> intBufferPositon_;
        /// \brief Associate float32_t variable position to their ID.
        std::unordered_map<std::string, float32_t *> floatBufferPositon_;
    };
} // End of jiminy namespace

#endif  //  EXO_SIMU_TELEMETRY_CLIENT_CLASS_H