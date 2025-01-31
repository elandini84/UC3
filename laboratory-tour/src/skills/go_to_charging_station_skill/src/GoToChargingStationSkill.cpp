/******************************************************************************
 *                                                                            *
 * Copyright (C) 2020 Fondazione Istituto Italiano di Tecnologia (IIT)        *
 * All Rights Reserved.                                                       *
 *                                                                            *
 ******************************************************************************/

#include "GoToChargingStationSkill.h"

#include <QTimer>
#include <QDebug>
#include <QTime>
#include <iostream>
#include <QStateMachine>

GoToChargingStationSkill::GoToChargingStationSkill(std::string name ) :
        m_name(std::move(name))
{
    m_dataModel.set_name(m_name);
    m_stateMachine.setDataModel(&m_dataModel);
}


void GoToChargingStationSkill::spin(std::shared_ptr<rclcpp::Node> node)
{
    rclcpp::spin(node);  
    rclcpp::shutdown();  
}


bool GoToChargingStationSkill::start(int argc, char*argv[])
{

    if(!rclcpp::ok())
    {
        rclcpp::init(/*argc*/ argc, /*argv*/ argv);
    }

    m_node = rclcpp::Node::make_shared(m_name + "Skill");

    
    RCLCPP_DEBUG_STREAM(m_node->get_logger(), "GoToChargingStationSkill::start");
    std::cout << "GoToChargingStationSkill::start";
    m_tickService = m_node->create_service<bt_interfaces::srv::TickCondition>(m_name + "Skill/tick",  
                                                                                std::bind(&GoToChargingStationSkill::tick,
                                                                                this,
                                                                                std::placeholders::_1,
                                                                                std::placeholders::_2));

    m_stateMachine.start();
    m_threadSpin = std::make_shared<std::thread>(spin, m_node);
    return true;
}

void GoToChargingStationSkill::tick( [[maybe_unused]] const std::shared_ptr<bt_interfaces::srv::TickCondition::Request> request,
                                       std::shared_ptr<bt_interfaces::srv::TickCondition::Response>      response)
{
    std::lock_guard<std::mutex> lock(m_requestMutex);
    RCLCPP_INFO_STREAM(m_node->get_logger(), m_name << "::tick");    
    std::cout <<  m_name << "::tick";  
    bool done = false; 
    m_stateMachine.submitEvent("CMD_START");
    std::this_thread::sleep_for (std::chrono::milliseconds(200));

    auto message = bt_interfaces::msg::ConditionResponse();
    do {
        for (const auto& state : m_stateMachine.activeStateNames()) {
            RCLCPP_INFO_STREAM(m_node->get_logger(), m_name << "::tick, state " << state.toStdString() );    
            if (state == "success") {
                m_stateMachine.submitEvent("CMD_OK");
                response->status.status = message.SKILL_RUNNING;
                response->is_ok = true;
                done = true;
            }else if (state == "failure") {
                m_stateMachine.submitEvent("CMD_OK");
                response->status.status = message.SKILL_FAILURE;
                response->is_ok = true;
                done = true;
            }
        }
        std::this_thread::sleep_for (std::chrono::milliseconds(200));

    } while (!done);
}


void GoToChargingStationSkill::halt( [[maybe_unused]] const std::shared_ptr<bt_interfaces::srv::HaltAction::Request> request,
                                       std::shared_ptr<bt_interfaces::srv::HaltAction::Response>      response)
{
    std::lock_guard<std::mutex> lock(m_requestMutex);
    RCLCPP_INFO_STREAM(m_node->get_logger(), m_name << "::tick");    
    std::cout <<  m_name << "::tick";  
    bool done = false; 
    m_stateMachine.submitEvent("CMD_HALT");
    std::this_thread::sleep_for (std::chrono::milliseconds(200));
    bool halted = false;

    do {
        for (const auto& state : m_stateMachine.activeStateNames()) {
            RCLCPP_DEBUG_STREAM(m_node->get_logger(), state.toStdString());
            if (state == "idle") {
                halted = true;
            }
        }
        std::this_thread::sleep_for (std::chrono::milliseconds(200));
    } while (!halted);
}
