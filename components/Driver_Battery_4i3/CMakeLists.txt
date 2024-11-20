SET(SOURCES )

idf_component_register( SRCS 
                        "Buzzer/Buzzer.c"
                        "eMMC/emmc.c"
                        "LCD_ILI9806/LCD_ILI9806.c"
                        "Driver_Battery_4i3.c"
                        "Timer/TimerApp.c"
                        "USB_Serial/usb_serial.c"
                        "SNTP/SNTP.c"
                        "Key/Key.c"
                        INCLUDE_DIRS "."
                        INCLUDE_DIRS 
                        "Buzzer"
                        "eMMC"
                        "LCD_ILI9806"
                        "Timer"
                        "USB_Serial/include"
                        "SNTP"
                        "Key"
                        REQUIRES 
                        lvgl
                        driver
                        esp_lcd
                        fatfs
                        espressif__esp_tinyusb
                        esp_partition
                        app_update
                        esp_netif)