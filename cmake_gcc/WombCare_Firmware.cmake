####################################################################
# Automatically-generated file. Do not edit!                       #
####################################################################

set(SDK_PATH "C:/Users/royma/.silabs/slt/installs/conan/p/simpl508ee6c1a6569/p")
set(COPIED_SDK_PATH "simplicity_sdk_2026.6.0")
set(PKG_PATH "C:/Users/royma/.silabs/slt/installs")

add_library(slc OBJECT
    "../${COPIED_SDK_PATH}/boards/hardware/board/src/sl_board_control_gpio.c"
    "../${COPIED_SDK_PATH}/boards/hardware/board/src/sl_board_init.c"
    "../${COPIED_SDK_PATH}/boards/hardware/driver/mx25_flash_shutdown/src/sl_mx25_flash_shutdown_eusart/sl_mx25_flash_shutdown.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/common/src/sl_assert.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/common/src/sl_core_cortexm.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/common/src/sl_syscalls.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/Device/SiliconLabs/EFR32MG26/Source/startup_efr32mg26.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/Device/SiliconLabs/EFR32MG26/Source/system_efr32mg26.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/driver/gpio/src/sl_gpio.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/emlib/src/em_cmu.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/emlib/src/em_emu.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/emlib/src/em_eusart.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/emlib/src/em_gpio.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/emlib/src/em_iadc.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/emlib/src/em_ldma.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/emlib/src/em_letimer.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/emlib/src/em_msc.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/emlib/src/em_prs.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/emlib/src/em_system.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/peripheral/src/sl_hal_eusart.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/peripheral/src/sl_hal_gpio.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/peripheral/src/sl_hal_prs.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/peripheral/src/sl_hal_syscfg.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/peripheral/src/sl_hal_system.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/clock_manager/src/sl_clock_manager.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/clock_manager/src/sl_clock_manager_hal_s2.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/clock_manager/src/sl_clock_manager_init.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/clock_manager/src/sl_clock_manager_init_hal_s2.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/device_init/src/sl_device_init_dcdc_s2.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/device_init/src/sl_device_init_emu_s2.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/device_manager/clocks/sl_device_clock_efr32xg26.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/device_manager/devices/sl_device_peripheral_hal_efr32xg26.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/device_manager/dma/sl_device_dma_s2.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/device_manager/src/sl_device_clock.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/device_manager/src/sl_device_dma.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/device_manager/src/sl_device_gpio.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/device_manager/src/sl_device_peripheral.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/interrupt_manager/src/sl_interrupt_manager_cortexm.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/memory_manager/src/sl_memory_manager_region.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/sl_main/src/sl_main_init.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/sl_main/src/sl_main_init_memory.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/sl_main/src/sl_main_process_action.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/udelay/src/sl_udelay.c"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/udelay/src/sl_udelay_armv6m_gcc.S"
    "../app.cpp"
    "../autogen/sl_board_default_init.c"
    "../autogen/sl_event_handler.c"
    "../main.c"
)

target_include_directories(slc PUBLIC
   "../config"
   "../autogen"
   "../."
    "../${COPIED_SDK_PATH}/platform_core/platform/Device/SiliconLabs/EFR32MG26/Include"
    "../${COPIED_SDK_PATH}/boards/hardware/board/inc"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/clock_manager/inc"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/clock_manager/src"
    "../${COPIED_SDK_PATH}/cmsis/Core/Include"
    "../${COPIED_SDK_PATH}/cmsis/Core/Include/m-profile"
    "../${COPIED_SDK_PATH}/cmsis/Core/Include/a-profile"
    "../${COPIED_SDK_PATH}/cmsis_dsp/Include"
    "../${COPIED_SDK_PATH}/cmsis_dsp/Include/dsp"
    "../${COPIED_SDK_PATH}/cmsis_nn"
    "../${COPIED_SDK_PATH}/cmsis_nn/Include"
    "../${COPIED_SDK_PATH}/platform_core/platform/common/inc"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/device_manager/inc"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/device_init/inc"
    "../${COPIED_SDK_PATH}/platform_core/platform/emlib/inc"
    "../${COPIED_SDK_PATH}/platform_core/platform/driver/gpio/inc"
    "../${COPIED_SDK_PATH}/platform_core/platform/peripheral/inc"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/interrupt_manager/inc"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/interrupt_manager/src"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/interrupt_manager/inc/arm"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/memory_manager/inc"
    "../${COPIED_SDK_PATH}/boards/hardware/driver/mx25_flash_shutdown/inc/sl_mx25_flash_shutdown_eusart"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/sl_main/inc"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/sl_main/src"
    "../${COPIED_SDK_PATH}/platform_core/platform/service/udelay/inc"
)

target_compile_definitions(slc PUBLIC
    "DEBUG_EFM=1"
    "EFR32MG26B510F3200IM68=1"
    "SL_CODE_COMPONENT_SYSTEM=system"
    "HARDWARE_BOARD_DEFAULT_RF_BAND_2400=1"
    "HARDWARE_BOARD_SUPPORTS_1_RF_BAND=1"
    "HARDWARE_BOARD_SUPPORTS_RF_BAND_2400=1"
    "HFXO_FREQ=39000000"
    "SL_BOARD_NAME=\"BRD2608A\""
    "SL_BOARD_REV=\"A04\""
    "SL_CODE_COMPONENT_CLOCK_MANAGER=clock_manager"
    "SL_COMPONENT_CATALOG_PRESENT=1"
    "SL_CODE_COMPONENT_DEVICE_PERIPHERAL=device_peripheral"
    "SL_CODE_COMPONENT_GPIO=gpio"
    "SL_CODE_COMPONENT_HAL_COMMON=hal_common"
    "SL_CODE_COMPONENT_HAL_GPIO=hal_gpio"
    "SL_CODE_COMPONENT_INTERRUPT_MANAGER=interrupt_manager"
    "CMSIS_NVIC_VIRTUAL=1"
    "CMSIS_NVIC_VIRTUAL_HEADER_FILE=\"cmsis_nvic_virtual.h\""
    "SL_CODE_COMPONENT_CORE=core"
)

