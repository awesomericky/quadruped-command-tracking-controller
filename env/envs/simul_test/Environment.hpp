//----------------------------//
// This file is part of RaiSim//
// Copyright 2020, RaiSim Tech//
//----------------------------//

#pragma once

#include <stdlib.h>
#include <set>
#include "../../RaisimGymEnv.hpp"

// [Tip]
//
// // Logging example
// Eigen::IOFormat CommaInitFmt(Eigen::StreamPrecision, Eigen::DontAlignCols, ", ", ", ", "", "", " << ", ";");
// std::cout << current_leg_phase.format(CommaInitFmt) << std::endl;
//std::cout << obstacle_distance_cost << "\n";
//
// // To make new function
// 1. Environment.hpp
// 2. raisim_gym.cpp (if needed)
// 3. RaisimGymEnv.hpp
// 4. VectorizedEnvironment.hpp
// 5. RaisimGymVecEnv.py (if needed)

namespace raisim
{

    class ENVIRONMENT : public RaisimGymEnv
    {

    public:
        explicit ENVIRONMENT(const std::string &resourceDir, const Yaml::Node &cfg, bool visualizable, int sample_env_type, int seed, double sample_obstacle_grid_size, double sample_obstacle_dr)
        : RaisimGymEnv(resourceDir, cfg), visualizable_(visualizable)
        {
            /*
             * For testing, we do not use sampled sample_env_type, seed, sample_obstacle_grid_size, sample_obstacle_dr.
             * We rather use deterministic configuration.
             */

            /// create world
            world_ = std::make_unique<raisim::World>();

            double hm_centerX = 0.0, hm_centerY = 0.0;
            hm_sizeX = 40., hm_sizeY = 40.;
            double hm_samplesX = hm_sizeX * 12, hm_samplesY = hm_sizeY * 12;
            double unitX = hm_sizeX / hm_samplesX, unitY = hm_sizeY / hm_samplesY;
            double obstacle_height = 2;

            random_seed = cfg["seed"]["evaluate"].template As<int>();
            static std::default_random_engine env_generator(random_seed);

            visualize_path = cfg["visualize_path"].As<bool>();

            /// sample obstacle center
            double obstacle_grid_size = cfg["test_obstacle_grid_size"].template As<double>();
            int n_x_grid = int(hm_sizeX / obstacle_grid_size);
            int n_y_grid = int(hm_sizeY / obstacle_grid_size);
            n_obstacle = n_x_grid * n_y_grid;

            obstacle_centers.setZero(n_obstacle, 2);
            int obstacle_center_distribution_type = cfg["test_obstacle_center_distribution_type"].template As<int>();
            std::uniform_real_distribution<> uniform_distrib_0(0.3, obstacle_grid_size - 0.3);
            std::uniform_real_distribution<> uniform_distrib_1(0.8, obstacle_grid_size - 0.8);
            std::uniform_real_distribution<> uniform_distrib_2(0.55, obstacle_grid_size - 0.55);
            std::uniform_real_distribution<> uniform_distrib_3(1.0, obstacle_grid_size - 1.0);
            for (int i=0; i<n_obstacle; i++) {
                int current_n_y = int(i / n_x_grid);
                int current_n_x = i - current_n_y * n_x_grid;
                double sampled_x;
                double sampled_y;
                if (obstacle_center_distribution_type == 0) {
                    sampled_x = uniform_distrib_0(env_generator);
                    sampled_y = uniform_distrib_0(env_generator);
                } 
                else if (obstacle_center_distribution_type == 1) {
                            sampled_x = uniform_distrib_1(env_generator);
                            sampled_y = uniform_distrib_1(env_generator);
                    }
                else if (obstacle_center_distribution_type == 2) {
                    sampled_x = uniform_distrib_2(env_generator);
                    sampled_y = uniform_distrib_2(env_generator);
                }
                else {
                    sampled_x = uniform_distrib_3(env_generator);
                    sampled_y = uniform_distrib_3(env_generator);
                }

                sampled_x +=  obstacle_grid_size * current_n_x;
                sampled_x -= hm_sizeX/2;
                sampled_y += obstacle_grid_size * current_n_y;
                sampled_y -= hm_sizeY/2;
                obstacle_centers(i, 0) = sampled_x;
                obstacle_centers(i, 1) = sampled_y;
            }

            /// generate obstacles
            std::uniform_int_distribution<> random_obstacle_sampling(1, 2);
            std::uniform_real_distribution<> uniform_cylinder_obstacle(0.3, 0.5);
            std::uniform_real_distribution<> uniform_box_obstacle(0.6, 1.0);
            Eigen::VectorXd obstacle_type_list, obstacle_circle_dr, obstacle_box_size;
            obstacle_type_list.setZero(n_obstacle);
            obstacle_circle_dr.setZero(n_obstacle);
            obstacle_box_size.setZero(n_obstacle);
            for (int i=0; i<n_obstacle; i++) {
                int random_obstacle = random_obstacle_sampling(env_generator);
                obstacle_type_list[i] = random_obstacle;
                if (random_obstacle == 1) {
                    /// generate cylinder
                    obstacle_circle_dr[i] = uniform_cylinder_obstacle(env_generator);
                } else {
                    /// generate box
                    obstacle_box_size[i] = uniform_box_obstacle(env_generator);
                }
            }

            /// set raw height value
            for (int j=0; j<hm_samplesY; j++) {
                for (int i=0; i<hm_samplesX; i++) {
                    double x = (i - (hm_samplesX / 2)) * unitX, y = (j - (hm_samplesY / 2)) * unitY;
                    bool available_obstacle = false, available_init = true;
                    // consider box and cylinder obstacle
                    for (int k=0; k<n_obstacle; k++) {
                        double obstacle_x = obstacle_centers(k, 0), obstacle_y = obstacle_centers(k, 1);
                        if (obstacle_type_list[k] == 1) {
                            // cylinder obstacle
                            if (sqrt(pow(x - obstacle_x, 2) + pow(y - obstacle_y, 2)) < obstacle_circle_dr[k])
                                available_obstacle = true;
                            if (sqrt(pow(x - obstacle_x, 2) + pow(y - obstacle_y, 2)) < (obstacle_circle_dr[k] + 0.8))
                                available_init = false;
                        } else {
                            // box obstacle
                            if (abs(x - obstacle_x) <= obstacle_box_size[k]/2 && abs(y - obstacle_y) <= obstacle_box_size[k]/2)
                                available_obstacle = true;
                            if (sqrt(pow(x - obstacle_x, 2) + pow(y - obstacle_y, 2)) < ((obstacle_box_size[k] * sqrt(2)) / 2 + 0.8))
                                available_init = false;
                        }
                    }

                    if (j==0 || j==hm_samplesY-1)
                        available_obstacle = true;
                    if (i==0 || i==hm_samplesX-1)
                        available_obstacle = true;

                    if ( x < (- hm_sizeX/2 + 3) || (hm_sizeX/2 - 3) < x ||
                         y < (- hm_sizeY/2 + 3) || (hm_sizeY/2 - 3) < y)
                        available_init = false;

                    if (available_obstacle)
                        hm_raw_value.push_back(obstacle_height);
                    else
                        hm_raw_value.push_back(0.0);

                    if (available_init) {
                        init_set.push_back({x, y});

                        // For goal point distance condition, should also consider the "hm_size" because there should be enough goals in all four squares for evaluating in point_goal_initialize
                        if (sqrt(pow(x, 2) + pow(y, 2)) > 15)
                            goal_set.push_back({x, y});
                    }
                }
            }

            n_init_set = init_set.size();
            n_goal_set = goal_set.size();
            int total_n_point_goal = cfg_["n_goals_per_env"].template As<int>();

            /// initialization for each specific task
            point_goal_initialize = cfg["test_initialize"]["point_goal"].template As<bool>();
            safe_control_initialize = cfg["test_initialize"]["safety_control"].template As<bool>();

            if (point_goal_initialize) {
                /// sample goals for point goal navigation (sample equally in each frame)
                static std::default_random_engine generator(random_seed);
                std::vector<int> one_square_goal, two_square_goal, three_square_goal, four_square_goal;

                for (int i=0; i<n_goal_set; i++) {
                    if (goal_set[i][0] >= 0 && goal_set[i][1] >= 0)
                        one_square_goal.push_back(i);
                    if (goal_set[i][0] < 0 && goal_set[i][1] >= 0)
                        two_square_goal.push_back(i);
                    if (goal_set[i][0] < 0 && goal_set[i][1] < 0)
                        three_square_goal.push_back(i);
                    if (goal_set[i][0] >= 0 && goal_set[i][1] < 0)
                        four_square_goal.push_back(i);
                }

                std::uniform_int_distribution<> uniform_one_square_goal(0, one_square_goal.size()-1);
                std::uniform_int_distribution<> uniform_two_square_goal(0, two_square_goal.size()-1);
                std::uniform_int_distribution<> uniform_three_square_goal(0, three_square_goal.size()-1);
                std::uniform_int_distribution<> uniform_four_square_goal(0, four_square_goal.size()-1);

                int current_sampled_n_goals = 0;
                while (sampled_goal_set.size() <= total_n_point_goal) {
                    int current_sampled_goal = 0;
                    if (current_sampled_n_goals < total_n_point_goal / 4)
                        current_sampled_goal = one_square_goal[uniform_one_square_goal(generator)];
                    else if (current_sampled_n_goals < total_n_point_goal * 2 / 4)
                        current_sampled_goal = two_square_goal[uniform_two_square_goal(generator)];
                    else if (current_sampled_n_goals < total_n_point_goal * 3 / 4)
                        current_sampled_goal = three_square_goal[uniform_three_square_goal(generator)];
                    else
                        current_sampled_goal = four_square_goal[uniform_four_square_goal(generator)];

                    bool already_exist = false;
                    for (int j = 0; j < sampled_goal_set.size(); j++) {
                        if (sampled_goal_set[j] == current_sampled_goal) {
                            already_exist = true;
                            break;
                        }
                    }
                    if (!already_exist) {
                        sampled_goal_set.push_back(current_sampled_goal);
                        current_sampled_n_goals += 1;
                    }
                }
            }

            /// add heightmap
            hm = world_->addHeightMap(hm_samplesX, hm_samplesY, hm_sizeX, hm_sizeY, hm_centerX, hm_centerY, hm_raw_value);

            /// add objects
            anymal_ = world_->addArticulatedSystem(resourceDir_ + "/anymal_c/urdf/anymal.urdf");
            anymal_->setName("anymal");
            anymal_->setControlMode(raisim::ControlMode::PD_PLUS_FEEDFORWARD_TORQUE);

            /// add box for contact checking for anymal
            anymal_box_ = world_->addBox(1.1, 0.55, 0.2, 0.1, "default", raisim::COLLISION(2), RAISIM_STATIC_COLLISION_GROUP);
            anymal_box_->setName("anymal_box");

            /// get robot data
            gcDim_ = anymal_->getGeneralizedCoordinateDim(); // 3(base position) + 4(base orientation) + 12(joint position) = 19
            gvDim_ = anymal_->getDOF();                      // 3(base linear velocity) + 3(base angular velocity) + 12(joint velocity) = 18
            nJoints_ = gvDim_ - 6;                           // 12

            /// set depth sensor (lidar)
            lidar_theta = M_PI * 2;
            delta_lidar_theta = M_PI / 180;
            scanSize = int(lidar_theta / delta_lidar_theta);

            /// initialize containers
            gc_.setZero(gcDim_);
            gc_init_.setZero(gcDim_);
            random_gc_init.setZero(gcDim_);
            gv_.setZero(gvDim_);
            gv_init_.setZero(gvDim_);
            random_gv_init.setZero(gvDim_);
            pTarget_.setZero(gcDim_);
            vTarget_.setZero(gvDim_);
            pTarget12_.setZero(nJoints_);
            joint_position_error_history.setZero(nJoints_ * n_history_steps);
            joint_velocity_history.setZero(nJoints_ * n_history_steps);

            if (point_goal_initialize) {
                /// find the point closest to the center
                double min_distance= 100;
                double distance;
                for (int i=0; i<n_init_set; i++) {
                    double x = init_set[i][0];
                    double y = init_set[i][1];
                    distance = sqrt(pow(x, 2) + pow(y, 2));
                    if (distance < min_distance) {
                        point_goal_init[0] = x;
                        point_goal_init[1] = y;
                        min_distance = distance;
                    }
                }
            }

            /// nominal configuration of anymal_c
            gc_init_ << 0, 0, 0.7, 1.0, 0.0, 0.0, 0.0, 0.03, 0.5, -0.9, -0.03, 0.5, -0.9, 0.03, -0.5, 0.9, -0.03, -0.5, 0.9;  //0.5
            random_gc_init = gc_init_; random_gv_init = gv_init_;

            /// set pd gains
            Eigen::VectorXd jointPgain(gvDim_), jointDgain(gvDim_);
            jointPgain.setZero();
            jointPgain.tail(nJoints_).setConstant(50.0);
            jointDgain.setZero();
            jointDgain.tail(nJoints_).setConstant(0.2);
            anymal_->setPdGains(jointPgain, jointDgain);
            anymal_->setGeneralizedForce(Eigen::VectorXd::Zero(gvDim_));

            /// MUST BE DONE FOR ALL ENVIRONMENTS
            obDim_ = 81 + scanSize;
            actionDim_ = nJoints_;
            actionMean_.setZero(actionDim_);
            actionStd_.setZero(actionDim_);
            obDouble_.setZero(obDim_);
            coordinateDouble.setZero(3);

            /// action scaling
            actionMean_ = gc_init_.tail(nJoints_);
            actionStd_.setConstant(0.3);

            /// indices of links that could make contact
            footIndices_.insert(anymal_->getBodyIdx("LF_SHANK"));
            footIndices_.insert(anymal_->getBodyIdx("RF_SHANK"));
            footIndices_.insert(anymal_->getBodyIdx("LH_SHANK"));
            footIndices_.insert(anymal_->getBodyIdx("RH_SHANK"));
            // footIndices_.insert(anymal_->getBodyIdx("LF_THIGH"));
            // footIndices_.insert(anymal_->getBodyIdx("RF_THIGH"));
            // footIndices_.insert(anymal_->getBodyIdx("LH_THIGH"));
            // footIndices_.insert(anymal_->getBodyIdx("RH_THIGH"));

            /// visualize if it is the first environment
            if (visualizable_)
            {
                server_ = std::make_unique<raisim::RaisimServer>(world_.get());
                server_->launchServer();

                /// visualize scans
                for (int i=0; i<scanSize; i++)
                    scans.push_back(server_->addVisualBox("box" + std::to_string(i), 0.05, 0.05, 0.05, 1, 0, 0));

                /// visualize trajectory
                for (int i=0; i<n_prediction_step; i++) {
                    desired_command_traj.push_back(server_->addVisualBox("desired_command_pos" + std::to_string(i), 0.08, 0.08, 0.08, 1, 1, 0));  // yellow
                    modified_command_traj.push_back(server_->addVisualBox("modified_command_pos" + std::to_string(i), 0.08, 0.08, 0.08, 0, 0, 1));  // blue
                }

                if (point_goal_initialize) {
                    /// goal
                    server_->addVisualCylinder("goal", 0.4, 0.8, 2, 1, 0);

                    /// set path
                    if (visualize_path) {
                        double max_time = 180., delta_time = 0.1;
                        max_num_path_slice = int(max_time / delta_time);
                        for (int i=0; i<max_num_path_slice; i++)
                            server_->addVisualBox("path" + std::to_string(i+1), 0.1, 0.1, 0.1, 1, 0, 0);
                    }
                }

                server_->focusOn(anymal_);
            }
        }

