#include "DialogComponent.hpp"

#include <fstream>
#include <iostream>
#include <random>

using namespace std::chrono_literals;

DialogComponent::DialogComponent()
{
    m_speechTranscriberClientName = "/DialogComponent/speechTranscriberClient:i";
    m_speechTranscriberServerName = "/speechTranscriberServer:o";
    m_tourLoadedAtStart = false;
    m_currentPoiName = "";
}

bool DialogComponent::ConfigureYARP(yarp::os::ResourceFinder &rf)
{
    // -------------------------Port init----------------------------------------------
    bool okCheck = rf.check("DIALOGCOMPONENT");
    if (okCheck)
    {
        yarp::os::Searchable &component_config = rf.findGroup("DIALOGCOMPONENT");
        if (component_config.check("local-suffix"))
        {
            m_speechTranscriberClientName = "/DialogComponent" + component_config.find("local-suffix").asString();
        }
        if (component_config.check("remote-port"))
        {
            m_speechTranscriberServerName = component_config.find("remote-port").asString();
        }

        m_speechTranscriberPort.useCallback(m_speechTranscriberCallback);
        m_speechTranscriberPort.open(m_speechTranscriberClientName);
        // Try Automatic port connection
        if(! yarp::os::Network::connect(m_speechTranscriberServerName, m_speechTranscriberClientName))
        {
            yWarning() << "[DialogComponent::start] Unable to connect to: " << m_speechTranscriberServerName;
        }
    }

    // -------------------------Speech Synthesizer nwc---------------------------------
    std::string device = "speechSynthesizer_nwc_yarp";
    std::string local = "/DialogComponent/speechClient";
    std::string remote = "/speechSynthesizer/speechServer";
    {
        okCheck = rf.check("SPEECHSYNTHESIZER-CLIENT");
        if (okCheck)
        {
            yarp::os::Searchable &speech_config = rf.findGroup("SPEECHSYNTHESIZER-CLIENT");
            if (speech_config.check("device"))
            {
                device = speech_config.find("device").asString();
            }
            if (speech_config.check("local-suffix"))
            {
                local = "/DialogComponent" + speech_config.find("local-suffix").asString();
            }
            if (speech_config.check("remote"))
            {
                remote = speech_config.find("remote").asString();
            }
        }

        yarp::os::Property prop;
        prop.put("device", device);
        prop.put("local", local);
        prop.put("remote", remote);

        m_speechSynthPoly.open(prop);
        if (!m_speechSynthPoly.isValid())
        {
            yError() << "[DialogComponent::ConfigureYARP] Error opening speech synthesizer Client PolyDriver. Check parameters";
            return false;
        }
        m_speechSynthPoly.view(m_iSpeechSynth);
        if (!m_iSpeechSynth)
        {
            yError() << "[DialogComponent::ConfigureYARP] Error opening iSpeechSynth interface. Device not available";
            return false;
        }

        //Try automatic yarp port Connection
        if(! yarp::os::Network::connect(remote, local))
        {
            yWarning() << "[DialogComponent::ConfigureYARP] Unable to connect: " << local << " to: " << remote;
        }
    }
    // -------------------------Dialog Flow chatBot nwc---------------------------------
    {
        okCheck = rf.check("CHATBOT-CLIENT");
        device = "chatBot_nws_yarp";
        local = "/DialogComponent/chatBotClient";
        remote = "/dialogFlow/portname";

        if (okCheck)
        {
            yarp::os::Searchable &chatBot_config = rf.findGroup("CHATBOT-CLIENT");
            if (chatBot_config.check("device"))
            {
                device = chatBot_config.find("device").asString();
            }
            if (chatBot_config.check("local-suffix"))
            {
                local = "/DialogComponent" + chatBot_config.find("local-suffix").asString();
            }
            if (chatBot_config.check("remote"))
            {
                remote = chatBot_config.find("remote").asString();
            }
        }

        yarp::os::Property chatbot_prop;
        chatbot_prop.put("device", device);
        chatbot_prop.put("local", local);
        chatbot_prop.put("remote", remote);

        m_chatBotPoly.open(chatbot_prop);
        if (!m_chatBotPoly.isValid())
        {
            yError() << "[DialogComponent::ConfigureYARP] Error opening chatBot Client PolyDriver. Check parameters";
            return false;
        }
        m_chatBotPoly.view(m_iChatBot);
        if (!m_iChatBot)
        {
            yError() << "[DialogComponent::ConfigureYARP] Error opening iChatBot interface. Device not available";
            return false;
        }

        //Try automatic yarp port Connection
        if(! yarp::os::Network::connect(remote, local))
        {
            yWarning() << "[DialogComponent::ConfigureYARP] Unable to connect: " << local << " to: " << remote;
        }
    }
    // ---------------------Microphone Activation----------------------------
    {
        okCheck = rf.check("AUDIORECORDER-CLIENT");
        device = "audioRecorder_nwc_yarp";
        local = "/DialogComponent/audio:i";
        remote = "/audioRecorder/audio:o";

        if (okCheck)
        {
            yarp::os::Searchable &mic_config = rf.findGroup("AUDIORECORDER-CLIENT");
            if (mic_config.check("device"))
            {
                device = mic_config.find("device").asString();
            }
            if (mic_config.check("local-suffix"))
            {
                local = "/DialogComponent" + mic_config.find("local-suffix").asString();
            }
            if (mic_config.check("remote"))
            {
                remote = mic_config.find("remote").asString();
            }
        }

        yarp::os::Property audioRecorder_prop;
        audioRecorder_prop.put("device", device);
        audioRecorder_prop.put("local", local);
        audioRecorder_prop.put("remote", remote);

        m_audioRecorderPoly.open(audioRecorder_prop);
        if (!m_audioRecorderPoly.isValid())
        {
            yError() << "[DialogComponent::ConfigureYARP] Error opening audioRecorder Client PolyDriver. Check parameters";
            return false;
        }
        m_audioRecorderPoly.view(m_iAudioGrabberSound);
        if (!m_iAudioGrabberSound)
        {
            yError() << "[DialogComponent::ConfigureYARP] Error opening audioRecorderSound interface. Device not available";
            return false;
        }

        //Try automatic yarp port Connection
        if(! yarp::os::Network::connect(remote, local))
        {
            yWarning() << "[DialogComponent::ConfigureYARP] Unable to connect: " << local << " to: " << remote;
        }
    }

    // TOUR MANAGER
    {
        if (! m_tourLoadedAtStart)
        {
            okCheck = rf.check("TOUR-MANAGER");
            if (okCheck)
            {
                yarp::os::Searchable &tour_config = rf.findGroup("TOUR-MANAGER");
                if (tour_config.check("path"))
                {
                    m_jsonPath = tour_config.find("path").asString();
                }
                if (tour_config.check("tour_name"))
                {
                    m_tourName = tour_config.find("tour_name").asString();
                }
                
                m_tourStorage = std::make_shared<TourStorage>(); 
                if( !m_tourStorage->LoadTour(m_jsonPath, m_tourName))
                {
                    yError() << "[DialogComponent::ConfigureYARP] Unable to load tour from the given arguments: " << m_jsonPath << " and: " << m_tourName;
                    return false;
                }
            }
        }
    }

    // SPEAKERS
    {
        okCheck = rf.check("SPEAKERS");
        if (okCheck)
        {
            yarp::os::Searchable &speakersConfig = rf.findGroup("SPEAKERS");
            std::string localName = "/DialogComponent/audioOut";
            std::string remoteName = "/audioPlayerWrapper";
            if (speakersConfig.check("remote"))
            {
                remoteName = speakersConfig.find("remote").asString();
            }
            if (speakersConfig.check("localName"))
            {
                localName = speakersConfig.find("localName").asString();
            }

            m_speakersAudioPort.open(localName);
            if (!yarp::os::Network::connect(remoteName, localName))
            {
                yWarning() << "[DialogComponent::ConfigureYARP] Unable to connect port: " << remoteName << " with: " << localName;
            }
        }
    }

    yInfo() << "[DialogComponent::ConfigureYARP] Successfully configured component";
    return true;
}

