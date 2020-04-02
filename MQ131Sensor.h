#pragma once

#include <stdint.h>

#define MQ131_POWER_VOLTAGE 5000
#define MQ131_ADC_VOLTAGE 3300
#define MQ131_DEFAULT_R_LOAD 10000
#define MQ131_DEFAULT_R0_SENSOR 30000.0f

struct MQ131Sensor
{
    enum class Unit
    {
        PPM,
        PPB,
        MG_M3,
        UG_M3
    };

    struct Env
    {
        int temperature;
        int humidity;
    };

    MQ131Sensor( int pin_sensor,
                 uint32_t r_load = MQ131_DEFAULT_R_LOAD,
                 uint32_t power_voltage = MQ131_POWER_VOLTAGE,
                 uint32_t adc_voltage = MQ131_ADC_VOLTAGE );

    void sample( );

    float get_o3( Unit unit, const Env& env ) const;

    uint16_t
    get_adc_data( ) const
    {
        return adc_data_;
    }

    float get_r_sensor( ) const;

    inline float
    get_r0_sensor( ) const
    {
        return r0_sensor_;
    }

    inline void
    set_r0_sensor( float r0_sensor )
    {
        r0_sensor_ = r0_sensor;
    }

    void start_calibration( );
    void cancel_calibration( );
    void calibration_step( );
    void apply_calibration_data( );
    bool is_calibration_finished( ) const;

private:
    inline static const Env&
    get_default_env( )
    {
        return default_env;
    }

    static float get_env_correction_ratio( const Env& env );

private:
    const int pin_sensor_;
    const uint32_t r_load_;
    const float c1_;
    float r0_sensor_;
    uint16_t adc_data_;

    struct CalibrationData
    {
        static const uint8_t array_len = 20;
        uint32_t rsensor_data[ array_len ]{};
        uint8_t index;
        uint32_t all_sum;
        float previous_average;
        bool array_infilled;
        float r0_sensor_;
    } calibration_data_;

    static const Env default_env;
};