#include "MQ131Sensor.h"

#include <Arduino.h>

namespace
{
float
convert( float input, MQ131Sensor::Unit unitIn, MQ131Sensor::Unit unitOut )
{
    if ( unitIn == unitOut )
    {
        return input;
    }

    float concentration = 0;

    switch ( unitOut )
    {
    case MQ131Sensor::Unit::PPM:
        // We assume that the unit IN is PPB as the sensor provide only in PPB and
        // PPM depending on the type of sensor (METAL or BLACK_BAKELITE) So, convert
        // PPB to PPM
        return input / 1000.0;
    case MQ131Sensor::Unit::PPB:
        // We assume that the unit IN is PPM as the sensor provide only in PPB and
        // PPM depending on the type of sensor (METAL or BLACK_BAKELITE) So, convert
        // PPM to PPB
        return input * 1000.0;
    case MQ131Sensor::Unit::MG_M3:
        if ( unitIn == MQ131Sensor::Unit::PPM )
        {
            concentration = input;
        }
        else
        {
            concentration = input / 1000.0;
        }
        return concentration * 48.0 / 22.71108;
    case MQ131Sensor::Unit::UG_M3:
        if ( unitIn == MQ131Sensor::Unit::PPB )
        {
            concentration = input;
        }
        else
        {
            concentration = input * 1000.0;
        }
        return concentration * 48.0 / 22.71108;
    default:
        return input;
    }
}
}  // namespace

const MQ131Sensor::Env MQ131Sensor::default_env{20, 60};

MQ131Sensor::MQ131Sensor( int pin_sensor,
                          uint32_t r_load,
                          uint32_t power_voltage,
                          uint32_t adc_voltage )
    : pin_sensor_( pin_sensor )
    , r_load_( r_load )
    , c1_( float( power_voltage * 1024 ) / float( adc_voltage ) * float( r_load_ ) )
    , r0_sensor_( MQ131_DEFAULT_R0_SENSOR )
    , adc_data_( 0 )
    , calibration_data_( )
{
    pinMode( pin_sensor_, INPUT );
}

void
MQ131Sensor::sample( )
{
    adc_data_ = analogRead( pin_sensor_ );
}

float
MQ131Sensor::get_r_sensor( ) const
{
    return c1_ / float( adc_data_ ) - float( r_load_ );
    // return float( 1024 ) * float( r_load_ ) / float( analogRead( pin_sensor_ ) ) - float( r_load_
    // );
}

float
MQ131Sensor::get_o3( Unit unit, const Env& env ) const
{
    const auto r_sensor = get_r_sensor( );

    // Use the equation to compute the O3 concentration in ppm
    // R^2 = 0.99
    // Compute the ratio Rs/R0 and apply the environmental correction
    const auto ratio = r_sensor / r0_sensor_ * get_env_correction_ratio( env );
    return convert( 8.1399 * pow( ratio, 2.3297 ), Unit::PPM, unit );
}

float
MQ131Sensor::get_env_correction_ratio( const Env& env )
{
    // Select the right equation based on humidity
    // If default value, ignore correction ratio
    if ( env.humidity == default_env.humidity && env.temperature == default_env.temperature )
    {
        return 1.0;
    }
    // For humidity > 75%, use the 85% curve
    if ( env.humidity > 75 )
    {
        // R^2 = 0.9986
        return -0.0141 * env.temperature + 1.5623;
    }
    // For humidity > 50%, use the 60% curve
    if ( env.humidity > 50 )
    {
        // R^2 = 0.9976
        return -0.0119 * env.temperature + 1.3261;
    }

    // Humidity < 50%, use the 30% curve
    // R^2 = 0.996
    return -0.0103 * env.temperature + 1.1507;
}

void
MQ131Sensor::start_calibration( )
{
    calibration_data_.all_sum = 0;
    calibration_data_.array_infilled = false;
    calibration_data_.index = 0;
    calibration_data_.previous_average = 0.0f;
    calibration_data_.r0_sensor_ = -1.0f;
    for ( auto& elem : calibration_data_.rsensor_data )
    {
        elem = 0;
    }
    Serial.println( "Calibration started" );
}

void
MQ131Sensor::calibration_step( )
{
    Serial.println( "Calibration step" );
    calibration_data_.all_sum -= calibration_data_.rsensor_data[ calibration_data_.index ];
    calibration_data_.rsensor_data[ calibration_data_.index ] = get_r_sensor( );
    calibration_data_.all_sum += calibration_data_.rsensor_data[ calibration_data_.index ];
    if ( ++calibration_data_.index == calibration_data_.array_len )
    {
        calibration_data_.index = 0;
        calibration_data_.array_infilled = 1;
    }

    if ( calibration_data_.array_infilled )
    {
        const float actual_average
            = float( calibration_data_.all_sum ) / float( calibration_data_.array_len );

        const float diff = ( actual_average > calibration_data_.previous_average
                                 ? actual_average - calibration_data_.previous_average
                                 : calibration_data_.previous_average - actual_average )
                           * 100 / actual_average;

        Serial.print( "Average: " );
        Serial.print( actual_average );
        Serial.print( ", Percentage diff: " );
        Serial.println( diff );

        if ( diff <= 0.0f )
        {
            calibration_data_.r0_sensor_ = actual_average;
        }

        calibration_data_.previous_average = actual_average;
    }
}

void
MQ131Sensor::apply_calibration_data( )
{
    Serial.println( "Calibration data applied" );
    r0_sensor_ = calibration_data_.r0_sensor_;
}

void
MQ131Sensor::cancel_calibration( )
{
    Serial.println( "Calibration cancelled" );
    calibration_data_.r0_sensor_ = -1.0f;
}

bool
MQ131Sensor::is_calibration_finished( ) const
{
    return calibration_data_.r0_sensor_ >= 0.0f;
}