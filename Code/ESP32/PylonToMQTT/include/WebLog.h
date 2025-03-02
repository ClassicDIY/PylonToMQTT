#pragma once

#include <Arduino.h>
#include "esp_log.h"
#include <time.h>
#include "defines.h"
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>

// Store HTML content with JavaScript to receive serial log data via WebSocket
const char web_serial_html[] PROGMEM = R"rawliteral(
	<!DOCTYPE html>
	<html>
	<head>
	  <title>ESP32 Serial Log</title>
	  <script>
	
		function initWebSocket() {
			const socket = new WebSocket('ws://' + window.location.hostname + ':7668');
			socket.onmessage = function(event) {
				document.getElementById('log').innerText += event.data;
			};

			socket.onerror = function(error) {
				console.error('WebSocket error:', error);
			};

			window.addEventListener('beforeunload', function() {
				if (socket) {
					socket.close();
				}
			});
		}
		window.onload = function() {
		  initWebSocket();
		}
	  </script>
	</head>
	<body>
	  <h1>ESP32 Serial Log</h1>
	  <pre>Connecting to WebSocket...</pre><br>
	  <div id="log"></div>
	</body>
	</html>
	)rawliteral";

class WebLog
{
public:
	WebLog() {};
	void begin(AsyncWebServer *pwebServer);
	void process();

private:
};
