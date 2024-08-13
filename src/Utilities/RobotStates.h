#pragma once

enum class RobotState{
    DISABLING = 0x1,
    DISABLED = 0x2,
    ENABLING = 0x3,
    ENABLED = 0x4,
    EMERGENCY_STOPPING = 0x5,
    EMERGENCY_STOPPED = 0x6
};

enum class SpeedMode{
    LOW_SPEED = 0x0,
    MEDIUM_SPEED = 0x1,
    HIGH_SPEED = 0x2,
    NO_SPEED = 0x3
};