    void init() final {}

    void reset() final
    {
        static std::default_random_engine generator(random_seed);

        if (safe_control_initialize) {
            if (current_n_step == 0) {
                /// Random initialization by sampling available x, y position
                std::uniform_int_distribution<> uniform_init(0, n_init_set-1);
                std::normal_distribution<> normal(0, 1);
                int n_init = uniform_init(generator);
                raisim::Vec<3> random_axis;
                raisim::Vec<4> random_quaternion;

                // Random position
                for (int i=0; i<2; i++)
                    random_gc_init[i] = init_set[n_init][i];

                // Random orientation (just randomizing yaw angle)
                random_axis[0] = 0;
                random_axis[1] = 0;
                random_axis[2] = 1;
                std::uniform_real_distribution<> uniform_angle(-1, 1);
                double random_angle = uniform_angle(generator) * M_PI;
                raisim::angleAxisToQuaternion(random_axis, random_angle, random_quaternion);
                random_gc_init.segment(3, 4) = random_quaternion.e();

                current_random_gc_init = random_gc_init;
                current_random_gv_init = random_gv_init;
                anymal_->setState(random_gc_init, random_gv_init);
                initHistory();
            }
            else {
                anymal_->setState(current_random_gc_init, current_random_gv_init);
                initHistory();
            }
        }
        else if (point_goal_initialize) {
            if (current_n_step == 0) {
                // position
                for (int i = 0; i < 2; i++)
                    random_gc_init[i] = point_goal_init[i];

                // random orientation (just randomizing yaw angle)
                raisim::Vec<4> random_quaternion;
                std::uniform_real_distribution<> uniform_angle(-1, 1);
                double random_angle = uniform_angle(generator) * M_PI;
                raisim::angleAxisToQuaternion({0, 0, 1}, random_angle, random_quaternion);
                random_gc_init.segment(3, 4) = random_quaternion.e();

                current_random_gc_init = random_gc_init;
                current_random_gv_init = random_gv_init;
                anymal_->setState(random_gc_init, random_gv_init);
                initHistory();
            }
            else {
                anymal_->setState(current_random_gc_init, current_random_gv_init);
                initHistory();
            }
        }
        else {
            anymal_->setState(gc_init_, gv_init_);
            initHistory();
        }

        updateObservation();
    }

