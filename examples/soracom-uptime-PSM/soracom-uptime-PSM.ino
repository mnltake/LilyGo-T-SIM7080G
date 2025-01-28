/**
 * @file soracom-uptime-PSM.ino
 * @author mnltake
 * @brief 
 * @version 0.1
 * @date 2025-01-21
 * 
 * @copyright Copyright (c) 2025
 * 
 * 参考スケッチ
 *  https://github.com/Xinyuan-LilyGO/LilyGo-T-SIM7080G/blob/master/examples/MinimalModemPowerSaveMode/MinimalModemPowerSaveMode.ino
 * @file      MinimalModemPowerSaveMode.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-16
 *
 */
#include <Arduino.h>
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "utilities.h"
#include <esp_sleep.h>

//WDT
#include "esp_system.h"
#include "esp_task_wdt.h"
const int wdtTimeout = 30*1000;  //time in ms to trigger the watchdog
unsigned long lastMillis = 0;
hw_timer_t *timer = NULL;
XPowersPMU  PMU;

// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

#define TINY_GSM_RX_BUFFER 1024

#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>


#include <ArduinoHttpClient.h>
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(Serial1, Serial);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(Serial1);
#endif

const char *register_info[] = {
    "Not registered, MT is not currently searching an operator to register to.The GPRS service is disabled, the UE is allowed to attach for GPRS if requested by the user.",
    "Registered, home network.",
    "Not registered, but MT is currently trying to attach or searching an operator to register to. The GPRS service is enabled, but an allowable PLMN is currently not available. The UE will start a GPRS attach as soon as an allowable PLMN is available.",
    "Registration denied, The GPRS service is disabled, the UE is not allowed to attach for GPRS if it is requested by the user.",
    "Unknown.",
    "Registered, roaming.",
};

enum {
    MODEM_CATM = 1,
    MODEM_NB_IOT,
    MODEM_CATM_NBIOT,
};



// Your GPRS credentials, if any
const char apn[] = "soracom.io";
const char gprsUser[] = "sora";
const char gprsPass[] = "sora";
bool  level = false;

// Soracom  の接続先情報
const char* server = "uni.soracom.io"; // Soracomのエンドポイント
const int port = 23080;                // Soracom のポート番号
RTC_DATA_ATTR int count = 0;
// LTEオブジェクト
TinyGsmClient client(modem);
HttpClient    http(client, server, port);

// Function declaration
void getPsmTimer();
void IRAM_ATTR reset();
void IRAM_ATTR wakeUpHandler();
void wakeUpModem();


void IRAM_ATTR reset(){
    PMU.disableDC3();
    esp_restart();
}

void wakeUpModem() {
    // Pull down PWRKEY for more than 1 second according to manual requirements
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
    delay(1000);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
    delay(200);
}

// 割り込みハンドラ
void IRAM_ATTR wakeUpHandler() {
    Serial.println("URC:");
}

