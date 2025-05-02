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
4/30 更新內容 : <br> 
<ul>
     <li>1. [INFO]增加驅動板設定時間的結構體SetTime_ControlPacket_t與設定驅動板時間function void write_controlboard_set_time(void);</li> 
     <li>2. [INFO]在update_state中手動修改系統時間後加入void write_controlboard_set_time(void);確保畫面與驅動板時間同步</li> 

</ul>
5/2 更新內容 : <br>  
<ul>
     <li>1. [INFO]修改read_controlboard_data_thread()，避免從資料中間開始收的時候會一直收不到標頭</li>  
     <li>2. [INFO]修改read_controlboard_data_thread()中判斷不同組電流是依靠pkt->page_number(0x31、0x32、0x33)</li>
     <li>3. [INFO]修改format_current_value 電流精度是除100.0</li> 
     <li>4. [INFO]加入page31_received、page32_received、page33_received三個bool來判斷has_ct_data是否為true，開機時才可以在收到完整資料後開始上傳log</li>
     <li>5. [INFO]在pkt->page_number == 0x33中加入與som1系統對時，如果誤差60秒以上重新校時送出write_controlboard_set_time();</li>  
     <li>6. [INFO]在ct_timer.on_timeout中加入對CT電流的判斷，決定圖片是綠色或白色</li> 
     <li>7. [INFO]在ct_next_data_button->on_click中加入對CT電流的判斷，決定圖片是綠色或白色</li> 
</ul>