#pragma once

#include <Arduino.h>
#include <common/mavlink.h>

#include "Utilities/Timer.h"
#include "RobotBoard.h"

class Mavlink{
    public:

    void setRobotBoard(RobotBoard* rb){ robot = rb; }
    RobotBoard* robot = nullptr;

    uint8_t systemId = 1;
    uint8_t componentId = 1;

    uint8_t targetSystem = 0;
    uint8_t targetComponent = 0;

    float xy_vel_lim = 0.0;
    float r_vel_lim = 0.0;
    float xy_acc_lim = 0.0;
    float r_acc_lim = 0.0;

    RobotState stacatoState = RobotState::DISABLED;

    FrequencyTimer timer_HEARTBEAT = FrequencyTimer(5.0);
    FrequencyTimer timer_POSITION_TARGET_LOCAL_NED = FrequencyTimer(50.0);
    FrequencyTimer timer_MANUAL_CONTROLS = FrequencyTimer(50.0);

    bool b_online = false;
    LengthTimer heartbeatTimeout = LengthTimer(0.5);

    uint8_t outBuffer[MAVLINK_MAX_PACKET_LEN];

    void sendMessage(const mavlink_message_t& msg){
        uint16_t bufferSize = mavlink_msg_to_send_buffer(outBuffer, &msg);
        if(Serial) Serial.write(outBuffer, bufferSize);
    }

    void update(){
        while(Serial.available()){
            char c = Serial.read();
            mavlink_message_t msg;
            mavlink_status_t status;
            if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
                receiveMessage(&msg, &status);
            }
        }

        if(timer_HEARTBEAT.shouldTrigger()) sendMsg_Heartbeat();
        //if(timer_POSITION_TARGET_LOCAL_NED.shouldTrigger()) sendMsg_POSITION_TARGET_LOCAL_NED();
        if(timer_MANUAL_CONTROLS.shouldTrigger()) sendMsg_MANUAL_CONTROLS();

        if(b_online && heartbeatTimeout.hasExpired()) onDisconnection();

        //ENABLE COMMAND
        static bool b_previousEnable = false;
        bool b_enable = robot->remoteProxy.b_shouldEnable;
        if(b_enable && !b_previousEnable){
            sendCmd(MAV_CMD_COMPONENT_ARM_DISARM, 1.0);
        }
        b_previousEnable = b_enable;

        //DISABLE COMMAND
        static bool b_previousDisable = false;
        bool b_disable = robot->remoteProxy.b_shouldDisable;
        if(b_disable && !b_previousDisable){
            sendCmd(MAV_CMD_COMPONENT_ARM_DISARM, 0.0);
        }
        b_previousDisable = b_disable;

        //FLIGHT TERMINATION
        static bool b_previousLeft = false;
        bool b_left = robot->remoteProxy.b_leftButton;
        if(b_left && !b_previousLeft){
            sendCmd(MAV_CMD_DO_FLIGHTTERMINATION, 1.0);
        }
        b_previousLeft = b_left;