    float step(const Eigen::Ref<EigenVec> &action) final
    {
        current_n_step += 1;

        /// action scaling
        pTarget12_ = action.cast<double>();
        pTarget12_ = pTarget12_.cwiseProduct(actionStd_);
        pTarget12_ += actionMean_;
        pTarget_.tail(nJoints_) = pTarget12_;

        Eigen::VectorXd current_joint_position_error = pTarget12_ - gc_.tail(nJoints_);
        updateHistory(current_joint_position_error, gv_.tail(nJoints_));

        anymal_->setPdTarget(pTarget_, vTarget_);

        for (int i = 0; i < int(control_dt_ / simulation_dt_ + 1e-10); i++)
        {
            if (server_)
                server_->lockVisualizationServerMutex();
            world_->integrate();
            if (server_)
                server_->unlockVisualizationServerMutex();
        }

        updateObservation();

        return 0.0;
    }

    void updateObservation()
    {
        static std::default_random_engine generator(random_seed);
        std::normal_distribution<> lidar_noise(0., 0.1);

        anymal_->getState(gc_, gv_);
        raisim::Vec<4> quat;
        raisim::Mat<3, 3> rot;
        quat[0] = gc_[3];
        quat[1] = gc_[4];
        quat[2] = gc_[5];
        quat[3] = gc_[6];
        raisim::quatToRotMat(quat, rot);
        bodyLinearVel_ = rot.e().transpose() * gv_.segment(0, 3);
        bodyAngularVel_ = rot.e().transpose() * gv_.segment(3, 3);

        /// Visualize base path
        if (current_n_step != 0 && current_n_step % 10 == 0)
            if (visualizable_ && visualize_path) {
                server_->getVisualObject("path" + std::to_string(current_n_step/10))->setPosition(gc_.segment(0, 3));
            }

        /// Get depth data
        raisim::Vec<3> lidarPos;
        raisim::Mat<3,3> lidarOri;
        anymal_->getFramePosition("lidar_cage_to_lidar", lidarPos);
        anymal_->getFrameOrientation("lidar_cage_to_lidar", lidarOri);
        double ray_length = 10.;
        Eigen::Vector3d direction;
        Eigen::Vector3d rayDirection;

        lidar_scan_depth.setOnes(scanSize);

        for (int i=0; i<scanSize; i++) {
            const double angle = - lidar_theta / 4 + delta_lidar_theta * i;
            direction = {cos(angle), sin(angle), 0};
            rayDirection = lidarOri.e() * direction;

            /// front lidar
            auto &col = world_->rayTest(lidarPos.e(), rayDirection, ray_length, true);
            if (col.size() > 0) {
                if (visualizable_)
                    scans[i]->setPosition(col[0].getPosition());
                double lidar_noise_distance = lidar_noise(generator);
                double current_lidar_distance = (lidarPos.e() - col[0].getPosition()).norm();
                lidar_scan_depth[i] = std::max(std::min(current_lidar_distance + lidar_noise_distance, ray_length), 0.) / ray_length;
            }
            else {
                if (visualizable_)
                    scans[i]->setPosition({0, 0, 100});
            }
        }

        /// transformed user command should be concatenated to the observation
        obDouble_ << rot.e().row(2).transpose(),      /// body orientation (dim=3)
                gc_.tail(12),                    /// joint angles (dim=12)
                bodyLinearVel_, bodyAngularVel_, /// body linear&angular velocity (dim=3+3=6)
                gv_.tail(12),                  /// joint velocity (dim=12)
                joint_position_error_history,    /// joint position error history (dim=24)
                joint_velocity_history,          /// joint velocity history (dim=24)
                lidar_scan_depth;                /// Lidar scan data (normalized)

        /// Update coordinate
        double yaw = atan2(rot.e().col(0)[1], rot.e().col(0)[0]);

        coordinateDouble << gc_[0], gc_[1], yaw;

    }

