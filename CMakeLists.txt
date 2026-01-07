cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(infusion_pump)
target_include_directories(app PRIVATE
    include       
    utl           
)

target_sources(app PRIVATE
    src/main.c        
    src/cmd.c
    src/hub.c
    src/adc_driver.c
    src/sensor_data.c         
    utl/utl_io.c      
    utl/utl_crc16.c   
)