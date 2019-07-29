#include <iostream>
#include <cmath>
#include <algorithm>

#include <boost/algorithm/clamp.hpp>
#include <boost/numeric/odeint/iterator/n_step_iterator.hpp>

#include "pinocchio/parsers/urdf.hpp"
#include "pinocchio/algorithm/kinematics.hpp"
#include "pinocchio/algorithm/frames.hpp"
#include "pinocchio/multibody/model.hpp"
#include "pinocchio/algorithm/aba.hpp"
#include "pinocchio/algorithm/rnea.hpp"
#include "pinocchio/algorithm/joint-configuration.hpp"
#include "pinocchio/algorithm/energy.hpp"

#include "jiminy/core/Utilities.h"
#include "jiminy/core/TelemetryData.h"
#include "jiminy/core/TelemetryRecorder.h"
#include "jiminy/core/AbstractController.h"
#include "jiminy/core/AbstractSensor.h"
#include "jiminy/core/Model.h"
#include "jiminy/core/Engine.h"


namespace jiminy
{
    float64_t const MAX_TIME_STEP_MAX = 3e-3;

    Engine::Engine(void):
    engineOptions_(nullptr),
    isInitialized_(false),
    isTelemetryConfigured_(false),
    model_(nullptr),
    controller_(nullptr),
    engineOptionsHolder_(),
    callbackFct_([](float64_t const & t,
                    vectorN_t const & x) -> bool
                 {
                     return true;
                 }),
    telemetrySender_(),
    telemetryData_(nullptr),
    telemetryRecorder_(nullptr),
    stepperState_(),
    forcesImpulse_(),
    forceImpulseNextIt_(forcesImpulse_.begin()),
    forcesProfile_()
    {
        // Initialize the configuration options to the default.
        setOptions(getDefaultOptions());

        // Initialize the global telemetry data holder
        telemetryData_ = std::make_shared<TelemetryData>();
        telemetryData_->reset();

        // Initialize the global telemetry recorder
        telemetryRecorder_ = std::make_unique<TelemetryRecorder>(
            std::const_pointer_cast<TelemetryData const>(telemetryData_));

        // Initialize the engine-specific telemetry sender
        telemetrySender_.configureObject(telemetryData_, ENGINE_OBJECT_NAME);
    }

    Engine::~Engine(void)
    {
        // Empty
    }

    result_t Engine::initialize(Model              & model,
                                AbstractController & controller,
                                callbackFct_t        callbackFct)
    {
        result_t returnCode = result_t::SUCCESS;

        if (!model.getIsInitialized())
        {
            std::cout << "Error - Engine::initialize - Model not initialized." << std::endl;
            returnCode = result_t::ERROR_INIT_FAILED;
        }
        model_ = &model;

        if (returnCode == result_t::SUCCESS)
        {
            returnCode = stepperState_.initialize(*model_, vectorN_t::Zero(model_->nx()));
        }

        if (!controller.getIsInitialized())
        {
            std::cout << "Error - Engine::initialize - Controller not initialized." << std::endl;
            returnCode = result_t::ERROR_INIT_FAILED;
        }
        if (returnCode == result_t::SUCCESS)
        {
            controller_ = &controller;
        }

        // TODO: Check that the callback function is working as expected
        if (returnCode == result_t::SUCCESS)
        {
            callbackFct_ = callbackFct;
        }

        // Make sure the gravity is properly set at model level
        if (returnCode == result_t::SUCCESS)
        {
            setOptions(engineOptionsHolder_);
            isInitialized_ = true;
        }

        if (returnCode != result_t::SUCCESS)
        {
            isInitialized_ = false;
        }

        return returnCode;
    }

    void Engine::reset(bool const & resetDynamicForceRegister)
    {
        // Initialize the random number generators
        resetRandGenerators(engineOptions_->stepper.randomSeed);

        if (resetDynamicForceRegister)
        {
            forcesImpulse_.clear();
            forceImpulseNextIt_ = forcesImpulse_.begin();
            forcesProfile_.clear();
        }

        // Reset the internal state of the model and controller
        model_->reset();
        controller_->reset();

        // Reset the telemetry
        telemetryRecorder_->reset();
        telemetryData_->reset();

        /* Preconfigure the telemetry with quantities known at compile time.
           Note that registration is only locked at the beginning of the
           simulation to enable dynamic registration until then. */
        isTelemetryConfigured_ = false;
        configureTelemetry();
    }

