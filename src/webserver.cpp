//
// Created by viciu on 17.02.2020.
//

#include "webserver.h"
#include "wifi.h"
unsigned maxSizeTemp = 0;
void webserverPartialSend(String &s) {
    if (s.length() == 0) return;    //do not end by accident, when no data to send
    if (s.length() > maxSizeTemp) maxSizeTemp = s.length();
    server.client().setNoDelay(1);
    server.sendContent(s);
    s = F("");
}
void endPartialSend(){
    maxSizeTemp = 0;
    server.sendContent(F(""));
}
template<typename T, std::size_t N> constexpr std::size_t capacity_null_terminated_char_array(const T(&)[N]) {
    return N - 1;
}

String line_from_value(const String& name, const String& value) {
    String s = F("<br/>{n}: {v}");
    s.replace("{n}", name);
    s.replace("{v}", value);
    return s;
}

static int constexpr constexprstrlen(const char* str) {
    return *str ? 1 + constexprstrlen(str + 1) : 0;
}


void sendHttpRedirect(WEB_SERVER_TYPE &httpServer) {
    httpServer.sendHeader(F("Location"), F("http://192.168.4.1/config"));
    httpServer.send(302, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), "");
}

String wlan_ssid_to_table_row(const String& ssid, const String& encryption, int32_t rssi) {
    String s = F(	"<tr>"
                     "<td>"
                     "<a href='#wlanpwd' onclick='setSSID(this)' class='wifi'>{n}</a>&nbsp;{e}"
                     "</td>"
                     "<td style='width:80%;vertical-align:middle;'>"
                     "{v}%"
                     "</td>"
                     "</tr>");
    s.replace("{n}", ssid);
    s.replace("{e}", encryption);
    s.replace("{v}", String(calcWiFiSignalQuality(rssi)));
    return s;
}

String time2NextMeasure() {
    String s = FPSTR(INTL_TIME_TO_MEASUREMENT);
    s.replace("{v}", String(time2Measure()/1000));
    return s;
}


String timeSinceLastMeasure() {
    String s = F("");
    unsigned long time_since_last = msSince(starttime);
    if (time_since_last > cfg::sending_intervall_ms) {
        time_since_last = 0;
    }
    s += String((long)((time_since_last + 500) / 1000));
    s += FPSTR(INTL_TIME_SINCE_LAST_MEASUREMENT);
    s += F("<br/><br/>");
    return s;
}


void getTimeHeadings(String &page_content){
    if (first_cycle) page_content += F("<span style='color:red'>");
    page_content.concat(time2NextMeasure());
    if (first_cycle) page_content.concat(F(".</span><br/><br/>"));
    if (!first_cycle) {
        page_content.concat(F(", "));
        page_content.concat(timeSinceLastMeasure());
    }
}



void webserver_not_found() {
    last_page_load = millis();
    debug_out(F("output not found page: "), DEBUG_MIN_INFO, 0);
    debug_out(server.uri(),DEBUG_MIN_INFO);
    if (WiFi.status() != WL_CONNECTED) {
        if ((server.uri().indexOf(F("success.html")) != -1) || (server.uri().indexOf(F("detect.html")) != -1)) {
            server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), FPSTR(WEB_IOS_REDIRECT));
        } else {
            sendHttpRedirect(server);
        }
    } else {
        server.send(404, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), F("Not found."));
    }
}


void webserver_images() {
    if (server.arg("n") == F("l")) {
        server.sendHeader(F("Cache-Control"), F("max-age=604800"));
        server.send(200, FPSTR(TXT_CONTENT_TYPE_IMAGE_SVG), FPSTR(LUFTDATEN_INFO_LOGO_SVG));
    } else if (server.arg("n") == F("c"))  {    //config CSS
        server.sendHeader(F("Cache-Control"), F("max-age=604800"));
        server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_CSS), FPSTR(CONFIG_CSS));
    } else if (server.arg("n") == F("c1"))  {    //common CSS
        server.sendHeader(F("Cache-Control"), F("max-age=604800"));
        server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_CSS), FPSTR(COMMON_CSS));
    } else if (server.arg("n") == F("j"))  {    //config JS
        server.sendHeader(F("Cache-Control"), F("max-age=604800"));
        server.send(200, FPSTR(TXT_CONTENT_TYPE_APPLICATION_JS), FPSTR(CONFIG_JS));
    }
    else {
        webserver_not_found();
    }
}
/*****************************************************************
 * Webserver request auth: prompt for BasicAuth
 *
 * -Provide BasicAuth for all page contexts except /values and images
 *****************************************************************/
bool webserver_request_auth(bool dbg_msg) {
    if (dbg_msg) debug_out(F("validate request auth..."), DEBUG_MAX_INFO, 1);
    if (cfg::www_basicauth_enabled && ! wificonfig_loop) {
        if (!server.authenticate(cfg::www_username, cfg::www_password)) {
            server.requestAuthentication();
            return false;
        }
    }
    return true;
}
void webserver_dump_stack(){
    if (!SPIFFS.exists ("/stack_dump")) {
        server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), "No stack dump");
        return;
    }
    File dump;
    char buf[100];
    dump = SPIFFS.open("/stack_dump","r");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), "");
    unsigned size = dump.size();
    for (byte i=0; i<=size/100; i++) {
        dump.readBytes(buf,99);
        server.sendContent(buf);
    }
//    sprintf(buf,"File size: %i bytes\n",size);
//    server.sendContent(buf);

}
/*****************************************************************
 * Webserver root: show all options                              *
 *****************************************************************/
void webserver_root() {
    static bool firstAccess = true;

    if (WiFi.status() != WL_CONNECTED && firstAccess) {
        debug_out(F("redirect to config..."), DEBUG_MIN_INFO, 1);
        sendHttpRedirect(server);
        firstAccess = false;
    } else {
        if (!webserver_request_auth()) { return; }

        String page_content = make_header(cfg::fs_ssid);
        last_page_load = millis();
        debug_out(F("output root page..."), DEBUG_MIN_INFO, 1);
        page_content += FPSTR(WEB_ROOT_PAGE_CONTENT);
        page_content.replace("{t}", FPSTR(INTL_CURRENT_DATA));
        //page_content.replace(F("{map}"), FPSTR(INTL_ACTIVE_SENSORS_MAP));
        page_content.replace(F("{conf}"), FPSTR(INTL_CONFIGURATION));
        page_content.replace(F("{status}"), FPSTR(INTL_STATUS_PAGE));
        page_content.replace(F("{conf_delete}"), FPSTR(INTL_CONFIGURATION_DELETE));
        page_content.replace(F("{restart}"), FPSTR(INTL_RESTART_SENSOR));
        page_content.replace(F("{debug}"), FPSTR(INTL_DEBUG));
        page_content += make_footer();
        server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
    }
}

/*****************************************************************
 * html helper functions                                         *
 *****************************************************************/

String make_header(const String& title, bool configPage) {
    String s;
    if (configPage)
        s = FPSTR(CONFIG_WEB_PAGE_HEADER);
    else
        s = FPSTR(WEB_PAGE_HEADER);

    s.replace("{tt}", FPSTR(INTL_PM_SENSOR));
    s.replace("{h}", FPSTR(INTL_HOME));
    if (title != " ") {
        s.replace("{n}", F("&raquo;"));
    } else {
        s.replace("{n}", "");
    }
    String v = F("<a class=\"plain\" href=\"https://github.com/nettigo/namf/blob/");
    switch (cfg::update_channel) {
        case UPDATE_CHANNEL_STABLE:
            v += F("master/Versions.md");
            break;
        case UPDATE_CHANNEL_BETA:
            v += F("beta/Versions.md");
            break;
        case UPDATE_CHANNEL_ALFA:
            v += F("new_sds011/Versions-alfa.md");
            break;
    }
    v += F("\">");
    v += String(SOFTWARE_VERSION);
    v += F("</a>");

#ifdef BUILD_TIME
    v+="(";
    char timestamp[16];
    struct tm ts;
    time_t t = BUILD_TIME;
    ts = *localtime(&t);
    strftime(timestamp,16, "%Y%m%d-%H%M%S", &ts);
    v+=String(timestamp);
    v+=")";
#endif
    s.replace("{t}", title);
    s.replace("{sname}", cfg::fs_ssid);
    s.replace("{id}", esp_chipid());
    s.replace("{mac}", WiFi.macAddress());
    s.replace("{fwt}", FPSTR(INTL_FIRMWARE));
    s.replace("{fw}", v);
    return s;
}

