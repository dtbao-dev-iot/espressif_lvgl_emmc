set(srcs "tusb_msc_main.c")
set(requires fatfs console Driver_Battery_4i3 Test_Benchmark_FATFS mbedtls )

if(CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH)
    list(APPEND requires wear_levelling)
endif()

idf_component_register(
    SRCS "function_test.c" "${srcs}"
    INCLUDE_DIRS .
    REQUIRES "${requires}"
)