    result_t Engine::configureTelemetry(void)
    {
        result_t returnCode = result_t::SUCCESS;

        if (!getIsInitialized())
        {
            std::cout << "Error - Engine::configureTelemetry - The engine is not initialized." << std::endl;
            returnCode = result_t::ERROR_INIT_FAILED;
        }

        if (returnCode == result_t::SUCCESS)
        {
            if (!isTelemetryConfigured_)
            {
                // Register variables to the telemetry senders
                if (engineOptions_->telemetry.enableConfiguration)
                {
                    (void) registerNewVectorEntry(telemetrySender_,
                                                  model_->getPositionFieldNames(),
                                                  vectorN_t::Zero(model_->nq()));
                }
                if (engineOptions_->telemetry.enableVelocity)
                {
                    (void) registerNewVectorEntry(telemetrySender_,
                                                  model_->getVelocityFieldNames(),
                                                  vectorN_t::Zero(model_->nv()));
                }
                if (engineOptions_->telemetry.enableAcceleration)
                {
                    (void) registerNewVectorEntry(telemetrySender_,
                                                  model_->getAccelerationFieldNames(),
                                                  vectorN_t::Zero(model_->nv()));
                }
                if (engineOptions_->telemetry.enableCommand)
                {
                    (void) registerNewVectorEntry(telemetrySender_,
                                                  model_->getMotorTorqueFieldNames(),
                                                  vectorN_t::Zero(model_->getMotorsNames().size()));
                }
                if (engineOptions_->telemetry.enableEnergy)
                {
                    telemetrySender_.registerNewEntry<float64_t>("energy", 0.0);
                }
                isTelemetryConfigured_ = true;
            }
        }

        returnCode = controller_->configureTelemetry(telemetryData_);
        if (returnCode == result_t::SUCCESS)
        {
            returnCode = model_->configureTelemetry(telemetryData_);
        }

        if (returnCode != result_t::SUCCESS)
        {
            isTelemetryConfigured_ = false;
        }

        return returnCode;
    }

    void Engine::updateTelemetry(void)
    {
        // Update the telemetry internal state
        if (engineOptions_->telemetry.enableConfiguration)
        {
            updateVectorValue(telemetrySender_,
                              model_->getPositionFieldNames(),
                              stepperState_.qLast);
        }
        if (engineOptions_->telemetry.enableVelocity)
        {
            updateVectorValue(telemetrySender_,
                              model_->getVelocityFieldNames(),
                              stepperState_.vLast);
        }
        if (engineOptions_->telemetry.enableAcceleration)
        {
            updateVectorValue(telemetrySender_,
                              model_->getAccelerationFieldNames(),
                              stepperState_.aLast);
        }
        if (engineOptions_->telemetry.enableCommand)
        {
            updateVectorValue(telemetrySender_,
                              model_->getMotorTorqueFieldNames(),
                              stepperState_.uCommandLast);
        }
        if (engineOptions_->telemetry.enableEnergy)
        {
            telemetrySender_.updateValue<float64_t>("energy", stepperState_.energyLast);
        }
        controller_->updateTelemetry();
        model_->updateTelemetry();

        // Flush the telemetry internal state
        telemetryRecorder_->flushDataSnapshot(stepperState_.tLast);
    }