bool DialogComponent::start(int argc, char*argv[])
{
    // Loads the tour json from the file and saves a reference to the class, if the arguments are being passed at start.
    if (argc >= 2)
    {
        m_tourStorage = std::make_shared<TourStorage>(); 
        if( !m_tourStorage->LoadTour(argv[0], argv[1]))
        {
            yError() << "[DialogComponent::start] Unable to load tour from the given arguments: " << argv[0] << " and " << argv[1];
            return false;
        }
        m_tourLoadedAtStart = true;
    }

    if(!rclcpp::ok())
    {
        rclcpp::init(/*argc*/ argc, /*argv*/ argv);
    }
    m_node = rclcpp::Node::make_shared("DialogComponentNode");

    m_setLanguageService = m_node->create_service<dialog_interfaces::srv::SetLanguage>("/DialogComponent/SetLanguage",
                                                                                        std::bind(&DialogComponent::SetLanguage,
                                                                                                this,
                                                                                                std::placeholders::_1,
                                                                                                std::placeholders::_2));
    m_getLanguageService = m_node->create_service<dialog_interfaces::srv::GetLanguage>("/DialogComponent/GetLanguage",
                                                                                        std::bind(&DialogComponent::GetLanguage,
                                                                                                this,
                                                                                                std::placeholders::_1,
                                                                                                std::placeholders::_2));
    m_enableDialogService = m_node->create_service<dialog_interfaces::srv::EnableDialog>("/DialogComponent/EnableDialog",
                                                                                        std::bind(&DialogComponent::EnableDialog,
                                                                                                this,
                                                                                                std::placeholders::_1,
                                                                                                std::placeholders::_2));
    
    RCLCPP_INFO(m_node->get_logger(), "Started node");
    return true;
}