    void updateHistory(Eigen::VectorXd current_joint_position_error,
                       Eigen::VectorXd current_joint_velocity)
    {
        /// 0 ~ 11 : t-2 step
        /// 12 ~ 23 : t-1 step
        for (int i; i<n_history_steps-1; i++) {
            joint_position_error_history.segment(i * nJoints_, nJoints_) = joint_position_error_history.segment((i+1) * nJoints_, nJoints_);
            joint_velocity_history.segment(i * nJoints_, nJoints_) = joint_velocity_history.segment((i+1) * nJoints_, nJoints_);
        }
        joint_position_error_history.tail(nJoints_) = current_joint_position_error;
        joint_velocity_history.tail(nJoints_) = current_joint_velocity;
    }

    void initHistory()
    {
        joint_position_error_history.setZero(nJoints_ * n_history_steps);
        joint_velocity_history.setZero(nJoints_ * n_history_steps);
    }

    void observe(Eigen::Ref<EigenVec> ob) final
    {
        /// convert it to float
        /// 0 - 80 : proprioceptive sensor data
        /// 81 - : lidar sensor data
        ob = obDouble_.cast<float>();
    }

    void coordinate_observe(Eigen::Ref<EigenVec> coordinate)
    {
        /// convert it to float
        coordinate = coordinateDouble.cast<float>();
    }