    result_t Engine::simulate(vectorN_t const & x_init,
                              float64_t const & end_time)
    {
        result_t returnCode = result_t::SUCCESS;

        if(!isInitialized_)
        {
            std::cout << "Error - Engine::simulate - Engine not initialized. Impossible to run the simulation." << std::endl;
            return result_t::ERROR_INIT_FAILED;
        }

        std::vector<int32_t> const & rigidJointsPositionIdx = model_->getRigidJointsPositionIdx();
        std::vector<int32_t> const & rigidJointsVelocityIdx = model_->getRigidJointsVelocityIdx();
        if(x_init.rows() != (uint32_t) (rigidJointsPositionIdx.size() + rigidJointsVelocityIdx.size()))
        {
            std::cout << "Error - Engine::simulate - Size of x_init inconsistent with model size." << std::endl;
            return result_t::ERROR_BAD_INPUT;
        }

        if(end_time < 5e-2)
        {
            std::cout << "Error - Engine::simulate - The duration of the simulation cannot be shorter than 50ms." << std::endl;
            return result_t::ERROR_BAD_INPUT;
        }

        // Define the stepper iterators. (Do NOT use bind since it passes the arguments by value)
        auto rhsBind =
            [this](vectorN_t const & x,
                   vectorN_t       & dxdt,
                   float64_t const & t)
            {
                this->systemDynamics(t, x, dxdt);
            };

        stepper_t stepper;
        if (engineOptions_->stepper.solver == "runge_kutta_dopri5")
        {
            stepper = make_controlled(engineOptions_->stepper.tolAbs,
                                      engineOptions_->stepper.tolRel,
                                      runge_kutta_stepper_t());
        }
        else if (engineOptions_->stepper.solver == "explicit_euler")
        {
            stepper = explicit_euler();
        }
        else
        {
            std::cout << "Error - Engine::simulate - The requested solver is not available." << std::endl;
            return result_t::ERROR_BAD_INPUT;
        }

        // Reset the model, controller, and engine
        reset();

        // Initialize the flexible state based on the rigid initial state
        vectorN_t x0 = vectorN_t::Zero(model_->nx());
        for (uint32_t i=0; i<rigidJointsPositionIdx.size(); ++i)
        {
            x0[rigidJointsPositionIdx[i]] = x_init[i];
        }
        for (uint32_t i=0; i<rigidJointsVelocityIdx.size(); ++i)
        {
            x0[rigidJointsVelocityIdx[i] + model_->nq()] = x_init[i + rigidJointsPositionIdx.size()];
        }
        for (int32_t const & jointId : model_->getFlexibleJointsPositionIdx())
        {
            x0[jointId + 3] = 1.0;
        }

        // Initialize the stepper internal state
        forceImpulseNextIt_ = forcesImpulse_.begin();
        for (auto & forceProfile : forcesProfile_)
        {
            getFrameIdx(model_->pncModel_, forceProfile.first, std::get<0>(forceProfile.second));
        }
        stepperState_.initialize(*model_, x0);
        systemDynamics(0, stepperState_.x, stepperState_.dxdt);

        // Reset the telemetry recorder, write the header, and lock the registration of new variables
        telemetryRecorder_->initialize();

        // Compute the breakpoints' period (for command or observation) during the integration loop
        float64_t updatePeriod = 0.0;
        if (engineOptions_->stepper.sensorsUpdatePeriod < EPS)
        {
            updatePeriod = engineOptions_->stepper.controllerUpdatePeriod;
        }
        else if (engineOptions_->stepper.controllerUpdatePeriod < EPS)
        {
            updatePeriod = engineOptions_->stepper.sensorsUpdatePeriod;
        }
        else
        {
            updatePeriod = std::min(engineOptions_->stepper.sensorsUpdatePeriod,
                                    engineOptions_->stepper.controllerUpdatePeriod);
        }

        // Set the initial time step
        float64_t dt = 0.0;
        if (updatePeriod > EPS)
        {
            dt = updatePeriod; // The initial time step is the update period
        }
        else
        {
            dt = engineOptions_->stepper.dtMax; // Use the maximum allowed time step as default
        }

        // Integration loop based on boost::numeric::odeint::detail::integrate_times
        float64_t current_time = 0.0;
        float64_t next_time = 0.0;
        failed_step_checker fail_checker; // to throw a runtime_error if step size adjustment fail
        while (true)
        {
            // Update internal state of the stepper
            vectorN_t const & q = stepperState_.x.head(model_->nq());
            vectorN_t const & v = stepperState_.x.tail(model_->nv());
            vectorN_t const & a = stepperState_.dxdt.tail(model_->nv());
            stepperState_.uLast = pinocchio::rnea(model_->pncModel_, model_->pncData_, q, v, a); // Update uLast directly to avoid temporary
            // Get system energy, kinematic computation are not needed since they were already done in RNEA.
            float64_t energy =
                pinocchio::kineticEnergy(model_->pncModel_, model_->pncData_, q, v, false)
                + pinocchio::potentialEnergy(model_->pncModel_, model_->pncData_, q, false);
            stepperState_.updateLast(current_time,
                                     q,
                                     v,
                                     a,
                                     stepperState_.uLast,
                                     stepperState_.uCommandLast,
                                     energy); // uCommandLast are already up-to-date

            // Monitor current iteration number, and log the current time, state, command, and sensors data.
            updateTelemetry();

            /* Stop the simulation if the end time has been reached, if
               the callback returns false, or if the number of integration
               steps exceeds 1e5. */
            if (std::abs(end_time - current_time) < EPS || !callbackFct_(current_time, stepperState_.x))
            {
                break;
            }
            else if (engineOptions_->stepper.iterMax > 0
            && stepperState_.iterLast >= engineOptions_->stepper.iterMax)
            {
                break;
            }
            else if ((stepperState_.x.array() != stepperState_.x.array()).any()) // nan is NOT equal to itself
            {
                std::cout << "Error - Engine::simulate - The low-level solver failed. Consider increasing accuracy." << std::endl;
                return result_t::ERROR_GENERIC;
            }

            if (updatePeriod > 0)
            {
                // Get the current time
                current_time = next_time;

                // Update the sensor data if necessary (only for finite update frequency)
                if (engineOptions_->stepper.sensorsUpdatePeriod > 0)
                {
                    float64_t next_time_update_sensor =
                        std::round(current_time / engineOptions_->stepper.sensorsUpdatePeriod) *
                        engineOptions_->stepper.sensorsUpdatePeriod;
                    if (std::abs(current_time - next_time_update_sensor) < 1e-8)
                    {
                        model_->setSensorsData(stepperState_.tLast,
                                               stepperState_.qLast,
                                               stepperState_.vLast,
                                               stepperState_.aLast,
                                               stepperState_.uLast);
                    }
                }

                // Update the controller command if necessary (only for finite update frequency)
                if (engineOptions_->stepper.controllerUpdatePeriod > 0)
                {
                    float64_t next_time_update_controller =
                        std::round(current_time / engineOptions_->stepper.controllerUpdatePeriod) *
                            engineOptions_->stepper.controllerUpdatePeriod;
                    if (std::abs(current_time - next_time_update_controller) < 1e-8)
                    {
                        controller_->computeCommand(stepperState_.tLast,
                                                    stepperState_.qLast,
                                                    stepperState_.vLast,
                                                    stepperState_.uCommandLast);

                        std::vector<int32_t> const & motorsVelocityIdx = model_->getMotorsVelocityIdx();
                        for (uint32_t i=0; i < motorsVelocityIdx.size(); i++)
                        {
                            uint32_t jointId = motorsVelocityIdx[i];
                            float64_t torque_max = model_->pncModel_.effortLimit(jointId); // effortLimit is given in the velocity vector space
                            stepperState_.uControl[jointId] = stepperState_.uCommandLast[i];
                            stepperState_.uControl[i] = boost::algorithm::clamp(
                                stepperState_.uControl[i], -torque_max, torque_max);
                        }

                        /* Update the internal stepper state dxdt since the dynamics has changed.
                           Make sure the next impulse force iterator has NOT been updated at this point ! */
                        if (engineOptions_->stepper.solver != "explicit_euler")
                        {
                            systemDynamics(current_time, stepperState_.x, stepperState_.dxdt);
                        }
                    }
                }
            }

            // Get the next impulse force application time and update the iterator if necessary
            float64_t tForceImpulseNext = end_time;
            if (forceImpulseNextIt_ != forcesImpulse_.end())
            {
                float64_t tForceImpulseNextTmp = forceImpulseNextIt_->first;
                float64_t dtForceImpulseNext = std::get<1>(forceImpulseNextIt_->second);
                if (current_time > tForceImpulseNextTmp + dtForceImpulseNext)
                {
                    ++forceImpulseNextIt_;
                    tForceImpulseNextTmp = forceImpulseNextIt_->first;
                }

                if (forceImpulseNextIt_ != forcesImpulse_.end())
                {
                    if (tForceImpulseNextTmp > current_time)
                    {
                        tForceImpulseNext = tForceImpulseNextTmp;
                    }
                    else
                    {
                        if (forceImpulseNextIt_ != std::prev(forcesImpulse_.end()))
                        {
                            tForceImpulseNext = std::next(forceImpulseNextIt_)->first;
                        }
                    }
                }
            }

            if (updatePeriod > 0)
            {
                // Get the target time at next iteration
                next_time += min(updatePeriod,
                                 end_time - current_time,
                                 tForceImpulseNext - current_time);

                // Compute the next step using adaptive step method
                while (current_time < next_time)
                {
                    // adjust stepsize to end up exactly at the next breakpoint
                    float64_t current_dt = std::min(dt, next_time - current_time);
                    if (success == boost::apply_visitor(
                        [&](auto && one)
                        {
                            return one.try_step(rhsBind,
                                                stepperState_.x,
                                                stepperState_.dxdt,
                                                current_time,
                                                current_dt);
                        }, stepper))
                    {
                        fail_checker.reset(); // reset the fail counter, see #173
                        dt = std::max(dt, current_dt); // continue with the original step size if dt was reduced due to the next breakpoint
                    }
                    else
                    {
                        fail_checker();  // check for possible overflow of failed steps in step size adjustment
                        dt = current_dt;
                    }
                    dt = std::min(dt, engineOptions_->stepper.dtMax); // Make sure it never exceeds dtMax
                }
            }
            else
            {
                // Make sure it ends exactly at the end_time, never exceeds dtMax, and stop to apply impulse forces
                dt = min(dt,
                         engineOptions_->stepper.dtMax,
                         end_time - current_time,
                         tForceImpulseNext - current_time);

                // Compute the next step using adaptive step method
                controlled_step_result res = fail;
                while (res == fail)
                {
                    res = boost::apply_visitor(
                        [&](auto && one)
                        {
                            return one.try_step(rhsBind,
                                                stepperState_.x,
                                                stepperState_.dxdt,
                                                current_time,
                                                dt);
                        }, stepper);
                    if (res == success)
                    {
                        fail_checker.reset(); // reset the fail counter
                    }
                    else
                    {
                        fail_checker();  // check for possible overflow of failed steps in step size adjustment
                    }
                }
            }
        }

        return returnCode;
    }

