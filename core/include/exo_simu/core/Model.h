#ifndef SIMU_MODEL_H
#define SIMU_MODEL_H

#include <string>
#include <vector>

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <boost/circular_buffer.hpp>

#include "pinocchio/multibody/model.hpp"
#include "pinocchio/algorithm/frames.hpp"

#include "exo_simu/core/Types.h"


namespace exo_simu
{
    class Engine;
    class AbstractSensorBase;
    class TelemetryData;
    struct SensorDataHolder_t;

    class Model
    {
        friend Engine;

    public:
        // Disable the copy of the class
        Model(Model const & model) = delete;
        Model & operator = (Model const & other) = delete;

    public:
        virtual configHolder_t getDefaultJointOptions()
        {
            configHolder_t config;
            config["boundsFromUrdf"] = true; // Must be true since boundsMin and boundsMax are undefined
            config["boundsMin"] = vectorN_t();
            config["boundsMax"] = vectorN_t();

            return config;
        };

        struct jointOptions_t
        {
            bool      const boundsFromUrdf;
            vectorN_t const boundsMin;
            vectorN_t const boundsMax;

            jointOptions_t(configHolder_t const & options):
            boundsFromUrdf(boost::get<bool>(options.at("boundsFromUrdf"))),
            boundsMin(boost::get<vectorN_t>(options.at("boundsMin"))),
            boundsMax(boost::get<vectorN_t>(options.at("boundsMax")))
            {
                // Empty.
            }
        };

        virtual configHolder_t getDefaultOptions()
        {
            configHolder_t config;
            config["joints"] = getDefaultJointOptions();

            return config;
        };

        struct modelOptions_t
        {
            jointOptions_t const joints;

            modelOptions_t(configHolder_t const & options):
            joints(boost::get<configHolder_t>(options.at("joints")))
            {
                // Empty.
            }
        };

    public:
        Model(void);
        virtual ~Model(void);

        result_t initialize(std::string              const & urdfPath,
                            std::vector<std::string> const & contactFramesNames,
                            std::vector<std::string> const & jointsNames,
                            bool                     const & hasFreeflyer = true);
        void reset(void);

        template<typename TSensor>
        result_t addSensor(std::string              const & sensorName,
                           std::shared_ptr<TSensor>       & sensor);
        result_t removeSensor(std::string const & sensorType,
                              std::string const & sensorName);
        result_t removeSensors(std::string const & sensorType);
        void removeSensors(void);

        configHolder_t getOptions(void) const;
        result_t setOptions(configHolder_t mdlOptions); // Make a copy !
        configHolder_t getSensorOptions(std::string const & sensorType,
                                        std::string const & sensorName) const;
        configHolder_t getSensorsOptions(std::string const & sensorType) const;
        configHolder_t getSensorsOptions(void) const;
        void setSensorOptions(std::string    const & sensorType,
                              std::string    const & sensorName,
                              configHolder_t const & sensorOptions);
        void setSensorsOptions(std::string    const & sensorType,
                               configHolder_t const & sensorsOptions);
        void setSensorsOptions(configHolder_t const & sensorsOptions);
        bool getIsInitialized(void) const;
        bool getIsTelemetryConfigured(void) const;
        std::string getUrdfPath(void) const;
        std::map<std::string, std::vector<std::string> > getSensorsNames(void) const;
        result_t getSensorsData(std::string const & sensorType,
                                matrixN_t         & data) const;
        result_t getSensorData(std::string const & sensorType,
                               std::string const & sensorName,
                               vectorN_t         & data) const;
        void setSensorsData(float64_t const & t,
                            vectorN_t const & q,
                            vectorN_t const & v,
                            vectorN_t const & a,
                            vectorN_t const & u);
        void updateSensorsTelemetry(void);
        std::vector<int32_t> const & getContactFramesIdx(void) const;
        std::vector<int32_t> const & getJointsPositionIdx(void) const;
        std::vector<int32_t> const & getJointsVelocityIdx(void) const;
        uint32_t nq(void) const; // no get keyword for consistency with pinocchio C++ API
        uint32_t nv(void) const;
        uint32_t nx(void) const;

    protected:
        virtual result_t configureTelemetry(std::shared_ptr<TelemetryData> const & telemetryData);
        
        template<typename TSensor>
        std::shared_ptr<TSensor> getSensor(std::string const & sensorType,
                                           std::string const & sensorName);

        result_t setUrdfPath(std::string const & urdfPath, 
                             bool        const & hasFreeflyer);
        result_t getFrameIdx(std::string const & frameName,
                             int32_t           & frameIdx) const;
        result_t getFramesIdx(std::vector<std::string> const & framesNames,
                              std::vector<int32_t>           & framesIdx) const;
        result_t getJointIdx(std::string const & jointName,
                             int32_t           & jointPositionIdx,
                             int32_t           & jointVelocityIdx) const;
        result_t getJointsIdx(std::vector<std::string> const & jointsNames,
                              std::vector<int32_t>           & jointsPositionIdx,
                              std::vector<int32_t>           & jointsVelocityIdx) const;

    public:
        pinocchio::Model pncModel_;
        pinocchio::Data pncData_;
        std::unique_ptr<modelOptions_t const> mdlOptions_;
        pinocchio::container::aligned_vector<pinocchio::Force> contactForces_; // Buffer to store the contact forces

    protected:
        bool isInitialized_;
        bool isTelemetryConfigured_;
        std::string urdfPath_;
        configHolder_t mdlOptionsHolder_;

        std::shared_ptr<TelemetryData> telemetryData_;
        sensorsGroupHolder_t sensorsGroupHolder_;

        std::vector<std::string> contactFramesNames_;
        std::vector<std::string> jointsNames_;
        std::vector<int32_t> contactFramesIdx_;  // Indices of the contact frame in the model
        std::vector<int32_t> jointsPositionIdx_; // Indices of the actuated joints in the configuration representation
        std::vector<int32_t> jointsVelocityIdx_; // Indices of the actuated joints in the velocity vector representation

    private:
        std::map<std::string, std::shared_ptr<SensorDataHolder_t> > sensorsDataHolder_;
        uint32_t nq_;
        uint32_t nv_;
        uint32_t nx_;
    };

    struct SensorDataHolder_t
    {
        SensorDataHolder_t(void) :
        time_(),
        data_(),
        counters_(),
        sensors_(),
        num_()
        {
            // Empty.
        };

        ~SensorDataHolder_t(void)
        {
            // Empty.
        }; 

        boost::circular_buffer_space_optimized<float64_t> time_;
        boost::circular_buffer_space_optimized<matrixN_t> data_;
        std::vector<uint32_t> counters_;
        std::vector<AbstractSensorBase *> sensors_;
        uint32_t num_;
    };
}

#include "exo_simu/core/Model.tcc"

#endif //end of SIMU_MODEL_H