        //REBOOT
        static bool b_previousRight = false;
        bool b_right = robot->remoteProxy.b_rightButton;
        if(b_right && !b_previousRight){
            sendCmd(MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN, 0.0, REBOOT_SHUTDOWN_ACTION_REBOOT);
        }
        b_previousRight = b_right;
    }

    void receiveMessage(const mavlink_message_t* msg, const mavlink_status_t* status){
        switch(msg->msgid){
            case MAVLINK_MSG_ID_HEARTBEAT:
                handleMsg_HEARTBEAT(msg); break;
            case MAVLINK_MSG_ID_ODOMETRY:
                handleMsg_ODOMETRY(msg); break;
            case MAVLINK_MSG_ID_SYS_STATUS:
                handleMsg_SYS_STATUS(msg); break;
            case MAVLINK_MSG_ID_COMMAND_ACK:
                handleMsg_COMMAND_ACK(msg); break;
            case MAVLINK_MSG_ID_PARAM_VALUE:
                handleMsg_PARAM_VALUE(msg); break;
            default:
                break;
        }
    }





    void handleMsg_HEARTBEAT(const mavlink_message_t* msg){
        mavlink_heartbeat_t hb;
        mavlink_msg_heartbeat_decode(msg, &hb);
        heartbeatTimeout.start();

        switch(hb.system_status){
            case MAV_STATE_POWEROFF:    stacatoState = RobotState::EMERGENCY_STOPPING; break;
            case MAV_STATE_BOOT:        stacatoState = RobotState::EMERGENCY_STOPPING; break;
            case MAV_STATE_UNINIT:      stacatoState = RobotState::EMERGENCY_STOPPING; break;
            case MAV_STATE_CALIBRATING: stacatoState = RobotState::ENABLING; break;
            case MAV_STATE_EMERGENCY:   stacatoState = RobotState::EMERGENCY_STOPPED; break;
            case MAV_STATE_ACTIVE:      stacatoState = RobotState::ENABLED; break;
            case MAV_STATE_STANDBY:     stacatoState = RobotState::DISABLED; break;
            case MAV_STATE_CRITICAL:    stacatoState = RobotState::EMERGENCY_STOPPING; break;
            default: stacatoState = RobotState::EMERGENCY_STOPPING; break;
        }

        if(!b_online) onConnection();
    }
    void handleMsg_ODOMETRY(const mavlink_message_t* msg){
        mavlink_odometry_t odo;
        mavlink_msg_odometry_decode(msg, &odo);
    }
    void handleMsg_SYS_STATUS(const mavlink_message_t* msg){
        mavlink_sys_status_t sys;
        mavlink_msg_sys_status_decode(msg, &sys);



    }
    void handleMsg_COMMAND_ACK(const mavlink_message_t* msg){
        mavlink_command_ack_t ack;
        mavlink_msg_command_ack_decode(msg, &ack);
    }


    void handleMsg_PARAM_VALUE(const mavlink_message_t* msg){
        mavlink_param_value_t val;
        mavlink_msg_param_value_decode(msg, &val);

        if(strcmp(val.param_id, "xy_vel_lim") == 0){
            xy_vel_lim = val.param_value;
        }
        else if(strcmp(val.param_id, "r_vel_lim") == 0){
            r_vel_lim = val.param_value;
        }
        else if(strcmp(val.param_id, "xy_acc_lim") == 0){
            xy_acc_lim = val.param_value;
        }
        else if(strcmp(val.param_id, "r_acc_lim") == 0){
            r_acc_lim = val.param_value;
        }
    }



    void sendMsg_Heartbeat(){
        mavlink_message_t msg;
        mavlink_msg_heartbeat_pack(systemId, componentId, &msg, MAV_TYPE_RADIO, MAV_AUTOPILOT_GENERIC, 0, 0, 0);
        sendMessage(msg);
    }

    void sendMsg_POSITION_TARGET_LOCAL_NED(){
        uint32_t time_boot_ms;
        float x = robot->remoteProxy.leftJoystickX;
        float y = robot->remoteProxy.leftJoystickY;
        float z;
        float vx = robot->remoteProxy.leftJoystickY * xy_vel_lim;
        float vy = robot->remoteProxy.leftJoystickX * xy_vel_lim;
        float vz = robot->remoteProxy.rightJoystickX * r_vel_lim;
        float afx = xy_acc_lim;
        float afy = xy_acc_lim;
        float afz = r_acc_lim;
        float yaw = robot->remoteProxy.rightJoystickX;
        float yaw_rate = robot->remoteProxy.rightJoystickY;
        uint16_t type_mask;
        if(robot->remoteProxy.b_connected) type_mask = robot->remoteProxy.b_safetyClear ? 0x8000 : 0x0;
        else type_mask = 0x0;
        uint8_t coordinate_frame;

        if(!robot->remoteProxy.b_connected){
            x = 0;
            y = 0;
            z = 0;
            vx = 0;
            vy = 0;
            vz = 0;
            yaw = 0;
            yaw_rate = 0;
            type_mask = 0;
        }

        mavlink_message_t msg;
        mavlink_msg_position_target_local_ned_pack(0, 0, &msg, time_boot_ms, coordinate_frame, type_mask, x, y ,z, vx, vy, vz, afx, afy, afz, yaw, yaw_rate);
        sendMessage(msg);
    };

       void sendMsg_MANUAL_CONTROLS(){
        int16_t x = robot->remoteProxy.leftJoystickX * 1000.0;
        int16_t y = robot->remoteProxy.leftJoystickY * 1000.0;
        int16_t z = robot->remoteProxy.rightJoystickX * 1000.0;
        int16_t r = robot->remoteProxy.rightJoystickY * 1000.0;
        uint16_t buttons = 0;
        buttons |= (robot->remoteProxy.b_leftButton) << 0;
        buttons |= (robot->remoteProxy.b_rightButton) << 1;
        buttons |= (robot->remoteProxy.b_leftJoystick) << 2;
        buttons |= (robot->remoteProxy.b_rightJoystick) << 3;
        buttons |= (robot->remoteProxy.speedSelection == SpeedMode::HIGH_SPEED) << 4;
        buttons |= (robot->remoteProxy.speedSelection == SpeedMode::LOW_SPEED) << 5;
        buttons |= (robot->remoteProxy.b_directionToggle) << 6;
        buttons |= (robot->remoteProxy.b_safetyClear && robot->remoteProxy.b_connected) << 7;
        buttons |= (robot->remoteProxy.b_shouldDisable) << 8;
        buttons |= (robot->remoteProxy.b_shouldEnable) << 9;

        mavlink_message_t msg;
        mavlink_msg_manual_control_pack(systemId, componentId, &msg, 0, x ,y ,z, r, buttons, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        sendMessage(msg);
    };

    void sendMsg_PARAM_REQUEST_LIST(){
        mavlink_message_t msg;
        mavlink_msg_param_request_list_pack(systemId, componentId, &msg, 1, 1);
        sendMessage(msg);
    }

    void sendCmd(uint16_t command, float param1 = 0.0, float param2 = 0.0, float param3 = 0.0, float param4 = 0.0, float param5 = 0.0, float param6 = 0.0, float param7 = 0.0){
        mavlink_message_t msg;
        uint8_t confirmation = 0;
        mavlink_msg_command_long_pack(systemId, componentId, &msg, targetSystem, targetComponent,
            command, confirmation,
            param1, param2, param3, param4, param5, param6, param7);
        sendMessage(msg);
    }




    void onConnection(){
        b_online = true;
        sendMsg_PARAM_REQUEST_LIST();
    }
    void onDisconnection(){
        b_online = false;
    }

};