void setup()
{
    // WDTの初期化
    timer = timerBegin(0, 80, true);                  //timer 0, div 80
    timerAttachInterrupt(timer, &reset, true);  //attach callback
    timerAlarmWrite(timer, wdtTimeout * 1000, false); //set time in us
    timerAlarmEnable(timer);                          //enable interrupt
    timerWrite(timer, 0);

    Serial.begin(115200);
    delay(2000);
    Serial.println("Start...");
    Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);

    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    pinMode(BOARD_MODEM_RI_PIN, INPUT);//for modem URC report check
    pinMode(BOARD_MODEM_DTR_PIN, OUTPUT);//for modem sleep control
    digitalWrite(BOARD_MODEM_DTR_PIN, LOW);
    /*********************************
     *  step 1 : Initialize power chip,
     *  turn on modem and turn off gps antenna power channel
    ***********************************/
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        Serial.println("Failed to initialize power.....");
        while (1) {
            delay(5000);
        }
    }

    // If it is a power cycle, turn off the modem power. Then restart it
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED ) {
        PMU.disableDC3();
        // Wait a minute
        delay(200);
    }

    //Set the working voltage of the modem, please do not modify the parameters
    PMU.setDC3Voltage(3000);    //SIM7080 Modem main power channel 2700~ 3400V
    PMU.enableDC3();
    PMU.enableBLDO1();//Level Shift
    // Turn off other unused power domains
    PMU.disableDC2();
    PMU.disableDC4();
    PMU.disableDC5();
    PMU.disableALDO1();
    PMU.disableALDO2();
    PMU.disableALDO3();
    PMU.disableALDO4();
    
    PMU.disableBLDO2();//GPS ANT
    PMU.disableDLDO1();;
    PMU.disableDLDO2();
    PMU.disableCPUSLDO();
    // PMU.disableButtonBatteryCharge();
    // Turn off ADC data monitoring to save power
    PMU.disableTemperatureMeasure();
    // Disable internal ADC detection
    // PMU.disableBattDetection();
    // PMU.disableVbusVoltageMeasure();
    // PMU.disableBattVoltageMeasure();
    // PMU.disableSystemVoltageMeasure();
    // Set the minimum common working voltage of the PMU VBUS input,
    // below this value will turn off the PMU
    PMU.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);

    // Set the maximum current of the PMU VBUS input,
    // higher than this value will turn off the PMU
    PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);

    // Set VSY off voltage as 2600mV , Adjustment range 2600mV ~ 3300mV
    PMU.setSysPowerDownVoltage(2600);
    // TS Pin detection must be disable, otherwise it cannot be charged
    PMU.disableTSPinMeasure();
    PMU.enableBattDetection();
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();
    // Close  IRQs
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    PMU.clearIrqStatus();
        PMU.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ    | XPOWERS_AXP2101_BAT_REMOVE_IRQ      |   //BATTERY
        XPOWERS_AXP2101_VBUS_INSERT_IRQ   | XPOWERS_AXP2101_VBUS_REMOVE_IRQ     |   //VBUS
        XPOWERS_AXP2101_PKEY_SHORT_IRQ    | XPOWERS_AXP2101_PKEY_LONG_IRQ       |   //POWER KEY
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  | XPOWERS_AXP2101_BAT_CHG_START_IRQ       //CHARGE
    );
    /*********************************
     * Set PMU Charger params
    ***********************************/
    // Set the precharge charging current
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    // Set constant current charge current limit
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
    // Set stop charging termination current
    PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);

    // Set charge cut-off voltage
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V1);




    /*********************************
     * step 2 : start modem
    ***********************************/

    int retry = 0;
    while (!modem.testAT(1000)) {
        Serial.print(".");
        if (retry++ > 6) {
            wakeUpModem();
            retry = 0;
            Serial.println("Retry start modem .");
        }
    }
    Serial.println();
    Serial.print("Modem started!");

    modem.sendAT("E0");//Echo mode off
    modem.sendAT("+CFGRI=1");//Indicate RI When Using URC
    if (modem.waitResponse() != 1) {
        Serial.println("+CFGRI=1 failed!");
    }
    timerWrite(timer, 0);
    /*********************************
     * step 3 : Check if the SIM card is inserted
    ***********************************/
    String result ;


    if (modem.getSimStatus() != SIM_READY) {
        Serial.println("SIM Card is not insert!!!");
        return ;
    }


    // PSM の設定変更する時には、まず PSM モードをオフにします。
    // Assuming that PSM mode is enabled, turn it off first.
    // modem.sendAT("+CPSMS=0,,,\"01011111\",\"00000001\"");
    // if (modem.waitResponse(5000) == 1) {
    //     Serial.println("PSM Mode disable OK!");
    //     if (modem.waitResponse(30000, "+CPSMSTATUS:") == 1) {
    //         result = modem.stream.readStringUntil('\r');
    //         Serial.println();
    //         Serial.print("Relust:");
    //         Serial.println(result);
    //     }
    // }

    /*********************************
     * step 4 : Set the network mode to NB-IOT
    ***********************************/
 timerWrite(timer, 0);
    modem.setNetworkMode(38);    //LTE only

    modem.setPreferredMode(MODEM_CATM);

    uint8_t pre = modem.getPreferredMode();

    uint8_t mode = modem.getNetworkMode();

    Serial.printf("getNetworkMode:%u getPreferredMode:%u\n", mode, pre);


    /*********************************
    * step 5 : Wait for the network registration to succeed
    ***********************************/
    SIM70xxRegStatus s;
    do {
        s = modem.getRegistrationStatus();
        if (s != REG_OK_HOME && s != REG_OK_ROAMING) {
            Serial.print(".");
            PMU.setChargingLedMode(level ? XPOWERS_CHG_LED_ON : XPOWERS_CHG_LED_OFF);
            level ^= 1;
            delay(1000);
        }

    } while (s != REG_OK_HOME && s != REG_OK_ROAMING) ;

    Serial.println();
    Serial.print("Network register info:");
    Serial.println(register_info[s]);

    // Activate network bearer, APN can not be configured by default,
    // if the SIM card is locked, please configure the correct APN and user password, use the gprsConnect() method
    modem.sendAT("+CNACT=0,1");
    if (modem.waitResponse() != 1) {
        Serial.println("Activate network bearer Failed!");
        return;
    }

    // if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    //     return ;
    // }

    bool res = modem.isGprsConnected();
    Serial.print("GPRS status:");
    Serial.println(res ? "connected" : "not connected");

    String ccid = modem.getSimCCID();
    Serial.print("CCID:");
    Serial.println(ccid);

    String imei = modem.getIMEI();
    Serial.print("IMEI:");
    Serial.println(imei);

    String imsi = modem.getIMSI();
    Serial.print("IMSI:");
    Serial.println(imsi);

    String cop = modem.getOperator();
    Serial.print("Operator:");
    Serial.println(cop);

    IPAddress local = modem.localIP();
    Serial.print("Local IP:");
    Serial.println(local);

    int csq = modem.getSignalQuality();
    Serial.print("Signal quality:");
    Serial.println(csq);


    //Enable PSM Event report
    modem.sendAT("+CPSMSTATUS=1");
    if (modem.waitResponse() != 1) {
        Serial.println("Enable PSM Event report Failed!"); return;
    }

    // For a UE that wants to apply PSM, enable network registration and location
    modem.sendAT("+CEREG=4");
    if (modem.waitResponse() != 1) {
        Serial.println("PSM register Failed!"); return;
    }

    // Get default T3412, T3324 time
    getPsmTimer();

    /*
    * PSM タイマーの設定変更　設定はネットワークに登録後モデムの再起動後に有効になります
    * Change T3412, T3324 time, according to the operator,
    * some operators do not support changing this value,
    * please consult the communication operator
    * T3412, T3324 time Please check the manual or getPsmTimer description
    * */
   
    modem.sendAT("+CPSMS=1,,,\"00000110\",\"00000101\"");// 1hour cycle  10sec active
    if (modem.waitResponse(5000) != 1) {
        Serial.println("PSM Mode enable failed!");
        return ;
    }


    // sleep modeを使う場合は、以下のコマンドを実行してください
    //AT+CSCLK=1 : Enable sleep mode 1.
