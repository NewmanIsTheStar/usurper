/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef WEATHER_H
#define WEATHER_H



//prototypes
void weather_task(__unused void *params);
void weather_initialize(void);
int receive_weather_info_from_ecowitt(unsigned char *rx_bytes, int rx_len);
int init_web_variables(void);
int invalidate_weather_variables(void);
void set_irrigation_relay_test_zone(int zone);
int get_irrigation_relay_test_zone(void);

/* ecowitt command message format
    Fixed header, CMD, SIZE, DATA1, DATA2, … , DATAn, CHECKSUM
    Fixed header: 2 bytes, header is fixed as  = 0xffff
    CMD: 1 byte, Command
    SIZE: 1 byte, packet size，counted from CMD till CHECKSUM   NOTE: size is 2 bytes in responses!!!
    DATA: n bytes, payloads，variable length
    CHECKSUM: 1 byte, CHECKSUM=CMD+SIZE+DATA1+DATA2+…+DATAn
*/


// ecowitt CMD byte
typedef enum
{
  CMD_WRITE_SSID= 0x11, // send SSID and Password to WIFI module
  CMD_BROADCAST= 0x12, // UDP cast for device echo，answer back data size is 2 Bytes
  CMD_READ_ECOWITT= 0x1E, // read aw.net setting
  CMD_WRITE_ECOWITT= 0x1F, //write back awt.net setting
  CMD_READ_WUNDERGROUND= 0x20, // read Wunderground setting
  CMD_WRITE_WUNDERGROUND= 0x21, //write back Wunderground setting
  CMD_READ_WOW= 0x22, // read WeatherObservationsWebsite setting
  CMD_WRITE_WOW= 0x23, // write back WeatherObservationsWebsite setting
  CMD_READ_WEATHERCLOUD= 0x24, // read Weathercloud setting
  CMD_WRITE_WEATHERCLOUD= 0x25, //write back Weathercloud setting
  CMD_READ_SATION_MAC= 0x26, // read MAC address
  CMD_READ_CUSTOMIZED= 0x2A, // read Customized sever setting
  CMD_WRITE_CUSTOMIZED= 0x2B, // write back Customized sever setting
  CMD_WRITE_UPDATE= 0x43, // firmware upgrade
  CMD_READ_FIRMWARE_VERSION= 0x50, // read current firmware version number
  CMD_READ_USR_PATH= 0x51,
  CMD_WRITE_USR_PATH= 0x52,

  CMD_GW1000_LIVEDATA= 0x27, // read current data，reply data size is 2bytes.
  CMD_GET_SOILHUMIAD= 0x28, // read Soilmoisture Sensor calibration parameters
  CMD_SET_SOILHUMIAD= 0x29, // write back Soilmoisture Sensor calibration parameters
  CMD_GET_MulCH_OFFSET= 0x2C, // read multi channel sensor offset value
  CMD_SET_MulCH_OFFSET= 0x2D, // write back multi channel sensor OFFSET value
  CMD_GET_PM25_OFFSET= 0x2E, // read PM2.5OFFSET calibration data
  CMD_SET_PM25_OFFSET= 0x2F, // writeback PM2.5OFFSET calibration data
  CMD_READ_SSSS= 0x30, // read system info
  CMD_WRITE_SSSS= 0x31, // write back system info
  CMD_READ_RAINDATA= 0x34, // read rain data
  CMD_WRITE_RAINDATA= 0x35, // write back rain data
  CMD_READ_GAIN= 0x36, // read rain gainMode: GW1000 V1.0
  CMD_WRITE_GAIN= 0x37, // write back rain gain
  CMD_READ_CALIBRATION= 0x38, // read sensor set offset calibration value
  CMD_WRITE_CALIBRATION= 0x39, // write back sensor set offset value
  CMD_READ_SENSOR_ID= 0x3A, // read Sensors ID
  CMD_WRITE_SENSOR_ID= 0x3B, // write back Sensors ID
  CMD_READ_SENSOR_ID_NEW= 0x3C, //// this is reserved for newly added sensors
  CMD_WRITE_REBOOT= 0x40, // system restart
  CMD_WRITE_RESET= 0x41, // reset to default
  CMD_READ_CUSTOMIZED_PATH= 0x51,
  CMD_WRITE_CUSTOMIZED_PATH= 0x52,
  CMD_GET_CO2_OFFSET= 0x53, // CO2 OFFSET
  CMD_SET_CO2_OFFSET= 0x54, // CO2 OFFSET
  CMD_READ_RSTRAIN_TIME= 0x55, // read rain reset time
  CMD_WRITE_RSTRAIN_TIME= 0x56, // write back rain reset time
  CMD_READ_RAIN= 0x57,
  CMD_WRITE_RAIN= 0x58,
  CMD_LIST_UNKNOWN,
} CMD_LT;

