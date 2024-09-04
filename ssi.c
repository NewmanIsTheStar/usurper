/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"

#include "lwip/sockets.h"

#include "flash.h"
#include "weather.h"
#include "calendar.h"
#include "utility.h"
#include "config.h"

#include "pluto.h"
#include "led_strip.h"

#ifdef USE_GIT_HASH_AS_VERSION
#include "githash.h"
#endif




extern WEB_VARIABLES_T web;
extern NON_VOL_VARIABLES_T config;

/*List of SSI tags used in html files
  Notes:-
    1. This list is used to create a matching enum and string constant for each SSI tag
    2. Only append to end of list.  Do not insert, delete or reorder the existing items!
    3. Related items are assumed to be in sequence in the code e.g. days of week
*/
#define SSI_TAGS \
    x(usurped)   \
    x(time)      \
    x(temp)      \
    x(wind)      \
    x(rain)      \
    x(lstpck)    \
    x(dogtme)    \
    x(rainwk)    \
    x(status)    \
    x(sun)       \
    x(mon)       \
    x(tue)       \
    x(wed)       \
    x(thu)       \
    x(fri)       \
    x(sat)       \
    x(strt1)     \
    x(strt2)     \
    x(strt3)     \
    x(strt4)     \
    x(strt5)     \
    x(strt6)     \
    x(strt7)     \
    x(dur1)      \
    x(dur2)      \
    x(dur3)      \
    x(dur4)      \
    x(dur5)      \
    x(dur6)      \
    x(dur7)      \
    x(tz)        \
    x(dss)       \
    x(dse)       \
    x(ts1)       \
    x(ts2)       \
    x(ts3)       \
    x(ts4)       \
    x(dlse)      \
    x(ecoip)     \
    x(wkrn)      \
    x(dyrn)      \
    x(wndt)      \
    x(ssid)      \
    x(wpass)     \
    x(dhcp)      \
    x(ipad)      \
    x(nmsk)      \
    x(ltme)      \
    x(rly)       \
    x(gpio)      \
    x(lpat)      \
    x(lspd)      \
    x(lpin)      \
    x(lrgbw)     \
    x(lnum)      \
    x(slog)      \
    x(gvea)      \
    x(wthr)      \
    x(day1)      \
    x(day2)      \
    x(day3)      \
    x(day4)      \
    x(day5)      \
    x(day6)      \
    x(day7)      \
    x(gvee)      \
    x(gvei)      \
    x(gveu)      \
    x(gves)      \
    x(lie)       \
    x(lia)       \
    x(liu)       \
    x(lis)       \
    x(ghsh)      \
    x(lstsvn)    \
    x(dstu)      \
    x(spdu)      \
    x(tmpu)      \
    x(uau)       \
    x(stck)      \
    x(ipaddr)    \
    x(netmsk)    \
    x(gatewy)    \
    x(msck)      \
    x(bfail)     \
    x(cfail)     \
    x(sfail)     \
    x(wfail)     \
    x(gfail)     \
    x(simpe)     \
    x(mweek)     \
    x(colour)    \
    x(calpge)    \
    x(swlhst)    \
    x(swlurl)    \
    x(swlfle)    \
    x(wse)       \
    x(sloge)     \
    x(rsadr1)    \
    x(rsadr2)    \
    x(rsadr3)    \
    x(rsadr4)    \
    x(rsadr5)    \
    x(rsadr6)    \
    x(rse)       \
    x(pertyp)    \
    x(wific)     \
    x(gway)      \
    x(soilm1)    \
    x(soilt1)

//enum used to index array of pointers to SSI string constants  e.g. index 0 is SSI_usurped
enum ssi_index
{
#define x(name) SSI_ ## name,
SSI_TAGS
#undef x
};

// array of pointers to SSI string constants
const char * ssi_tags[] =
{
#define x(name) #name,
SSI_TAGS
#undef x
};


