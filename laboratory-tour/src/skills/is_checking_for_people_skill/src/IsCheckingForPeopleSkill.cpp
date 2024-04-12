/******************************************************************************
 *                                                                            *
 * Copyright (C) 2020 Fondazione Istituto Italiano di Tecnologia (IIT)        *
 * All Rights Reserved.                                                       *
 *                                                                            *
 ******************************************************************************/

#include "IsCheckingForPeopleSkill.h"
#include <future>
#include <QTimer>
#include <QDebug>
#include <QTime>
#include <iostream>
#include <QStateMachine>

#include <type_traits>

template<typename T>
T convert(const std::string& str) {
    if constexpr (std::is_same_v<T, int>) {
        return std::stoi(str);
    } else if constexpr (std::is_same_v<T, double>) {
        return std::stod(str);
    } else if constexpr (std::is_same_v<T, float>) {
        return std::stof(str);
    } 
    else if constexpr (std::is_same_v<T, std::string>) {
        return str;
    }
    else {
        // Handle unsupported types
        throw std::invalid_argument("Unsupported type conversion");
    }
}

IsCheckingForPeopleSkill::IsCheckingForPeopleSkill(std::string name ) :
		m_name(std::move(name))
{
}

void IsCheckingForPeopleSkill::spin(std::shared_ptr<rclcpp::Node> node)
{
	rclcpp::spin(node);
	rclcpp::shutdown();
}

bool IsCheckingForPeopleSkill::start(int argc, char*argv[])
{
	if(!rclcpp::ok())
	{
		rclcpp::init(/*argc*/ argc, /*argv*/ argv);
	}

	m_node = rclcpp::Node::make_shared(m_name + "Skill");
	RCLCPP_DEBUG_STREAM(m_node->get_logger(), "IsCheckingForPeopleSkill::start");
	std::cout << "IsCheckingForPeopleSkill::start";

	m_tickService = m_node->create_service<bt_interfaces::srv::TickCondition>(m_name + "Skill/tick",
                                                                           	std::bind(&IsCheckingForPeopleSkill::tick,
                                                                           	this,
                                                                           	std::placeholders::_1,
                                                                           	std::placeholders::_2));

    m_stateMachine.connectToEvent("BlackboardComponent.GetInt.Call", [this]([[maybe_unused]]const QScxmlEvent & event){
        std::shared_ptr<rclcpp::Node> nodeGetInt = rclcpp::Node::make_shared(m_name + "SkillNodeGetInt");
        std::shared_ptr<rclcpp::Client<blackboard_interfaces::srv::GetIntBlackboard>> clientGetInt = nodeGetInt->create_client<blackboard_interfaces::srv::GetIntBlackboard>("/BlackboardComponent/GetInt");
        auto request = std::make_shared<blackboard_interfaces::srv::GetIntBlackboard::Request>();
        auto eventParams = event.data().toMap();
        request->field_name = convert<decltype(request->field_name)>(eventParams["field_name"].toString().toStdString());
        bool wait_succeded{true};
        while (!clientGetInt->wait_for_service(std::chrono::seconds(1))) {
            if (!rclcpp::ok()) {
                RCLCPP_ERROR(rclcpp::get_logger("rclcpp"), "Interrupted while waiting for the service 'GetInt'. Exiting.");
                wait_succeded = false;
                m_stateMachine.submitEvent("BlackboardComponent.GetInt.Return");
            } 
        }
        if (wait_succeded) {
            // send the request                                                                    
            auto result = clientGetInt->async_send_request(request);
            auto futureResult = rclcpp::spin_until_future_complete(nodeGetInt, result);
            auto response = result.get();
            if (futureResult == rclcpp::FutureReturnCode::SUCCESS) 
            {
                if( response->is_ok ==true) {
                    QVariantMap data;
                    data.insert("result", "SUCCESS");
                    data.insert("value", response->value);
                    m_stateMachine.submitEvent("BlackboardComponent.GetInt.Return", data);
                    RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "BlackboardComponent.GetInt.Return");
                } else {
                    QVariantMap data;
                    data.insert("result", "FAILURE");
                    m_stateMachine.submitEvent("BlackboardComponent.GetInt.Return", data);
                    RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "BlackboardComponent.GetInt.Return");
                }
            }
        }
    });

	m_stateMachine.connectToEvent("TICK_RESPONSE", [this]([[maybe_unused]]const QScxmlEvent & event){
		RCLCPP_INFO(m_node->get_logger(), "IsCheckingForPeopleSkill::tickReturn %s", event.data().toMap()["result"].toString().toStdString().c_str());
		std::string result = event.data().toMap()["result"].toString().toStdString();
		if (result == "SUCCESS" )
		{
			m_tickResult.store(Status::success);
		}
		else if (result == "FAILURE" )
		{ 
			m_tickResult.store(Status::failure);
		}
	});

	m_stateMachine.start();
	m_threadSpin = std::make_shared<std::thread>(spin, m_node);

	return true;
}

void IsCheckingForPeopleSkill::tick( [[maybe_unused]] const std::shared_ptr<bt_interfaces::srv::TickCondition::Request> request,
                                std::shared_ptr<bt_interfaces::srv::TickCondition::Response>      response)
{
    std::lock_guard<std::mutex> lock(m_requestMutex);
    RCLCPP_INFO(m_node->get_logger(), "IsCheckingForPeopleSkill::tick");
    auto message = bt_interfaces::msg::ConditionResponse();
    m_tickResult.store(Status::undefined); //here we can put a struct
    m_stateMachine.submitEvent("CMD_TICK");
   
    while(m_tickResult.load()== Status::undefined) 
    {
        std::this_thread::sleep_for (std::chrono::milliseconds(100));
        // qInfo() <<  "active names" << m_stateMachine.activeStateNames();
    }
    switch(m_tickResult.load()) 
    {
        case Status::failure:
            response->status.status = message.SKILL_FAILURE;
            break;
        case Status::success:
            response->status.status = message.SKILL_SUCCESS;
            break;            
    }
    RCLCPP_INFO(m_node->get_logger(), "IsCheckingForPeopleSkill::tickDone");
   
    response->is_ok = true;
}

