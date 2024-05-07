/******************************************************************************
 *                                                                            *
 * Copyright (C) 2020 Fondazione Istituto Italiano di Tecnologia (IIT)        *
 * All Rights Reserved.                                                       *
 *                                                                            *
 ******************************************************************************/


#include "ExecuteDanceComponent.h"




bool ExecuteDanceComponent::start(int argc, char*argv[])
{
    if(!rclcpp::ok())
    {
        rclcpp::init(/*argc*/ argc, /*argv*/ argv);
    }
    // Ctp Service
    // calls the GetPartNames service
    auto getPartNamesClientNode = rclcpp::Node::make_shared("ExecuteDanceComponentGetPartNamesNode");
    std::shared_ptr<rclcpp::Client<dance_interfaces::srv::GetPartNames>> getPartNamesClient =
    getPartNamesClientNode->create_client<dance_interfaces::srv::GetPartNames>("/DanceComponent/GetPartNames");
    auto getPartNamesRequest = std::make_shared<dance_interfaces::srv::GetPartNames::Request>();
    while (!getPartNamesClient->wait_for_service(std::chrono::seconds(1))) {
        if (!rclcpp::ok()) {
            RCLCPP_ERROR(rclcpp::get_logger("rclcpp"), "Interrupted while waiting for the service 'getPartNamesClient'. Exiting.");
        }
    }
    auto getPartNamesResult = getPartNamesClient->async_send_request(getPartNamesRequest);
    auto futureGetPartNamesResult = rclcpp::spin_until_future_complete(getPartNamesClientNode, getPartNamesResult);
    auto ctpServiceParts = getPartNamesResult.get()->parts;
    if (!ctpServiceParts.empty())
    {
        for (std::string part : ctpServiceParts)
        {
            yarp::os::Port *ctpPort = new yarp::os::Port;
            std::string portName = "/ExecuteDanceComponentCtpServiceClient/" + part + "/rpc";
            bool b = ctpPort->open(portName);
            if (!b)
            {
                yError() << "Cannot open" << part << " ctpService port";
                return false;
            }
            m_pCtpService.insert({part, *ctpPort});
            yarp::os::Network::connect(portName, "/ctpservice/" + part + "/rpc");
        }
    }
    else
    {
        yWarning() << "Movement part names are empty. No movements will be executed!";
    }


    
    m_node = rclcpp::Node::make_shared("ExecuteDanceComponentNode");
    m_executeDanceService = m_node->create_service<execute_dance_interfaces::srv::ExecuteDance>("/ExecuteDanceComponent/ExecuteDance",  
                                                                                std::bind(&ExecuteDanceComponent::ExecuteDance,
                                                                                this,
                                                                                std::placeholders::_1,
                                                                                std::placeholders::_2));
    m_isDancingService = m_node->create_service<execute_dance_interfaces::srv::IsDancing>("/ExecuteDanceComponent/IsDancing",
                                                                                std::bind(&ExecuteDanceComponent::IsDancing,
                                                                                this,
                                                                                std::placeholders::_1,
                                                                                std::placeholders::_2));

    RCLCPP_DEBUG(m_node->get_logger(), "ExecuteDanceComponent::start");
    std::cout << "ExecuteDanceComponent::start";        
    return true;

}

bool ExecuteDanceComponent::close()
{
    for (auto port : m_pCtpService)
    {
        delete &port.second;
    }
    rclcpp::shutdown();  
    return true;
}

void ExecuteDanceComponent::spin()
{
    rclcpp::spin(m_node);  
}

void ExecuteDanceComponent::executeTask(const std::shared_ptr<execute_dance_interfaces::srv::ExecuteDance::Request> request)
{
    bool done_with_getting_dance = false;
    do {
        //calls the GetMovement service
        auto getMovementClientNode = rclcpp::Node::make_shared("ExecuteDanceComponentGetMovementNode");
        std::shared_ptr<rclcpp::Client<dance_interfaces::srv::GetMovement>> getMovementClient =
        getMovementClientNode->create_client<dance_interfaces::srv::GetMovement>("/DanceComponent/GetMovement");
        auto getMovementRequest = std::make_shared<dance_interfaces::srv::GetMovement::Request>();
        while(!getMovementClient->wait_for_service(std::chrono::seconds(1))) {
            if (!rclcpp::ok()) {
                RCLCPP_ERROR(rclcpp::get_logger("rclcpp"), "Interrupted while waiting for the service 'getMovementClient'. Exiting.");
            }
        }
        auto getMovementResult = getMovementClient->async_send_request(getMovementRequest);
        auto futureGetMovementResult = rclcpp::spin_until_future_complete(getMovementClientNode, getMovementResult);
        auto movement = getMovementResult.get();
        if (movement->is_ok == false) {
            RCLCPP_INFO_STREAM(rclcpp::get_logger("rclcpp"), "ExecuteDanceComponent::ExecuteDance. Movement not found, skipping...");
        } else {
            bool status = SendMovement(movement->time, movement->offset, movement->joints, m_pCtpService.at(movement->part_name));
            if (!status)
            {
                RCLCPP_INFO_STREAM(rclcpp::get_logger("rclcpp"), "Movement failed to sent. Is ctpService for part" << movement->part_name << "running?");
                continue;
            }            
        }
        //calls the UpdateMovement service
        auto updateMovementClientNode = rclcpp::Node::make_shared("ExecuteDanceComponentUpdateMovementNode");
        std::shared_ptr<rclcpp::Client<dance_interfaces::srv::UpdateMovement>> updateMovementClient =
        updateMovementClientNode->create_client<dance_interfaces::srv::UpdateMovement>("/DanceComponent/UpdateMovement");
        auto updateMovementRequest = std::make_shared<dance_interfaces::srv::UpdateMovement::Request>();
        while(!updateMovementClient->wait_for_service(std::chrono::seconds(1))) {
            if (!rclcpp::ok()) {
                RCLCPP_ERROR(rclcpp::get_logger("rclcpp"), "Interrupted while waiting for the service 'updateMovementClient'. Exiting.");
            }
        }
        auto updateMovementResult = updateMovementClient->async_send_request(updateMovementRequest);
        auto futureUpdateMovementResult = rclcpp::spin_until_future_complete(updateMovementClientNode, updateMovementResult);
        auto updateMovementResponse = updateMovementResult.get();
        if (updateMovementResponse->is_ok == false) {
            RCLCPP_INFO_STREAM(rclcpp::get_logger("rclcpp"), "ExecuteDanceComponent::ExecuteDance. Movement not found, skipping...");
        }
        else
        {
            RCLCPP_INFO_STREAM(rclcpp::get_logger("rclcpp"), "ExecuteDanceComponent::ExecuteDance. Movement updated correctly");
        }
        done_with_getting_dance = updateMovementResponse->done_with_dance;
        std::cout << "done_with_getting_dance: " << done_with_getting_dance << std::endl;
    } while (!done_with_getting_dance);  
}