    void Engine::registerForceImpulse(std::string const & frameName,
                                      float64_t   const & t,
                                      float64_t   const & dt,
                                      vector3_t   const & F)
    {
        // Make sure that the forces do NOT overlap while taking into account dt.

        forcesImpulse_[t] = std::make_tuple(frameName, dt, F);
    }

    void Engine::registerForceProfile(std::string      const & frameName,
                                      external_force_t         forceFct)
    {
        forcesProfile_.emplace_back(frameName, std::make_tuple(0, forceFct));
    }

    void Engine::systemDynamics(float64_t const & t,
                                vectorN_t const & x,
                                vectorN_t       & dxdt)
    {
        /* Note that the position of the free flyer is in world frame, whereas the
           velocities and accelerations are relative to the parent body frame. */

        // Extract configuration and velocity vectors
        vectorN_t const & q = x.head(model_->nq());
        vectorN_t const & v = x.tail(model_->nv());

        // Compute kinematics information
        pinocchio::forwardKinematics(model_->pncModel_, model_->pncData_, q, v);
        pinocchio::framesForwardKinematics(model_->pncModel_, model_->pncData_);

        // Compute the external forces
        pinocchio::container::aligned_vector<pinocchio::Force> fext(
            model_->pncModel_.joints.size(), pinocchio::Force::Zero());
        std::vector<int32_t> const & contactFramesIdx = model_->getContactFramesIdx();
        for(uint32_t i=0; i < contactFramesIdx.size(); i++)
        {
            int32_t const & contactFrameIdx = contactFramesIdx[i];
            model_->contactForces_[i] = pinocchio::Force(contactDynamics(contactFrameIdx));
            int32_t const & parentIdx = model_->pncModel_.frames[contactFrameIdx].parent;
            fext[parentIdx] += model_->contactForces_[i];
        }

        /* Update the sensor data if necessary (only for infinite update frequency).
           Impossible to have access to the current acceleration and efforts. */
        if (engineOptions_->stepper.sensorsUpdatePeriod < EPS)
        {
            model_->setSensorsData(t, q, v, stepperState_.aLast, stepperState_.uLast);
        }

        /* Update the controller command if necessary (only for infinite update frequency).
           Be careful, in this particular case uCommandLast is not guarantee to be the last command. */
        if (engineOptions_->stepper.controllerUpdatePeriod < EPS)
        {
            controller_->computeCommand(t, q, v, stepperState_.uCommandLast);

            std::vector<int32_t> const & motorsVelocityIdx = model_->getMotorsVelocityIdx();
            for (uint32_t i=0; i < motorsVelocityIdx.size(); i++)
            {
                uint32_t const & jointId = motorsVelocityIdx[i];
                float64_t const & torque_max = model_->pncModel_.effortLimit(jointId); // effortLimit is given in the velocity vector space
                stepperState_.uControl[jointId] = stepperState_.uCommandLast[i];
                stepperState_.uControl[i] = boost::algorithm::clamp(
                    stepperState_.uControl[i], -torque_max, torque_max);
            }
        }

        // Add the effect of user-defined external forces
        if (forceImpulseNextIt_ != forcesImpulse_.end())
        {
            float64_t const & tForceImpulseNext = forceImpulseNextIt_->first;
            float64_t const & dt = std::get<1>(forceImpulseNextIt_->second);
            if (tForceImpulseNext <= t && t <= tForceImpulseNext + dt)
            {
                int32_t frameIdx;
                std::string const & frameName = std::get<0>(forceImpulseNextIt_->second);
                vector3_t const & F = std::get<2>(forceImpulseNextIt_->second);
                getFrameIdx(model_->pncModel_, frameName, frameIdx);
                int32_t const & parentIdx = model_->pncModel_.frames[frameIdx].parent;
                fext[parentIdx] += pinocchio::Force(computeFrameForceOnParentJoint(frameIdx, F));
            }
        }

        for (auto const & forceProfile : forcesProfile_)
        {
            int32_t const & frameIdx = std::get<0>(forceProfile.second);
            int32_t const & parentIdx = model_->pncModel_.frames[frameIdx].parent;
            external_force_t const & forceFct = std::get<1>(forceProfile.second);
            fext[parentIdx] += pinocchio::Force(computeFrameForceOnParentJoint(frameIdx, forceFct(t, x)));
        }

        // Compute command and internal dynamics
        controller_->internalDynamics(t, q, v, stepperState_.uInternal); // TODO: Send the values at previous iteration instead
        boundsDynamics(q, v, stepperState_.uBounds);
        vectorN_t u = stepperState_.uBounds + stepperState_.uInternal + stepperState_.uControl;

        // Compute dynamics
        vectorN_t a = pinocchio::aba(model_->pncModel_, model_->pncData_, q, v, u, fext);

        /* Hack to compute the configuration vector derivative, including the
           quaternions on SO3 automatically. Note that the time difference must
           not be too small to avoid failure. */
        float64_t dt = std::max(1e-5, t - stepperState_.tLast);
        vectorN_t qNext = vectorN_t::Zero(model_->nq());
        pinocchio::integrate(model_->pncModel_, q, v*dt, qNext);
        vectorN_t qDot = (qNext - q) / dt;

        // Fill up dxdt
        dxdt.resize(model_->nx());
        dxdt.head(model_->nq()) = qDot;
        dxdt.tail(model_->nv()) = a;
    }

