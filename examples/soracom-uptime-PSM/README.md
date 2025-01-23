関連Xポスト
https://twitter.com/mnlt18/status/1878809636452876698

・AT＋CPSMSコマンドを送信後モデム「再起動後」に有効になる
・ネットワークからのコマンドに従ってPSMに入る
・PSMから起きるにはタイマーか「PWRKEYをLOW」にするつまりBOARD_MODEM_PWR_PINをHIGHにする

・PSMに入ったらNETLIGHTのLEDは消える　STATUS LEDも消える
・SleepModeに入るにはUSB_VBUSの電源を切ったうえでBOARD_MODEM_DTR_PINをHIGHにする。LOWでwakeup
・モデムの消費電流はSleepMode（1.2ｍA）　PSM（3.5μA）
・PSMタイマーの設定（AT+CPSMS=）は０→１の時のみネットワークにリクエストできる　1→1の時は無視
・T3412（周期）-T3324（Active）が閾値以下の時はPSMにならない（デフォルトは60ｓ）AT＋CPSMCFGで変更可
・タイマー設定値
<Requested_Periodic-TAU>
    //      ! T3412
    //              GPRS Timer 3 value (octet 3)
    //              Bits 5 to 1 represent the binary coded timer value.
    //              Bits 6 to 8 defines the timer value unit for the GPRS timer as follows:
    //              Bits
    //              8 7 6
    //              0 0 0 value is incremented in multiples of 10 minutes (Min 2400sec Max 18600sec)
    //              0 0 1 value is incremented in multiples of 1 hour (Min 21600sec Max 111600sec)
    //              0 1 0 value is incremented in multiples of 10 hours (Min 144000sec Max 1116000sec)
    //              0 1 1 value is incremented in multiples of 2 seconds (Min 0sec Max 62sec)
    //              1 0 0 value is incremented in multiples of 30 seconds (Min 90sec Max 930sec)
    //              1 0 1 value is incremented in multiples of 1 minute (Min 960sec Max 1860sec)
    //              1 1 0 value is incremented in multiples of 320 hours (Min 1152000sec Max 35712000sec)
    //
    // <Requested_Active-Time>
    //      ! T3324
    //              GPRS Timer 3 value (octet 3)
    //              Bits 5 to 1 represent the binary coded timer value.
    //              Bits 6 to 8 defines the timer value unit for the GPRS timer as follows:
    //              Bits
    //              8 7 6
    //              0 0 0 value is incremented in multiples of 2 seconds (Min 0sec Max 62sec)
    //              0 0 1 value is incremented in multiples of 1 minute (Min 120sec Max 1860sec)
    //              0 1 0 value is incremented in multiples of 6 minutes (Min 2160sec Max 11160sec)