    void calculate_cost() {}

    void comprehend_contacts() {}

    void visualize_desired_command_traj(Eigen::Ref<EigenRowMajorMat> coordinate_desired_command,
                                        Eigen::Ref<EigenVec> P_col_desired_command,
                                        double collision_threshold=0.99)
    {
        double threshold = collision_threshold;
        for (int i=0; i<n_prediction_step; i++) {
            if (P_col_desired_command[i] < threshold) {
                /// not collide
                desired_command_traj[i]->setColor(1, 1, 0, 1);  // yellow
            }
            else {
                /// collide
                desired_command_traj[i]->setColor(1, 0, 0, 1);  // red
            }
            const double coordinate_z = 0;
            desired_command_traj[i]->setPosition({coordinate_desired_command(i, 0), coordinate_desired_command(i, 1), coordinate_z});

            /// reset modified command trajectory
            modified_command_traj[i]->setPosition({0, 0, 100});
        }
    }

    void visualize_modified_command_traj(Eigen::Ref<EigenRowMajorMat> coordinate_modified_command,
                                         Eigen::Ref<EigenVec> P_col_modified_command,
                                         double collision_threshold=0.99)
    {
        double threshold = collision_threshold;
        for (int i=0; i<n_prediction_step; i++) {
            if (P_col_modified_command[i] < threshold) {
                /// not collide
                modified_command_traj[i]->setColor(0, 0, 1, 1);  // blue
            }
            else {
                /// collide
                modified_command_traj[i]->setColor(1, 0, 0, 1);  // red
            }
            const double coordinate_z = 0;
            modified_command_traj[i]->setPosition({coordinate_modified_command(i, 0), coordinate_modified_command(i, 1), coordinate_z});
        }
    }

