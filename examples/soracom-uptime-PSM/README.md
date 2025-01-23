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
![PSMtimer.png](https://github.com/mnltake/LilyGo-T-SIM7080G/blob/PSM/image/PSMtimer.png)