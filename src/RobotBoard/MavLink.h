#pragma once

#include <Arduino.h>
#include <common/mavlink.h>

#include "Utilities/Timer.h"
#include <LiIonBattery.h>
#include "Utilities/RemoteProxy.h"
#include <RobotStates.h>
#include "Hardware/ioDevice.h"

class Mavlink{
    public:

    //the remote's controls, and the pack read by the board (nullptr: no BATTERY_STATUS, the remote in env:remote_direct)
    void setSources(RemoteProxy* remoteProxy, BatteryLevel* batteryReading, uint8_t cellCount){
        proxy = remoteProxy;
        battery = batteryReading;
        batteryCellCount = cellCount;
    }
    RemoteProxy* proxy = nullptr;
    BatteryLevel* battery = nullptr;
    uint8_t batteryCellCount = 0;

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
    FrequencyTimer timer_BATTERY_STATUS = FrequencyTimer(1.0);

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
        if(timer_BATTERY_STATUS.shouldTrigger()) sendMsg_BATTERY_STATUS();

        if(b_online && heartbeatTimeout.hasExpired()) onDisconnection();

        //ENABLE COMMAND
        static bool b_previousEnable = false;
        bool b_enable = proxy->b_shouldEnable;
        if(b_enable && !b_previousEnable){
            sendCmd(MAV_CMD_COMPONENT_ARM_DISARM, 1.0);
        }
        b_previousEnable = b_enable;

        //DISABLE COMMAND
        static bool b_previousDisable = false;
        bool b_disable = proxy->b_shouldDisable;
        if(b_disable && !b_previousDisable){
            sendCmd(MAV_CMD_COMPONENT_ARM_DISARM, 0.0);
        }
        b_previousDisable = b_disable;

        //FLIGHT TERMINATION
        static bool b_previousLeft = false;
        bool b_left = proxy->b_leftButton;
        if(b_left && !b_previousLeft){
            sendCmd(MAV_CMD_DO_FLIGHTTERMINATION, 1.0);
        }
        b_previousLeft = b_left;

        //REBOOT
        static bool b_previousRight = false;
        bool b_right = proxy->b_rightButton;
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
        float x = proxy->leftJoystickX;
        float y = proxy->leftJoystickY;
        float z;
        float vx = proxy->leftJoystickY * xy_vel_lim;
        float vy = proxy->leftJoystickX * xy_vel_lim;
        float vz = proxy->rightJoystickX * r_vel_lim;
        float afx = xy_acc_lim;
        float afy = xy_acc_lim;
        float afz = r_acc_lim;
        float yaw = proxy->rightJoystickX;
        float yaw_rate = proxy->rightJoystickY;
        uint16_t type_mask;
        if(proxy->b_connected) type_mask = proxy->b_safetyClear ? 0x8000 : 0x0;
        else type_mask = 0x0;
        uint8_t coordinate_frame;

        if(!proxy->b_connected){
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
        //D-44: while the radio link to the handheld is down the sticks read zero, never the last values received
        bool connected = proxy->b_connected;
        int16_t x = connected ? proxy->leftJoystickX * 1000.0 : 0;
        int16_t y = connected ? proxy->leftJoystickY * 1000.0 : 0;
        int16_t z = connected ? proxy->rightJoystickX * 1000.0 : 0;
        int16_t r = connected ? proxy->rightJoystickY * 1000.0 : 0;
        uint16_t buttons = 0;
        buttons |= (proxy->b_leftButton) << 0;
        buttons |= (proxy->b_rightButton) << 1;
        buttons |= (proxy->b_leftJoystick) << 2;
        buttons |= (proxy->b_rightJoystick) << 3;
        buttons |= (proxy->speedSelection == SpeedMode::HIGH_SPEED) << 4;
        buttons |= (proxy->speedSelection == SpeedMode::LOW_SPEED) << 5;
        buttons |= (proxy->b_directionToggle) << 6;
        buttons |= (proxy->b_safetyClear && proxy->b_connected) << 7;
        buttons |= (proxy->b_shouldDisable) << 8;
        buttons |= (proxy->b_shouldEnable) << 9;
        buttons |= (connected) << 10; //D-44: radio link to the handheld up

        mavlink_message_t msg;
        mavlink_msg_manual_control_pack(systemId, componentId, &msg, 0, x ,y ,z, r, buttons, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        sendMessage(msg);
    };

    //Pack voltage in voltages[0] (mV), the rest UINT16_MAX: cell voltages are not measured.
    //Above 65.534V (a full 16S pack is 67.2V) the overflow goes in voltages[1], as the message spec says.
    void sendMsg_BATTERY_STATUS(){
        if(!battery) return;
        float packVoltage = battery->getVoltage();
        uint8_t cellCount = batteryCellCount;
        float restingCellVoltage = cellCount > 0 ? battery->getRestingVoltage() / cellCount : 0.0;

        uint16_t voltages[10];
        for(int i = 0; i < 10; i++) voltages[i] = UINT16_MAX;
        uint16_t voltages_ext[4] = {0, 0, 0, 0};
        uint32_t millivolts = packVoltage > 0.0 ? uint32_t(packVoltage * 1000.0 + 0.5) : 0;
        if(millivolts > UINT16_MAX - 1){
            voltages[0] = UINT16_MAX - 1;
            voltages[1] = uint16_t(min(millivolts - (UINT16_MAX - 1), uint32_t(UINT16_MAX - 1)));
        }
        else voltages[0] = uint16_t(millivolts);

        int8_t remaining = -1;
        uint8_t chargeState = MAV_BATTERY_CHARGE_STATE_UNDEFINED;
        if(LiIonBattery::isPlausible(restingCellVoltage)){
            remaining = LiIonBattery::percentFromCellVoltage(restingCellVoltage);
            if(restingCellVoltage < LiIonBattery::criticalCellVoltage) chargeState = MAV_BATTERY_CHARGE_STATE_CRITICAL;
            else if(restingCellVoltage < LiIonBattery::lowCellVoltage) chargeState = MAV_BATTERY_CHARGE_STATE_LOW;
            else chargeState = MAV_BATTERY_CHARGE_STATE_OK;
        }

        mavlink_message_t msg;
        mavlink_msg_battery_status_pack(systemId, componentId, &msg,
            0,                              //id
            MAV_BATTERY_FUNCTION_ALL,
            MAV_BATTERY_TYPE_LION,
            INT16_MAX,                      //temperature: unknown
            voltages,
            -1,                             //current_battery: not measured
            -1,                             //current_consumed: not measured
            -1,                             //energy_consumed: not measured
            remaining,
            0,                              //time_remaining: not estimated
            chargeState,
            voltages_ext,
            MAV_BATTERY_MODE_UNKNOWN,
            0);                             //fault_bitmask: not reported
        sendMessage(msg);
    }

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