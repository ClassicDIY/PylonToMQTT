#include "Log.h"
#include "WebLog.h"

WebSocketsServer _webSocket = WebSocketsServer(WSOCKET_LOG_PORT);

int weblog_log_printfv(const char *format, va_list arg)
{
    static char loc_buf[256];
    char * temp = loc_buf;
    uint32_t len;
    va_list copy;
    va_copy(copy, arg);
    len = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if(len >= sizeof(loc_buf)){
        temp = (char*)malloc(len+1);
        if(temp == NULL) {
            return 0;
        }
    }
    vsnprintf(temp, len+1, format, arg);
    _webSocket.broadcastTXT(temp);
    if(len >= sizeof(loc_buf)){
        free(temp);
    }
    int rVal = ets_printf("%s", temp);
    return rVal;
}

int weblog(const char *format, ...)
{
    int len;
    va_list arg;
    va_start(arg, format);
    len = weblog_log_printfv(format, arg);
    va_end(arg);
    return len;
}

void WebLog::begin(AsyncWebServer *pwebServer)
{
    _webSocket.begin();
    _webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t *payload, size_t length)
                       { 
        if (type == WStype_DISCONNECTED)
		{
			logi("[%u] Web Log Disconnected!\n", num);
		}
		else if (type == WStype_CONNECTED)
		{
			logi("[%u] Web Log Connected!\n", num);
		} });
    pwebServer->on("/log", HTTP_GET, [](AsyncWebServerRequest *request)
                   { request->send(200, "text/html", web_serial_html); });
}

void WebLog::process()
{
    _webSocket.loop();
}