    vector6_t Engine::computeFrameForceOnParentJoint(int32_t   const & frameId,
                                                     vector3_t const & fextInWorld) const
    {
        // Get various transformations
        matrix3_t const & tformFrameRot = model_->pncData_.oMf[frameId].rotation();
        matrix3_t const & tformFrameJointRot = model_->pncModel_.frames[frameId].placement.rotation();
        vector3_t const & posFrameJoint = model_->pncModel_.frames[frameId].placement.translation();

        // Compute the forces at the origin of the parent joint frame
        vector6_t fextLocal = vector6_t::Zero();
        fextLocal.head<3>() = tformFrameJointRot * tformFrameRot.transpose() * fextInWorld;
        fextLocal.tail<3>() = posFrameJoint.cross(fextLocal.head<3>());

        return fextLocal;
    }

    vectorN_t Engine::contactDynamics(int32_t const & frameId) const
    {
        // /* /!\ Note that the contact dynamics depends only on kinematics data. /!\ */

        contactOptions_t const * const contactOptions_ = &engineOptions_->contacts;

        matrix3_t const & tformFrameRot = model_->pncData_.oMf[frameId].rotation();
        vector3_t const & posFrame = model_->pncData_.oMf[frameId].translation();

        vectorN_t fextLocal = vectorN_t::Zero(6);

        if(posFrame(2) < 0.0)
        {
            // Initialize the contact force
            vector3_t fextInWorld = vector3_t::Zero();

            vector3_t motionFrame = pinocchio::getFrameVelocity(model_->pncModel_,
                                                                model_->pncData_,
                                                                frameId).linear();
            vector3_t vFrameInWorld = tformFrameRot * motionFrame;

            // Compute normal force
            float64_t damping = 0;
            if(vFrameInWorld(2) < 0)
            {
                damping = -contactOptions_->damping * vFrameInWorld(2);
            }
            fextInWorld(2) = -contactOptions_->stiffness * posFrame(2) + damping;

            // Compute friction forces
            Eigen::Vector2d const & vxy = vFrameInWorld.head<2>();
            float64_t vNorm = vxy.norm();
            float64_t frictionCoeff;
            if(vNorm > contactOptions_->dryFrictionVelEps)
            {
                if(vNorm < 1.5 * contactOptions_->dryFrictionVelEps)
                {
                    frictionCoeff = -2.0 * vNorm * (contactOptions_->frictionDry -
                        contactOptions_->frictionViscous) / contactOptions_->dryFrictionVelEps
                        + 3.0 * contactOptions_->frictionDry - 2.0*contactOptions_->frictionViscous;
                }
                else
                {
                    frictionCoeff = contactOptions_->frictionViscous;
                }
            }
            else
            {
                frictionCoeff = vNorm * contactOptions_->frictionDry /
                    contactOptions_->dryFrictionVelEps;
            }
            fextInWorld.head<2>() = -vxy * frictionCoeff * fextInWorld(2);

            // Make sure that the tangential force never exceeds 1e5 N for the sake of numerical stability
            fextInWorld.head<2>() = clamp(fextInWorld.head<2>(), -1e5, 1e5);

            // Compute the forces at the origin of the parent joint frame
            fextLocal = computeFrameForceOnParentJoint(frameId, fextInWorld);

            // Add blending factor
            float64_t blendingFactor = -posFrame(2) / contactOptions_->transitionEps;
            float64_t blendingLaw = std::tanh(2 * blendingFactor);
            fextLocal *= blendingLaw;
        }

        return fextLocal;
    }