#if 1
    // AT+CSCLK=1
    modem.sendAT("+CSCLK=1");
    if (modem.waitResponse() != 1) {
        Serial.println("Enable modem sleep failed!");
    }
#endif
}

void loop()
{
    String result;
    timerWrite(timer, 0);
    timerAlarmEnable(timer); 
    // ウェイクアップの原因を表示
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); break;
        default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
    }

    Serial.print("isCharging:"); Serial.println(PMU.isCharging() ? "YES" : "NO");
    Serial.print("isVbusIn:"); Serial.println(PMU.isVbusIn() ? "YES" : "NO");
    Serial.print("getBattVoltage:"); Serial.print(PMU.getBattVoltage()); Serial.println("mV");
    Serial.print("getVbusVoltage:"); Serial.print(PMU.getVbusVoltage()); Serial.println("mV");
    Serial.print("getSystemVoltage:"); Serial.print(PMU.getSystemVoltage()); Serial.println("mV");

    // The battery percentage may be inaccurate at first use, the PMU will automatically
    // learn the battery curve and will automatically calibrate the battery percentage
    // after a charge and discharge cycle
    if (PMU.isBatteryConnect()) {
        Serial.print("getBatteryPercent:"); Serial.print(PMU.getBatteryPercent()); Serial.println("%");
    }
    Serial.println();
    digitalWrite(BOARD_MODEM_DTR_PIN, LOW);
    PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_4HZ);
    while(!modem.isGprsConnected()){
            // atach
        modem.sendAT("+CGATT=1");
        if (modem.waitResponse() != 1) {
            Serial.println("+CGATT=1 Failed!");
        }

            // Network Active
        modem.sendAT("+CNACT=0,1");
        if (modem.waitResponse(5000,"ACTIVE") != 1) {
            Serial.println("AT+CNACT=0,1 Failed!");
        }
        delay(200);
    }


    // TCP/UDP通信の準備
    // UDP通信の場合は、/lib/TinyGSM/src/TinyGsmClientSIM7080.h#L523 のTCPをUDPへ変更
        if (!client.connect(server, port)) {
        Serial.println("Failed to connect to Soracom.");
        return;
    }
 
    int sleepMS = 300 * 1000 - (millis() - lastMillis);
    char buffer[1024] = {0};
    // uptime を計算 (秒単位)
    unsigned long uptime = millis() / 1000;
    // JSON形式のメッセージを作成
    char jsonMessage[128];
    snprintf(jsonMessage, sizeof(jsonMessage), "{\"uptime\": %lu, \"count\": %d, \"wakeup\": %d ,\"bat\" :%d}", uptime, count ,int(wakeup_reason) ,PMU.getBatteryPercent());
    Serial.print("Try publish payload: ");
    Serial.println(jsonMessage);
    client.print(jsonMessage);
    uint32_t timeout = millis();
    while (client.connected() && millis() - timeout < 5000L) {
    // Print available data
        while (client.available()) {
            char c = client.read();
            Serial.print(c);
            timeout = millis();
        }
    }
    client.stop();//+CACLOSE=0
    count ++;

    // Pulling up DTR pin, module will go to normal sleep mode
    digitalWrite(BOARD_MODEM_DTR_PIN, HIGH);

    while(1){
        timerWrite(timer, 0);
        if (Serial1.available()) {
            String URCreport = Serial1.readStringUntil('\n');
            URCreport.trim();
            Serial.println(URCreport);
            // URC通知を確認
            if (URCreport.indexOf("ENTER PSM") != -1) {
                Serial.println("URC: +CPSMSTATUS: \"ENTER PSM\"");
                PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
                Serial.println("Enter PSM mode!");

                // タイマーによるウェイクアップを設定
                sleepMS = (3600  - 20) * 1000; //<early_wakeup_time> =defoult 3s see AT+CPSMCFGEXT Configure Modem Optimization of PSM                Serial.printf("Set timer wakeup! %d sec\n", sleepMS / 1000);
                esp_sleep_enable_timer_wakeup((sleepMS) * 1000); // ms to us

                // GPIOピンによるウェイクアップを設定
                attachInterrupt(digitalPinToInterrupt(BOARD_MODEM_RI_PIN), wakeUpHandler, FALLING);
                // esp_sleep_enable_ext0_wakeup(GPIO_NUM_3, 0); // BOARD_MODEM_RI_PINがLOWになったらウェイクアップ
                timerAlarmDisable(timer);
                delay(1000);
                lastMillis = millis();

                esp_light_sleep_start();       

                detachInterrupt(digitalPinToInterrupt(BOARD_MODEM_RI_PIN));
                Serial.printf("Wake up ESP from sleep mode! %d ms \n", millis() - lastMillis);

                lastMillis = millis();
                timerWrite(timer, 0);
                timerAlarmEnable(timer); 
                // digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
                //Pulling down DTR pin will wake module up from sleep mode.
                digitalWrite(BOARD_MODEM_DTR_PIN, LOW);
                wakeUpModem(); 
                while(!modem.testAT(500)){
                    wakeUpModem();
                    Serial.print(".");
                }
                Serial.println("modem Wake up from PSM!");
                // break;
            }else if  (URCreport.indexOf("EXIT PSM") != -1) {
            Serial.println("start data send...");
            break;
            }
        }
    }
}

