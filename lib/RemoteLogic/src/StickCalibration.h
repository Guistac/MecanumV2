#pragma once
#include <stdint.h>

//Remote joystick calibration [D-146 point 8, H15]: per axis the raw extremes found by circling the stick and the raw
//rest position. While driving, the centre is still the zero captured at each boot when the sticks are released (it
//follows drift); the calibrated centre decides when a stick counts as released. The 1 % deadband and 99 % saturation
//are kept, applied to the calibrated range. Without a calibration the output is the same as before.
struct AxisCalibration{

    static constexpr uint16_t fullScale = 1023;     //10 bit analog readings
    //at least a quarter of the full scale on each side of the centre, else the axis is refused
    static constexpr uint16_t minimumSpan = 256;

    uint16_t low = 0;
    uint16_t centre = 512;
    uint16_t high = fullScale;

    bool isValid() const {
        return low < centre && centre < high && high <= fullScale
            && centre - low >= minimumSpan && high - centre >= minimumSpan;
    }
};

struct StickCalibration{

    static constexpr uint8_t axisCount = 4;         //left X, left Y, right X, right Y
    static constexpr uint8_t address = 20;          //EEPROM: a marker byte, then 6 bytes per axis
    static constexpr uint8_t size = 1 + axisCount * 6;
    static constexpr uint8_t marker = 0xC1;

    AxisCalibration axes[axisCount];

    void toBytes(uint8_t bytes[size]) const {
        bytes[0] = marker;
        for(int i = 0; i < axisCount; i++){
            const uint16_t values[3] = {axes[i].low, axes[i].centre, axes[i].high};
            for(int v = 0; v < 3; v++){
                bytes[1 + i * 6 + v * 2] = values[v] & 0xFF;
                bytes[1 + i * 6 + v * 2 + 1] = values[v] >> 8;
            }
        }
    }

    //no marker (never calibrated, blank EEPROM): all defaults; an invalid axis: that axis default
    static StickCalibration fromBytes(const uint8_t bytes[size]){
        StickCalibration calibration;
        if(bytes[0] != marker) return calibration;
        for(int i = 0; i < axisCount; i++){
            uint16_t values[3];
            for(int v = 0; v < 3; v++) values[v] = bytes[1 + i * 6 + v * 2] | (bytes[1 + i * 6 + v * 2 + 1] << 8);
            AxisCalibration axis;
            axis.low = values[0];
            axis.centre = values[1];
            axis.high = values[2];
            if(axis.isValid()) calibration.axes[i] = axis;
        }
        return calibration;
    }
};

namespace StickMath{

    static constexpr float deadband = 0.01f;        //as before (Joystick2Axis minLevel)
    static constexpr float saturation = 0.99f;      //as before (maxLevel)

    //raw reading to -1..1 around the boot zero, scaled to the calibrated extremes. Same arithmetic as the former
    //Joystick2Axis::readingToValue (Teensy map() in float), which used low 0 and high 1023.
    inline float normalize(uint16_t reading, int zero, uint16_t low, uint16_t high, bool b_invert){
        float fLow = float(low < zero ? low : zero - 1);
        float fHigh = float(high > zero ? high : zero + 1);
        float x = float(reading);
        float normalized;
        if(reading >= zero) normalized = (x - float(zero)) * (1.0f - 0.0f) / (fHigh - float(zero)) + 0.0f;
        else normalized = (x - fLow) * (0.0f - (-1.0f)) / (float(zero) - fLow) + (-1.0f);
        bool sign = normalized > 0.0f;
        float absolute = sign ? normalized : -normalized;
        float output;
        if(absolute > saturation) output = 1.0f;
        else if(absolute < deadband) return 0.0f;
        else {
            output = (absolute - deadband) * (1.0f - 0.0f) / (saturation - deadband) + 0.0f;
            if(output > 1.0f) output = 1.0f;
            else if(output < 0.0f) output = 0.0f;
        }
        if(!sign) output = -output;
        if(b_invert) output = -output;
        return output;
    }

    //released at boot: within the threshold (fraction of the half scale) of the calibrated rest position
    inline bool isNearCentre(uint16_t reading, const AxisCalibration& axis, float threshold){
        float distance = float(reading) - float(axis.centre);
        if(distance < 0.0f) distance = -distance;
        return distance < threshold * float(AxisCalibration::fullScale) / 2.0f;
    }
};

//The calibration procedure: wait until all four axes rest still, average the rest positions, then record the
//extremes while the operator circles both sticks
class StickCalibrator{
public:

    enum class Step{ RELEASE, CIRCLE };

    static constexpr uint32_t restMillis = 700;     //all axes still for this long
    static constexpr uint16_t restNoise = 12;       //counts of variation still counted as rest

    void start(uint32_t nowMillis){
        step = Step::RELEASE;
        restartWindow(nowMillis);
    }

    void update(const uint16_t raw[StickCalibration::axisCount], uint32_t nowMillis){
        if(step == Step::RELEASE){
            for(int i = 0; i < StickCalibration::axisCount; i++){
                if(raw[i] < windowMin[i]) windowMin[i] = raw[i];
                if(raw[i] > windowMax[i]) windowMax[i] = raw[i];
                sum[i] += raw[i];
                if(windowMax[i] - windowMin[i] > restNoise){
                    restartWindow(nowMillis);
                    return;
                }
            }
            samples++;
            if(nowMillis - windowStartMillis < restMillis) return;
            for(int i = 0; i < StickCalibration::axisCount; i++){
                uint16_t centre = uint16_t((sum[i] + samples / 2) / samples);
                result.axes[i].centre = centre;
                result.axes[i].low = centre;
                result.axes[i].high = centre;
            }
            step = Step::CIRCLE;
            return;
        }
        for(int i = 0; i < StickCalibration::axisCount; i++){
            if(raw[i] < result.axes[i].low) result.axes[i].low = raw[i];
            if(raw[i] > result.axes[i].high) result.axes[i].high = raw[i];
        }
    }

    Step getStep() const { return step; }

    //every axis covers enough on both sides: the calibration can be saved
    bool isComplete() const {
        if(step != Step::CIRCLE) return false;
        for(int i = 0; i < StickCalibration::axisCount; i++){
            if(!result.axes[i].isValid()) return false;
        }
        return true;
    }

    const StickCalibration& getResult() const { return result; }

private:

    void restartWindow(uint32_t nowMillis){
        windowStartMillis = nowMillis;
        samples = 0;
        for(int i = 0; i < StickCalibration::axisCount; i++){
            windowMin[i] = 0xFFFF;
            windowMax[i] = 0;
            sum[i] = 0;
        }
    }

    Step step = Step::RELEASE;
    uint32_t windowStartMillis = 0;
    uint32_t samples = 0;
    uint16_t windowMin[StickCalibration::axisCount];
    uint16_t windowMax[StickCalibration::axisCount];
    uint32_t sum[StickCalibration::axisCount];
    StickCalibration result;
};
