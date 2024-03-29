#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Hash.h>

#include "ESP_EEPROM.h"
#include "MQ131Sensor.h"

const char* ssid = "Danila_legacy";
const char* password = "victory@2021";

DHT dht( D4, DHT11 );
MQ131Sensor mq131( A0 );

#define RELAY_PIN D3

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
    float expectedConcentration;
    uint32_t secondsPassed;
    bool activated;
    uint32_t secondsAfterModeChange;
    static const uint8_t minModeSeconds = 10;
} execContext{30, 30.0f};

enum EEPROM_OFFSET
{
    EEPROM_OFFSET_EXEC_TIME = 0,
    EEPROM_OFFSET_R0 = EEPROM_OFFSET_EXEC_TIME + sizeof( int ),
    EEPROM_OFFSET_EXPECTED_CONCENTRATION = EEPROM_OFFSET_R0 + sizeof( float ),
    EEPROM_OFFSET_LAST = EEPROM_OFFSET_EXPECTED_CONCENTRATION + sizeof( float )
};

const char headStr[] PROGMEM = R"rawliteral(
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
	button, input[type=button], input[type=submit], input[type=reset] {
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
)rawliteral";

const char bodyMain[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
%HEAD%
<body>
	<h2>Ozone generator</h2>
    <button onClick="window.location.href=window.location.href">Refresh Page</button>
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
	<span class="dht-labels">ADC data</span>
	<span id="adc_data">%ADC_DATA%</span>
	<sup class="units">1/1024</sup>
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
	<input type="submit" name="execute" value="Execute"><br/>
	<input type="submit" name="calibrate" value="Calibrate">
	</p>
	</form>
</body>
</html>
)rawliteral";

const char bodyExecute[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
%HEAD%
<body>
	<h2>Ozone generator execution</h2>
    <button onClick="window.location.href=window.location.href">Refresh Page</button>
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
	<span class="dht-labels">ADC data</span>
	<span id="adc_data">%ADC_DATA%</span>
	<sup class="units">1/1024</sup>
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
%HEAD%
<body>
	<h2>Ozone generator calibration</h2>
    <button onClick="window.location.href=window.location.href">Refresh Page</button>
	<form action="/" method="post">
	<p>
	<span class="dht-labels">ADC data</span>
	<span id="adc_data">%ADC_DATA%</span>
	<sup class="units">1/1024</sup>
	</p>
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
sendBody( AsyncWebServerRequest* request, const char* body )
{
    request->send_P( 200, "text/html", body, []( const String& var ) {
        if ( var == "HEAD" )
        {
            return String( headStr );
        }
        else if ( var == "TEMPERATURE" )
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
        else if ( var == "ADC_DATA" )
        {
            return String( mq131.get_adc_data( ) );
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
            return String( execContext.expectedConcentration );
        }
        else if ( var == "TIME_PASSED" )
        {
            return String( execContext.secondsPassed / 60 );
        }

        return String( );
    } );
}

void
loadEEPROMData( )
{
    EEPROM.get( EEPROM_OFFSET_EXEC_TIME, execContext.executionTime );
    float tmpR0;
    EEPROM.get( EEPROM_OFFSET_R0, tmpR0 );
    mq131.set_r0_sensor( tmpR0 );
    EEPROM.get( EEPROM_OFFSET_EXPECTED_CONCENTRATION, execContext.expectedConcentration );
}

void
saveEEPROMData( )
{
    EEPROM.put( EEPROM_OFFSET_EXEC_TIME, execContext.executionTime );
    EEPROM.put( EEPROM_OFFSET_R0, mq131.get_r0_sensor( ) );
    EEPROM.put( EEPROM_OFFSET_EXPECTED_CONCENTRATION, execContext.expectedConcentration );
    boolean ok = EEPROM.commit( );
    Serial.println( ( ok ) ? "EEPROM commit OK" : "EEPROM commit FAILED" );
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
            bool eeprom_changed = false;
            if ( request->hasParam( "r0", true ) )
            {
                const auto newR0 = request->getParam( "r0", true )->value( ).toFloat( );
                if ( newR0 != mq131.get_r0_sensor( ) )
                {
                    mq131.set_r0_sensor( newR0 );
                    eeprom_changed = true;
                }
            }

            if ( request->hasParam( "t_exec", true ) )
            {
                const auto newExecutionTime
                    = request->getParam( "t_exec", true )->value( ).toInt( );
                if ( newExecutionTime != execContext.executionTime )
                {
                    execContext.executionTime = newExecutionTime;
                    eeprom_changed = true;
                }
            }

            if ( request->hasParam( "expected_concentration", true ) )
            {
                const auto newExpectedConcentration
                    = request->getParam( "expected_concentration", true )->value( ).toFloat( );
                if ( newExpectedConcentration != execContext.expectedConcentration )
                {
                    execContext.expectedConcentration = newExpectedConcentration;
                    eeprom_changed = true;
                }
            }

            if ( eeprom_changed )
            {
                saveEEPROMData( );
            }

            if ( request->hasParam( "execute", true ) )
            {
                sendBody( request, bodyExecute );
                startExecution( );
                mode = modeExecute;
                break;
            }
            else if ( request->hasParam( "calibrate", true ) )
            {
                sendBody( request, bodyCalibrate );
                mq131.start_calibration( );
                mode = modeCalibrate;
                break;
            }
        }

        sendBody( request, bodyMain );
        break;
    }
    case modeExecute:
    {
        if ( request->method( ) == HTTP_POST && request->hasParam( "cancel", true ) )
        {
            sendBody( request, bodyMain );
            mode = modeMain;
            stopExecution( );
            break;
        }

        sendBody( request, bodyExecute );
        break;
    }
    case modeCalibrate:
    {
        if ( request->method( ) == HTTP_POST && request->hasParam( "cancel", true ) )
        {
            sendBody( request, bodyMain );
            mode = modeMain;
            mq131.cancel_calibration( );
            break;
        }

        sendBody( request, bodyCalibrate );
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
    Serial.println( );

    dht.begin( );
    pinMode( RELAY_PIN, OUTPUT );
    digitalWrite( RELAY_PIN, HIGH );

    EEPROM.begin( EEPROM_OFFSET_LAST );
    loadEEPROMData( );

    // Serial.println( "Configuring access point..." );
    // WiFi.softAP( ssid, password );
    // IPAddress myIP = WiFi.softAPIP( );
    // Serial.print( "AP IP address: " );
    // Serial.println( myIP );

    Serial.println( );
    Serial.println( );
    Serial.print( "Connecting to " );
    Serial.println( ssid );

    /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
       would try to act as both a client and an access-point and could cause
       network-issues with your other WiFi-devices on your WiFi-network. */
    WiFi.mode( WIFI_STA );
    WiFi.begin( ssid, password );

    while ( WiFi.status( ) != WL_CONNECTED )
    {
        delay( 500 );
        Serial.print( "." );
    }

    Serial.println( "" );
    Serial.println( "WiFi connected" );
    Serial.println( "IP address: " );
    Serial.println( WiFi.localIP( ) );

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
        saveEEPROMData( );
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

        if ( ( concentration < ( execContext.expectedConcentration * 0.9 ) )
             && ( execContext.secondsAfterModeChange >= execContext.minModeSeconds )
             && !execContext.activated )
        {
            activateGenerator( );
        }
        else if ( ( concentration > ( execContext.expectedConcentration * 1.1 ) )
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