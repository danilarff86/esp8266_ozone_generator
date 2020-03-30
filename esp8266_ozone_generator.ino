#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Hash.h>

#include "MQ131Sensor.h"

const char* ssid = "OzoneGenerator";
const char* password = "80672807408";

DHT dht( 0, DHT11 );
MQ131Sensor mq131( A0 );

#define RELAY_PIN 2

AsyncWebServer server( 80 );

enum Mode
{
    modeMain = 0,
    modeExecute = 1,
    modeCalibrate = 2
};

Mode mode = modeMain;

float temperature = 0.0f;
float humidity = 0.0f;

struct ExecutionContext
{
    int executionTime;
    float expected_concentration;
    uint32_t secondsPassed;
    bool activated;
    uint32_t secondsAfterModeChange;
    static const uint8_t minModeSeconds = 10;
} execContext{30, 30.0f};

const char bodyMain[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<style>
	html {
		font-family: Arial;
		display: inline-block;
		margin: 0px auto;
		text-align: center;
	}
	h2 { font-size: 3.0rem; }
	p { font-size: 1.5rem; }
	.units { font-size: 1.2rem; }
	.dht-labels{
		font-size: 1.5rem;
		vertical-align:middle;
		padding-bottom: 15px;
	}
	input[type=button], input[type=submit], input[type=reset] {
		background-color: #808080;
		font-size: 1.5rem;
		border: none;
		padding: 16px 32px;
		text-decoration: none;
		margin: 4px 2px;
		cursor: pointer;
	}
	input[type=text] {
		font-size: 1.5rem;
		border: none;
		text-align: center;
		border-bottom: 2px solid grey;
	}
	</style>
</head>
<body>
	<h2>Ozone generator</h2>
	<form action="/" method="post">
	<p>
	<span class="dht-labels">Temperature</span> 
	<span id="temperature">%TEMPERATURE%</span>
	<sup class="units">&deg;C</sup>
	</p>
	<p>
	<span class="dht-labels">Humidity</span>
	<span id="humidity">%HUMIDITY%</span>
	<sup class="units">&#37;</sup>
	</p>
	<p>
	<span class="dht-labels">Concentration</span>
	<span id="concentration">%CONCENTRATION%</span>
	<sup class="units">mg/m3</sup>
	</p>
	<p>
	<span class="dht-labels">Resistence</span>
	<span id="resistence">%RESISTENCE%</span>
	<sup class="units">Ohms</sup>
	</p>
	<p>
	<span class="dht-labels">Base resistence</span>
	<input type="text" name="r0" size="7" value="%BASE_RESISTENCE%">
	<sup class="units">Ohms</sup>
	</p>
    <p>
	<span class="dht-labels">Expected concentration</span>
	<input type="text" name="expected_concentration" size="5" value="%EXPECTED_CONCENTRATION%">
	<sup class="units">mg/m3</sup>
	</p>
	<p>
	<span class="dht-labels">Execution time</span>
	<input type="text" name="t_exec" size="3" value="%EXECUTION_TIME%">
	<sup class="units">minutes</sup>
	</p>
	<p>
	<input type="submit" name="execute" value="Execute">
	<input type="submit" name="calibrate" value="Calibrate">
	</p>
	</form>
</body>
</html>
)rawliteral";

const char bodyExecute[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<style>
	html {
		font-family: Arial;
		display: inline-block;
		margin: 0px auto;
		text-align: center;
	}
	h2 { font-size: 3.0rem; }
	p { font-size: 1.5rem; }
	.units { font-size: 1.2rem; }
	.dht-labels{
		font-size: 1.5rem;
		vertical-align:middle;
		padding-bottom: 15px;
	}
	input[type=button], input[type=submit], input[type=reset] {
		background-color: #808080;
		font-size: 1.5rem;
		border: none;
		padding: 16px 32px;
		text-decoration: none;
		margin: 4px 2px;
		cursor: pointer;
	}
	input[type=text] {
		font-size: 1.5rem;
		border: none;
		text-align: center;
		border-bottom: 2px solid grey;
	}
	</style>
</head>
<body>
	<h2>Ozone generator execution</h2>
	<form action="/" method="post">
	<p>
	<span class="dht-labels">Temperature</span> 
	<span id="temperature">%TEMPERATURE%</span>
	<sup class="units">&deg;C</sup>
	</p>
	<p>
	<span class="dht-labels">Humidity</span>
	<span id="humidity">%HUMIDITY%</span>
	<sup class="units">&#37;</sup>
	</p>
	<p>
	<span class="dht-labels">Concentration</span>
	<span id="concentration">%CONCENTRATION%</span>
	<sup class="units">mg/m3</sup>
	</p>
	<p>
	<span class="dht-labels">Resistence</span>
	<span id="resistence">%RESISTENCE%</span>
	<sup class="units">Ohms</sup>
	</p>
	<p>
	<span class="dht-labels">Base resistence</span>
    <span id="r0">%BASE_RESISTENCE%</span>
	<sup class="units">Ohms</sup>
	</p>
    <p>
	<span class="dht-labels">Expected concentration</span>
    <span id="expected_concentration">%EXPECTED_CONCENTRATION%</span>
	<sup class="units">mg/m3</sup>
	</p>
	<p>
	<span class="dht-labels">Execution time</span>
    <span id="t_exec">%EXECUTION_TIME%</span>
	<sup class="units">minutes</sup>
	</p>
    <p>
	<span class="dht-labels">Time passed</span>
    <span id="t_passed">%TIME_PASSED%</span>
	<sup class="units">minutes</sup>
	</p>
	<p>
	<input type="submit" name="cancel" value="Cancel">
	</p>
	</form>
</body>
</html>
)rawliteral";