    void Engine::boundsDynamics(vectorN_t const & q,
                                vectorN_t const & v,
                                vectorN_t       & u)
    {
        // Enforce the bounds of the actuated joints of the model
        u = vectorN_t::Zero(model_->nv());

        Model::jointOptions_t const & mdlJointOptions_ = model_->mdlOptions_->joints;
        Engine::jointOptions_t const & engineJointOptions_ = engineOptions_->joints;

        std::vector<int32_t> const & motorsPositionIdx = model_->getMotorsPositionIdx();
        std::vector<int32_t> const & motorsVelocityIdx = model_->getMotorsVelocityIdx();
        for (uint32_t i = 0; i < motorsPositionIdx.size(); i++)
        {
            float64_t const qJoint = q(motorsPositionIdx[i]);
            float64_t const vJoint = v(motorsVelocityIdx[i]);
            float64_t const qJointMin = mdlJointOptions_.boundsMin(i);
            float64_t const qJointMax = mdlJointOptions_.boundsMax(i);

            float64_t forceJoint = 0;
            float64_t qJointError = 0;
            if (qJoint > qJointMax)
            {
                qJointError = qJoint - qJointMax;
                float64_t damping = -engineJointOptions_.boundDamping * std::max(vJoint, 0.0);
                forceJoint = -engineJointOptions_.boundStiffness * qJointError + damping;
            }
            else if (qJoint < qJointMin)
            {
                qJointError = qJointMin - qJoint;
                float64_t damping = -engineJointOptions_.boundDamping * std::min(vJoint, 0.0);
                forceJoint = engineJointOptions_.boundStiffness * qJointError + damping;
            }

            float64_t blendingFactor = qJointError / engineJointOptions_.boundTransitionEps;
            float64_t blendingLaw = std::tanh(2 * blendingFactor);
            forceJoint *= blendingLaw;

            u(motorsVelocityIdx[i]) += forceJoint;
        }
    }

