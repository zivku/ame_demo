3/26 main分支目前已加入WIFI上傳機制與補傳機制框架<br>
4/18 tag v1.2 加入control board通訊架構<br>
4/21 更新內容 : <br>
<ul>
     <li>1. 優化時間設定頁面，離開頁面後才將設定的時間寫入系統，避免在設定過程中一直產生不同日期的log</li>
     <li>2. 將egt的timer都改成thread形式，避免執行某些功能時造成占用影響畫面的刷新率</li>
</ul>
4/22 更新內容 : <br>
<ul>
     <li>1. [INFO]加入讀取CT電流轉換為螢幕顯示的字串，以及與CT資料上傳時與讀取時的mutex</li>
     <li>2. [INFO]將uartTransmitCurrentToWiFi()中資料陣列優化成結構體TransmitCurrentPacket</li>
     <li>3. [debug]ControlPacket_t結構體中少一個byte process_flag，已修正</li>
     <li>4. [INFO]增加format_current_value將讀取的CT電流值轉成字串填入ct_current_data_list中</li>
     <li>5. [INFO]在ct_timerr加入switch case 根據頁面變數current_ct_group決定顯示的CT電流</li>
</ul>
4/23 更新內容 : <br>
<ul>
     <li>1. [INFO]在copy_csv_to_sd中加入對SD容量的判斷，如果容量已滿嘗試刪除最舊的檔案，失敗會返回-1</li>
</ul>
     