target_link_libraries(slc PUBLIC
    "-Wl,--start-group"
    "stdc++"
    "gcc"
    "c"
    "m"
    "nosys"
    "${CMAKE_CURRENT_LIST_DIR}/../${COPIED_SDK_PATH}/cmsis_dsp/lib/gcc/cortex-m33/libCMSISDSP.a"
    "${CMAKE_CURRENT_LIST_DIR}/../${COPIED_SDK_PATH}/cmsis_nn/lib/gcc/cortex-m33/libcmsis-nn.a"
    "-Wl,--end-group"
)
target_compile_options(slc PUBLIC
    $<$<COMPILE_LANGUAGE:C>:-mcpu=cortex-m33>
    $<$<COMPILE_LANGUAGE:C>:-mthumb>
    $<$<COMPILE_LANGUAGE:C>:-mfpu=fpv5-sp-d16>
    $<$<COMPILE_LANGUAGE:C>:-mfloat-abi=hard>
    $<$<COMPILE_LANGUAGE:C>:-mcmse>
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:C>:-Os>
    $<$<COMPILE_LANGUAGE:C>:-fdata-sections>
    $<$<COMPILE_LANGUAGE:C>:-ffunction-sections>
    $<$<COMPILE_LANGUAGE:C>:-fomit-frame-pointer>
    $<$<COMPILE_LANGUAGE:C>:-g>
    $<$<COMPILE_LANGUAGE:C>:--specs=nano.specs>
    $<$<COMPILE_LANGUAGE:C>:-fno-lto>
    $<$<COMPILE_LANGUAGE:CXX>:-mcpu=cortex-m33>
    $<$<COMPILE_LANGUAGE:CXX>:-mthumb>
    $<$<COMPILE_LANGUAGE:CXX>:-mfpu=fpv5-sp-d16>
    $<$<COMPILE_LANGUAGE:CXX>:-mfloat-abi=hard>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-mcmse>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-Os>
    $<$<COMPILE_LANGUAGE:CXX>:-fdata-sections>
    $<$<COMPILE_LANGUAGE:CXX>:-ffunction-sections>
    $<$<COMPILE_LANGUAGE:CXX>:-fomit-frame-pointer>
    $<$<COMPILE_LANGUAGE:CXX>:-g>
    $<$<COMPILE_LANGUAGE:CXX>:--specs=nano.specs>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-lto>
    $<$<COMPILE_LANGUAGE:ASM>:-mcpu=cortex-m33>
    $<$<COMPILE_LANGUAGE:ASM>:-mthumb>
    $<$<COMPILE_LANGUAGE:ASM>:-mfpu=fpv5-sp-d16>
    $<$<COMPILE_LANGUAGE:ASM>:-mfloat-abi=hard>
    "$<$<COMPILE_LANGUAGE:ASM>:SHELL:-x assembler-with-cpp>"
)

set(post_build_command )
set_property(TARGET slc PROPERTY C_STANDARD 17)
set_property(TARGET slc PROPERTY CXX_STANDARD 17)
set_property(TARGET slc PROPERTY CXX_EXTENSIONS OFF)

target_link_options(slc INTERFACE
    -mcpu=cortex-m33
    -mthumb
    -mfpu=fpv5-sp-d16
    -mfloat-abi=hard
    "-T${CMAKE_CURRENT_LIST_DIR}/../autogen/linkerfile.ld"
    --specs=nano.specs
    -Wl,-Map=$<TARGET_FILE_DIR:WombCare_Firmware>/WombCare_Firmware.map
    -fno-lto
    -Wl,--gc-sections
)