    configHolder_t Engine::getOptions(void) const
    {
        return engineOptionsHolder_;
    }

    void Engine::setOptions(configHolder_t const & engineOptions)
    {
        // Update the internal options
        engineOptionsHolder_ = engineOptions;

        // Make sure some parameters are in the required bounds
        float64_t & dtMax = boost::get<float64_t>(
            boost::get<configHolder_t>(engineOptionsHolder_.at("stepper")).at("dtMax"));
        dtMax = std::min(std::max(dtMax, MIN_TIME_STEP_MAX), MAX_TIME_STEP_MAX);

        // Create a fast struct accessor
        engineOptions_ = std::make_unique<engineOptions_t const>(engineOptionsHolder_);

        // Propagate the options at Model level
        if (isInitialized_)
        {
            model_->pncModel_.gravity = engineOptions_->world.gravity; // It is reversed (Third Newton law)
        }
    }

    bool Engine::getIsInitialized(void) const
    {
        return isInitialized_;
    }

    Model const & Engine::getModel(void) const
    {
        return *model_;
    }

    void Engine::getLogData(std::vector<std::string> & header,
                            matrixN_t                & logData)
    {
        std::vector<float32_t> timestamps;
        std::vector<std::vector<int32_t> > intData;
        std::vector<std::vector<float32_t> > floatData;
        telemetryRecorder_->getData(header, timestamps, intData, floatData);

        // Never empty since it contains at least the initial state
        logData.resize(timestamps.size(), 1 + intData[0].size() + floatData[0].size());
        logData.col(0) = Eigen::Matrix<float32_t, 1, Eigen::Dynamic>::Map(
            timestamps.data(), timestamps.size()).cast<float64_t>();
        for (uint32_t i=0; i<intData.size(); i++)
        {
            logData.block(i, 1, 1, intData[i].size()) =
                Eigen::Matrix<int32_t, 1, Eigen::Dynamic>::Map(
                    intData[i].data(), intData[i].size()).cast<float64_t>();
        }
        for (uint32_t i=0; i<floatData.size(); i++)
        {
            logData.block(i, 1 + intData[0].size(), 1, floatData[i].size()) =
                Eigen::Matrix<float32_t, 1, Eigen::Dynamic>::Map(
                    floatData[i].data(), floatData[i].size()).cast<float64_t>();
        }
    }