u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen)
{
    size_t printed;
    char timestamp[50];
    uint32_t us_now;
    long temp;

    switch(iIndex) {
        case SSI_usurped:  // usurped
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.last_usurped_timestring);    
        }
        break;
        case SSI_time: // time
        {
            if(!get_timestamp(timestamp, sizeof(timestamp), false)) {
                printed = snprintf(pcInsert, iInsertLen, "%s", timestamp);
            }
            else {
                printed = snprintf(pcInsert, iInsertLen, "Time unavailable");   
            }
        }
        break;        
        case SSI_temp: // temp
        {
            if (!config.use_archaic_units)
            {
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", web.outside_temperature/10, web.outside_temperature%10); 
            }
            else
            {
                temp = (web.outside_temperature*9)/5 + 320;
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", temp/10, temp%10);
            }              
        }
        break;
        case SSI_wind: // wind
        {         
            if (!config.use_archaic_units)
            {
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", web.wind_speed/10, web.wind_speed%10); 
            }
            else
            {
                temp = (web.wind_speed*3281 + 500)/1000;
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", temp/10, temp%10);
            }              
        } 
        break;  
        case SSI_rain: // rain
        {
            if (!config.use_archaic_units)
            {
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", web.daily_rain/10, web.daily_rain%10); 
            }
            else
            {
                temp = (10*web.daily_rain + 127)/254;
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", temp/10, temp%10);
            }            
        }  
        break;
        case SSI_lstpck: // lstpck
        {
            if (web.us_last_rx_packet)
            {
                us_now = time_us_32();
                printed = snprintf(pcInsert, iInsertLen, "%d s", (us_now - web.us_last_rx_packet)/1000000);   
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "never");   
            }
        }  
        break;     
        case SSI_dogtme: // dogtme
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.watchdog_timestring); 
        }  
        break;   
        case SSI_rainwk: // rainwk
        {          
            if (!config.use_archaic_units)
            {
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", web.weekly_rain/10, web.weekly_rain%10); 
            }
            else
            {
                temp = (10*web.weekly_rain + 127)/254;
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", temp/10, temp%10);
            }             
        }  
        break;   
        case SSI_status: // status
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.status_message); 
        }  
        break;
        case SSI_sun:    // sun
        case SSI_mon:    // mon
        case SSI_tue:    // tue
        case SSI_wed:    // wed
        case SSI_thu:    // thu
        case SSI_fri:    // fri
        case SSI_sat:    // sat
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.day_schedule_enable[iIndex-SSI_sun]?"ON":"OFF"); 
        }  
        break;
        case SSI_strt1:
        case SSI_strt2:
        case SSI_strt3:
        case SSI_strt4:
        case SSI_strt5:
        case SSI_strt6:
        case SSI_strt7:
        {
            if (config.day_schedule_enable[iIndex-16])
            {
                printed = snprintf(pcInsert, iInsertLen, "%02d : %02d", config.day_start[iIndex-SSI_strt1]/60, config.day_start[iIndex-SSI_strt1]%60);
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "-- : --"); 
            }
        }
        break;
        case SSI_dur1:
        case SSI_dur2:
        case SSI_dur3:
        case SSI_dur4:
        case SSI_dur5:
        case SSI_dur6:
        case SSI_dur7:
        {
            if (config.day_schedule_enable[iIndex-23])
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", config.day_duration[iIndex-SSI_dur1]);    
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "--");  
            }
        }
        break;
        case SSI_tz:
        {
            if (config.timezone_offset > 0)
            {
                // leading + sign added
                if (config.timezone_offset%60 == 0)
                {
                    // normal time zone with whole number of hours
                    printed = snprintf(pcInsert, iInsertLen, "+%d", config.timezone_offset/60);                 
                }
                else
                {   // unusual time zone with hours and minutes
                    printed = snprintf(pcInsert, iInsertLen, "+%d:%d", config.timezone_offset/60, abs(config.timezone_offset%60));    
                }                
            }
            else
            {
                // leading - sign automatically added
                if (config.timezone_offset%60 == 0)
                {
                    // normal time zone with whole number of hours
                    printed = snprintf(pcInsert, iInsertLen, "%d", config.timezone_offset/60);                 
                }
                else
                {   // unusual time zone with hours and minutes
                    printed = snprintf(pcInsert, iInsertLen, "%d:%d", config.timezone_offset/60, abs(config.timezone_offset%60));    
                }
            }
        }
        break;
        case SSI_dss:
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.daylightsaving_start);
        }
        break;
        case SSI_dse:
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.daylightsaving_end);
        }
        break;   
        case SSI_ts1:    // ts1
        case SSI_ts2:    // ts2
        case SSI_ts3:    // ts3
        case SSI_ts4:    // ts4
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.time_server[iIndex-SSI_ts1]); 
        }  
        break; 
        case SSI_dlse:    // dlse -- daylight saving enable
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.daylightsaving_enable?"checked":""); 
        }  
        break;    
        case SSI_ecoip: //ecoip
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.weather_station_ip);
        }               
        break;
        case SSI_wkrn: //wkrn
        {
            printed = snprintf(pcInsert, iInsertLen, "%d.%d", config.rain_week_threshold/10, config.rain_week_threshold%10);   
        }               
        break;        
        case SSI_dyrn: //dyrn
        {
            printed = snprintf(pcInsert, iInsertLen, "%d.%d", config.rain_day_threshold/10, config.rain_day_threshold%10);
        }               
        break; 
        case SSI_wndt: //wndt
        {
            printed = snprintf(pcInsert, iInsertLen, "%d.%d", config.wind_threshold/10, config.wind_threshold%10);
        }               
        break;
        case SSI_ssid: //ssid
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.wifi_ssid);
        }               
        break;    
        case SSI_wpass: //wpass
        {
            //printed = snprintf(pcInsert, iInsertLen, "%s", config.wifi_password);
            printed = snprintf(pcInsert, iInsertLen, "********");
        }               
        break;    
        case SSI_dhcp: //dhcp
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.dhcp_enable?"checked":"");
        }               
        break;    
        case SSI_ipad: //ipad
        {
            if (!config.dhcp_enable)
            {
                if (strncasecmp(config.ip_address, "automatic+via+DHCP", sizeof(config.ip_address))==0)
                {
                    //config.ip_address[0] = 0;
                    STRNCPY(config.ip_address, web.ip_address_string, sizeof(config.ip_address));   
                }
                printed = snprintf(pcInsert, iInsertLen, "%s", config.ip_address);
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "automatic via DHCP");
            }            
        }               
        break;    
        case SSI_nmsk: //nmsk
        {
            if (!config.dhcp_enable)
            {
                if (strncasecmp(config.network_mask, "automatic+via+DHCP", sizeof(config.network_mask))==0)
                {
                    //config.network_mask[0] = 0;
                    STRNCPY(config.network_mask, web.network_mask_string, sizeof(config.network_mask));  
                }                
                printed = snprintf(pcInsert, iInsertLen, "%s", config.network_mask);
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "automatic via DHCP");
            }                          
        }               
        break;  
        case SSI_ltme: //ltme
        {
            printed = get_local_time_string(pcInsert, iInsertLen);
        }               
        break;
        case SSI_rly: //rly
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.relay_normally_open?"checked":"");
        }               
        break; 
        case SSI_gpio: //gpio
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.gpio_number);
        }               
        break;   
        case SSI_lpat: //lpat
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.led_pattern);
            //aled_pattern_web_selection(pcInsert, iInsertLen, config.led_pattern);   
        }               
        break; 
        case SSI_lspd: //lspd
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.led_speed);
        }               
        break;     
        case SSI_lpin: //lpin
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.led_pin);
        }               
        break;                                      
        case SSI_lrgbw: //lrgbw
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.led_rgbw);
        }               
        break;  
        case SSI_lnum: //lnum
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.led_number);
        }               
        break;  
        case SSI_slog: //slog
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.syslog_server_ip);
        }
        break;     
        case SSI_gvea: //gvea
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.govee_light_ip);
        } 
        break;   
        case SSI_wthr: //wthr
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.weather_station_ip);
        } 
        break; 
        case SSI_day1:    // day1 (sun)
        case SSI_day2:    // day2 (mon)
        case SSI_day3:    // day3 (tue)
        case SSI_day4:    // day4 (wed)
        case SSI_day5:    // day5 (thu)
        case SSI_day6:    // day6 (fri)
        case SSI_day7:    // day7 (sat)
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.day_schedule_enable[iIndex-SSI_day1]?"checked":""); 
        }  
        break;
        case SSI_gvee: //gvee
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.use_govee_to_indicate_irrigation_status?"checked":""); 
        }
        break;
        case SSI_gvei: //gvei
        {
            printed = snprintf(pcInsert, iInsertLen, "%d.%d.%d", config.govee_irrigation_active_red, config.govee_irrigation_active_green, config.govee_irrigation_active_blue);
        }
        break;
        case SSI_gveu: //gveu
        {
            printed = snprintf(pcInsert, iInsertLen, "%d.%d.%d", config.govee_irrigation_usurped_red, config.govee_irrigation_usurped_green, config.govee_irrigation_usurped_blue);
        }
        break;
        case SSI_gves: //gves
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.govee_sustain_duration);
        }
        break;                        
        case SSI_lie: //lie
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.use_led_strip_to_indicate_irrigation_status?"checked":""); 
        }
        break;
        case SSI_lia: //lia
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.led_pattern_when_irrigation_active);
            //aled_pattern_web_selection(pcInsert, iInsertLen, config.led_pattern_when_irrigation_active);             
        }
        break; 
        case SSI_liu: //liu
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.led_pattern_when_irrigation_usurped);
            //aled_pattern_web_selection(pcInsert, iInsertLen, config.led_pattern_when_irrigation_active);              
        }
        break; 
        case SSI_lis: //lis
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.led_sustain_duration);
        }
        break; 
        case SSI_ghsh: //ghsh
        {
            #ifdef USE_GIT_HASH_AS_VERSION
            printed = snprintf(pcInsert, iInsertLen, "%s", GITHASH);
            #else
            printed = snprintf(pcInsert, iInsertLen, "%s", PLUTO_VER);
            #endif
        }                        
        break;
        case SSI_lstsvn: //lstsvn
        {
            if (!config.use_archaic_units)
            {
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", web.trailing_seven_days_rain/10, web.trailing_seven_days_rain%10); 
            }
            else
            {
                temp = (10*web.trailing_seven_days_rain + 127)/254;
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", temp/10, temp%10);
            }                
            
        }                        
        break;    
        case SSI_dstu: //dstu
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.use_archaic_units?"inches":"mm"); 
        }                        
        break;  
        case SSI_spdu: //spdu
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.use_archaic_units?"ft/s":"m/s"); 
        }                        
        break;  
        case SSI_tmpu: //tmpu
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.use_archaic_units?"F":"C"); 
        }                        
        break;
        case SSI_uau: //uau
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.use_archaic_units?"checked":"");
        }               
        break; 
        case SSI_stck: //stck
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.stack_message);
        }               
        break; 
        case SSI_ipaddr: //ipaddr
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.ip_address_string);
        }               
        break;   
        case SSI_netmsk: //netmsk
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.network_mask_string);
        }               
        break;   
        case SSI_gatewy: //gatewy
        {
            if (!config.dhcp_enable)
            {
                if (strncasecmp(config.gateway, "automatic+via+DHCP", sizeof(config.gateway))==0)
                {
                    //config.gateway[0] = 0;
                    STRNCPY(config.gateway, web.gateway_string, sizeof(config.gateway));                  
                }                
                printed = snprintf(pcInsert, iInsertLen, "%s", config.gateway);
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "automatic via DHCP");
            }                          
        }                   
        break;
        case SSI_msck: //msck
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.socket_max);
        }               
        break;  
        case SSI_bfail: //bfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.bind_failures);
        }               
        break;  
        case SSI_cfail: //cfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.connect_failures);
        }               
        break;  
        case SSI_sfail: //sfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.syslog_transmit_failures);
        }               
        break;  
        case SSI_wfail: //wfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.weather_station_transmit_failures);
        }               
        break;  
        case SSI_gfail: //gfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.govee_transmit_failures);
        }               
        break;   
        case SSI_simpe: //simpe
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.use_simplified_english?"checked":"");
        }  
        break;         
        case SSI_mweek: //mweek
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.use_monday_as_week_start?"checked":"");
        }  
        break;         
        case SSI_colour: //colour
        {
            if (config.use_simplified_english)
            {
                printed = snprintf(pcInsert, iInsertLen, "color");
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "colour");
            }
        }                                                                                                                                                   
        break; 
        case SSI_calpge: //calpge
        {
            switch(config.personality)
            {
            default:
            case NO_PERSONALITY:
                //printf("redirecting to personality.shtml (%d)\n", config.personality);
                printed = snprintf(pcInsert, iInsertLen, "/personality.shtml");
                break;
            case SPRINKLER_USURPER:
            case SPRINKLER_CONTROLLER:
                if (config.use_monday_as_week_start)
                {
                    //printf("redirecting to landscape_monday.shtml\n");
                    printed = snprintf(pcInsert, iInsertLen, "/landscape_monday.shtml");
                }
                else
                {
                    //printf("redirecting to landscape.shtml\n");
                    printed = snprintf(pcInsert, iInsertLen, "/landscape.shtml");
                }
                break;
            case LED_STRIP_CONTROLLER:
                printed = snprintf(pcInsert, iInsertLen, "/addressable_led.shtml");
                break;
            }
        }                                                                                                                                                   
        break; 
        case SSI_swlhst: //swlhst
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.software_server);
        }               
        break; 
        case SSI_swlurl: //swlurl
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.software_url);
        }               
        break; 
        case SSI_swlfle: //swlfle
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.software_file);
        }               
        break;    
        case SSI_wse: //wse
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.weather_station_enable?"checked":"");
        }   
        break;   
        case SSI_sloge: //sloge
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.syslog_enable?"checked":"");
        } 
        break;                                    
        case SSI_rsadr1: //rsdar1
        case SSI_rsadr2: //rsdar2
        case SSI_rsadr3: //rsdar3
        case SSI_rsadr4: //rsdar4
        case SSI_rsadr5: //rsdar5
        case SSI_rsadr6: //rsdar6               
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.led_strip_remote_ip[iIndex-SSI_rsadr1]); 
        }                     
        break; 
        case SSI_rse: //rse
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.led_strip_remote_enable?"checked":"");
        } 
        break;
        case SSI_pertyp: //pertyp
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.personality);
        } 
        break; 
        case SSI_wific: //wific
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.wifi_country);
        } 
        break;  
        case SSI_gway: //gway
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.gateway_string);
        }                       
        break;
        case SSI_soilm1: //soilm1
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.soil_moisture[0]);
        }                       
        break;    
        case SSI_soilt1: //soilt1
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.soil_moisture_threshold[0]);
        }                       
        break;             

        default:
        {
            printed = snprintf(pcInsert, iInsertLen, "Unhandled SSI tag");    
        }
        break;        
    }


    return ((u16_t)printed);
}

void ssi_init(void)
{
    // configure SSI handler
    http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
}