//ecowitt parameters
typedef enum
{
  ITEM_INTEMP  = 0x01, //Indoor Temperature (℃)2
  ITEM_OUTTEMP  = 0x02, //Outdoor Temperature (℃)2
  ITEM_DEWPOINT  = 0x03, //Dew point (℃)2
  ITEM_WINDCHILL  = 0x04, //Wind chill (℃)2
  ITEM_HEATINDEX  = 0x05, //Heat index (℃)2
  ITEM_INHUMI  = 0x06, //Indoor Humidity (%)1
  ITEM_OUTHUMI  = 0x07, //Outdoor Humidity (%)1
  ITEM_ABSBARO  = 0x08, //Absolutely Barometric (hpa)2
  ITEM_RELBARO  = 0x09, //Relative Barometric (hpa)2
  ITEM_WINDDIRECTION  = 0x0A, //Wind Direction (360°)2
  ITEM_WINDSPEED  = 0x0B, //Wind Speed (m/s)2
  ITEM_GUSTSPEED  = 0x0C, //Gust Speed (m/s)2
  ITEM_RAINEVENT  = 0x0D, //Rain Event (mm)2
  ITEM_RAINRATE  = 0x0E, //Rain Rate (mm/h)2
  ITEM_RAINHOUR  = 0x0F, //Rain hour (mm)2
  ITEM_RAINDAY  = 0x10, //Rain Day (mm)2
  ITEM_RAINWEEK  = 0x11, //Rain Week (mm)2
  ITEM_RAINMONTH  = 0x12, //Rain Month (mm)4
  ITEM_RAINYEAR  = 0x13, //Rain Year (mm)4
  ITEM_RAINTOTALS  = 0x14, //Rain Totals (mm)4
  ITEM_LIGHT  = 0x15, //Light (lux)4
  ITEM_UV  = 0x16, //UV (uW/m2)2
  ITEM_UVI  = 0x17, //UVI (0-15 index)1
  ITEM_TIME  = 0x18, //Date and time6
  ITEM_DAYLWINDMAX  = 0x19, //Day max wind(m/s)2
  ITEM_TEMP1  = 0x1A, //Temperature 1(℃)2
  ITEM_TEMP2  = 0x1B, //Temperature 2(℃)2
  ITEM_TEMP3  = 0x1C, //Temperature 3(℃)2
  ITEM_TEMP4  = 0x1D, //Temperature 4(℃)2
  ITEM_TEMP5  = 0x1E, //Temperature 5(℃)2
  ITEM_TEMP6  = 0x1F, //Temperature 6(℃)2
  ITEM_TEMP7  = 0x20, //Temperature 7(℃)2
  ITEM_TEMP8  = 0x21, //Temperature 8(℃)2
  ITEM_HUMI1  = 0x22, //Humidity 1, 0-100%1
  ITEM_HUMI2  = 0x23, //Humidity 2, 0-100%1
  ITEM_HUMI3  = 0x24, //Humidity 3, 0-100%1
  ITEM_HUMI4  = 0x25, //Humidity 4, 0-100%1
  ITEM_HUMI5  = 0x26, //Humidity 5, 0-100%1
  ITEM_HUMI6  = 0x27, //Humidity 6, 0-100%1
  ITEM_HUMI7  = 0x28, //Humidity 7, 0-100%1
  ITEM_HUMI8  = 0x29, //Humidity 8, 0-100%1Mode: GW1000 V1.0
  ITEM_PM25_CH1  = 0x2A, //PM2.5 Air Quality Sensor(μg/m3)2
  ITEM_SOILTEMP1  = 0x2B, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE1  = 0x2C, //Soil Moisture(%)1
  ITEM_SOILTEMP2  = 0x2D, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE2  = 0x2E, //Soil Moisture(%)1
  ITEM_SOILTEMP3  = 0x2F, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE3  = 0x30, //Soil Moisture(%)1
  ITEM_SOILTEMP4  = 0x31, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE4 = 0x32, //Soil Moisture(%)1
  ITEM_SOILTEMP5  = 0x33, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE5 = 0x34, //Soil Moisture(%)1
  ITEM_SOILTEMP6 = 0x35, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE6  = 0x36, //Soil Moisture(%)1
  ITEM_SOILTEMP7  = 0x37, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE7  = 0x38, //Soil Moisture(%)1
  ITEM_SOILTEMP8  = 0x39, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE8  = 0x3A, //Soil Moisture(%)1
  ITEM_SOILTEMP9  = 0x3B, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE9  = 0x3C, //Soil Moisture(%)1
  ITEM_SOILTEMP10  = 0x3D, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE10  = 0x3E, //Soil Moisture(%)1
  ITEM_SOILTEMP11  = 0x3F, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE11  = 0x40, //Soil Moisture(%)1
  ITEM_SOILTEMP12  = 0x41, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE12  = 0x42, //Soil Moisture(%)1
  ITEM_SOILTEMP13  = 0x43, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE13  = 0x44, //Soil Moisture(%)1
  ITEM_SOILTEMP14  = 0x45, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE14  = 0x46, //Soil Moisture(%)1
  ITEM_SOILTEMP15  = 0x47, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE15  = 0x48, //Soil Moisture(%)1
  ITEM_SOILTEMP16  = 0x49, //Soil Temperature(℃)2
  ITEM_SOILMOISTURE16  = 0x4A, //Soil Moisture(%)1
  ITEM_LOWBATT  = 0x4C, //All sensor lowbatt 16 char16
  ITEM_PM25_24HAVG1  = 0x4D, //for pm25_ch12
  ITEM_PM25_24HAVG2  = 0x4E, //for pm25_ch22
  ITEM_PM25_24HAVG3  = 0x4F, //for pm25_ch32
  ITEM_PM25_24HAVG4  = 0x50, //for pm25_ch42
  ITEM_PM25_CH2  = 0x51, //PM2.5 Air Quality Sensor(μg/m3)2
  ITEM_PM25_CH3  = 0x52, //PM2.5 Air Quality Sensor(μg/m3)2
  ITEM_Piezo_Rain_Rate  = 0x80, //2
  ITEM_Piezo_Event_Rain  = 0x81, //2
  ITEM_Piezo_Hourly_Rain  = 0x82, //2
  ITEM_Piezo_Daily_Rain  = 0x83, //4
  ITEM_Piezo_Weekly_Rain  = 0x84, //4
  ITEM_Piezo_Monthly_Rain  = 0x85, //4
  ITEM_Piezo_yearly_Rain  = 0x86, //4
  ITEM_Piezo_Gain10  = 0x87, //2*10
  ITEM_RST_RainTime  = 0x88, //3

  //started appearing after upgrading ecowitt firmware
  ITEM_UNKNOWN_6C = 0x6C,  // guess 4 bytes
}PARAM_LT;

typedef struct WEB_VARIABLES
{
  char last_usurped_timestring[50];
  int outside_temperature;
  int wind_speed;
  int daily_rain;
  int weekly_rain;                  // this comes from weather station based on calendar weeks and is not useful for irrigation decisions
  int trailing_seven_days_rain;     // this is accumulated from the daily totals as it is more relevant to irrigation decision
  uint8_t soil_moisture[16];
  char watchdog_timestring[50];
  uint32_t us_last_rx_packet;
  char status_message[50];
  char stack_message[256];
  char ip_address_string[50];
  char network_mask_string[50];
  char gateway_string[50];
  int socket_max;
  int bind_failures;
  int connect_failures;  
  int syslog_transmit_failures;
  int govee_transmit_failures;
  int weather_station_transmit_failures;
  int pluto_transmit_failures;
  char software_server[100];
  char software_url[100];
  char software_file[100];
  int led_current_pattern;
  int led_current_transition_delay;
  char led_last_request_ip[32];
  int irrigation_test_enable;
  int thermostat_set_point;
  int thermostat_hysteresis;
  
} WEB_VARIABLES_T;                  //remember to add initialization code when adding to this structure !!!

#endif