    void set_user_command(Eigen::Ref<EigenVec> command) {}

    void reward_logging(Eigen::Ref<EigenVec> rewards, Eigen::Ref<EigenVec> rewards_w_coeff, int n_rewards) {}

    void noisify_Dynamics() {}

    void noisify_Mass_and_COM() {}

    void contact_logging(Eigen::Ref<EigenVec> contacts) {}

    void torque_and_velocity_logging(Eigen::Ref<EigenVec> torque_and_velocity) {}

    void set_goal(Eigen::Ref<EigenVec> goal_pos)
    {
        /// Only used when point goal initialize
        Eigen::VectorXd goal_pos_;
        goal_pos_.setZero(2);

        // Goal position
        for (int i=0; i<2; i++)
            goal_pos_[i] = goal_set[sampled_goal_set[current_n_goal]][i];

        goal_pos = goal_pos_.cast<float>();

        if (visualizable_)
            server_->getVisualObject("goal")->setPosition({goal_pos_[0], goal_pos_[1], 0.05});

        current_n_goal += 1;
    }

    void baseline_compute_reward(Eigen::Ref<EigenRowMajorMat> sampled_command,
                                 Eigen::Ref<EigenVec> goal_Pos_local,
                                 Eigen::Ref<EigenVec> rewards_p,
                                 Eigen::Ref<EigenVec> collision_idx,
                                 int steps, double delta_t, double must_safe_time)
    {
        int n_sample = sampled_command.rows();
        bool constant_command_sequence = true;
        if (sampled_command.cols() != 3)
            constant_command_sequence = false;
        raisim::Vec<3> future_coordinate;
        raisim::Vec<4> future_quaternion;
        Eigen::VectorXd rewards_cp;
        Eigen::VectorXd collision_idx_cp;
        int must_safe_n_steps = int(must_safe_time / delta_t);

        rewards_cp.setZero(n_sample);
        collision_idx_cp.setZero(n_sample);
        double current_goal_distance = goal_Pos_local.norm();

        if (server_)
            server_->lockVisualizationServerMutex();

        for (int i=0; i<n_sample; i++) {
            double local_x = 0.;
            double local_y = 0.;
            double local_yaw = 0.;
            double final_local_x = 0.;
            double final_local_y = 0.;
            bool not_collide = true;
            for (int j=0; j<steps; j++) {
                if (not_collide) {
                    if (constant_command_sequence) {
                        local_yaw += sampled_command(i, 2) * delta_t;
                        local_x += sampled_command(i, 0) * delta_t * cos(local_yaw) - sampled_command(i, 1) * delta_t * sin(local_yaw);
                        local_y += sampled_command(i, 0) * delta_t * sin(local_yaw) + sampled_command(i, 1) * delta_t * cos(local_yaw);
                    } else {
                        local_yaw += sampled_command(i, 3*j + 2) * delta_t;
                        local_x += sampled_command(i, 3*j) * delta_t * cos(local_yaw) - sampled_command(i, 3*j + 1) * delta_t * sin(local_yaw);
                        local_y += sampled_command(i, 3*j) * delta_t * sin(local_yaw) + sampled_command(i, 3*j + 1) * delta_t * cos(local_yaw);
                    }
                    future_coordinate[0] = local_x * cos(coordinateDouble[2]) - local_y * sin(coordinateDouble[2]) + coordinateDouble[0];
                    future_coordinate[1] = local_x * sin(coordinateDouble[2]) + local_y * cos(coordinateDouble[2]) + coordinateDouble[1];
                    future_coordinate[2] = local_yaw + coordinateDouble[2];

                    raisim::angleAxisToQuaternion({0, 0, 1}, future_coordinate[2], future_quaternion);

                    anymal_box_->setPosition(future_coordinate[0], future_coordinate[1], 0.5);
                    anymal_box_->setOrientation(future_quaternion);

                    world_->integrate1();

                    int num_anymal_future_contact = anymal_box_->getContacts().size();
                    if (num_anymal_future_contact > 0) {
                        not_collide = false;
                        if (j < must_safe_n_steps)
                            collision_idx_cp[i] = 1;
                    }

                    if (future_coordinate[0] < -hm_sizeX / 2 || hm_sizeX / 2 < future_coordinate[0] ||
                        future_coordinate[1] < -hm_sizeY / 2 || hm_sizeY / 2 < future_coordinate[1]) {
                        not_collide = false;
                        if (j < must_safe_n_steps)
                            collision_idx_cp[i] = 1;
                    }

                    final_local_x = local_x;
                    final_local_y = local_y;
                }

                double future_goal_distance = sqrt(pow(final_local_x - goal_Pos_local[0], 2) + pow(final_local_y - goal_Pos_local[1], 2));
                rewards_cp[i] += current_goal_distance - future_goal_distance;
            }
        }

        if (server_)
            server_->unlockVisualizationServerMutex();

        rewards_p = rewards_cp.cast<float>();
        collision_idx = collision_idx_cp.cast<float>();
    }