void ExecuteDanceComponent::ExecuteDance(const std::shared_ptr<execute_dance_interfaces::srv::ExecuteDance::Request> request,
             std::shared_ptr<execute_dance_interfaces::srv::ExecuteDance::Response>      response) 
{
    // calls the SetDance service
    auto setDanceClientNode = rclcpp::Node::make_shared("ExecuteDanceComponentSetDanceNode");
    std::shared_ptr<rclcpp::Client<dance_interfaces::srv::SetDance>> setDanceClient =
    setDanceClientNode->create_client<dance_interfaces::srv::SetDance>("/DanceComponent/SetDance");
    auto setDanceRequest = std::make_shared<dance_interfaces::srv::SetDance::Request>();
    setDanceRequest->dance = request->dance_name;
    while (!setDanceClient->wait_for_service(std::chrono::seconds(1))) {
        if (!rclcpp::ok()) {
            RCLCPP_ERROR(rclcpp::get_logger("rclcpp"), "Interrupted while waiting for the service 'setDanceClient'. Exiting.");
        }
    }
    auto setDanceResult = setDanceClient->async_send_request(setDanceRequest);
    auto futureSetDanceResult = rclcpp::spin_until_future_complete(setDanceClientNode, setDanceResult);
    auto setDanceResponse = setDanceResult.get();
    if (setDanceResponse->is_ok != true) {
        RCLCPP_INFO_STREAM(rclcpp::get_logger("rclcpp"), "ExecuteDanceComponent::ExecuteDance. Dance: " << request->dance_name);
        response->is_ok = false;
        response->error_msg = "Dance not found";
        return;
    }
    RCLCPP_INFO_STREAM(rclcpp::get_logger("rclcpp"), "ExecuteDanceComponent::ExecuteDance. Dance set correctly: " << request->dance_name);
    //create new thread with lambda function

    if (m_threadExecute.joinable()) {
        m_threadExecute.join();
    }
    m_threadExecute = std::thread([this, request]() { executeTask(request); });
    response->is_ok = true;
}


// float TourManager::DoDance(const std::string danceName)
// {
//     bool status;
//     for (Movement currentMove : currentDance.GetMovements())
//     {
//         if (!m_moveStorage->GetMovementsContainer().GetPartNames().count(currentMove.GetPartName()))
//         {
//             yWarning() << "Part" << currentMove.GetPartName() << "not supported. Skipping...";
//             continue;
//         }
//         yarp::os::Port &port = m_pCtpService.at(currentMove.GetPartName());
//         status = SendMovement(currentMove.GetTime(), currentMove.GetOffset(), currentMove.GetJoints(), port);
//         if (!status)
//         {
//             yError() << "Movement failed to sent. Is ctpService for part" << currentMove.GetPartName() << "running?";
//             continue;
//         }
//     }
//     yDebug() << "I danced:" << danceName << "with duration:" << currentDance.GetDuration();
//     return currentDance.GetDuration();
// }


void ExecuteDanceComponent::IsDancing(const std::shared_ptr<execute_dance_interfaces::srv::IsDancing::Request> request,
             std::shared_ptr<execute_dance_interfaces::srv::IsDancing::Response>      response) 
{
    response->is_dancing = true;
}


bool ExecuteDanceComponent::SendMovement(float time, int offset, std::vector<float> joints, yarp::os::Port &port)
{
    yarp::os::Bottle res;
    yarp::os::Bottle cmd;
    cmd.addVocab32("ctpq");
    cmd.addVocab32("time");
    cmd.addFloat64(time);
    cmd.addVocab32("off");
    cmd.addFloat64(offset);
    cmd.addVocab32("pos");
    yarp::os::Bottle &list = cmd.addList();
    for (auto joint : joints)
    {
        list.addFloat64(joint);
    }
    return port.write(cmd, res);
}