# BEGIN_SIMPLICITY_STUDIO_METADATA=eJztfQtz2ziW7l9JubZu7d6JJb5F5aZ7Ku04Pd6bdHJtZ2an1lssiqJsTvgaknKcmZr/fgGKpPgAKQIEQGh2e3e6LYk85/sOgIOD18HfL+5uPn35eHN1c/9n6+7+6/ubz9aX95/uLt5cvP39S+A/PLx6dpPUi8KfHi7khfRwAb5xQyfaeuEj+Orr/YdL8+Hi9z8/PDwk4H/h2ziJ/uI6GXgstAMXPLJ3FkG03fvuInWzfbzYO1dRuPMeF3+Kgs2VnbjWBy8JvoM/Fo+OkysAMmI3yX7cOeC/QEQp86JSAx4C//92F/lbNznqcnLJnefKpz3fPT6b+pbjR843K7BD+9FNrCh1PN+3syixDnIWTzmaRzd0Eztzt+ClLNm7+Ze+F37Lv9nZfgq+WhKpzBLXZahsE9nJFsrPkshnqCdwgyj5UbFK3EdQYRjq27rPnuNaXuhl1tbZOpxUucGepRFfFN3a+Xb6ZKVP+2wbfQ8td5/aScayOkYJjQr4dnloie2vvdDx91v3i509gY/7xINas/3Wi94si0a9LNvsUebb8rfqm1fsnNG9G8Sg0bss3JG9zyJgz3H+6N2v17/d313efXz3yyLY5oo3e8/PvLBeEt3iwXMGW3dn7/0sr9ALh7KW2/tr6yoK4ih0wywtKhRVEk4p3XLszPajRxZK3Geo4MkOt76bsFdAuxTyxprA7xb+hHo0pU1XFX+WRl088MnN7C2oJbO3bPDgotDkuel/xxKpmu1d/pFBgaQecOOe42U/rHT7zVIkxVgYEHtPAbVeh13ALkqCvEPseenEq4Nv9bz7Po8uRrzZ8/6dBzhH4Ud7k44W0iPq+sOtqnz6VTEwBfUhi/YJBjOUxKbXzEAgtI8td5eoSvAIStdBRibdBmA730BUCsMMO1zGy7yi6JLpuoYj24ZurMGXjeKvPi0PxbOsWXlZmWl5YLhEAkP7aRLaP9LMDQRkjcBFQrrHpRFVuZuDG6RX5yp2G12WdqoiSV5gmEVAMF8RFDyX/fCoVb9KhWU7QSwe8woWC8ZuKiLhAyoGfDf7xA4EZFzhYsM5c8TkfMDFgLMT7AVkXKBiwBdOEAlIuITFgjFAEu4iEUkfkbHgHdhAQ+okXpxFiYj0OwBZWCH2fRG5F7AYMHaF9GguM492mBUWkXIFjAHrXZo4Ivq0ChcDzo+xk4jYeVW4mHD2RCzmEhYjxlYcCdmkG9gYcH/aidmqK1xMOL+ISfmFFWNPEdGLFahY8LWFHHOUsFgwdmznyRWRcwWMAetv7o/UsUMBadeQMeDtO1sBOReo2PBNdmIyPuBiwRmMVkWkXMBixPhlY4s4c1CHxpA53B7ihUJOfKMgsrCEm3mBK2QVOCJjwVvQONxnGIf7YsbhPrs4PLA9fxO9CEi6howF79h+2oi5vFWHxoJ5KuJApEDFgu+ziMvUBSoGfGMnFHHaqITFgnEiYnhSoGLD10q9xxBwFJN3HR0D/qkrbq/VwMaCu5ArWymzla30R+rsHkWkXAFjw1rMbShHYAxYizrOYjnK2os6zNqzHGeJulzNcrX6eWuL2KJLWAwYf99GInruEhY9xkFxKFAcsnVE7Laei0IXBYzb3nPMl0Y/Pv7BzskXN5l2iKRxJHnqMZI0cQgPkAyflK6dV2RaAwtrLhval4DVshcSUd1r0/UQwq0nm8tOviHOQ7ioEO+Rb6WKgMVdQ8aAvFiEmVVtAWs1wwotZF0mrcZkXWa7j/BChn3EbDULsOrpI9jULLGYUiPpicUSAYdpk8F9vN22inwlwkVzBa7cmDydYdMepTdso6HTPAupcIOKKAwLLFT55TuhRSFYgqHKMHYTL35yExAViMKzCem8e+1G6+Po31sGLrqxNhrqvkAUhhM2zg37AlEITjmiMcYXiMKzCYkRWyv7EfPZCIjJ+QhsDi+YuwncZCtjHOFhPvPlkU+Ojx6bH9i1vWIT2hxWB66LrsnhSVy+o/KWpQGAJQrOLNbNQVCu1LUGC2cbRKjdBU+0V+mAPIdhHpxemDrGox7XVWk5Z+g8oO5Wz9GAQ7WzLJOCCsST6NA5HS9Cfa7gmN51Fk+dm7g5CGsjYlKbxGJ7BCS0P/TCzE2SfZzRmvki94rtTJGjMgOOrC1OkHqpFYLisZ69JNvzHZR0jJx7IEBwicbFPSFcu2V1AM9vrT5UZ+2xPdEsXa7yUTD16LoFwWXuS8DTefcxH0YntCdv5pIXZwEDmeOeZ1k3AZQF3QvrvKeI0bQ4+pSWtQvX3QtL6CYFgdteX7LqOdoSQMN9R1eht2o4dQx0uoRKZFFPRGBXg0Jpeb4jmmOrbLJEY6FYlnESOW6aWraTcXb2qOLsojlzH1+1wBlqUOnQ6xgYeQEB2HFpHjPz7KIRulPeb13f/iFOn3zAw9PHHTSWLu6on04NPciz7CR4NgLr0XEWd/NSa0E5b99dlBbHJl+YtGjxR/1iNHLy4xtOFATR+NicQpmiblXawqVeO029nefYvLz5gXq1nRQNArN8EdtJBSDXi2IiO8sN91ySmjSLqtQ6FT2whsvnZGgT/1HvVAYHuXM0llIvBQax5/OZsO1wqDRPZ5FwOY/ZZpAQnLnsok8zO9tz2WvVxH/UO5bB5Kko7Hi112twiFQLaxVh3FEvjdrKc7mgyaOtfXLthZlGfT/lz6SumXoNJg/p3MD3NjNGdG7A65KXnGnuTY46J1Wng5i8a7C5BAZtBjXVkyPPOcqhqXdqWfAL0BrlQCk+A5I2fLrVBvoNXpfa2w64hZbNNkAnsgSSEjvY7UMuuwAbDGp6p1Io7kHlTqGmd3o14hMZtyoRhcC4kGPlF+N6/OtRW/lUMpx2ijY4kO0H7ULnlsKpiZ4wQ1Mfgbniio72qXR4HbtqsCA7XtUFz+tyjAZ4sqsvuuB5ZcVvgCfLeY8Azy+5eRM/aeryLgVO+Ywb8ImyFSOhz+WAmqqnEuGUgbfBgCi/bhc6vwSjDfSk6UORBDKXy5R/m0Chlg6B2YK6rvqzmfoshvUcZtkOFoOTbEedNKJf3tBdStAPYSh39JVaKuEib/hkmTl6wkXe4EulVMJF3uB9oqwv/eEid/xHvTTCRd7wC500oize0AudlIIU3uiPagVaDNom3jPGOZbO+9CNirNLkFdPcrDaEqorF/sI+xPBNs7xmrmpG7BY7SecvxFux9wxacO8u+ZgxghO60tHymVh1pRPXb/PM19wm+BFU6E001tI49XI0FyozJMWsuD1OnC33Yy1rAGBEqdZ2dDiwW8uCU2F0qTSURqniaVeNjRmmBrSeM4yDbE6y+mmpmfmEPPVLFiEfE391HqG2bhQmRKpObLZiNAYJza92GxUjvopetE52cw4BB58ZPjHtvvaRHayHU5J13nnCbzy3U7GXIWC1kc+nqDhbXMI9I/FHyy5LI1z+FzWmabOqS3gIA2gyZKIga8dwaStm1dvS2M82WBANWRBG64IUzpq6dQC6ge7R5DAPch9BtN/wYuiWzvfTp+s9GmfbaPvM+bxQOQV6cIrIjZ6Sc/Qaph6lmL2DKG2ysjQS7znV+KTzRxuKespX2o58OatKCyd0EBFKXMTYFeU/ybX2VEMFvP0g3ix4lVEFicWlxBOcOiXcRLl9XZSvHDIuPjoOBbdOZtc7hJap7pvsUK8bCmdFCjAbAhmYAUx3U3Rg/ibOpnFht0j6Lndtm6cuI6duVvL9j07pZxdH8H8lO7RFkDVPSYHhxEkutomwWZxpKMXNcE5jhbo7G95aO6+0A2mEYibmibZGKY44WLfQhEp2PwgbKCqzMHWFZ0GO0PnaW3TGK8DHd8RdsdEJ3QNFNnGTj3Q+9jZkwXPeME0Ggy8KLRGVXjw7z61hBUPJc7ayXSvWB7PpFRNzOaHm3Iujo5GOti5FQJSK7EbA4rdF/6tol8xVSacCmVYOTGjfFoNBC6cSwatliILbqXSr5qQzdbd7Oku26OhV3pIcXppZoeOy7XmoJVSY8Cp1vQrJmSys9OMv4Pt0UqPA6fiGNBMysXzMzfxwke+5YHWSo8Dr/Lo10zIJb9vII78PLEd1zIZ0EyXC6eyOaGdkBNofIn3wrVgUCopoedUFH1qp7HYZ57P0f6VOkLUYRTSzR+CRluqIUT5170Nmk0Imwz3XnpQNyEfmJkPxDCew3dg3aeWIgtObXdINSmbfRxHSca3QFA6aeHnVRR9ekl5PAfW1t15IYsFlC76prYJmLnWm7Y+Grh51ReUTtLlk8QO03z3JE/r92ilx4FTSQxoJuTCK/iZGvV898Jt9J1rpUGppLJWhCRoJ2WqPyuzNz5zV4rUN4LeCdQcGkKvTkL0YZpZaZbsnYyPzVv6pqPmZnOETiL0T67v7QMuAy+UOiLMcCTBAWuphhgjp8pQV0WOlcGFTANwce5c6kOc/Yi5uOamsol4edaJhkIy3M8uvy6wqWwiXl527igkwv3sOry8Wl3VJKycLNxWN4R5zOag9oabcenou6lHnPH7vxH3GsELBi4DVZ220RVgv/p0d3P3/u7LwmZSEjCDDKC6PCJetpQKd5YOaamPN1fXv91dM7FRJRttCpIdZ+HwQRaaG85u4JpKiJUvpOMSwpDhjlcgvXIIJdglQivb8WAYsg06aiSR6ohcdchwANsFPLHvhhK4GXeqXVlOfnTQ4k57oAAXE81ccaN0/k/33cCeW+4yDBl036A40L13Xed/794bmIis8+77utXy4njhxPFI2En0F9fJltVLbUAI4WPbcE00ogm2BCeuvQ3cRbDFE1577YSCd79e/3Z/VyrI07/Awzf5bP7eRWo9IRHecVyceSUT577kfmv7BfS5P1f43i4b3w+9k1cyz/GyH1a6/WYpkmIsjIXUjRKpyTzW3WGR3sElw596yw5hkTGv9QFsZOV4eCg/Pjy8d+F1tQ8Pdx54LQo/2pv04eH6w62qfPpVMR4eqjCWKpwiXcZDlQLj4ZDQ4iE/w8uHeXFT78OD40fONxDbhcBFJUJAyI+8U4VwOHL6kB8jZVSmSBUPtfOjHLTZLLXlh45YGq+p4CE/d8RACxzJshHLyDi97ae4jHqWJrvNHee8bqPAAPOIcAVwuDGSp8YiL8lDnl6Yp+Ja9tZZijjfX5vs42zemoaAQb+TIrbGAxjccsdyWC9jWSzdKKlsBqg0OwdLnMhdwtlGEA0YCMxTZyvlc9RUINm3f/QRD2wnid7DbXoenIY5DpveX//y9Vfr+sMnnJeqcP0XXZY+qIok3XwyTBwJdx+tq8/vr8G/Pn35/BsYa1l3f767v/6UD9WebX+fb67Lk/XhiP3Du9v3f3p3e2398hn8Zb2//vDu68d76/aD9cu7395biiZJE8Tdff3y5fPt/Z0llxJpCCMG9+E/Plsfbq//X8Nm6lrK/8EsjAOm3959um5I+19/3UfZ//nl9r1iSOa7wyciybfXf0QIfidpRDJbVefq4+er/2t9evcbGLXfNtQ0hzm4Sir57+7fffz8q/Xl9voOfJ4G9v31H8GQ3fpyfXvz5Q/Xt+8+NgAXAVY9hfsUZb9+ufnckH+4LWGKyD+8y03z6fNvDcEwuWYRHU8W30Fd5oadJvrmt/vr29uvX+6RVaXb02MoyxdDrd9AyVp/vLm9/wpLdcrb1h+u372/vrU+3HxENchi/AOqivXsJdne9hdPVBrS59umury36YoEwXhiJz8+HCbgCAeeBOvKFBQTzYi39FanN7bO73439ul8tWHco6MfRPWNyAfDCPSliIezKPI/x0W1gB9u8jnT6tvF3lnAT84TjGtgzYjy74ceWzjxvl2JqqUSPgh2LQS7+Fm/RE5usFHvR3Zm2Ruv5cQS1Lz4GATlEvcwgGohPHWdPUyoE23dBoLDXDgRgnz2/IT+wzPFf+4c0INlDfX/Uq1B7LPo0YUNET4JZ88WPifTlOuQFrBRvh5Jy0CYOLZ2Zs+NIbTDyHIs4DCoIbDT1A02JyFUj7HAgFFVGWjHLIMo8DJrlwAvbcVRHoLMVRkiy31x3HjWChlZSZZ5c1QEFuzx9M/EvFwr/WTHecgwT8k7FsxHss17yHrkIKMmFVjof3npQfC738krPhi+20nohY/pwvb9mYqhguC+ZIk9N4jY3dph5jnNUK5n9Z5pgYBgCowNoySdCwp8IvD+lue8aE6ReX/j4CRYqMcNmGCyKMt3n91m49i6O3vvoyaa0SAC+5ubR5x2EixgeszMTh7drI2i57HO2OYyAN/8RDDCmYgje9oHmxaS4js+ANpDrMsAfPNTMdC63MoGNyDIwRaAA7+/BN//hDXw6qg59hEnER0f7evQLtNs+xNOrzago9hUNg4Q3FDW18cdQGF1dJRhocKfy10YXR6+nQ1UT0yYQ6v/xrdulSGb1TfOB04pwOiZKNqNIzQci5U9udUOrS7/lH/D31DsERHZpxv3Xf6p+G5GGzFFhWOn3jjo8jMVL4BrIdZ4cGzTP6N1uYO/XR5/428oruBwrDY8H3m5K3+f1XqzgMRql8OzaJc7+MBl/sBl9cAMzXUemFituGeYdfk4S7NligbHLn0zxZdgtOM66U/w90X+5xx24oEOPV3Q81j/ItC0tZ+JqHI7CWSlKlw+fLYCO26FOf7ry092/NO//Ovnr/dfvt5b729u/235L//65fbzv19f3cP9Ov+2yN8aCfiwWL7wtu6iWI1rYy12oERxM6Jwd4mqBI+KsdFlaQe3fHkBcssXZmXvmMpL+07l5aI/emlWiW8MyPwMtT9kecRDt0jp4jz5JqwJl4/OUBc7jarvPwdDzlGocmmBRfhL8eCKUY/+o4Dx8Ip6dQLlsUg9396kuX9JPVU5NPVttjjsqdpu9p6/zXdKLB7D/aLWjW3s4mRhzTY1ga2nDw8toPEWUfbkJj4gy9OMvZdVDmkL3DQFVrj03fAxe/oJtUeTh9Hh/BaO2evP/4/hpxm+6PhHmh0+XZp859uPfXc4sutxwNtwGuYy+f4CfMVj4IbZbL4Cw3YNy/nbfy7bdSKU3l03ZJ3LC/hQibz87mVPl+jD6BQ6xbOBzq7C4opzvMTZ+3aydWM33Lqh84N8oV4cVmGUZtvO0AxviX2KS6ZA5ejeMYvobbl3ufrm1dvfvwQ+fOVwQSZ4SV5IuRAgLdp64SP46uv9h0swBPv9UVA5pjvmE3YWQbTdgyaXutk+XlzlW/S/HB77Amz+S07iT1GwuYLbHD54SQBPfi3y7cpADpAYu0n2484B/wUCq1Fju0hiICy3yF3mxj8DSo3PM1AtdgbcuVmW7+Ig4Lhkj5JGUXCAuXcKoDSqSjuXTjEn1Ofu248vUj9fXs+Gc5218oMsnMSBOXN33iP8M0cM6yeoHFWTfEDlDunp/EcmheFY4WMvzJ3RpBp08fqimFyybj9/vr94c/H3h4vb64/v7m/+eG3Vf3q4eAOsuni4+Ad45+7m05ePN1c393+27u6/vr/5bH36/P7rx+s7IOA//w4TygTRs7sF7+Qu/fXDRWH760PaEuD23/znfx2/vov2iXP89sAz11ja4c2nT/mXr4Axw/RN8e1PgMPFU5bFb5bL79+/ly4cePNlmi7LOuweTlE+XBxrwENR3PBLb5t/xrMvfDHeBg1JP+dGDV8VZz9haaevYjuDCezyBxf/G/57WTxX1ZWS288PF0eTAPZQ7j9e/5OYE6sr+h/rYlr3n8yu+R2bwHUvU9/KT6lbxcWXpUt/ypUeH2qc/LSi1PF8386iZNzzWeK6vU/ml1Wjf6vlx7C2ztYZ85wb7Psea574txL3Ea7C9j3cewq/9oYw9XPvXB1AnXu9LBe0DhnDLu8+vvslT2v2+vjT7f21BbxdHIVwCqAouJ6FsNovVVUvItm8usCZ+eYzTinZcuzM9qPHlgLwiPsMf36yw20xtz/0s1i15N4NYLID95+mnoD/LYo4zIO/CWPrQsUnN7PhlpizMXgjmeBruCd9ZBq+EU/Xk0P2FWxTe5W68XWVnvH1MTnh60ZqxQH1h0wsyzIRy+HzEpi30wEWGiYIyt0KiZQ06cLJz+kXVCdIq7m6sVIOqWqWiG6wpNvfQ/b8immVAQQFOXwEwzbIa+kS5qSr0u4eKm49KTbJ+3Cqi/DVYlhL8HoeWAWqiv9qlfQPZh1+NgMriPdTpFRGsAJ8ManvWYWzcePEdeBsgmX7np26Kb607G9503JfTrXRkXdKTZNQ3sBAIKV1z9I0CcQ42pcPYQso780gepEYduvCHLL3q4TnE14nZ9C42WXC68QI6reeEL9MpB3+vbFTrxRRT4ZORdAEVD/cdDKghgxiLLDP8t0XGjZCi5qALI9tQH86GVdXEDGqfHcs2ZsePAjmuBPZdMUQc9nZaUaj4BFyyDGBWN9NvPBxKqauHGJM+abwOPLzIx8TcfXIIsaGutB+spCpaMg6+vqN69gvDt53ji2t78pxKoKIrYu8d3u6FHI8zVuoid6fyAV1KzO2lJ6bkWnIIcZE3oRQV/ZSnqEZl0huzETPmPulcOQgr3AiEIARNA/ccYT/Ll6pjbr4B2u+bczDp1P5DUhp5NStPi0PF1Qsa/dTLKt8txXH6jiEZTtBfIIbJUUnC5KOns0+sU/NelDTlJ2aZqKjyQlOTQfR0QPX/PgoAo+Hu4iPrsAG+tL8LFd0yg1SUhn7pya26ShyOVWMwyQvF1W7NHH4VIzH2En4VPd8UYGXIgv2W1y0Pe14FdXT7oWPIk/hUyM8m5On9RzbeTo1EKSj6pv7I3XsUysndHT5zpaXnmTHRxPop7gpetnYfHrCUtnI0S0lpW7mBSdHPJR0cfOBPi8fGNiev4le+OiK7acNr4A9SPn43OCZz5AqdkI+HX2c8Gm4QI+Veo+hfXLCho6+1OVZ11NOwXoKuuHdqZUVaqp4jYH5ufQ9P5/Ob1j1vLX5lNP3bcSl7jUSebBUGBTbnRnqONwUZVXcGKg67PmDqyhJto9rqoZ3aE1R1SZFpOmwfafcAgcPx5I2mKYkmNrRcnwg0NuBMVJ2esvXSLHwEyVJY+btR8pKCKtvU44b7gkjtaYcuJa3J+ziG5I86uVY7HQsKtqUKltIKs5bwAn+gIY82LvbPgiQiGQVezzhPFFZGOSTU3VhBboR22d7hOV3pOaYgNOY0s4bgjak1awhhXj+vS0lb9U2DWJTHE1LEKlv6Iix8tOnHmGc0RBHPLHdlDIhvkIIold65G2uIYZ8FrEhhnzqqSlmyrRLQxLxQL0thV6JEQ+BG1ISO4AryhQkTRnktQWBMI2aIIo+YNxO/JOSYJQw3YfD/q2QMqF7K6S4dKQcvNt0QVM77UJM7o2mi8m9EQUxhTeaLgl6o+lSoAOZLqVorUSCjreylqEfvJiUOEJCi5vS5aIlkveVaHnEnrxXXD4uoip0infvlUjs5ockTvP3NcnFAKJWh6ZW8ppEcg+HlkfcmNHiiuKmK5HYTxR3si8bOQWqqYT6l2SFPl7+mLOdBEo8dizKQX9DPsVSQMk/FLjCXM2IQ610lDAj5HUNR7/w20pKRtM01VJulE2lna2DqQLiATlKflHynXQjU8v8hA6YqoSSirLI86JOa4oOZZ9Per8QT3r3KDt8rGs7Ov9Dx8lGbWDXVMLtlJSN2KxuuQWp1DW0fOLJjlHSyaPVUeKPBc5FyagN+Hiqmu3yUNg0K1NTPvFYcpR08rBylPhaYU9Skp+US/Zx1ihvGy4tHE4igKesZy/J9lOrFVoTYNT5gbaewnKdH6YtvZzS59Em1kyZVWX+QCXSoqqoTPCBVDTJdFCq7R2TmIC/KYT1fVLHJRkgEB4nEeh6U8t2yJcY2/JLo1cmoWLottTSJNSFt0xCUb5HuUD3W9e3f5TlefhERWJhjkLiJAugJFowHYwR5Ols7gTKtVXlbLvLP55Lqi3Bc16Oy/h8LmY9pow7JjxedrIYC2P8UxmiuZn9vy5eXzhR7LnbD57vpkUC4iqvc/HY6yr78Rc7e8pth5s8NEq8Ry+0/UpA/m2x6Aa+kF/nMjPQzMGny9ValTVJ1uW8YkzA001OiodElg1d1xRNWuEjGci9ignCkCRJWa91BR/EiLSomGUjGyvZ0NSVRmCS4ZyvmEBMXdXktaoSVJLBnLK49litQeEYBChGZaHFBqSsNG1lmgZB04na2Xpxm60pr1RFH2+LMV4bG4a+1gxZ1hQTFwUiLS2e5vVKkUDTMNe4mofS1+JWRxlUR+CvdGwQyNS7eNpXa01fA+9AYoC+3Lx4CAxTMSVFMjUCBJ3cviRtTzUMQxntGk8kHsYte03STE2XV9js25l2ierdem2AbqHreMpwrq2c45ZzTDK6rKx1SZO7jownF/SedszYYa0YIHTQFH0WKqNOUuDWtLWhgDq+krutbBZK9bQmmFQ0Y2WujbUkSOHU8qZgElE1dQ38rtJ1PPMQqSdmwXaiiiLpqqKo4nCpjr1hNn4dBAOytJJFoXLcF4lbKKq8kuS1qXeDunmY1NbAcalIprqWNNSIYSYqjfw4mAGPpIJoB0YdopBBJeDB4wRcmS6tTHHK55jgB7P5r6S1bsqyMF7ZJWz+oMnAiiYZovSTjY2nuFxkTQNDZLU7mTMPl3oOJExXBkJL1TRXa1GKpZ5kCdsr66ChmOuVKEFlbW8H5vSHsVqDXl8oIvU0UZgdjCHrpr6aZ1A5nIUKe55QMSVdkVaSMFxeyKjIqmSsNThwEYTJMZEWbgdpgm7FMGVRHFjtjB0mE3Ota7Kk6qJE+o1UYLjdiixrhqorpigRWDPZGC4bXZWk1VqUGOyYzAx7LKmtNEUUR1zPlYbriE3gvBRNFaaHrB2JxZzslgERU1uJ4r5a2d4wy2Wt6+uVuhbFg/Vkk8Nt/ZJqGpIGhsii0GocnMZd4AMdjKSshBm7+MThmGyomiarujhOgDQau1RMY22C3lIUL9BMc4Y7n7TSdFVWRImRWykDMZeD1xL4P8RC7ExcUtKxMRgcr8HYRZjg8pjzEHsa2dRAc9GFmbGoZVXEHOWvdAOUiCh163jOGXcFbK2sTFnWRYkru1khcb3xeg06e2ktytilnXYSd//b2lBNFbWjYyY6hFPIuiSt12tVEqZY6ofuMftIWTZV0PTXorSZRmJO3FGlAUIXaS1M8EIeHIOWopmaJs5i635CdKzIa0VbgXGyKFyI11vg/IsJqtlKlJZfS46KGU5qYByp6StRBl+15Ku4066yspZNXZ+50beSrWKSAGVhqOvVzJMvPdlccXt53dTXkm4yc11kmR9wNyGt15ourREbwjmwOJGJAXvBW5MMld1696QkGdgHBmQQDhumymyqBTs9CiYDEP6aK1NjN1dElkcEc1uLKa0VUxOhiU+oTYamm4aqGcymU8kSBuGGV+uVrukK4ggNdxakU0KqbmhwR94cDNA5lXB7C0XVZNNAnDGjzGB0Tgrc80+mqUqGYTCbK8VMeoG7Ym2o6kpieF4AN6sG7igceKGVLrGbFCVL24HHwlyrkrkyDGZjCuwsPLiDblkxDclgN6GLmeYHd0ENBN26pqi8G0FvHiHstXRjbegrc8VsfpAsUxHuWM5UQXNmtzNrSiYk3EkCwzRBheLdoEfmJsPdaGauYNhqMI8yxmchw+2kFbgXU2W3lYlW+jbMfkNZSYbMr8WMSEWIG/7JwGtJDLebYOc6xF9oUtdwQpAngZPJFHGn/zVZA703r9YxKlkjZkswdM00VmvmQzmCxGrYa336Sl+p7Pb4EWduw950pWvSWpPZHeEhz9WGPTMrGbqksY+viLPc4e7yMxRzpZiII/KUCeEnncPMdWDKEtyBwTzSwk/Thz9Bq69lyWA/v3kiGR1229BB2M5wazVetjvcHAOqapjyCpGrgSn63nR3uKeMtLXKbtUeOxUgdg9haCro63Ru1u9JSYl9iEhXFclczYd7SqWRDV0BbVZRmQfdo7Nq4k4UyBqIu032jXYg5yPuhMAKTjCxOwOMkVMSf5J1rcN0DOyO/41I2onr1jVVVXSGx3vHXi2KvRFTVVeapDIb0Y++SxM3oFzppiGtERn6mBi8di0pZrxorExJMiRmdXng9lrsDDsywAmCE2b9+8BFtrgp18A/is5uL+TgRbm4+wh0EGUb7KYKei/ixcVpmPCEL7Mx58AFvbheS5VU1VyxO8A7cFMvHlTVWGm6obLb83byJmDsuXdJh7MPrDsE9FXD+El1DAWmoWHW7fbc0Yt9VmulK5ouM3NXw5cAY+edNU3gW9klXRq4OBN3NV5RDUNVDGa+tf/KaNwlX8NUFLjgyAXphtCzyqqkayvZZLfRqv+Gadw1BMWUVdVgN209cKsubncFt0zpMrsZq4E7bXEPYprwoBzoBDiVP2GsAuI/Q1oxPJF86uJx7CwKkm6ADoBZbNVzszl2EsSVATfIsZtn6r87HReqkq8ooLIus4M6pVu9lEGoooMQm92mgL7737GjKti8Vga7I009N7FjLsGapqGuTHaHYYdvesfdEbKGGTlNTt3VhHOGsqFrsqEZKh8f0Lg0GDt1CDCpZph8oj/kZcTYTctYqRrL7a89V8njHkTRVBBb6aw9QOeuelxz6sYaelWV8VClxFm/NBp3vwho/nC4wgUo8fZmWZFUuJWQcfjXufUeexO2IcFjt3ys2bjoGnddALQhFcSpzAy6TTygfAlLvJxTIS59Y63CxHjs1unqYItpwAlhiqKqumnq7JIQoy+lJx1bA+cvGZK6Zrd1BI13SpStwK0uJst1RDRm4lqhwKMxJghgORsZJhCBa0RkVUOT4XkMnd1RyV7QZJG3LCm6sma3BxUNd0pQC+czNV1nl4+tFzJpdCsra0XWNXY75oYQT4pydV034V08PKpH0eXV/BxJx6eZumLAMQ9fxMQdNQh8Vzo8icbuGCMaMfQYhIBlbb3S2E2BovEWLoMoFDbWq5UpmexW7nshk0abIHrXZRAVK9NrRX4vWbp8Av+G91MePpcwD5eWTbisDGY0MWnMMo6ACfXCmxjJj3xKKzB4g1vLGcEtvG8DLuEkkwYMuwJdMw+kxNsZVV1ZmSaNDIltlMX4AnHDYrWVtPfyxZ5fCbe7w6TcK43CDU8YFMtdj9gUyTp5Qzd1iUIy+Pwcy/IKusYyvU1wCV7eeb5bHHJ5dByLMBmnJK0lGZQGW5hww6MZWEFMeNOJqawOt72ygJnvu8vtuHXjxHWgZMv2PTslPG0Kz9CtFbhGwALuAeq0LVfySodpzFQK21h6EU5ZZ4WZ1lYKXG1ngS/7W96NuC9k/lnTpNVqJVE4fttrO7gzmGh4b8rw6jxjekyGgpZvngpUlSwruwE8Ighwx0882HG8cOKYpPpoa1NfKSbG1BdURuaaTFAdDHU9viUlrr0NXLK7dC8VTc0b7vjKd7i/l0wd3CQPuojxvQM8UkC4sW4FxvKayW5jXWOxivhKkUtQh9eqzHAbaGNuPcdJFGKtlJWiq2tO6/9T8uebii7pisZpMa1ESlRJgV9RVVNhd8dqw6ik84+yrioGHJMxm2FqWJR00kNeG6auqgqF4XgRvqVx1VnBvzd26oHAGLxswc1p8DAF4dKErsi6aawprKOOR2rtZLK8lZdrVTfMlUohb2Uf2h9uOtGkMgiRZRDRU2hKo0CSW1PWDidSKWxNQSOFobzvvlCpqLJmyqYm43TZNMASW1fXgG2NNUb0hAs3ny4CA6XJllXWcp6XipW3QkEltisYo5jweh1WYLfuZk+21ATGnqYKJx0UVs5066WZHTruxBLX4NWEcAmdG8wJHn8FAmeYe5IR1p2dZjT8E1AjK4YmU4hKRwMltyrwTBIYUVNY4egB6/mZm3jh41TfpAC/BKJ9MBLlh5TYrDLcf2CaKwrRKRpsnpoljvz89Oxk06orEyZlZxWm9IAlr7RA32oNagKzcAW0rsR7mdyd6pq2hlkSOaEkr65rWYepQGVWXWmBdJ95pJfryGsVDPZlffqeHjTCMApJbwOQNB1uQmUE7K97IC4JYcuhEjyrhqGrEo1zs2i88Ewv6PU9Z/Loaa0AnKs1s64JhZS4Ca0VcyUpawqbi3rA7mN4k/bk4pfAP3BrDjecEwal6ipPc8iqZaXPwbQbPlfAZ0qGTmuRBAlwYnkbuq5IYPDJBeKEol6rwIXqhswquMsSO0zzSb5pBoU3VGqqpLFqPgic5FZVZW0FBnissE7ozRUV3o2tMpsY++6F2+j79IkmSTVWKwpHDLoo7aQ8+mxl9sYnXYUHvQ5Mk0RhT90IiFNGmYoua9SWuhEwQzAmTrNk72SkhQ1zNeuaarIINzoYyUN1aQ0TMCtMWjWE+eT63j6YEqqvNM2EJ4hYIYRhMOEShwnvnZDWLCKKEhlxyRraCi5qMJnOrMBNSeEHDyes1rrOoo+uAJLnv7+EPbMJY1vm+CYEOjD4XmsGhS39PSCf3SkdCjxOqalwcZVFxNgESO4D5ZVpaBLoVBhhfHYdci8DPDS8MVuicCR5GB2x/RRDM+S1TuEA8hEgXCt/dJzlIfHUZaCq8KurT3c3d+/vvixssmm+1VpT14ZEs0V/vLm6/u3ummimR5N1mMCDVtMIw6pcb+AUKUCSF3AYTt36KK/WhqEAV0MfaQFworOGNxmv17JEbRqqi3BKWK1qwEvrqsmgpEt05Pe8yLppGjQugO/BNm3UZK6UNYgTKJyj6oFXzDBNnFxU1itVXykKxRaCdoH5z5dhSOQC4djOANE+rZgQoGTi/8CgBrnhdJ9F4DFghvCbm8Bd8gufZIsmMIMEdyZR2EIxaqMf8aVesgm3yWoMLzVv7vMjvYLvUtVUMNw1pX/818Xri7ubT19Atbi5/7N1d//1/c1n68vt5y/Xt/c313cXby4AkT9FwebKTlzrg5cE8AxKruLvDw8Av/3sbu+yyPn2Rzvxcp8Lv34D/wUfgP9cxHaSfY7D8uOb8o/q8uKNLks7VZEkLzDM8tfX5R9OFHtAyfbbx+iQL7gjqMeK5c//OPwLmuS9u7P3foZJwANjd9v3e/XDuv1mefVm+TUFJl8m0Y/AXi5Sz4d3N6c+vP8nF5ACJ+Y8ec/u8tm43Nipe/lsLJSFdKmYyvKuInGX7bdedGks0yy1Nja8TivcLjt2ib893hzk5sWOBrUcBasj+4drJx2J0K5dFNMKdxs56bx2BQjoV7lKUoEQwJ+X5RFHqbZsFf8APgC0+H+/vrq37j5/vb3Km/3b378E/qvCg/z0cCEDbhev3NCJtl74CL74ev/h0ny4+P3PD8lD+Lbwa6/Af2M3ASCABd2fKn/3cJE/9urV213kb93kVWgH8Gfgk3beY/Ur/B0Yo/y1c/FulDqeD3xhBO+igW9CP/1qn3g/9VixbRVgOet7lHxLY9txlx2/tjxI7d74i1S8HIk6S1yXM96Wyn6kzROyrEH2aevHh7zthjnOU1r78XYuSGMNdUDhOJRuniuYH8imvoGS7z3Eyr74x6geaP2HFNyM23tTSYnm7fLgYNHutojIG/628cAi9Z08L7tbe6bFcOEkVS0Df9IlWI4ZjkCWHX01yzfZtoAejoVd3n1890t+NowFzraOU552e4g/q9QJLDD1KusBd3t/bV1FQRyFYHiQ0q6yJaqulqEWVDxngZDJ9iPq7ahmKqSufmjuM3z0yQ63xfwcI1gdPWMhsatUHT09kFrjfRZoWip6gIAnF0VA6oFhHRMkbR3jHHHf6KHXMTfmA1q+Gflg45nOU+/zDrn1TOepOw9gjMKPYBzSebTz8PWHW1X59CsYIXYf7UqO9glCP6K3AX1Aku1jqxpO0q7gmDMwB8sta6ZZVsyXB1pLJOblCK6HzFtnRRUBGcW001P3Vo1iundM3UBPMdB2yjStVs5l9yMfUU2qly3bCeKzolshxqPpUo9KGLN0U2ySm31iB+dFs4KMSzRzzo7oATIW0eJOm/OhWQDGIgnnHM6LZYkYjybQEe6iM2N6BI1HNrDBu6mTeHEWUR/kMObcwY5HPfb9MyNcIMai6Z6ba3IJXNMx7fUZ8awwY1HdpYlzZs6pgoxF9DF2kjPrbyrImES9MyvQEjE2TQtu6Dk/rhVsLMJPu7NrqRVkTKIvZ8fzBZ+mp5yZOyoA45G0zy3ILxHj0XRs58k9M6IVZiyq31x47W54XlxroLHI+s72vIgWgHFJJruzo3mAjEe02J95RjwLxNg0Xzb2mQ2/66iJ6NbP5J8f7Tp6PPrH7KJnxPoIGo/s+cXAPlEM7J9dDOyTxMCB7fmb6OW8mNZA45GN7afN2S3V1FHj0U3PLPIvAOORfD6zZdQCMBbJ2AnPbJalRIxHMzmz2KEAjEvSSr3H0PbPj2wdOBbp1D3LjqYBG4/wua3SpASrNMc7Hc+IZ4UZl+rZbX04YsaieoajGbKxzP4MBzN7stHMGS6nkq2mPm/tM2ulJWIsmt+39DfWs6VZIh5DMyhOHZ0FwzpYko3CZ8ARhRlrp3DPD8iv0V+29uC7yZhN8I2Dlae3wafwhBJyl3PriEvjvCaL40CYhViYY9kAVl6aiUaLLL4mTw/xKrzkduYaO0R2CPIIxj1vW6lyXgVcA43N+myYEtbi86rAxHX33KrtUI3t71aa/tsLif23iDWiuBEYjRa/XZ8LxZHsvLOhh0A6uo73fd2s+MUZfeqBTiE3hy+IK2lSLX1JG+iY1lG8UyRiEp1aARODWL6n8wyYlTgxqMVu4sVPbgI6xzMg2ETLvntrNAYx3GLLPoXbbwPFbLVnQG1g89BQqz0DZkM7xk+32jMg2ERLRPOYKvR8yB4xkzuqvEGj0g4M+6rD/NLL4+yH53tMdqDVdlxN1ORGA64Cx2LwrJww47uWoQC2JQrpBOPkorCqVK1Kw7Gl4HWrIIhukh38bIYPcGh5euyAGYVUyc3EcoMQVssHNpBi+PsyM9p5EOw9jDm2KWKOHY/J7UTzVrmFmpF6GyxBLTgbmkeslL1Jfmdoso+z8fMR/T6lmQKvkxoJWShFwmxA1Xr2kmwvTLzZsUzeQAGrJRoydjKeZs3sqBPaDH2AmXsq74wMVS4SjLTUiDphHXLeB4I4rT7Kw8Ape7BmCll606nI1LSCGL6JrbR6L2L2s1ho1WI0zJaxCvfVi5hy9YSqbC+kWC+BNJG2MRSQqkpYhzfGyVUvlJemiU2rhnLUulfnRTEaRZMeGubo0ouTCAyHU8vOb4oRuAC7QDm4xqpBiFXypR+swyNqr2LTolCfxSXYBUq599pvXd/+Qa/zOsgTxEMcwJQO4ghtTD05PG2B4eCzEViPjrO4E5ZTCyV7l1eYUox2U1ikaDZHaNNayph9r4dLnU9te0UbtZuofQuXMOw09Xbe4Xqbme17YFdtCkLjQxi5szFIbGa9AE9Ss9xwP/fh5WYhlYBOQwdk3dmPoDTBHyGdhn94Uyj4R0ij4Fe3kopEoAI1hkIy94GQNvyk59BHGzq8DGU/90aAJvgjpDb8kZMQyPCsp9HPG5wVxIsA5ghpXJUTZIK0SaINbEQVhOm3fD8VikYd1KhqOCZGyu/ZpBAiuYEAWcAbt5v25vhG4M6dqz13f9uGX0M1Io4TrASakE6XghART6MERgc84NnN7B1WA/oG3Vkh6r4IgVqz3o+N08CziR3A67FFgl+DdBp/cemoSPhrkMbUntmDzFbdGRVjFk9a+bXVnlDVp43rNJP591I1CPTvmGrjFiHJQhP6QA4FNHoBw4YOsNNcBNhE36DQv1m+jVyAbM0N5P25mNvIBUjt2kDen7i1g1yI/J1N8EPZOdv450/z18Dem8QPgVtAd9NEdZrF/DnrGvB7M9K1cQuRu6sBfSgzFwJ95s49Fd5GXyAai17EaK2LjNn0YDGUnnc+6kAeTkcd4YyLUAXC7Y7GfYgHRYJeIRoZ1QmEvf/gMjKqEwh5iWdkVCcQcr/3HHxfVCcS+COkcVGdQNgLOOPiIYFwF3BGRxQCQT8iorY2sk28Z8SO9tZT0LHR26AlgNs+0F5CJOXS04Dzprx1SYDJiDr/Yv13YEqC8p6l40FWOvuW4GnY+RcnjqxKi9ZwnV6OzY/0ijBfiOYxeuKweF6AOo4mMnLmrXgaZjiHO5rErFwNdKMJiUplPAkhZirQPEZPWRyfn3/aopfKuPmLxvOCzGEMUeIymdH0m/NGOzVjFMFOExqGSxeRyMhhd83TiMhi3Lik6WZE5HGEhuUBBaUycbzV+ar7RdOzbCI72TbT2rSeeAIPQIotB4SScyq8HufGclFcT4MejLAsmR4+lwXThHO6jh2ed6IwSyK+TmwEjTYssu5o3DCpoZFX94y2QdEldxCNLU2eRx1HMOg72sh4ViZ4UXRr59vpk5U+7bNt9H3SEfDOkfKu+CJmGJNzBS1grqZXzHcgEFVnd3vZ9vzae4oP88oDjGkkLiU0U7MaKKHyNCp2CXG+lAK7s8+T/Az19VfRqX6+uJDjpKu4jJMIluepfuKQd+jRcSxug9Jc5RJSra4XqeAuW3hOdBDwEKoZWEHMbb/gIPgmnNF9e2vT+8EEWzdOXMfO3K1l+56d8ksjiqB4ClaHareK8T5/hmDRBTICN+f9zL2wBzYxN1Bnf8sDLPeFW8CEgNwEMcLK8Bj53BYuMAyjzY9dBao6J9o6hiWbTsrapvFQR4XuhtpRbUtGx5obO/WAo7ezJwseMICnoPl6OUizsiv8uw/RYKVAvWTtZG4Xe42nUqI6QeeHm4pTIB0wOOBFKAYkoBNeBmhwX4RqGf2YCKjMXyzDuE5QyucvQAghTtmgEWHTEKFc+lEN0tm6mz23JUI09grCMFAvzezQcUWpPGg8mBTmrzj9mAap7Ow0E8rP9gDCJTF/gQyAGibj+ZmbeOGjMCWCBoRLQoAS6Qc1SCbPvBtHfp6mSJRSGQBFQmb+0jkBbJAUaGaJ9yJK0aDQYMGfvzD6EI2hsc88X4wSqJAMwg6jkNuJdzTcEsEgzL/ubdBAQtg4ROqvB2ENEoIJmEDQ4jnCDLb7EGHTmL8BD6EaprOP4yjJhCkSFBw8AgIURh+kYSLPgbV1d17IeemjC78J5CRoUapOG8p44AJUGRSc4YWPxA7TfKuXIPbvAYRLYv6yGAA1SEaAQGhcBPTdC7fRd1HqDQrNyWWe7sJ4kXXKyuyNP6cHRUJZYkCftwH0wjlJIUwzK82SvZPNbv0WFBzoIlgfAecEhSfX9/bB3EMxFJITwOEYYl7AJYIRQOevG3UUYwDzvV5iAHPfDRJo2FyvER5A3XM18ABoQapIA8sp8M+uEJ1mE8do0AJYvIPlBPhn1xHA8dVRjAQ8v63bSOrAUbuBmjt0unmS24kAnO7e5M6NFDAJ9WWgqqf2mgJtV5/ubu7e331Z2LyNBtMqADbLI9xlCw+1Mz8t2h9vrq5/u7vmTbhSe+Q1butXGE7e+XUD1zJC5Hn/VksKw3l2hgLFVTsq0S4RgEgGXmE4W69dY4VEcsKvhfMMF7uoR/d78FkRzDzewjPNMnQgD80vdFEXU7iigEfBEbzry5lchiHfrg9YDt3z1eH8E/V8gO/pjq/9sVHd43jhxDFdzJXQZZ9Kys2pEIlUl7j2NnAXwZauyppYpNp3v17/dn9HXW1NLFItvNOQ9inEUmal0InCnfe4T/K9G6Xm9+7O3vsZUO3bG9dvfJOfhrvKk8x6G88HmsHPm2SrGJJpv5EW8P/eSRp4MraTrP1g4OwXwA8u3F2iKovgEbSA/E/410aXpZ2qSJIXGCZ4P4si33kCgNtCQJSzSD0ALV2k6WKXANCQ8yJOor+4TrbIE8+H0T14/wq+f0AFJIJWNyRr+22RZrYD/u2Bv9+U7XNhyZqk6IqxVmVFXWmaoq/XRs1vvnVfcg+//QIilp+rYn27bHyPfnp0NExB2tHD9AnbuqmTeDGsCz+/XdY/le6nUV/yb98uC7vnny7+8f8B5ZCEKw===END_SIMPLICITY_STUDIO_METADATA