const char bodyCalibrate[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<style>
	html {
		font-family: Arial;
		display: inline-block;
		margin: 0px auto;
		text-align: center;
	}
	h2 { font-size: 3.0rem; }
	p { font-size: 1.5rem; }
	.units { font-size: 1.2rem; }
	.dht-labels{
		font-size: 1.5rem;
		vertical-align:middle;
		padding-bottom: 15px;
	}
	input[type=button], input[type=submit], input[type=reset] {
		background-color: #808080;
		font-size: 1.5rem;
		border: none;
		padding: 16px 32px;
		text-decoration: none;
		margin: 4px 2px;
		cursor: pointer;
	}
	input[type=text] {
		font-size: 1.5rem;
		border: none;
		text-align: center;
		border-bottom: 2px solid grey;
	}
	</style>
</head>
<body>
	<h2>Ozone generator calibration</h2>
	<form action="/" method="post">
	<p>
	<span class="dht-labels">Resistence</span>
	<span id="resistence">%RESISTENCE%</span>
	<sup class="units">Ohms</sup>
	</p>
	<p>
	<input type="submit" name="cancel" value="Cancel">
	</p>
	</form>
</body>
</html>
)rawliteral";

void
sendBodyMain( AsyncWebServerRequest* request )
{
    request->send_P( 200, "text/html", bodyMain, []( const String& var ) {
        if ( var == "TEMPERATURE" )
        {
            return String( temperature );
        }
        else if ( var == "HUMIDITY" )
        {
            return String( humidity );
        }
        else if ( var == "CONCENTRATION" )
        {
            return String( mq131.get_o3( MQ131Sensor::Unit::MG_M3, {temperature, humidity} ) );
        }
        else if ( var == "RESISTENCE" )
        {
            return String( mq131.get_r_sensor( ) );
        }
        else if ( var == "BASE_RESISTENCE" )
        {
            return String( mq131.get_r0_sensor( ) );
        }
        else if ( var == "EXECUTION_TIME" )
        {
            return String( execContext.executionTime );
        }
        else if ( var == "EXPECTED_CONCENTRATION" )
        {
            return String( execContext.expected_concentration );
        }

        return String( );
    } );
}

void
sendBodyExecute( AsyncWebServerRequest* request )
{
    request->send_P( 200, "text/html", bodyExecute, []( const String& var ) {
        if ( var == "TEMPERATURE" )
        {
            return String( temperature );
        }
        else if ( var == "HUMIDITY" )
        {
            return String( humidity );
        }
        else if ( var == "CONCENTRATION" )
        {
            return String( mq131.get_o3( MQ131Sensor::Unit::MG_M3, {temperature, humidity} ) );
        }
        else if ( var == "RESISTENCE" )
        {
            return String( mq131.get_r_sensor( ) );
        }
        else if ( var == "BASE_RESISTENCE" )
        {
            return String( mq131.get_r0_sensor( ) );
        }
        else if ( var == "EXECUTION_TIME" )
        {
            return String( execContext.executionTime );
        }
        else if ( var == "EXPECTED_CONCENTRATION" )
        {
            return String( execContext.expected_concentration );
        }
        else if ( var == "TIME_PASSED" )
        {
            return String( execContext.secondsPassed / 60 );
        }

        return String( );
    } );
}

void
sendBodyCalibrate( AsyncWebServerRequest* request )
{
    request->send_P( 200, "text/html", bodyCalibrate, []( const String& var ) {
        if ( var == "RESISTENCE" )
        {
            return String( mq131.get_r_sensor( ) );
        }

        return String( );
    } );
}