String make_footer(bool configPage) {
    String s;
    if (configPage)
        s = FPSTR(CONFIG_WEB_PAGE_FOOTER);
    else
        s = FPSTR(WEB_PAGE_FOOTER);
    s.replace("{t}", FPSTR(INTL_BACK_TO_HOME));
    return s;
}
//Webserver - current config as JSON (txt) to save
void webserver_config_json() {

    if (!webserver_request_auth())
    { return; }
    String page_content = getMaskedConfigString();
    server.send(200, FPSTR(TXT_CONTENT_TYPE_JSON), page_content);
}


//Webserver - force update with custom URL
void webserver_config_force_update() {

    if (!webserver_request_auth()) { return; }
    String page_content = make_header(FPSTR(INTL_CONFIGURATION));
    if (server.method() == HTTP_POST) {
        if (server.hasArg("ver") && server.hasArg("lg") ) {
            page_content.concat(make_footer());
            server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
            delay(200);
            cfg::auto_update = false;
            writeConfig();
            cfg::auto_update = true;
            String p = F("/NAMF/data/2020-");
            p.concat(server.arg("ver"));
            p.concat(F("/latest_"));
            p.concat(server.arg("lg"));
            p.concat(F(".bin"));
            debug_out(F("Downgrade attempt to: "),DEBUG_ERROR, false);
            debug_out(p, DEBUG_ERROR);
            updateFW(F("fw.nettigo.pl"), F("80"), p);
            delay(5000);
            ESP.restart();
        }
        else {
            server.sendHeader(F("Location"), F("/"));
        }

    }else {

        page_content.concat(F("<h2>Force update</h2>"));
        page_content.concat(
                F("<p>It will disable autoupdate, and try to reinstall older NAMF version. To return to newest version "
                  "just re-eneable autoupdate in config.</p>"
                  "<form method='POST' action='/rollback' style='width:100%;'>")
                  );
        page_content.concat(F("Select version: <select name='ver'><option value='45'>2020-45</option>"));
        page_content.concat(F("Select version: <select name='ver'><option value='44'>2020-44</option>"));
        page_content.concat(F("Select version: <select name='ver'><option value='43'>2020-43</option><option value='42'>2020-42</option></select><br/>"));
        page_content.concat(F("Select language: <select name='lg'><option value='en'>English</option><option value='pl'>Polish</option></select><br/>"));
        page_content.concat(F("<br/>"));
        page_content.concat(form_submit(FPSTR(INTL_SAVE_AND_RESTART)));
        page_content.concat(F("</form>"));
        page_content.concat(make_footer());


    }
    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}


//Webserver - current config as JSON (txt) to save
void webserver_config_json_save() {

    if (!webserver_request_auth())
    { return; }
    String page_content = make_header(FPSTR(INTL_CONFIGURATION));
    if (server.method() == HTTP_POST) {
        if (server.hasArg("json")) {
            if (writeConfigRaw(server.arg("json"),"/test.json")) {
                server.send(500, TXT_CONTENT_TYPE_TEXT_PLAIN,F("Error writing config"));
                return; //we dont have reason to restart, current config was not altered yet
            };
            File tempCfg = SPIFFS.open ("/test.json", "r");
            if (readAndParseConfigFile(tempCfg)) {
                server.send(500, TXT_CONTENT_TYPE_TEXT_PLAIN,F("Error parsing config"));
                delay(500);
                ESP.restart(); // we dont know in what state config is. Maybe something was read maybe not
                return;
            }
            //now config is mix of and new config file. Should be save to save it
            writeConfig();
            server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
            delay(5000);
            Serial.println(F("RESET"));
            ESP.restart();
        }
        else {
            server.sendHeader(F("Location"), F("/"));
        }

    }else {
        page_content += F("<a href=\"/config.json\" target=\"_blank\">Sensor config in JSON</a><br/><br/>");

        page_content += F("<form name=\"json_form\" method='POST' action='/configSave.json' style='width:100%;'>");
        page_content += F("<textarea id=\"json\" name=\"json\" rows=\"10\" cols=\"120\"></textarea><br/>");
        page_content += form_submit(FPSTR(INTL_SAVE_AND_RESTART));
        page_content += F("</form>");
        page_content += make_footer();


    }
    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}


void readPwdParam(char **dst, const String key) {
    if (server.hasArg(key)) {
        String masked_pwd = F("");
        unsigned int respSize = server.arg(key).length();
        masked_pwd.reserve(respSize);
            for (uint8_t i=0;i<respSize;i++)
                masked_pwd += F("*");
            if (masked_pwd != server.arg(key) || server.arg(key) == F("")) {
                stringToChar(dst,server.arg(key));
            }
    }
}
/*****************************************************************
 * Webserver config: show config page                            *
 *****************************************************************/