bool DialogComponent::close()
{
    m_speechTranscriberPort.close();
    m_speakersAudioPort.close();
    if (m_dialogThread.joinable())
    {
        m_dialogThread.join();
    }
    
    rclcpp::shutdown();  
    return true;
}

void DialogComponent::spin()
{
    rclcpp::spin(m_node);  
}

void DialogComponent::EnableDialog(const std::shared_ptr<dialog_interfaces::srv::EnableDialog::Request> request,
                        std::shared_ptr<dialog_interfaces::srv::EnableDialog::Response> response)
{
    if (request->enable)
    {
        // Enable mic
        bool recording = false;
        m_iAudioGrabberSound->isRecording(recording);
        if (!recording)
        {
            if (! m_iAudioGrabberSound->startRecording())
            {
                yError() << "[DialogComponent::EnableDialog] Unable to start recording of the mic";
                response->is_ok=false;
                return;
            }
        }

        // Launch thread that periodically reads the callback from the port and manages the dialog
        if (m_dialogThread.joinable())
        {
            m_dialogThread.join();
        }
        m_dialogThread = std::thread(&DialogComponent::DialogExecution, this);

        response->is_ok=true;
    }
    else
    {
        // Disable mic
        bool recording = false;
        m_iAudioGrabberSound->isRecording(recording);
        if (recording)
        {
            if (! m_iAudioGrabberSound->stopRecording())
            {
                yError() << "[DialogComponent::EnableDialog] Unable to stop recording of the mic";
                response->is_ok=false;
                return; //should we still go on? TODO
            }
        }

        // TODO kill thread and stop the speaking
        if (m_dialogThread.joinable())
        {
            m_dialogThread.join();
        }
        
        response->is_ok=true;
    }
    
}

