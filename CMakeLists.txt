cmake_minimum_required(VERSION 3.20.0)

# O Zephyr lê o arquivo 'VERSION' automaticamente AQUI
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
    src/encoder.c
    src/motor_driver.c
    src/logic_engine.c
    src/ota_handler.c 
    utl/utl_io.c      
    utl/utl_crc16.c   
)

# Define a versão do aplicativo com base no arquivo VERSION
set(APP_VERSION_STRING "${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_PATCHLEVEL}")

# Feedback visual no log
message(STATUS "Blackpill Firmware Version: ${APP_VERSION_STRING}")

# Injeta a versão na assinatura do MCUboot
set(CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION "${APP_VERSION_STRING}" CACHE STRING "" FORCE)


# Define o nome do arquivo de saída: "blackpill_v${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_PATCHLEVEL}.bin"
set(OUTPUT_BIN_NAME "blackpill_v${APP_VERSION_STRING}.bin")

# 1. Cria um "Alvo Customizado" chamado 'artifact_gen'.
# A flag 'ALL' diz ao CMake: "Este alvo faz parte do build padrão, execute-o sempre!"
add_custom_target(artifact_gen ALL
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_BINARY_DIR}/zephyr/zephyr.signed.bin
    ${CMAKE_SOURCE_DIR}/${OUTPUT_BIN_NAME}
    
    COMMENT ">>> [SUCESSO] ${OUTPUT_BIN_NAME}"
)

# 2. Define a Ordem de Execução
# Dizemos: "O alvo 'artifact_gen' depende do 'zephyr_final' estar pronto".
# Isso garante que a cópia só tente rodar depois que o Zephyr terminar de linkar tudo.
add_dependencies(artifact_gen zephyr_final)