void getPsmTimer()
{
    // AT+CPSMS=[<mode>[,<Requested_Periodic-RAU>[,<Requested_GPRS-READY-timer>[,<Requested_Periodic-TAU>[,<Requested_Active-Time>]]]]]
    // <mode>
    //      0 - Disable the use of PSM  1 - Enable the use of PSM
    // <Requested_Periodic-RAU>
    //      Not supported
    // <Requested_GPRS-READY-timer>
    //      Not supported
    // <Requested_Periodic-TAU>
    //      ! T3412
    //      String type; one byte in an 8 bit format. Requested extended periodic TAU value (T3412) to be allocated to the UE in E-UTRAN.
    //      The requested extended periodic TAU value is coded as one byte (octet 3) of the GPRS Timer 3 information element coded as bit format (e.g. "01000111" equals 70 hours).
    //      For the coding and the value range, see the GPRS Timer 3 IE in 3GPP TS 24.008 [8] Table 10.5.163a/3GPP TS 24.008.
    //      See also 3GPP TS 23.682 [149] and 3GPP TS 23.401 [82]. The default value, if available, is manufacturer specific.
    //              GPRS Timer 3 value (octet 3)
    //              Bits 5 to 1 represent the binary coded timer value.
    //              Bits 6 to 8 defines the timer value unit for the GPRS timer as follows:
    //              Bits
    //              8 7 6
    //              0 0 0 value is incremented in multiples of 10 minutes
    //              0 0 1 value is incremented in multiples of 1 hour
    //              0 1 0 value is incremented in multiples of 10 hours
    //              0 1 1 value is incremented in multiples of 2 seconds
    //              1 0 0 value is incremented in multiples of 30 seconds
    //              1 0 1 value is incremented in multiples of 1 minute
    //              1 1 0 value is incremented in multiples of 320 hours
    //
    // <Requested_Active-Time>
    //      ! T3324
    //      String type; one byte in an 8 bit format.
    //      Requested Active Time valuen (T3324) to be allocated to the UE.
    //      The requested Active Time value is coded as one byte (octet 3) of the GPRS Timer 2 information element coded as bit format (e.g. "00100100" equals 4 minutes).
    //      For the coding and the value range,
    //      see the GPRS Timer 2 IE in 3GPP TS 24.008 [8] Table 10.5.163/3GPP TS 24.008.
    //      See also 3GPP TS 23.682 [149], 3GPP TS 23.060 [47] and 3GPP TS 23.401 [82]. The default value, if available, is manufacturer specific.
    //              GPRS Timer 3 value (octet 3)
    //              Bits 5 to 1 represent the binary coded timer value.
    //              Bits 6 to 8 defines the timer value unit for the GPRS timer as follows:
    //              Bits
    //              8 7 6
    //              0 0 0 value is incremented in multiples of 2 seconds
    //              0 0 1 value is incremented in multiples of 1 minute
    //              0 1 0 value is incremented in multiples of 6 minutes
    //
    String result;

    modem.sendAT("+CPSMRDP");

    if (modem.waitResponse(5000, "+CPSMRDP: ") != 1) {
        Serial.println("Failed to get CPSMRDP response");
        return;
    }

    result = modem.stream.readStringUntil('\r');
    Serial.println("Raw response: " + result);

    // 結果をパースして表示
    int mode, requestedActiveTime, requestedPeriodicTAU, networkActiveTime, networkT3412ExtValue, networkT3412Value;
    sscanf(result.c_str(), "%d,%d,%d,%d,%d,%d", &mode, &requestedActiveTime, &requestedPeriodicTAU, &networkActiveTime, &networkT3412ExtValue, &networkT3412Value);

    Serial.println("Parsed response:");
    Serial.print("Mode: ");
    Serial.println(mode == 1 ? "enable" : "disable");
    Serial.print("Requested Active Time: ");
    Serial.println(requestedActiveTime);
    Serial.print("Requested Periodic TAU: ");
    Serial.println(requestedPeriodicTAU);
    Serial.print("Network Active Time: ");
    Serial.println(networkActiveTime);
    Serial.print("Network T3412 EXT Value: ");
    Serial.println(networkT3412ExtValue);
    Serial.print("Network T3412 Value: ");
    Serial.println(networkT3412Value);
}