    void initialize_n_step()
    {
        current_n_step = 0;

        if (visualizable_ && visualize_path) {
            for (int i=0; i<max_num_path_slice; i++)
                server_->getVisualObject("path" + std::to_string(i+1))->setPosition({0, 0, 0});
        }
    }

    void computed_heading_direction(Eigen::Ref<EigenVec> heading_direction_) {}

    bool collision_check() {
        /// if the contact body is not feet, count as collision
        for (auto &contact : anymal_->getContacts()) {
            if (footIndices_.find(contact.getlocalBodyIndex()) == footIndices_.end()) {
                return true;
            }
        }
        return false;
    }

    bool isTerminalState(float &terminalReward) final
    {
        terminalReward = 0.f;
	    return collision_check();

        ///  if anymal falls down, count as failure
        //raisim::Vec<3> base_position;
        //anymal_->getFramePosition("base_to_base_inertia", base_position);
        //if (base_position[2] < 0.3)
        //    return true;

        //return false;
    }

    private:
    int gcDim_, gvDim_, nJoints_;
    bool visualizable_ = false;
    raisim::ArticulatedSystem *anymal_;
    raisim::Box *anymal_box_;
    Eigen::VectorXd gc_init_, gv_init_, gc_, gv_, pTarget_, pTarget12_, vTarget_;
    double terminalRewardCoeff_ = -10.;
    Eigen::VectorXd actionMean_, actionStd_, obDouble_;
    Eigen::Vector3d bodyLinearVel_, bodyAngularVel_;
    std::set<size_t> footIndices_;

