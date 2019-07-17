#ifndef SIMU_ABSTRACT_CONTROLLER_H
#define SIMU_ABSTRACT_CONTROLLER_H

#include "exo_simu/core/Types.h"


namespace exo_simu
{
    std::string const CONTROLLER_OBJECT_NAME("Controller");

    class Model;

    class AbstractController
    {
    public:
        // Disable the copy of the class
        AbstractController(AbstractController const & controller) = delete;
        AbstractController & operator = (AbstractController const & controller) = delete;

    public:
        virtual configHolder_t getDefaultOptions()
        {
            configHolder_t config;
            config["telemetryEnable"] = false;

            return config;
        };

        struct controllerOptions_t
        {
            bool const telemetryEnable;

            controllerOptions_t(configHolder_t const & options):
            telemetryEnable(boost::get<bool>(options.at("telemetryEnable")))
            {
                // Empty.
            }
        };

    public:
        AbstractController(void);
        virtual ~AbstractController(void);

        virtual result_t initialize(Model const & model);
        virtual void reset(void);

        virtual result_t configureTelemetry(std::shared_ptr<TelemetryData> const & telemetryData);
        virtual void updateTelemetry(void) = 0;

        configHolder_t getOptions(void) const;
        void setOptions(configHolder_t const & ctrlOptions);
        bool getIsInitialized(void) const;
        bool getIsTelemetryConfigured(void) const;

        // It assumes that the model internal state is consistent with other input arguments
        virtual result_t computeCommand(float64_t const & t,
                                        vectorN_t const & q,
                                        vectorN_t const & v,
                                        vectorN_t       & u) = 0;
        virtual result_t internalDynamics(float64_t const & t,
                                          vectorN_t const & q,
                                          vectorN_t const & v,
                                          vectorN_t       & u) = 0;

    public:
        std::unique_ptr<controllerOptions_t const> ctrlOptions_;

    protected:
        Model const * model_; // Raw pointer to avoid managing its deletion
        bool isInitialized_;
        bool isTelemetryConfigured_;
        configHolder_t ctrlOptionsHolder_;
        TelemetrySender telemetrySender_;
    };
}

#endif //end of SIMU_ABSTRACT_CONTROLLER_H