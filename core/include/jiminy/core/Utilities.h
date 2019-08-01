#ifndef SIMU_UTILITIES_H
#define SIMU_UTILITIES_H

#include <chrono>
#include <vector>
#include <random>

#include "jiminy/core/Types.h"
#include "pinocchio/multibody/model.hpp"
#include "pinocchio/algorithm/frames.hpp"


namespace jiminy
{
    class TelemetrySender;

    // ************************ Timer *****************************

    class Timer
    {
        typedef std::chrono::high_resolution_clock Time;

    public:
        Timer(void);
        void tic(void);
        void toc(void);

    public:
        std::chrono::time_point<Time> t0;
        std::chrono::time_point<Time> tf;
        float32_t dt;
    };

    // ************ Random number generator utilities **************

    void resetRandGenerators(uint32_t seed);

    float64_t randUniform(float64_t const & lo,
                          float64_t const & hi);

    float64_t randNormal(float64_t const & mean,
                         float64_t const & std);

    vectorN_t randVectorNormal(uint32_t  const & size,
                               float64_t const & mean,
                               float64_t const & std);

    vectorN_t randVectorNormal(uint32_t  const & size,
                               float64_t const & std);

    vectorN_t randVectorNormal(vectorN_t const & std);

    vectorN_t randVectorNormal(vectorN_t const & mean,
                               vectorN_t const & std);

    // ******************* Telemetry utilities **********************

    void registerNewVectorEntry(TelemetrySender                & telemetrySender,
                                std::vector<std::string> const & fieldNames,
                                vectorN_t                const & initialValues,
                                std::vector<uint32_t>    const & idx);
    void registerNewVectorEntry(TelemetrySender                & telemetrySender,
                                std::vector<std::string> const & fieldNames,
                                vectorN_t                const & initialValues);

    void updateVectorValue(TelemetrySender                & telemetrySender,
                           std::vector<std::string> const & fieldNames,
                           vectorN_t                const & values,
                           std::vector<uint32_t>    const & idx);
    void updateVectorValue(TelemetrySender                & telemetrySender,
                           std::vector<std::string> const & fieldNames,
                           vectorN_t                const & values);

    std::vector<std::string> defaultVectorFieldnames(std::string const & baseName,
                                                     uint32_t    const & size);

    std::string removeFieldnameSuffix(std::string         fieldname,
                                      std::string const & suffix);
    std::vector<std::string> removeFieldnamesSuffix(std::vector<std::string>         fieldnames, // Make a copy
                                                    std::string              const & suffix);

    // ******************** Pinocchio utilities *********************

    void computePositionDerivative(pinocchio::Model            const & model,
                                   Eigen::Ref<vectorN_t const>         q,
                                   Eigen::Ref<vectorN_t const>         v,
                                   Eigen::Ref<vectorN_t>               qDot,
                                   float64_t                           dt = 1e-5); // Make a copy

    // Pinocchio joint types
    enum class joint_t : int32_t
    {
        // CYLINDRICAL are not available so far

        NONE = 0,
        LINEAR = 1,
        ROTARY = 2,
        PLANAR = 3,
        SPHERICAL = 4,
        FREE = 5,
    };

    result_t getJointNameFromPositionId(pinocchio::Model const & model,
                                        int32_t          const & idIn,
                                        std::string            & jointNameOut);

    result_t getJointNameFromVelocityId(pinocchio::Model const & model,
                                        int32_t          const & idIn,
                                        std::string            & jointNameOut);

    result_t getJointTypeFromId(pinocchio::Model const & model,
                                int32_t          const & idIn,
                                joint_t                & jointTypeOut);

    result_t getJointTypePositionSuffixes(joint_t                  const & jointTypeIn,
                                          std::vector<std::string>       & jointTypeSuffixesOut);

    result_t getJointTypeVelocitySuffixes(joint_t                  const & jointTypeIn,
                                          std::vector<std::string>       & jointTypeSuffixesOut);

    result_t getFrameIdx(pinocchio::Model const & model,
                         std::string      const & frameName,
                         int32_t                & frameIdx);
    result_t getFramesIdx(pinocchio::Model         const & model,
                          std::vector<std::string> const & framesNames,
                          std::vector<int32_t>           & framesIdx);

    result_t getJointPositionIdx(pinocchio::Model     const & model,
                                 std::string          const & jointName,
                                 std::vector<int32_t>       & jointPositionIdx);
    result_t getJointPositionIdx(pinocchio::Model const & model,
                                 std::string      const & jointName,
                                 int32_t                & jointPositionFirstIdx);
    result_t getJointsPositionIdx(pinocchio::Model         const & model,
                                  std::vector<std::string> const & jointsNames,
                                  std::vector<int32_t>           & jointsPositionIdx,
                                  bool                     const & firstJointIdxOnly = false);

    result_t getJointVelocityIdx(pinocchio::Model     const & model,
                                 std::string          const & jointName,
                                 std::vector<int32_t>       & jointVelocityIdx);
    result_t getJointVelocityIdx(pinocchio::Model const & model,
                                 std::string      const & jointName,
                                 int32_t                & jointVelocityFirstIdx);
    result_t getJointsVelocityIdx(pinocchio::Model         const & model,
                                  std::vector<std::string> const & jointsNames,
                                  std::vector<int32_t>           & jointsVelocityIdx,
                                  bool                     const & firstJointIdxOnly = false);

    result_t insertFlexibilityInModel(pinocchio::Model       & modelInOut,
                                      std::string      const & childJointNameIn,
                                      std::string      const & newJointNameIn);

    // ********************** Math utilities *************************

    template<typename T0, typename T1, typename... Ts>
    typename std::common_type<T0, T1, Ts...>::type min(T0 && val1, T1 && val2, Ts &&... vs);

    float64_t saturateSoft(float64_t const & in,
                           float64_t const & mi,
                           float64_t const & ma,
                           float64_t const & r);

    vectorN_t clamp(Eigen::Ref<vectorN_t const>         data,
                    float64_t                   const & minThr = -INF,
                    float64_t                   const & maxThr = +INF);

    // *********************** Miscellaneous **************************

    template<typename KeyType, typename ValueType>
    std::vector<ValueType> getMapValues(std::map<KeyType, ValueType> m);

    template<typename typeOut, typename typeIn>
    std::vector<std::shared_ptr<typeOut>> staticCastSharedPtrVector(std::vector<std::shared_ptr<typeIn>> vIn);

    template<class F, class dF=std::decay_t<F> >
    auto not_f(F&& f);

    template<typename type>
    void swapVectorBlocks(Eigen::Matrix<type, Eigen::Dynamic, 1>       & vector,
                          uint32_t                               const & firstBlockStart,
                          uint32_t                               const & firstBlockLength,
                          uint32_t                               const & secondBlockStart,
                          uint32_t                               const & secondBlockLength);
}

#include "jiminy/core/Utilities.tcc"

#endif  // SIMU_UTILITIES_H