    void Engine::writeLogTxt(std::string const & filename)
    {
        std::vector<std::string> header;
        matrixN_t log;
        getLogData(header, log);

        std::ofstream myfile = std::ofstream(filename,
                                             std::ios::out |
                                             std::ofstream::trunc);

        auto indexConstantEnd = std::find(header.begin(), header.end(), START_COLUMNS);
        std::copy(header.begin() + 1,
                  indexConstantEnd - 1,
                  std::ostream_iterator<std::string>(myfile, ", ")); // Discard the first one (start constant flag)
        std::copy(indexConstantEnd - 1,
                  indexConstantEnd,
                  std::ostream_iterator<std::string>(myfile, "\n"));
        std::copy(indexConstantEnd + 1,
                  header.end() - 2,
                  std::ostream_iterator<std::string>(myfile, ", "));
        std::copy(header.end() - 2,
                  header.end() - 1,
                  std::ostream_iterator<std::string>(myfile, "\n")); // Discard the last one (start data flag)

        Eigen::IOFormat CSVFormat(Eigen::StreamPrecision, Eigen::DontAlignCols, ", ", "\n");
        myfile << log.format(CSVFormat);

        myfile.close();
    }

    void Engine::writeLogBinary(std::string const & filename)
    {
        telemetryRecorder_->writeDataBinary(filename);
    }
}