    int n_history_steps = 2;
    Eigen::VectorXd joint_position_error_history, joint_velocity_history;

    /// Lidar
    int scanSize;
    double lidar_theta, delta_lidar_theta;
    std::vector<raisim::Visuals *> scans;
    Eigen::VectorXd lidar_scan_depth;

    /// Random data generator
    int random_seed = 0;

    /// Randon intialization & Random external force
    Eigen::VectorXd random_gc_init, random_gv_init, current_random_gc_init, current_random_gv_init;
    int current_n_step = 0;

    /// Heightmap
    double hm_sizeX, hm_sizeY;
    raisim::HeightMap* hm;

    /// Obstacle
    int n_obstacle = 0;
    double obstacle_dr = 0.5;
    std::vector<double> hm_raw_value = {};
    std::vector<raisim::Vec<2>> init_set = {};
    int n_init_set = 0;
    Eigen::MatrixXd obstacle_centers;

    /// Observation to be predicted
    Eigen::VectorXd coordinateDouble;

    /// Visualizing predicted trajectory
    int n_prediction_step = 12;   // Determined manually
    std::vector<raisim::Visuals *> desired_command_traj, modified_command_traj;

    /// Task specific initialization for evaluation
    bool point_goal_initialize= false, safe_control_initialize= false;
    raisim::Vec<2> point_goal_init;

    /// goal position
    std::vector<raisim::Vec<2>> goal_set = {};
    int n_goal_set;
    int current_n_goal = 0;
    std::vector<int> sampled_goal_set = {};

    /// use for visualizing path
    bool visualize_path= false;
    int max_num_path_slice=0;

    };
}
