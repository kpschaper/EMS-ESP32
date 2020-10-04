/*
 * EMS-ESP - https://github.com/proddy/EMS-ESP
 * Copyright 2019  Paul Derbyshire
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EMSESP_EMSDEVICE_H_
#define EMSESP_EMSDEVICE_H_

#include <string>
#include <vector>
#include <functional>

#include "emsfactory.h"
#include "telegram.h"
#include "mqtt.h"
#include "helpers.h"

namespace emsesp {

class EMSdevice {
  public:
    static constexpr uint8_t EMS_DEVICES_MAX_TELEGRAMS = 20;

    // device_type defines which derived class to use, e.g. BOILER, THERMOSTAT etc..
    EMSdevice(uint8_t device_type, uint8_t device_id, uint8_t product_id, const std::string & version, const std::string & name, uint8_t flags, uint8_t brand)
        : device_type_(device_type)
        , device_id_(device_id)
        , product_id_(product_id)
        , version_(version)
        , name_(name)
        , flags_(flags)
        , brand_(brand) {
    }

    virtual ~EMSdevice() = default; // destructor of base class must always be virtual because it's a polymorphic class

    inline uint8_t device_id() const {
        return device_id_;
    }

    std::string        device_type_name() const;
    static std::string device_type_2_device_name(const uint8_t device_type);
    static uint8_t     device_name_2_device_type(const char * topic);

    inline uint8_t product_id() const {
        return product_id_;
    }

    void product_id(uint8_t product_id) {
        product_id_ = product_id;
    }

    inline bool is_device_id(uint8_t device_id) {
        return ((device_id & 0x7F) == (device_id_ & 0x7F));
    }

    inline uint8_t flags() const {
        return flags_;
    }

    inline void flags(uint8_t flags) {
        flags_ = flags;
    }

    // see enum DeviceType below
    inline uint8_t device_type() const {
        return device_type_;
    }

    inline void version(std::string & version) {
        version_ = version;
    }

    inline std::string version() const {
        return version_;
    }

    inline void brand(uint8_t brand) {
        brand_ = brand;
    }

    inline uint8_t brand() const {
        return brand_;
    }

    inline void name(const std::string & name) {
        name_ = name;
    }

    inline std::string name() const {
        return name_;
    }

    inline uint8_t unique_id() const {
        return unique_id_;
    }

    inline void unique_id(uint8_t unique_id) {
        unique_id_ = unique_id;
    }

    std::string    brand_to_string() const;
    static uint8_t decode_brand(uint8_t value);

    std::string to_string() const;
    std::string to_string_short() const;

    void show_telegram_handlers(uuid::console::Shell & shell);
    void show_mqtt_handlers(uuid::console::Shell & shell);

    using process_function_p = std::function<void(std::shared_ptr<const Telegram>)>;
    void register_telegram_type(const uint16_t telegram_type_id, const __FlashStringHelper * telegram_type_name, bool fetch, process_function_p cb);
    bool handle_telegram(std::shared_ptr<const Telegram> telegram);

    void write_command(const uint16_t type_id, const uint8_t offset, uint8_t * message_data, const uint8_t message_length, const uint16_t validate_typeid);
    void write_command(const uint16_t type_id, const uint8_t offset, const uint8_t value, const uint16_t validate_typeid);
    void write_command(const uint16_t type_id, const uint8_t offset, const uint8_t value);
    void read_command(const uint16_t type_id);

    void register_mqtt_topic(const std::string & topic, mqtt_subfunction_p f);
    void register_mqtt_cmd(const __FlashStringHelper * cmd, cmdfunction_p f);

    // virtual functions overrules by derived classes
    virtual void show_values(uuid::console::Shell & shell) = 0;
    virtual void publish_values()                          = 0;
    virtual bool updated_values()                          = 0;
    virtual void device_info_web(JsonArray & root)         = 0;

    std::string telegram_type_name(std::shared_ptr<const Telegram> telegram);

    void fetch_values();
    void toggle_fetch(uint16_t telegram_id, bool toggle);
    bool get_toggle_fetch(uint16_t telegram_id);

    void reserve_mem(size_t n) {
        telegram_functions_.reserve(n);
    }

    static void print_value_json(uuid::console::Shell &      shell,
                                 const __FlashStringHelper * key,
                                 const __FlashStringHelper * name,
                                 const __FlashStringHelper * suffix,
                                 JsonObject &                json);

    // prints a ems device value to the console, handling the correct rendering of the type
    // padding is # white space
    // name is the name of the parameter
    // suffix is any string to be appended after the value
    // format:
    //  for ints its  0=no division, 255=handle as boolean, other divide by the value given and render with a decimal point
    //  for floats its the precision in number of decimal places from 0 to 8
    //  for bools its EMS_VALUE_BOOL (0xFF)
    template <typename Value>
    static void print_value(uuid::console::Shell &      shell,
                            uint8_t                     padding,
                            const __FlashStringHelper * name,
                            Value &                     value,
                            const __FlashStringHelper * suffix,
                            const uint8_t               format = 0) {
        char buffer[15];
        if (Helpers::render_value(buffer, value, format) == nullptr) {
            return;
        }

        uint8_t i = padding;
        while (i-- > 0) {
            shell.print(F(" "));
        }

        shell.printf(PSTR("%s: %s"), uuid::read_flash_string(name).c_str(), buffer);

        if (suffix != nullptr) {
            shell.println(uuid::read_flash_string(suffix).c_str());
        } else {
            shell.println();
        }
    }

    // takes a value from an ems device and creates a nested json (name, value)
    // which can be passed to the web UI
    template <typename Value>
    static void render_value_json(JsonArray &                 json,
                                  const std::string &         prefix,
                                  const __FlashStringHelper * name,
                                  Value &                     value,
                                  const __FlashStringHelper * suffix,
                                  const uint8_t               format = 0) {
        // create the value as a string using the render_value function
        char buffer[15];
        if (Helpers::render_value(buffer, value, format) == nullptr) {
            return;
        }

        JsonObject dataElement = json.createNestedObject();

        // append suffix to end
        if (suffix != nullptr) {
            char text[20];
            snprintf_P(text, sizeof(text), PSTR("%s%s"), buffer, uuid::read_flash_string(suffix).c_str());
            dataElement["value"] = text;
        } else {
            dataElement["value"] = buffer;
        }

        // add prefix to name
        char text2[100];
        snprintf_P(text2, sizeof(text2), PSTR("%s%s"), prefix.c_str(), uuid::read_flash_string(name).c_str());
        dataElement["name"] = text2;
    }

    static void print_value(uuid::console::Shell & shell, uint8_t padding, const __FlashStringHelper * name, const __FlashStringHelper * value);
    static void print_value(uuid::console::Shell & shell, uint8_t padding, const __FlashStringHelper * name, const char * value);

    enum Brand : uint8_t {
        NO_BRAND, // 0
        BOSCH,    // 1
        JUNKERS,  // 2
        BUDERUS,  // 3
        NEFIT,    // 4
        SIEGER,   // 5
        WORCESTER // 11
    };

    enum DeviceType : uint8_t {
        SYSTEM = 0, // this is us (EMS-ESP)
        SENSOR,     // for internal dallas sensors
        BOILER,
        THERMOSTAT,
        MIXING,
        SOLAR,
        HEATPUMP,
        GATEWAY,
        SWITCH,
        CONTROLLER,
        CONNECT,
        UNKNOWN
    };

    // device IDs
    static constexpr uint8_t EMS_DEVICE_ID_BOILER = 0x08; // fixed device_id for Master Boiler/UBA
    static constexpr uint8_t EMS_DEVICE_ID_MODEM  = 0x48; // gateways like the KM200

    // type IDs
    static constexpr uint16_t EMS_TYPE_VERSION    = 0x02; // type ID for Version information. Generic across all EMS devices.
    static constexpr uint16_t EMS_TYPE_UBADevices = 0x07; // EMS connected devices

    // device flags: The lower 4 bits hold the unique identifier, the upper 4 bits are used for specific flags
    static constexpr uint8_t EMS_DEVICE_FLAG_NONE = 0;

    // Solar Module
    static constexpr uint8_t EMS_DEVICE_FLAG_SM10  = 1;
    static constexpr uint8_t EMS_DEVICE_FLAG_SM100 = 2;
    static constexpr uint8_t EMS_DEVICE_FLAG_ISM   = 3;

    // Mixing Module
    static constexpr uint8_t EMS_DEVICE_FLAG_MMPLUS = 1;
    static constexpr uint8_t EMS_DEVICE_FLAG_MM10   = 2;
    static constexpr uint8_t EMS_DEVICE_FLAG_IPM    = 3;

    // Thermostats
    static constexpr uint8_t EMS_DEVICE_FLAG_NO_WRITE  = (1 << 7); // last bit
    static constexpr uint8_t EMS_DEVICE_FLAG_EASY      = 1;
    static constexpr uint8_t EMS_DEVICE_FLAG_RC10      = 2;
    static constexpr uint8_t EMS_DEVICE_FLAG_RC20      = 3;
    static constexpr uint8_t EMS_DEVICE_FLAG_RC20_2    = 4; // Variation on RC20, Older, like ES72?
    static constexpr uint8_t EMS_DEVICE_FLAG_RC30_1    = 5; // variation on RC30, Newer?
    static constexpr uint8_t EMS_DEVICE_FLAG_RC30      = 6;
    static constexpr uint8_t EMS_DEVICE_FLAG_RC35      = 7;
    static constexpr uint8_t EMS_DEVICE_FLAG_RC300     = 8;
    static constexpr uint8_t EMS_DEVICE_FLAG_RC100     = 9;
    static constexpr uint8_t EMS_DEVICE_FLAG_JUNKERS   = 10;
    static constexpr uint8_t EMS_DEVICE_FLAG_JUNKERS_2 = (1 << 6); // 6th bit set if older models

  private:
    uint8_t     unique_id_;
    uint8_t     device_type_ = DeviceType::SYSTEM;
    uint8_t     device_id_   = 0;
    uint8_t     product_id_  = 0;
    std::string version_;
    std::string name_; // the long name for the EMS model
    uint8_t     flags_ = 0;
    uint8_t     brand_ = Brand::NO_BRAND;

    static uuid::log::Logger logger_;

    struct TelegramFunction {
        uint16_t                    telegram_type_id_;   // it's type_id
        const __FlashStringHelper * telegram_type_name_; // e.g. RC20Message
        bool                        fetch_;              // if this type_id be queried automatically

        process_function_p process_function_;

        TelegramFunction(uint16_t telegram_type_id, const __FlashStringHelper * telegram_type_name, bool fetch, process_function_p process_function)
            : telegram_type_id_(telegram_type_id)
            , telegram_type_name_(telegram_type_name)
            , fetch_(fetch)
            , process_function_(process_function) {
        }
    };
    std::vector<TelegramFunction> telegram_functions_; // each EMS device has its own set of registered telegram types
};

} // namespace emsesp

#endif
