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

DHT dht( 2, DHT11 );
MQ131Sensor mq131( A0 );

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
int executionTime = 30;

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
	<form action="/">
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
<html>
<head>
<title>Ozone generator</title>
<meta charset="UTF-8">
</head>
<body>
<form action="/">
Execution<br>
<input type="submit" name="cancel" value="Cancel">
</form>
</body>
</html>
)rawliteral";

const char bodyCalibrate[] PROGMEM = R"rawliteral(
<html>
<head>
<title>Ozone generator</title>
<meta charset="UTF-8">
</head>
<body>
<form action="/">
Calibration<br>
<input type="submit" name="cancel" value="Cancel">
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
            return String( executionTime );
        }

        return String( );
    } );
}

void
sendBodyExecute( AsyncWebServerRequest* request )
{
    request->send_P( 200, "text/html", bodyExecute, []( const String& var ) { return String( ); } );
}

void
sendBodyCalibrate( AsyncWebServerRequest* request )
{
    request->send_P( 200, "text/html", bodyCalibrate,
                     []( const String& var ) { return String( ); } );
}

void
handleRoot( AsyncWebServerRequest* request )
{
    switch ( mode )
    {
    case modeMain:
    {
        if ( request->hasArg( "execute" ) )
        {
            sendBodyExecute( request );
            mode = modeExecute;
        }
        else if ( request->hasArg( "calibrate" ) )
        {
            sendBodyCalibrate( request );
            mode = modeCalibrate;
        }
        else
        {
            sendBodyMain( request );
        }

        break;
    }
    case modeExecute:
    {
        if ( request->hasArg( "cancel" ) )
        {
            sendBodyMain( request );
            mode = modeMain;
        }
        else
        {
            sendBodyExecute( request );
        }
        break;
    }
    case modeCalibrate:
    {
        if ( request->hasArg( "cancel" ) )
        {
            sendBodyMain( request );
            mode = modeMain;
        }
        else
        {
            sendBodyCalibrate( request );
        }
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

    WiFi.softAP( ssid, password );
    IPAddress myIP = WiFi.softAPIP( );
    Serial.print( "AP IP address: " );
    Serial.println( myIP );

    server.on( "/", HTTP_GET, handleRoot );

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
handleSecond( )
{
    measureDHT11( );

    mq131.sample( );
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