void webserverConfigBasic(String  & page_content) {
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-macro-parentheses"
void parse_config_request(String &page_content) {
    using namespace cfg;
    String masked_pwd = "";

#define readCharParam(param) \
        if (server.hasArg(#param)){ \
            server.arg(#param).toCharArray(param, sizeof(param)); \
        }

#define readBoolParam(param) \
        param = false; \
        if (server.hasArg(#param)){ \
            (param) = server.arg(#param) == "1";\
        }

#define readIntParam(param) \
        if (server.hasArg(#param)){ \
            param = server.arg(#param).toInt(); \
        }

#define readFloatParam(param) \
        if (server.hasArg(#param)){ \
            param = server.arg(#param).toFloat(); \
        }

#define readTimeParam(param) \
        if (server.hasArg(#param)){ \
            int val = server.arg(#param).toInt(); \
            param = val*1000; \
        }

#define readPasswdParam(param) \
        if (server.hasArg(#param)){ \
            masked_pwd = ""; \
            for (uint8_t i=0;i<server.arg(#param).length();i++) \
                masked_pwd += "*"; \
            if (masked_pwd != server.arg(#param) || server.arg(#param) == "") {\
                server.arg(#param).toCharArray(param, sizeof(param)); \
            }\
        }

    if (server.hasArg(F("wlanssid")) && server.arg(F("wlanssid")) != F("")) {
        if (server.hasArg(F("wlanssid"))){
            stringToChar(&wlanssid,server.arg(F("wlanssid")));
        }
        readPwdParam(&wlanpwd,F("wlanpwd"));
    }
    if (server.hasArg(F("fbssid")) && server.arg(F("fbssid")) != F("")) {
        if (server.hasArg(F("fbssid"))){
            stringToChar(&fbssid,server.arg(F("fbssid")));
        }
        readPwdParam(&fbpwd,F("fbpwd"));
    }
    //always allow to change output power
    readFloatParam(outputPower);
    readIntParam(phyMode);

    readCharParam(current_lang);

    if (server.hasArg(F("www_username"))){
        stringToChar(&www_username,server.arg(F("www_username")));
    }
    readPwdParam(&www_password,F("www_password"));

//            readPasswdParam(www_password);
    readBoolParam(www_basicauth_enabled);
    if (server.hasArg(F("fs_ssid"))){
        stringToChar(&fs_ssid,server.arg(F("fs_ssid")));
    }
    if (server.hasArg(F("fs_pwd")) &&
        ((server.arg(F("fs_pwd")).length() > 7) || (server.arg(F("fs_pwd")).length() == 0))) {
        readPwdParam(&fs_pwd,F("fs_pwd"));
    }
#ifdef NAM_LORAWAN

    parseHTTP(F("lw_enable"), lw_en);
    parseHTTP(F("lw_d_eui"), lw_d_eui);
    parseHTTP(F("lw_a_eui"), lw_a_eui);
    parseHTTP(F("lw_app_key"), lw_app_key);
//    parseHTTP(F("lw_nws_key"), lw_nws_key);
//    parseHTTP(F("lw_apps_key"), lw_apps_key);
//    parseHTTP(F("lw_dev_addr"), lw_dev_addr);

#endif

    readBoolParam(send2dusti);
    readBoolParam(ssl_dusti);
    readBoolParam(send2madavi);
    readBoolParam(ssl_madavi);
    readBoolParam(dht_read);
    readBoolParam(sds_read);
    readBoolParam(pms_read);
    readBoolParam(bmp280_read);
    readBoolParam(bme280_read);
    readBoolParam(heca_read);
    readBoolParam(ds18b20_read);
    readBoolParam(gps_read);

    readIntParam(debug);
    readTimeParam(sending_intervall_ms);
    readTimeParam(time_for_wifi_config);

    readBoolParam(send2csv);

    readBoolParam(send2fsapp);

    readBoolParam(send2sensemap);
    readCharParam(senseboxid);

    readBoolParam(send2custom);
    parseHTTP(F("host_custom"), host_custom);
    parseHTTP(F("url_custom"), url_custom);

    readIntParam(port_custom);
    readCharParam(user_custom);
    readPasswdParam(pwd_custom);
    if (server.hasArg(F("user_custom"))){
        stringToChar(&user_custom,server.arg(F("user_custom")));
    }
    if (server.hasArg(F("pwd_custom"))) {
        readPwdParam(&pwd_custom,F("pwd_custom"));
    }
    readBoolParam(send2aqi);
    parseHTTP(F("token_AQI"), token_AQI);

    readBoolParam(send2influx);

    parseHTTP(F("host_influx"), host_influx);
    parseHTTP(F("url_influx"), url_influx);

    readIntParam(port_influx);

    if (server.hasArg(F("user_influx"))){
        stringToChar(&user_influx,server.arg(F("user_influx")));
    }
    if (server.hasArg(F("pwd_custom"))) {
        readPwdParam(&pwd_custom,F("pwd_custom"));
    }


    readBoolParam(auto_update);
    parseHTTP(F("channel"), update_channel);

    readBoolParam(has_display);
    has_lcd1602 = false;
    has_lcd1602_27 = false;
    has_lcd2004_27 = false;
    has_lcd2004_3f = false;
    if (server.hasArg("has_lcd")) {
        switch (server.arg("lcd_type").toInt()) {
            case 1:
                has_lcd1602_27 = true;
                break;
            case 2:
                has_lcd1602 = true;
                break;
            case 3:
                has_lcd2004_27 = true;
                break;
            case 4:
                has_lcd2004_3f = true;
                break;
        }
    }
    readBoolParam(show_wifi_info);
    readBoolParam(sh_dev_inf);
    readBoolParam(has_ledbar_32);

#undef readCharParam
#undef readBoolParam
#undef readIntParam
#undef readTimeParam
#undef readPasswdParam


    page_content.concat(formSectionHeader(FPSTR(INTL_SENSOR_IS_REBOOTING), 1));

}
#pragma clang diagnostic pop

void tabTemplate(String &page_content, const __FlashStringHelper *id, const __FlashStringHelper *name){
    page_content.concat(F("<button class=\"tablinks\" onclick=\"tab(event, '"));
    page_content.concat(id);
    page_content.concat(F("')\">"));
    page_content.concat(name);
    page_content.concat(F("</button>\n"));
}
void webserver_config(){
    if (!webserver_request_auth()) { return; }

    server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
    server.sendHeader(F("Pragma"), F("no-cache"));
    server.sendHeader(F("Expires"), F("0"));

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    String page_content;
    page_content.reserve(5000);
    page_content = make_header(FPSTR(INTL_CONFIGURATION), true);
    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
    page_content = F("");

    last_page_load = millis();

    debug_out(F("output config page ..."), DEBUG_MIN_INFO, 1);
    if (wificonfig_loop) {  // scan for wlan ssids
        page_content += FPSTR(WEB_CONFIG_SCRIPT);
    }
//    webserverPartialSend(page_content);

    using namespace cfg;
//    webserverPartialSend(page_content);
    if (server.method() == HTTP_GET) {
        page_content.concat(F("<div class=\"tab\">\n"));
        tabTemplate(page_content, F("basic"), FPSTR(INTL_TAB_BASIC));
        tabTemplate(page_content, F("api"), FPSTR(INTL_TAB_API));
        tabTemplate(page_content, F("adv"), FPSTR(INTL_TAB_ADVANCED));
        tabTemplate(page_content, F("sensors"), FPSTR(INTL_TAB_SENSORS));

        page_content.concat(F("</div>"));
        page_content.concat(F("<form method='POST' id='oldForm' action='/config'  >"));
        page_content.concat(F("<div id='basic' class='tabcontent'>"));
        page_content.concat(F("<div class='gc'>"));
        page_content.concat( formSectionHeader(FPSTR(INTL_WIFI_SETTINGS)));
        debug_out(F("output config page 1"), DEBUG_MIN_INFO, 1);
        if (wificonfig_loop) {  // scan for wlan ssids
            page_content.concat(F("<div class='row'><button onclick='load_wifi_list();return false'>Refresh</button>"));
            page_content.concat(F("</div>"));
            page_content.concat(F("<div id='wifilist' class='row'>"));
            page_content.concat(FPSTR(INTL_WIFI_NETWORKS));
            page_content.concat(F("</div>"));
            page_content.concat(F("<script>load_wifi_list()</script>"));
        }

        page_content.concat(formInputGrid(F("wlanssid"), FPSTR(INTL_FS_WIFI_NAME), wlanssid,
                                       35));
        if (!wificonfig_loop) {
            page_content.concat(formPasswordGrid(F("wlanpwd"), FPSTR(INTL_PASSWORD), wlanpwd,
                                              65));
        } else {
            page_content.concat(formInputGrid(F("wlanpwd"), FPSTR(INTL_PASSWORD), F(""),
                                           65));
        }

        page_content.concat(formSectionHeader(FPSTR(INTL_BASICAUTH)));
        page_content.concat(formCheckboxGrid("www_basicauth_enabled", FPSTR(INTL_ENABLE), www_basicauth_enabled));

        page_content.concat(formInputGrid(F("www_username"), FPSTR(INTL_USER), www_username,
                                          30));
        page_content.concat(formPasswordGrid(F("www_password"), FPSTR(INTL_PASSWORD), www_password,
                                             30));


        page_content.concat(formSectionHeader(FPSTR(INTL_MORE_SETTINGS)));

        page_content.concat(formCheckboxOpenGrid("has_lcd", FPSTR(INTL_LCD),
                                                 has_lcd1602 || has_lcd1602_27 || has_lcd2004_3f || has_lcd2004_27));
        page_content.concat(F("<div class='c2'><select name=\"lcd_type\">"));
        page_content.concat(form_option("1", FPSTR(INTL_LCD1602_27), has_lcd1602_27));
        page_content.concat(form_option("2", FPSTR(INTL_LCD1602_3F), has_lcd1602));
        page_content.concat(form_option("3", FPSTR(INTL_LCD2004_27), has_lcd2004_27));
        page_content.concat(form_option("4", FPSTR(INTL_LCD2004_3F), has_lcd2004_3f));
        page_content.concat(F("</select></div>"));
        page_content.concat(formCheckboxGrid("show_wifi_info", FPSTR(INTL_SHOW_WIFI_INFO), show_wifi_info));
        page_content.concat(formCheckboxGrid("sh_dev_inf", FPSTR(INTL_SHOW_DEVICE_INFO), sh_dev_inf));


        webserverPartialSend(page_content);


        page_content.concat(form_select_lang());
        page_content.concat(formInputGrid("sending_intervall_ms", FPSTR(INTL_MEASUREMENT_INTERVAL),
                                       String(sending_intervall_ms / 1000), 5));
        page_content.concat(formSubmitGrid(FPSTR(INTL_SAVE_AND_RESTART)));

        page_content.concat(F("</div>"));   //grid
        page_content.concat(F("</div>"));   //tabcontent


        page_content.concat(F("<div id='api' class='tabcontent'>"));
        page_content.concat(F("<div class='gc'>"));
        formSectionHeader(page_content, tmpl(FPSTR(INTL_SEND_TO), F("API Sensor Community")));

        page_content.concat(formCheckboxGrid("send2dusti", FPSTR(INTL_ENABLE), send2dusti));
        page_content.concat(formCheckboxGrid("ssl_dusti", FPSTR(INTL_USE_HTTPS), ssl_dusti));
        formSectionHeader(page_content, tmpl(FPSTR(INTL_SEND_TO), F("API Madavi.de")));
        page_content.concat(formCheckboxGrid("send2madavi", FPSTR(INTL_ENABLE), send2madavi));
        page_content.concat(formCheckboxGrid("ssl_madavi", FPSTR(INTL_USE_HTTPS), ssl_madavi));

        webserverPartialSend(page_content);

#ifdef NAM_LORAWAN
        formSectionHeader(page_content, "LoRaWAN setup");
        page_content.concat(formCheckboxGrid(F("lw_enable"), FPSTR(INTL_ENABLE), lw_en));
        page_content.concat(formInputGrid(F("lw_a_eui"), "App EUI", lw_a_eui, 60));
        page_content.concat(formInputGrid(F("lw_d_eui"), "Device EUI", lw_d_eui, 60));
        page_content.concat(formInputGrid(F("lw_app_key"), "App key", lw_app_key, 60));
//        page_content.concat(formInputGrid(F("lw_nws_key"), "Nws key", lw_nws_key, 60));
//        page_content.concat(formInputGrid(F("lw_apps_key"), "Apps key", lw_apps_key, 60));
//        page_content.concat(formInputGrid(F("lw_dev_addr"), "Device address", lw_dev_addr, 60));
#endif

        formSectionHeader(page_content, tmpl(FPSTR(INTL_SEND_TO), FPSTR(INTL_AQI_ECO_API)));
        page_content.concat(formCheckboxGrid("send2aqi", FPSTR(INTL_ENABLE), send2aqi));
        page_content.concat(formInputGrid(F("token_AQI"), FPSTR(INTL_AQI_TOKEN), token_AQI, 60));


        formSectionHeader(page_content, tmpl(FPSTR(INTL_SEND_TO), FPSTR(INTL_SEND_TO_OWN_API)));



        page_content.concat(formCheckboxGrid("send2custom", FPSTR(INTL_ENABLE), send2custom));
        page_content.concat(formInputGrid(F("host_custom"), FPSTR(INTL_SERVER), host_custom, 60));
        page_content.concat(formInputGrid(F("url_custom"), FPSTR(INTL_PATH), url_custom, 60));
        constexpr int max_port_digits = constexprstrlen("65535");
        page_content.concat(formInputGrid(F("port_custom"), FPSTR(INTL_PORT), String(port_custom), max_port_digits));
        page_content.concat(formInputGrid(F("user_custom"), FPSTR(INTL_USER), user_custom,
                                       35));
        page_content.concat(formPasswordGrid("pwd_custom", FPSTR(INTL_PASSWORD), pwd_custom,
                                          35));

        webserverPartialSend(page_content);

        formSectionHeader(page_content, tmpl(FPSTR(INTL_SEND_TO), F("InfluxDB")));
        page_content.concat(formCheckboxGrid("send2influx", FPSTR(INTL_ENABLE), send2influx));
        page_content.concat(formInputGrid(F("host_influx"), FPSTR(INTL_SERVER), host_influx, 60));
        page_content.concat(formInputGrid(F("url_influx"), FPSTR(INTL_PATH), url_influx, 60));
        page_content.concat(formInputGrid("port_influx", FPSTR(INTL_PORT), String(port_influx), max_port_digits));
        page_content.concat(formInputGrid(F("user_influx"), FPSTR(INTL_USER), user_influx,
                                       35));
        page_content.concat(formPasswordGrid(F("pwd_influx"), FPSTR(INTL_PASSWORD), pwd_influx,
                                          35));

        formSectionHeader(page_content,  FPSTR(INTL_OTHER_APIS));

        page_content.concat(formCheckboxGrid("send2csv", tmpl(FPSTR(INTL_SEND_TO), F("CSV")), send2csv));
        page_content.concat(formCheckboxGrid("send2fsapp", tmpl(FPSTR(INTL_SEND_TO), F("Feinstaub-App")), send2fsapp));
        formSectionHeader(page_content, tmpl(FPSTR(INTL_SEND_TO), F("OpenSenseMap")));
        page_content.concat(
                formCheckboxGrid("send2sensemap", FPSTR(INTL_ENABLE), send2sensemap));
        page_content.concat(formInputGrid("senseboxid", "senseBox-ID: ", senseboxid,
                                          capacity_null_terminated_char_array(senseboxid)));

        page_content.concat(formSubmitGrid(FPSTR(INTL_SAVE_AND_RESTART)));
        page_content.concat(F("</div>"));   //grid
        page_content.concat(F("</div>"));   //tabcontent

        webserverPartialSend(page_content);


        page_content.concat(F("<div id='adv' class='tabcontent'>"));
        page_content.concat(F("<div class='gc'>"));
        formSectionHeader(page_content, FPSTR(INTL_MORE_SETTINGS));
        page_content.concat(formSectionHeader(FPSTR(INTL_FALBACK_WIFI)));

        page_content.concat(formInputGrid(F("fbssid"), FPSTR(INTL_FS_WIFI_NAME), fbssid,
                                          35));
        page_content.concat(formPasswordGrid(F("fbpwd"), FPSTR(INTL_PASSWORD), fbpwd,
                                             65));
        page_content.concat(formInputGrid("debug", FPSTR(INTL_DEBUG_LEVEL), String(debug), 1));
        page_content.concat(formInputGrid("time_for_wifi_config", FPSTR(INTL_DURATION_ROUTER_MODE),
                                          String(time_for_wifi_config / 1000), 5));
        page_content.concat(formInputGrid("outputPower", FPSTR(INTL_WIFI_TX_PWR), String(outputPower), 5));
        page_content.concat(formInputGrid("phyMode", FPSTR(INTL_WIFI_PHY_MODE), String(phyMode), 5));

        page_content.concat(formSectionHeader(FPSTR(INTL_FS_WIFI)));
        page_content.concat(formSectionHeader(FPSTR(INTL_FS_WIFI_DESCRIPTION), 3));

        page_content.concat(formInputGrid(F("fs_ssid"), FPSTR(INTL_FS_WIFI_NAME), fs_ssid,
                                          35));
        page_content.concat(formPasswordGrid(F("fs_pwd"), FPSTR(INTL_PASSWORD), fs_pwd,
                                             65));



        webserverPartialSend(page_content);

        page_content.concat(formCheckboxOpenGrid("auto_update", FPSTR(INTL_AUTO_UPDATE), auto_update));
        page_content.concat(F("<div class='c2'><select name=\"channel\">"));
        page_content.concat(form_option(String(UPDATE_CHANNEL_STABLE), FPSTR(INTL_UPDATE_STABLE),
                                        update_channel == UPDATE_CHANNEL_STABLE));
        page_content.concat(form_option(String(UPDATE_CHANNEL_BETA), FPSTR(INTL_UPDATE_BETA),
                                        update_channel == UPDATE_CHANNEL_BETA));
        page_content.concat(form_option(String(UPDATE_CHANNEL_ALFA), FPSTR(INTL_UPDATE_ALFA),
                                        update_channel == UPDATE_CHANNEL_ALFA));
        page_content.concat(F("</select></div>"));
        page_content.concat(formCheckboxGrid("has_display", FPSTR(INTL_DISPLAY), has_display));
        page_content.concat(formCheckboxGrid("has_ledbar_32", FPSTR(INTL_LEDBAR_32), has_ledbar_32));
        page_content.concat(formCheckboxGrid("send_diag", FPSTR(INTL_DIAGNOSTIC), send_diag));

        webserverPartialSend(page_content);

        page_content.concat(formSubmitGrid(FPSTR(INTL_SAVE_AND_RESTART)));

        page_content.concat(F("</div>")); //grid
        page_content.concat(F("</div>")); //tabcontent

        page_content.concat(F("<div id='sensors' class='tabcontent'>"));
        page_content.concat(F("<div class='gc'>"));

        page_content.concat(formSectionHeader(FPSTR(INTL_SENSORS)));
        page_content.concat(formCheckboxGrid("pms_read", FPSTR(INTL_PMS), pms_read));
        page_content.concat(formCheckboxGrid("dht_read", FPSTR(INTL_DHT22), dht_read));
        page_content.concat(formCheckboxGrid("ds18b20_read", FPSTR(INTL_DS18B20), ds18b20_read));
        page_content.concat(formCheckboxGrid("gps_read", FPSTR(INTL_NEO6M), gps_read));
        page_content.concat(formSubmitGrid(FPSTR(INTL_SAVE_AND_RESTART)));
        page_content.concat(F("</form>"));

        page_content.concat(F("</div>"));
        scheduler.getConfigForms(page_content);
        page_content.concat(F("</div>"));
        webserverPartialSend(page_content);


    } else {
        parse_config_request(page_content);
    }
    webserverPartialSend(page_content);
//    page_content.concat(F("<br>"));
//    page_content.concat(String(maxSizeTemp));
    page_content.concat(F("<script>const beforeUnloadListener = (event) => {\n"
                          "  event.preventDefault();\n"
                          "  return event.returnValue = \"Are you sure you want to exit?\";"
                          "};"
                          "function warn(){addEventListener(\"beforeunload\", beforeUnloadListener, {capture: true});};\n"
                          "function submit(){removeEventListener(\"beforeunload\", beforeUnloadListener, {capture: true});};\n"
                          "document.querySelectorAll(\"#oldForm input:not(#ncf input)\").forEach(e=>{e.addEventListener(\"change\",warn)});\n"
                          "document.querySelector(\"#oldForm\").addEventListener(\"submit\",submit,false);\n"
                          "</script>"));
    page_content.concat(make_footer(true));
    webserverPartialSend(page_content);
    endPartialSend();

    if (server.method() == HTTP_POST) {
        debug_out(F("Writing config and restarting"), DEBUG_MIN_INFO, true);
        display_debug(F("Writing config"), F("and restarting"));
        writeConfig();
        delay(500);
        ESP.restart();
    }
    if (wificonfig_loop) {

    }

//send.
}

/**************************************************************************
 * Parse sensor config - new, simple scheduler
 **************************************************************************/

void webserver_simple_config() {

    if (!webserver_request_auth()) { return; }

    String page_content = make_header(FPSTR(INTL_CONFIGURATION));
    last_page_load = millis();

    debug_out(F("output config page ..."), DEBUG_MIN_INFO, 1);

//    if (server.method() == HTTP_POST) {

        if (server.hasArg(F("sensor"))) {
            SimpleScheduler::LoopEntryType sensor;
            sensor = static_cast<SimpleScheduler::LoopEntryType>(server.arg(F("sensor")).toInt());
            JsonObject &ret = SimpleScheduler::parseHTTPConfig(sensor);
            ret.printTo(Serial);
            if (ret.containsKey(F("err"))){
                page_content += F("<h2>");
                page_content += String(ret.get<char *>(F("err")) );//ret.get<char *>(F("err"));
                page_content += F("</h2>");

            } else {
                SimpleScheduler::readConfigJSON(sensor, ret);
                page_content.concat("<h3>");
                page_content.concat(SimpleScheduler::findSlotKey(sensor));
                page_content.concat("</h3>");
                page_content.concat(FPSTR(INTL_CONFIG_SAVED));
            }

            writeConfig();

        }

        page_content += make_footer();

        server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
        server.sendHeader(F("Pragma"), F("no-cache"));
        server.sendHeader(F("Expires"), F("0"));
        server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
//    } else {
//        webserver_not_found();
//    }
}


String table_row_from_value(const String &sensor, const String &param, const String &value, const String &unit) {
    String s = F("<tr>"
                 "<td>{s}</td>"
                 "<td>{p}</td>"
                 "<td class='r'>{v}&nbsp;{u}</td>"
                 "</tr>");
    s.replace("{s}", sensor);
    s.replace("{p}", param);
    s.replace("{v}", value);
    s.replace("{u}", unit);
    return s;
}


/*****************************************************************
 * Webserver wifi: show available wifi networks                  *
 *****************************************************************/
void webserver_wifi() {
    if (server.hasArg(String(F("r"))) || count_wifiInfo == -1) {
        debug_out(F("Updating WiFi SSID list...."),DEBUG_ERROR);

        NAMWiFi::rescanWiFi();

    }
    debug_out(F("wifi networks found: "), DEBUG_MIN_INFO, 0);
    debug_out(String(count_wifiInfo), DEBUG_MIN_INFO, 1);
    String page_content = "";
    if (count_wifiInfo == 0) {
        page_content += BR_TAG;
        page_content += FPSTR(INTL_NO_NETWORKS);
        page_content += BR_TAG;
    } else {
        std::unique_ptr<int[]> indices(new int[count_wifiInfo]);
        for (int i = 0; i < count_wifiInfo; i++) {
            indices[i] = i;
        }
        for (int i = 0; i < count_wifiInfo; i++) {
            for (int j = i + 1; j < count_wifiInfo; j++) {
                if (wifiInfo[indices[j]].RSSI > wifiInfo[indices[i]].RSSI) {
                    std::swap(indices[i], indices[j]);
                }
            }
        }
        int duplicateSsids = 0;
        for (int i = 0; i < count_wifiInfo; i++) {
            if (indices[i] == -1) {
                continue;
            }
            for (int j = i + 1; j < count_wifiInfo; j++) {
                if (strncmp(wifiInfo[indices[i]].ssid, wifiInfo[indices[j]].ssid, 35) == 0) {
                    indices[j] = -1; // set dup aps to index -1
                    ++duplicateSsids;
                }
            }
        }

        page_content += FPSTR(INTL_NETWORKS_FOUND);
        page_content += String(count_wifiInfo - duplicateSsids);
        page_content += FPSTR(BR_TAG);
        page_content += FPSTR(BR_TAG);
        page_content += FPSTR(TABLE_TAG_OPEN);
        //if(n > 30) n=30;
        for (int i = 0; i < count_wifiInfo; ++i) {
            if (indices[i] == -1 || wifiInfo[indices[i]].isHidden) {
                continue;
            }
            // Print SSID and RSSI for each network found
#ifdef ARDUINO_ARCH_ESP8266
            page_content += wlan_ssid_to_table_row(wifiInfo[indices[i]].ssid, ((wifiInfo[indices[i]].encryptionType == ENC_TYPE_NONE) ? " " : u8"🔒"), wifiInfo[indices[i]].RSSI);
#else
            page_content += wlan_ssid_to_table_row(wifiInfo[indices[i]].ssid, ((wifiInfo[indices[i]].encryptionType == WIFI_AUTH_OPEN) ? " " : u8"🔒"), wifiInfo[indices[i]].RSSI);
#endif
        }
        page_content += FPSTR(TABLE_TAG_CLOSE_BR);
        page_content += FPSTR(BR_TAG);
    }
    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
 * Webserver root: show latest values                            *
 *****************************************************************/
void webserver_values() {

    String page_content;
    page_content.reserve(4000);
    page_content = make_header(FPSTR(INTL_CURRENT_DATA));
    const String unit_PM = "µg/m³";
    const String unit_T = "°C";
    const String unit_H = "%";
    const String unit_P = "hPa";
    last_page_load = millis();

    debug_out(F("output values to web page..."), DEBUG_MIN_INFO, 1);
    getTimeHeadings(page_content);

    if (cfg::send2madavi) {
        String link(
                F("<a class=\"plain\" href=\"https://api-rrd.madavi.de/grafana/d/GUaL5aZMz/pm-sensors?orgId=1&var-chipID="));
        link.concat(String(F(PROCESSOR_ARCH)));
        link.concat(String(F("-{id}\" target=\"_blank\">{n}</a>")));
        link.replace(F("{id}"), esp_chipid());
        link.replace(F("{n}"), FPSTR(INTL_MADAVI_LINK));
        page_content.concat(link);
    }

    page_content.concat(F("<table cellspacing='0' border='1' cellpadding='5'>"));
    page_content.concat(
            tmpl(F("<tr><th>{v1}</th><th>{v2}</th><th>{v3}</th>"), FPSTR(INTL_SENSOR), FPSTR(INTL_PARAMETER),
                 FPSTR(INTL_VALUE)));
    if (cfg::pms_read) {
        page_content.concat(FPSTR(EMPTY_ROW));
        page_content.concat(
                table_row_from_value(FPSTR(SENSORS_PMSx003), "PM1", check_display_value(last_value_PMS_P0, -1, 1, 0),
                                     unit_PM));
        page_content.concat(
                table_row_from_value(FPSTR(SENSORS_PMSx003), "PM2.5", check_display_value(last_value_PMS_P2, -1, 1, 0),
                                     unit_PM));
        page_content.concat(
                table_row_from_value(FPSTR(SENSORS_PMSx003), "PM10", check_display_value(last_value_PMS_P1, -1, 1, 0),
                                     unit_PM));
    }
    if (cfg::dht_read) {
        page_content.concat(FPSTR(EMPTY_ROW));
        page_content.concat(table_row_from_value(FPSTR(SENSORS_DHT22), FPSTR(INTL_TEMPERATURE),
                                                 check_display_value(last_value_DHT_T, -128, 1, 0), unit_T));
        page_content.concat(table_row_from_value(FPSTR(SENSORS_DHT22), FPSTR(INTL_HUMIDITY),
                                                 check_display_value(last_value_DHT_H, -1, 1, 0), unit_H));
    }


    if (cfg::ds18b20_read) {
        page_content.concat(FPSTR(EMPTY_ROW));
        page_content.concat(table_row_from_value(FPSTR(SENSORS_DS18B20), FPSTR(INTL_TEMPERATURE),
                                                 check_display_value(last_value_DS18B20_T, -128, 1, 0), unit_T));
    }
    if (cfg::gps_read) {
        page_content.concat(FPSTR(EMPTY_ROW));
        page_content.concat(table_row_from_value(F("GPS"), FPSTR(INTL_LATITUDE),
                                                 check_display_value(last_value_GPS_lat, -200.0, 6, 0), "°"));
        page_content.concat(table_row_from_value(F("GPS"), FPSTR(INTL_LONGITUDE),
                                                 check_display_value(last_value_GPS_lon, -200.0, 6, 0), "°"));
        page_content.concat(table_row_from_value(F("GPS"), FPSTR(INTL_ALTITUDE),
                                                 check_display_value(last_value_GPS_alt, -1000.0, 2, 0), "m"));
        page_content.concat(table_row_from_value(F("GPS"), FPSTR(INTL_DATE), last_value_GPS_date, ""));
        page_content.concat(table_row_from_value(F("GPS"), FPSTR(INTL_TIME), last_value_GPS_time, ""));
    }
    SimpleScheduler::getResultsAsHTML(page_content);

    page_content.concat(FPSTR(EMPTY_ROW));
    page_content.concat(table_row_from_value(F("NAM"), FPSTR(INTL_NUMBER_OF_MEASUREMENTS), String(count_sends), ""));
    page_content.concat(table_row_from_value(F("NAM"), F("Uptime"), millisToTime(millis()), ""));

    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
 * Webserver set debug level                                     *
 *****************************************************************/
const char *DEBUG_NAMES[] PROGMEM = {INTL_NONE, INTL_ERROR, INTL_WARNING, INTL_MIN_INFO, INTL_MED_INFO, INTL_MAX_INFO};

void webserver_debug_level() {

    if (!webserver_request_auth()) { return; }

    String page_content = make_header(FPSTR(INTL_DEBUG_LEVEL));
    last_page_load = millis();
    debug_out(F("output change debug level page..."), DEBUG_MIN_INFO, 1);
    page_content.concat(F("<div id='logField' style='height:300px;font-family:monospace;overflow-y:scroll;white-space:break-spaces'>"));
    page_content.concat(Debug.popLines());
    page_content.concat(("</div>\n"));
    page_content.concat(FPSTR(DEBUG_JS));
    page_content.concat(FPSTR(WEB_DEBUG_PAGE_CONTENT));
    page_content.replace(F("{debug_level}"), FPSTR(INTL_DEBUG_LEVEL));
    page_content.replace(F("{none}"), FPSTR(INTL_NONE));
    page_content.replace(F("{error}"), FPSTR(INTL_ERROR));
    page_content.replace(F("{warning}"), FPSTR(INTL_WARNING));
    page_content.replace(F("{min_info}"), FPSTR(INTL_MIN_INFO));
    page_content.replace(F("{med_info}"), FPSTR(INTL_MED_INFO));
    page_content.replace(F("{max_info}"), FPSTR(INTL_MAX_INFO));


    if (server.hasArg("lvl")) {
        const int lvl = server.arg("lvl").toInt();
        if (lvl >= 0 && lvl <= 5) {
            cfg::debug = lvl;
            page_content.concat(F("<h3>"));
            page_content += FPSTR(INTL_DEBUG_SETTING_TO);
            page_content.concat(F(" "));
            page_content += FPSTR(DEBUG_NAMES[lvl]);
            page_content += F(".</h3>");
        }
    } else {
        page_content.concat(F("<h3>"));
        page_content.concat(FPSTR(INTL_DEBUG_STATUS));
        page_content.concat(F(" "));
        page_content.concat(FPSTR(DEBUG_NAMES[cfg::debug]));
        page_content.concat(F(".</h3>"));
    }
    page_content += make_footer();
    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
 * Webserver enable ota                                          *
 *****************************************************************/
 void webserver_enable_ota() {
    String page_content;
    if (cfg::www_basicauth_enabled) {
        if (!webserver_request_auth()) { return; }
        page_content = make_header(FPSTR(INTL_ENABLE_OTA));

        last_page_load = millis();
        enable_ota_time = millis() + 60 * 1000;
        ArduinoOTA.setPassword(cfg::www_password);
#ifdef ARDUINO_ARCH_ESP8266
        ArduinoOTA.begin(true);
#else

        ArduinoOTA.setMdnsEnabled(true);
        ArduinoOTA.setHostname(cfg::fs_ssid);
        ArduinoOTA.begin();
#endif
        page_content += FPSTR(INTL_ENABLE_OTA_INFO);

        page_content += make_footer();
    } else {
        page_content = make_header(FPSTR(INTL_ENABLE_OTA));
        page_content += FPSTR(INTL_ENABLE_OTA_REFUSE);
        page_content += make_footer();

    }
    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
 * Webserver remove config                                       *
 *****************************************************************/
void webserver_removeConfig() {
    if (!webserver_request_auth()) { return; }

    String page_content = make_header(FPSTR(INTL_DELETE_CONFIG));
    String message_string = F("<h3>{v}.</h3>");
    last_page_load = millis();
    debug_out(F("output remove config page..."), DEBUG_MIN_INFO, 1);

    if (server.method() == HTTP_GET) {
        page_content += FPSTR(WEB_REMOVE_CONFIG_CONTENT);
        page_content.replace("{t}", FPSTR(INTL_CONFIGURATION_REALLY_DELETE));
        page_content.replace("{b}", FPSTR(INTL_DELETE));
        page_content.replace("{c}", FPSTR(INTL_CANCEL));

    } else {
        if (SPIFFS.exists("/config.json")) {	//file exists
            debug_out(F("removing config.json..."), DEBUG_MIN_INFO, 1);
            if (SPIFFS.remove("/config.json")) {
                page_content += tmpl(message_string, FPSTR(INTL_CONFIG_DELETED));
            } else {
                page_content += tmpl(message_string, FPSTR(INTL_CONFIG_CAN_NOT_BE_DELETED));
            }
        } else {
            page_content += tmpl(message_string, FPSTR(INTL_CONFIG_NOT_FOUND));
        }
    }
    page_content += make_footer();
    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
 * Webserver reset NodeMCU                                       *
 *****************************************************************/
void webserver_reset() {
    if (!webserver_request_auth()) { return; }

    String page_content = make_header(FPSTR(INTL_RESTART_SENSOR));
    last_page_load = millis();
    debug_out(F("output reset NodeMCU page..."), DEBUG_MIN_INFO, 1);

    if (server.method() == HTTP_GET) {
        page_content += FPSTR(WEB_RESET_CONTENT);
        page_content.replace("{t}", FPSTR(INTL_REALLY_RESTART_SENSOR));
        page_content.replace("{b}", FPSTR(INTL_RESTART));
        page_content.replace("{c}", FPSTR(INTL_CANCEL));
    } else {
        String page_content = make_header(FPSTR(INTL_SENSOR_IS_REBOOTING));
        page_content += F("<p>");
        page_content += FPSTR(INTL_SENSOR_IS_REBOOTING_NOW);
        page_content += F("</p>");
        page_content += make_footer();
        server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
        debug_out(F("restarting..."), DEBUG_MIN_INFO, 1);
        delay(300);
        ESP.restart();
    }
    page_content += make_footer();
    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}



/********************************
 *
 * Display status page
 *
 *********************************/

void webserver_status_page(void) {
    if (!webserver_request_auth()) { return; }

    const int signal_quality = calcWiFiSignalQuality(WiFi.RSSI());

    debug_out(F("output status page"), DEBUG_MIN_INFO, 1);

    String page_content = make_header(FPSTR(INTL_STATUS_PAGE));
    page_content.reserve(6000);
    getTimeHeadings(page_content);
    page_content.concat(F("<table cellspacing='0' border='1' cellpadding='5'>"));
    page_content.concat(FPSTR(EMPTY_ROW));
    String I2Clist = F("");
    for (uint8_t addr = 0x07; addr <= 0x7F; addr++) {
        // Address the device
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            I2Clist += String(addr, 16);
            I2Clist += F(", ");
        }
    }
    page_content.concat(table_row_from_value(F("I2C"), FPSTR(INTL_I2C_BUS), I2Clist, F("")));

    SimpleScheduler::getStatusReport(page_content);
    // Check for ACK (detection of device), NACK or error
    page_content.concat(FPSTR(EMPTY_ROW));
    if (sntp_time_is_set) {
        time_t now = time(nullptr);
        String tmp = (ctime(&now));
        page_content.concat(table_row_from_value(F("WiFi"), FPSTR(INTL_NTP_TIME), tmp, F("")));
    } else {
        page_content.concat(table_row_from_value(F("WiFi"), FPSTR(INTL_NTP_TIME), FPSTR(INTL_NTP_TIME_NOT_ACC),
                                             F("")));
    }
    page_content.concat(table_row_from_value(F("WiFi"), FPSTR(INTL_SIGNAL_STRENGTH), String(WiFi.RSSI()), "dBm"));
    page_content.concat(table_row_from_value(F("WiFi"), FPSTR(INTL_SIGNAL_QUALITY), String(signal_quality), "%"));
    page_content.concat(table_row_from_value(F("WiFi"), F("Status"), String(NAMWiFi::state), ""));
    page_content.concat(FPSTR(EMPTY_ROW));
    page_content.concat(table_row_from_value(F("NAM"), FPSTR(INTL_NUMBER_OF_MEASUREMENTS), String(count_sends), ""));
    page_content.concat(table_row_from_value(F("NAM"), F("Uptime"), millisToTime(millis()), ""));
    page_content.concat(table_row_from_value(F("NAM"), FPSTR(INTL_TIME_FROM_UPDATE), millisToTime(msSince(last_update_attempt)), ""));
    page_content.concat(table_row_from_value(F("NAM"), F("Internet connection"),String(cfg::internet), ""));
    page_content.concat(FPSTR(EMPTY_ROW));
//    page_content.concat(table_row_from_value(F("NAMF"),F("LoopEntries"), String(SimpleScheduler::NAMF_LOOP_SIZE) ,""));
    page_content.concat(table_row_from_value(F("NAMF"),F("Sensor slots"), String(scheduler.sensorSlots()) ,""));
    page_content.concat(table_row_from_value(F("NAMF"),F("Free slots"), String(scheduler.freeSlots()) ,""));

    page_content.concat(table_row_from_value(F("NAMF"),F("Sensors"), scheduler.registeredNames() ,""));
#ifdef DBG_NAMF_TIMES
    page_content.concat(table_row_from_value(F("NAMF"),F("Max loop time <a style='display:inline;padding:initial' href='/time'>(reset)</a>"), String(scheduler.runTimeMax()) ,F("µs")));
    page_content.concat(table_row_from_value(F("NAMF"),F("Max time for"), scheduler.maxRunTimeSystemName() ,F("")));
    page_content.concat(table_row_from_value(F("NAMF"),F("Last loop time"), String(scheduler.lastRunTime()) ,F("µs")));
#endif
    page_content.concat(FPSTR(EMPTY_ROW));
#ifdef ARDUINO_ARCH_ESP8266
    page_content.concat(table_row_from_value(F("ESP"),F("Reset Reason"), String(ESP.getResetReason()),""));
#endif
    page_content.concat(table_row_from_value(F("ESP"),F("Processor"), F(PROCESSOR_ARCH),""));
    String tmp = String(memoryStatsMin.maxFreeBlock) + String("/") + String(memoryStatsMax.maxFreeBlock);
    page_content.concat(table_row_from_value(F("ESP"),F("Max Free Block Size"), tmp,"B"));
    tmp = String(memoryStatsMin.frag) + String("/") + String(memoryStatsMax.frag);
    page_content.concat(table_row_from_value(F("ESP"),F("Heap Fragmentation"), tmp,"%"));
    tmp = String(memoryStatsMin.freeContStack) + String("/") + String(memoryStatsMax.freeContStack);
    page_content.concat(table_row_from_value(F("ESP"),F("Free Cont Stack"), tmp,"B"));
    tmp = String(memoryStatsMin.freeHeap) + String("/") + String(memoryStatsMax.freeHeap);
    page_content.concat(table_row_from_value(F("ESP"),F("Free Memory"), tmp,"B"));
#ifdef ARDUINO_ARCH_ESP8266
    page_content.concat(table_row_from_value(F("ESP"),F("Flash ID"), String(ESP.getFlashChipId()),""));
    page_content.concat(table_row_from_value(F("ESP"),F("Flash Vendor ID"), String(ESP.getFlashChipVendorId()),""));
#endif
    page_content.concat(table_row_from_value(F("ESP"),F("Flash Speed"), String(ESP.getFlashChipSpeed()/1000000.0),"MHz"));
    page_content.concat(table_row_from_value(F("ESP"),F("Flash Mode"), String(ESP.getFlashChipMode()),""));
    page_content.concat(FPSTR(EMPTY_ROW));
#ifdef ARDUINO_ARCH_ESP8266
    page_content.concat(table_row_from_value(F("ENV"),F("Core version"), String(ESP.getCoreVersion()),""));
#endif
    page_content.concat(table_row_from_value(F("ENV"),F("SDK version"), String(ESP.getSdkVersion()),""));
    page_content.concat(FPSTR(EMPTY_ROW));
    String dbg = F("");
#ifdef DBG_NAMF_TIMES
    dbg.concat("NAMF_TIMES ");
#endif
#ifdef DBG_NAMF_SDS_NO_DATA
    dbg.concat("SDS_NO_DATA ");
#endif

    page_content.concat(table_row_from_value(F("DEBUG"),F("Debug options"), dbg, F("")));

    page_content.concat(FPSTR(TABLE_TAG_CLOSE_BR));
    page_content.concat(make_footer());

    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);

}

/*****************************************************************
 * Webserver data.json                                           *
 *****************************************************************/
void webserver_data_json() {
    String s1 = "";
    unsigned long age = 0;
    debug_out(F("output data json..."), DEBUG_MIN_INFO, 1);
    if (first_cycle) {
        s1 = FPSTR(data_first_part);
        s1.replace("{v}", String(SOFTWARE_VERSION));
        s1 += "]}";
        age = cfg::sending_intervall_ms - msSince(starttime);
        if (age > cfg::sending_intervall_ms) {
            age = 0;
        }
        age = 0 - age;
    } else {
        s1 = last_data_string;
        debug_out(F("last data: "), DEBUG_MIN_INFO, 0);
        debug_out(s1, DEBUG_MIN_INFO, 1);
        age = msSince(starttime);
        if (age > cfg::sending_intervall_ms) {
            age = 0;
        }
    }
    String s2 = F(", \"age\":\"");
    s2 += String((long)((age + 500) / 1000));
    s2 += String(F("\", \"measurements\":\""));
    s2 += String((unsigned long)(count_sends));
    s2 += String(F("\", \"uptime\":\""));
    s2 += String((unsigned long)(millis()/1000));
    s2 += F("\", \"sensordatavalues\"");
    debug_out(F("replace with: "), DEBUG_MIN_INFO, 0);
    debug_out(s2, DEBUG_MIN_INFO, 1);
    s1.replace(F(", \"sensordatavalues\""), s2);
    debug_out(F("replaced: "), DEBUG_MIN_INFO, 0);
    debug_out(s1, DEBUG_MIN_INFO, 1);
    server.send(200, FPSTR(TXT_CONTENT_TYPE_JSON), s1);
}

/*****************************************************************
 * Webserver prometheus metrics endpoint                         *
 *****************************************************************/
void webserver_prometheus_endpoint() {
    debug_out(F("output prometheus endpoint..."), DEBUG_MIN_INFO, 1);
    String data_4_prometheus = F("software_version{version=\"{ver}\",{id}} 1\nuptime_ms{{id}} {up}\nsending_intervall_ms{{id}} {si}\nnumber_of_measurements{{id}} {cs}\n");
    String id = F("node=\")");
    id.concat(String(F(PROCESSOR_ARCH)));
    id.concat(String(F("-")));
    id += esp_chipid() + "\"";
    debug_out(F("Parse JSON for Prometheus"), DEBUG_MIN_INFO, 1);
    debug_out(last_data_string, DEBUG_MED_INFO, 1);
    data_4_prometheus.replace("{id}", id);
    data_4_prometheus.replace("{ver}", String(SOFTWARE_VERSION));
    data_4_prometheus.replace("{up}", String(msSince(time_point_device_start_ms)));
    data_4_prometheus.replace("{si}", String(cfg::sending_intervall_ms));
    data_4_prometheus.replace("{cs}", String(count_sends));
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json2data = jsonBuffer.parseObject(last_data_string);
    if (json2data.success()) {
        for (uint8_t i = 0; i < json2data["sensordatavalues"].size() - 1; i++) {
            String tmp_str = json2data["sensordatavalues"][i]["value_type"].as<char*>();
            data_4_prometheus += tmp_str + "{" + id + "} ";
            tmp_str = json2data["sensordatavalues"][i]["value"].as<char*>();
            data_4_prometheus += tmp_str + "\n";
        }
        data_4_prometheus += F("last_sample_age_ms{");
        data_4_prometheus += id + "} " + String(msSince(starttime)) + "\n";
    } else {
        debug_out(FPSTR(DBG_TXT_DATA_READ_FAILED), DEBUG_ERROR, 1);
    }
    debug_out(data_4_prometheus, DEBUG_MED_INFO, 1);
    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), data_4_prometheus);
}

#ifdef DBG_NAMF_TIMES
void webserver_reset_time(){
    scheduler.resetRunTime();
    server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), F("OK"));
}
#endif


static void webserver_serial() {
    if (!webserver_request_auth(false)) { return; }

    String payload(Debug.popLines());

    server.send(payload.length() ? 200 : 204, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), payload);
}


/*****************************************************************
 * Webserver setup                                               *
 *****************************************************************/
void setup_webserver() {
    server.on(F("/"), webserver_root);
    server.on(F("/config"), webserver_config);
    server.on(F("/simple_config"), webserver_simple_config);
    server.on(F("/config.json"), HTTP_GET, webserver_config_json);
    server.on(F("/configSave.json"), webserver_config_json_save);
    server.on(F("/rollback"), webserver_config_force_update);
    server.on(F("/wifi"), webserver_wifi);
    server.on(F("/values"), webserver_values);
    server.on(F("/debug"), webserver_debug_level);
    server.on(F("/serial"), webserver_serial);
    server.on(F("/ota"), webserver_enable_ota);
    server.on(F("/removeConfig"), webserver_removeConfig);
    server.on(F("/reset"), webserver_reset);
    server.on(F("/data.json"), webserver_data_json);
    server.on(F("/metrics"), webserver_prometheus_endpoint);
    server.on(F("/images-" SOFTWARE_VERSION_SHORT), webserver_images);
    server.on(F("/stack_dump"), webserver_dump_stack);
    server.on(F("/status"), webserver_status_page);
#ifdef DBG_NAMF_TIMES
    server.on(F("/time"), webserver_reset_time);
#endif
    server.onNotFound(webserver_not_found);

    debug_out(F("Starting Webserver... "), DEBUG_MIN_INFO, 0);
//	debug_out(IPAddress2String(WiFi.localIP()), DEBUG_MIN_INFO, 1);
    debug_out(WiFi.localIP().toString(), DEBUG_MIN_INFO, 1);
    server.begin();
    server.client().setNoDelay(true);
}