void
handleRoot( AsyncWebServerRequest* request )
{
    switch ( mode )
    {
    case modeMain:
    {
        if ( request->method( ) == HTTP_POST )
        {
            if ( request->hasParam( "r0", true ) )
            {
                const auto r0 = request->getParam( "r0", true )->value( ).toFloat( );
                mq131.set_r0_sensor( r0 );
            }

            if ( request->hasParam( "t_exec", true ) )
            {
                execContext.executionTime = request->getParam( "t_exec", true )->value( ).toInt( );
            }

            if ( request->hasParam( "expected_concentration", true ) )
            {
                execContext.expected_concentration
                    = request->getParam( "expected_concentration", true )->value( ).toFloat( );
            }

            if ( request->hasParam( "execute", true ) )
            {
                sendBodyExecute( request );
                startExecution( );
                mode = modeExecute;
                break;
            }
            else if ( request->hasParam( "calibrate", true ) )
            {
                sendBodyCalibrate( request );
                mq131.start_calibration( );
                mode = modeCalibrate;
                break;
            }
        }

        sendBodyMain( request );
        break;
    }
    case modeExecute:
    {
        if ( request->method( ) == HTTP_POST && request->hasParam( "cancel", true ) )
        {
            sendBodyMain( request );
            mode = modeMain;
            stopExecution( );
            break;
        }

        sendBodyExecute( request );
        break;
    }
    case modeCalibrate:
    {
        if ( request->method( ) == HTTP_POST && request->hasParam( "cancel", true ) )
        {
            sendBodyMain( request );
            mode = modeMain;
            mq131.cancel_calibration( );
            break;
        }

        sendBodyCalibrate( request );
        break;
    }
    default:
        request->send( 400, "text/plain", "Bad request" );
        break;
    }
}

void
setup( )
{
    delay( 1000 );
    Serial.begin( 115200 );
    dht.begin( );
    pinMode( RELAY_PIN, OUTPUT );
    digitalWrite( RELAY_PIN, HIGH );

    WiFi.softAP( ssid, password );
    IPAddress myIP = WiFi.softAPIP( );
    Serial.print( "AP IP address: " );
    Serial.println( myIP );

    server.on( "/", HTTP_GET, handleRoot );
    server.on( "/", HTTP_POST, handleRoot );

    server.begin( );
    Serial.println( "HTTP server started" );
}

void
measureDHT11( )
{
    float newT = dht.readTemperature( );
    if ( isnan( newT ) )
    {
        Serial.println( "Failed to read from DHT sensor!" );
    }
    else
    {
        temperature = newT;
    }

    float newH = dht.readHumidity( );
    if ( isnan( newH ) )
    {
        Serial.println( "Failed to read from DHT sensor!" );
    }
    else
    {
        humidity = newH;
    }
}

void
handleCalibration( )
{
    if ( mq131.is_calibration_finished( ) )
    {
        mq131.apply_calibration_data( );
        mode = modeMain;
    }
    else
    {
        mq131.calibration_step( );
    }
}

inline void
activateGenerator( )
{
    execContext.secondsAfterModeChange = 0;
    execContext.activated = true;
    Serial.println( "Generator activated" );
    digitalWrite( RELAY_PIN, LOW );
}

inline void
deactivateGenerator( )
{
    execContext.secondsAfterModeChange = 0;
    execContext.activated = false;
    Serial.println( "Generator deactivated" );
    digitalWrite( RELAY_PIN, HIGH );
}

void
startExecution( )
{
    deactivateGenerator( );
    execContext.secondsAfterModeChange = execContext.minModeSeconds;
    execContext.secondsPassed = 0;
    Serial.println( "Execution started" );
}

void
stopExecution( )
{
    deactivateGenerator( );
    Serial.println( "Execution stopped" );
}

void
handleExecution( )
{
    if ( execContext.secondsPassed >= ( execContext.executionTime * 60 ) )
    {
        stopExecution( );
        mode = modeMain;
    }
    else
    {
        const auto concentration
            = mq131.get_o3( MQ131Sensor::Unit::MG_M3, {temperature, humidity} );

        if ( ( concentration < ( execContext.expected_concentration * 0.9 ) )
             && ( execContext.secondsAfterModeChange >= execContext.minModeSeconds )
             && !execContext.activated )
        {
            activateGenerator( );
        }
        else if ( ( concentration > ( execContext.expected_concentration * 1.1 ) )
                  && ( execContext.secondsAfterModeChange >= execContext.minModeSeconds )
                  && execContext.activated )
        {
            deactivateGenerator( );
        }
    }
    ++execContext.secondsPassed;
    ++execContext.secondsAfterModeChange;
}

void
handleSecond( )
{
    measureDHT11( );

    mq131.sample( );

    if ( mode == modeExecute )
    {
        handleExecution( );
    }

    if ( mode == modeCalibrate )
    {
        handleCalibration( );
    }
}

void
loop( )
{
    static const unsigned long second_interval = 1000;
    static unsigned long previous_ms = 0;
    if ( ( millis( ) - previous_ms ) >= second_interval )
    {
        handleSecond( );
        previous_ms = millis( );
    }
}