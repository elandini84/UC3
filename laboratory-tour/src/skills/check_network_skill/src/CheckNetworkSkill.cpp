/******************************************************************************
 *                                                                            *
 * Copyright (C) 2020 Fondazione Istituto Italiano di Tecnologia (IIT)        *
 * All Rights Reserved.                                                       *
 *                                                                            *
 ******************************************************************************/

#include "CheckNetworkSkill.h"

#include <QTimer>
#include <QDebug>
#include <QTime>
#include <iostream>
#include <QStateMachine>

CheckNetworkSkill::CheckNetworkSkill(std::string name ) :
        m_name(std::move(name))
{
    m_dataModel.set_name(name+"dataModel");
    m_stateMachine.setDataModel(&m_dataModel);
}


void CheckNetworkSkill::spin(std::shared_ptr<rclcpp::Node> node)
{
    rclcpp::spin(node);  
    rclcpp::shutdown();  
}


bool CheckNetworkSkill::start(int argc, char*argv[])
{

    if(!rclcpp::ok())
    {
        rclcpp::init(/*argc*/ argc, /*argv*/ argv);
    }

    m_node = rclcpp::Node::make_shared(m_name);

    
    RCLCPP_DEBUG(m_node->get_logger(), "CheckNetworkSkill::start");
    std::cout << "CheckNetworkSkill::start";
    m_tickService = m_node->create_service<bt_interfaces::srv::TickCondition>(m_name + "Skill/tick",  
                                                                                std::bind(&CheckNetworkSkill::tick,
                                                                                this,
                                                                                std::placeholders::_1,
                                                                                std::placeholders::_2));

    m_stateMachine.start();
    m_threadSpin = std::make_shared<std::thread>(spin, m_node);
    return true;
}


void CheckNetworkSkill::tick( [[maybe_unused]] const std::shared_ptr<bt_interfaces::srv::TickCondition::Request> request,
                                       std::shared_ptr<bt_interfaces::srv::TickCondition::Response>      response)
{
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
                response->status.status = message.SKILL_SUCCESS;
                response->is_ok = true;
                done = true;
            }else if (state == "failure") {
                m_stateMachine.submitEvent("CMD_OK");
                response->status.status = message.SKILL_FAILURE;
                response->is_ok = true;
                done = true;
            }
        }
    } while (!done);
}
