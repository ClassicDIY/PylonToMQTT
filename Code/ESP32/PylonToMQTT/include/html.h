#pragma once

#include "Arduino.h"

const char home_html[] PROGMEM = R"rawliteral(
	<!DOCTYPE html><html lang=\"en\">
	<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>
	<title>{n}</title>

	</head>
	
	<body>
	<h2>{n}</h2>
	<div style='font-size: .6em;'>Firmware config version '{v}'</div>
	<hr>
	
	<p>
	<div style='padding-top:25px;'>
	<p><a href='settings' onclick="javascript:event.target.port={cp}" >View Current Settings</a></p>
	
	</div></body></html>
	)rawliteral";