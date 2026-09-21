idf_component_register(
    SRCS 
        "main.cpp"
        "../components/arduino/libraries/NetworkClientSecure/src/ssl_client.cpp"
        "../components/arduino/libraries/Adafruit_GFX/Adafruit_GFX.cpp"
        "../components/arduino/libraries/Adafruit_GFX/Adafruit_GrayOLED.cpp"
        "../components/arduino/libraries/Adafruit_GFX/Adafruit_SPITFT.cpp"
        "../components/arduino/libraries/Adafruit_BusIO/Adafruit_BusIO_Register.cpp"
        "../components/arduino/libraries/Adafruit_BusIO/Adafruit_I2CDevice.cpp"
        "../components/arduino/libraries/Adafruit_BusIO/Adafruit_SPIDevice.cpp"
        "../components/arduino/libraries/Adafruit_ILI9341/Adafruit_ILI9341.cpp"
        "../components/arduino/libraries/ESP32Encoder/src/ESP32Encoder.cpp"
        "../components/arduino/libraries/TJpg_Decoder/src/TJpg_Decoder.cpp"
    INCLUDE_DIRS 
        "."
        "../components/arduino/libraries/NetworkClientSecure/src"
        "../components/arduino/libraries/ArduinoJson/src"
        "../components/arduino/libraries/Adafruit_GFX"
        "../components/arduino/libraries/Adafruit_BusIO"
        "../components/arduino/libraries/Adafruit_ILI9341"
        "../components/arduino/libraries/ESP32Encoder/src"
        "../components/arduino/libraries/TJpg_Decoder/src"
    REQUIRES 
        arduino
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=reorder -Wno-error)