void DialogComponent::DialogExecution()
{
    std::chrono::duration wait_ms = 200ms;
    while (true)
    {
        // Check if new message has been transcribed
        std::string questionText = "";
        if (m_speechTranscriberCallback.hasNewMessage())
        {
            if (!m_speechTranscriberCallback.getText(questionText))
            {
                std::this_thread::sleep_for(wait_ms);
                continue;
            }
        }
        else
        {
            std::this_thread::sleep_for(wait_ms);
            continue;
        }

        // Get the poi object from the Tour manager
        PoI currentPoi;
        if(!m_tourStorage->GetTour().getPoI(m_currentPoiName, currentPoi))
        {
            yError() << "[DialogComponent::DialogExecution] Unable to get the current PoI name: " << m_currentPoiName;
            std::this_thread::sleep_for(wait_ms);
            continue;
        }

        // Pass it to DialogFlow
        std::string answerText = "";
        if(!m_iChatBot->interact(questionText, answerText))
        {
            yError() << "[DialogComponent::DialogExecution] Unable to interact with chatBot with question: " << questionText;
            std::this_thread::sleep_for(wait_ms);
            continue;
        }

        // Get the Action to perform from the PoI
        std::vector<Action> actionList;
        if(!currentPoi.getActions("speak", actionList))
        {
            yError() << "[DialogComponent::DialogExecution] Unable to get the the Action List of the current PoI: " << m_currentPoiName;
            std::this_thread::sleep_for(wait_ms);
            continue;
        }
        std::string text = actionList.at(0).getParam();

        // Synthetise the answer text
        yarp::sig::Sound synthesizedSound;
        if (!m_iSpeechSynth->synthesize(answerText, synthesizedSound))
        {
            yError() << "[DialogComponent::DialogExecution] Unable to synthesize text: " << answerText;
            std::this_thread::sleep_for(wait_ms);
            continue;
        }

        // Pass the sound to the speaker -> Do I have to shut down also the mic ?? TODO
        m_speakersAudioPort.prepare() = synthesizedSound;
        //auto & buffer = m_speakersAudioPort.prepare();
        //buffer.clear();
        //buffer = synthesizedSound;
        m_speakersAudioPort.write();
        //std::this_thread::sleep_for(wait_ms);
    }
    return;
}

