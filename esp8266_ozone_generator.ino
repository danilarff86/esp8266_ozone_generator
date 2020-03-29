/*
   Copyright (c) 2015, Majenko Technologies
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

 * * Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

 * * Redistributions in binary form must reproduce the above copyright notice, this
     list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.

 * * Neither the name of Majenko Technologies nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Create a WiFi access point and provide a web server on it. */

#include <DHT.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#ifndef APSSID
#define APSSID "OzoneGenerator"
#define APPSK "80672807408"
#endif

#define DHTPIN D4
#define DHTTYPE DHT11

DHT dht( DHTPIN, DHTTYPE );

/* Set these to your desired credentials. */
const char* ssid = APSSID;
const char* password = APPSK;

ESP8266WebServer server( 80 );

enum Mode
{
    modeMain = 0,
    modeExecute = 1,
    modeCalibrate = 2
};

Mode mode = modeMain;

float temperature = 0.0f;
float humidity = 0.0f;
float resistence = 12100.3f;
float r0 = 11000.1f;
float ozoneMgM3 = 13.5f;
int executionTime = 30;

const char* bodyMain = R"(
<html>
<head>
<title>Ozone generator</title>
<meta charset="UTF-8">
</head>
<body>
<form action="/">
Temperature = %.2f °C<br>
Humidity = %.2f %%<br>
Resistence = %.2f Ohms<br>
Concentration = %.2f mg/m3<br>
Base resistence:<input type="text" size="10" name="r0" value="%.2f"> Ohms<br>
Execution time:<input type="text" size="10" name="t" value="%d"> minutes<br>
<input type="submit" name="execute" value="Execute"><input type="submit" name="calibrate" value="Calibrate">
</form>
</body>
</html>
)";

const char* bodyExecute = R"(
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
)";

const char* bodyCalibrate = R"(
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
)";

char bodyStr[ 2048 ];

void
prepareBodyMain( )
{
    sprintf( bodyStr, bodyMain, temperature, humidity, resistence, ozoneMgM3, r0, executionTime );
}

void
prepareBodyExecute( )
{
    sprintf( bodyStr, bodyExecute );
}

void
prepareBodyCalibrate( )
{
    sprintf( bodyStr, bodyCalibrate );
}

/* Just a little test message.  Go to http://192.168.4.1 in a web browser
   connected to this access point to see it.
*/
void
handleRoot( )
{
    switch ( mode )
    {
    case modeMain:
    {
        if ( server.hasArg( "execute" ) )
        {
            prepareBodyExecute( );
            mode = modeExecute;
        }
        else if ( server.hasArg( "calibrate" ) )
        {
            prepareBodyCalibrate( );
            mode = modeCalibrate;
        }
        else
        {
            prepareBodyMain( );
        }

        break;
    }
    case modeExecute:
    {
        if ( server.hasArg( "cancel" ) )
        {
            prepareBodyMain( );
            mode = modeMain;
        }
        else
        {
            prepareBodyExecute( );
        }
        break;
    }
    case modeCalibrate:
    {
        if ( server.hasArg( "cancel" ) )
        {
            prepareBodyMain( );
            mode = modeMain;
        }
        else
        {
            prepareBodyCalibrate( );
        }
        break;
    }
    default:
        break;
    }

    server.send( 200, "text/html", bodyStr );
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
        Serial.print( "New value of temperature: " );
        Serial.println( temperature );
        temperature = newT;
    }

    float newH = dht.readHumidity( );
    if ( isnan( newH ) )
    {
        Serial.println( "Failed to read from DHT sensor!" );
    }
    else
    {
        Serial.print( "New value of humidity: " );
        Serial.println( humidity );
        humidity = newH;
    }
}

void
handleSecond( )
{
    measureDHT11( );
}

void
setup( )
{
    delay( 1000 );
    Serial.begin( 115200 );
    dht.begin( );
    Serial.println( );
    Serial.print( "Configuring access point..." );
    /* You can remove the password parameter if you want the AP to be open. */
    WiFi.softAP( ssid, password );

    IPAddress myIP = WiFi.softAPIP( );
    Serial.print( "AP IP address: " );
    Serial.println( myIP );
    server.on( "/", handleRoot );
    server.begin( );
    Serial.println( "HTTP server started" );
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

    server.handleClient( );
}