bool DialogComponent::InterpretCommand(const std::string &command, PoI currentPoI, PoI genericPoI, std::string & phrase)
{
    bool isOk = false;
    std::vector<Action> actions;
    std::string cmd;

    bool isCurrent = currentPoI.isCommandValid(command);
    bool isGeneric = genericPoI.isCommandValid(command);

    if (isCurrent || isGeneric) // If the command is available either in the current PoI or the generic ones
    {
        int cmd_multiples;
        if (isCurrent) // If it is in the current overwrite the generic
        {
            cmd_multiples = currentPoI.getCommandMultiplesNum(command);
        }
        else
        {
            cmd_multiples = genericPoI.getCommandMultiplesNum(command);
        }

        // could be removed
        if (cmd_multiples > 1)
        {
            std::uniform_int_distribution<std::mt19937::result_type> uniform_distrib;
            uniform_distrib.param(std::uniform_int_distribution<std::mt19937::result_type>::param_type(1, cmd_multiples));
            std::mt19937 random_gen;
            int index = uniform_distrib(random_gen) - 1;

            cmd = command;
            if (index != 0)
            {
                cmd = cmd.append(std::to_string(index));
            }
        }
        else // The is only 1 command. It cannot be 0 because we checked if the command is available at the beginning
        {
            cmd = command;
        }

        if (isCurrent)
        {
            isOk = currentPoI.getActions(cmd, actions);
        }
        else
        {
            isOk = genericPoI.getActions(cmd, actions);
        }
    }
    else // Command is not available anywhere, return error and skip
    {
        yWarning() << "Command: " << command << " , not supported in either the PoI or the generics list. Skipping...";
    }

    if (isOk && !actions.empty())
    {
        int actionIndex = 0;
        bool isCommandBlocking = true;
        Action lastNonSignalAction;

        while (actionIndex < actions.size())
        {
            std::vector<Action> tempActions;
            for (int i = actionIndex; i < actions.size(); i++)
            {
                tempActions.push_back(actions[i]);
                if (actions[i].getType() != ActionTypes::SIGNAL)
                {
                    lastNonSignalAction = actions[i];
                }
                if (actions[i].isBlocking())
                {
                    actionIndex = i + 1;
                    break;
                }
                else
                {
                    if (i == actions.size() - 1)
                    {
                        actionIndex = actions.size();
                        if (!lastNonSignalAction.isBlocking())
                        {
                            isCommandBlocking = false;
                        }
                    }
                }
            }

            bool containsSpeak = false;

            for (Action action : tempActions) // Loops through all the actions until the blocking one. Execute all of them
            {
                switch (action.getType())
                {
                    case ActionTypes::SPEAK:
                    {
                        // Speak, but make it invalid if it is a fallback or it is an error message
                        //Speak(action.getParam(), (cmd != "fallback" && cmd.find("Error") == std::string::npos));
                        phrase = action.getParam();
                        containsSpeak = true;
                        return true;    // we are interested into phrase
                        break;
                    }
                    case ActionTypes::INVALID:
                    {
                        yError() << "I got an INVALID ActionType in command" << command;
                        break;
                    }
                    default:
                    {
                        yError() << "I got an unknown ActionType: " << command;
                        break;
                    }
                }
            }
            return false;

            //if (containsSpeak && isCommandBlocking) // Waits for the longest move in the temp list of blocked moves and speak. If there is nothing in the temp list because we are not blocking it is skipped.
            //{
            //    m_
            //    while (containsSpeak && !m_headSynchronizer.isSpeaking())
            //    {
            //        yarp::os::Time::delay(0.1);
            //    }
            //    while (m_headSynchronizer.isSpeaking())
            //    {
            //        yarp::os::Time::delay(0.1);
            //    }
            //}
        }

        //if (cmd == "fallback")
        //{
        //    m_fallback_repeat_counter++;
        //    if (m_fallback_repeat_counter == m_fallback_threshold)
        //    { // If the same command has been received as many times as the threshold, then repeat the question.
        //        Speak(m_last_valid_speak, true);
        //        BlockSpeak();
        //        m_fallback_repeat_counter = 0;
        //    }
        //    Signal("startHearing"); // Open the ears after we handled the fallback to get a response.
        //}
        //else
        //{
        //    m_fallback_repeat_counter = 0;
        //}
        //return true;
    }
    return false;
}

void DialogComponent::SetLanguage(const std::shared_ptr<dialog_interfaces::srv::SetLanguage::Request> request,
                        std::shared_ptr<dialog_interfaces::srv::SetLanguage::Response> response)
{
    if (request->new_language=="")
    {
        response->is_ok=false;
        response->error_msg="Empty string passed to setting language";
        return;
    }

    if (!m_iSpeechSynth->setLanguage(request->new_language))
    {
        response->is_ok=false;
        response->error_msg="Unable to set new language to speech Synth";
        return;
    }
    if (m_iChatBot->setLanguage(request->new_language))
    {
        response->is_ok=false;
        response->error_msg="Unable to set new language";
        return;
    }
    // TODO TourManagerStorage
}

void DialogComponent::GetLanguage([[maybe_unused]] const std::shared_ptr<dialog_interfaces::srv::GetLanguage::Request> request,
                        std::shared_ptr<dialog_interfaces::srv::GetLanguage::Response> response)
{
    std::string current_language="";
    if (!m_iSpeechSynth->getLanguage(current_language))
    {
        response->is_ok=false;
        response->error_msg="Unable to get language from speechSynthesizer";
        response->current_language = current_language;
    }
    else
    {
        response->current_language = current_language;
        response->is_ok=true;
    }
}
