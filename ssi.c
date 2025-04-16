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
#include "thermostat.h"

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
    x(soilt1)    \
    x(z1d1d)     \
    x(z1d2d)     \
    x(z1d3d)     \
    x(z1d4d)     \
    x(z1d5d)     \
    x(z1d6d)     \
    x(z1d7d)     \
    x(z2d1d)     \
    x(z2d2d)     \
    x(z2d3d)     \
    x(z2d4d)     \
    x(z2d5d)     \
    x(z2d6d)     \
    x(z2d7d)     \
    x(z3d1d)     \
    x(z3d2d)     \
    x(z3d3d)     \
    x(z3d4d)     \
    x(z3d5d)     \
    x(z3d6d)     \
    x(z3d7d)     \
    x(z4d1d)     \
    x(z4d2d)     \
    x(z4d3d)     \
    x(z4d4d)     \
    x(z4d5d)     \
    x(z4d6d)     \
    x(z4d7d)     \
    x(z5d1d)     \
    x(z5d2d)     \
    x(z5d3d)     \
    x(z5d4d)     \
    x(z5d5d)     \
    x(z5d6d)     \
    x(z5d7d)     \
    x(z6d1d)     \
    x(z6d2d)     \
    x(z6d3d)     \
    x(z6d4d)     \
    x(z6d5d)     \
    x(z6d6d)     \
    x(z6d7d)     \
    x(z7d1d)     \
    x(z7d2d)     \
    x(z7d3d)     \
    x(z7d4d)     \
    x(z7d5d)     \
    x(z7d6d)     \
    x(z7d7d)     \
    x(z8d1d)     \
    x(z8d2d)     \
    x(z8d3d)     \
    x(z8d4d)     \
    x(z8d5d)     \
    x(z8d6d)     \
    x(z8d7d)     \
    x(clpat)     \
    x(cltran)    \
    x(clreq)     \
    x(z1gpio)    \
    x(z2gpio)    \
    x(z3gpio)    \
    x(z4gpio)    \
    x(z5gpio)    \
    x(z6gpio)    \
    x(z7gpio)    \
    x(z8gpio)    \
    x(z1viz)     \
    x(z2viz)     \
    x(z3viz)     \
    x(z4viz)     \
    x(z5viz)     \
    x(z6viz)     \
    x(z7viz)     \
    x(z8viz)     \
    x(zmax)      \
    x(rpage)     \
    x(z1bviz)    \
    x(z2bviz)    \
    x(z3bviz)    \
    x(z4bviz)    \
    x(z5bviz)    \
    x(z6bviz)    \
    x(z7bviz)    \
    x(z8bviz)    \
    x(z1iviz)    \
    x(z2iviz)    \
    x(z3iviz)    \
    x(z4iviz)    \
    x(z5iviz)    \
    x(z6iviz)    \
    x(z7iviz)    \
    x(z8iviz)    \
    x(z1zviz)    \
    x(z2zviz)    \
    x(z3zviz)    \
    x(z4zviz)    \
    x(z5zviz)    \
    x(z6zviz)    \
    x(z7zviz)    \
    x(z8zviz)    \
    x(z1dur)     \
    x(pernme)    \
    x(irgnow)    \
    x(sp1viz)    \
    x(sp2viz)    \
    x(sp3viz)    \
    x(sp4viz)    \
    x(sp5viz)    \
    x(sp6viz)    \
    x(sp7viz)    \
    x(sp8viz)    \
    x(sp9viz)    \
    x(sp10viz)   \
    x(sp11viz)   \
    x(sp12viz)   \
    x(sp1nme)    \
    x(sp2nme)    \
    x(sp3nme)    \
    x(sp4nme)    \
    x(sp5nme)    \
    x(sp6nme)    \
    x(sp7nme)    \
    x(sp8nme)    \
    x(sp9nme)    \
    x(sp10nme)    \
    x(sp11nme)    \
    x(sp12nme)    \
    x(sp13nme)    \
    x(sp14nme)    \
    x(sp15nme)    \
    x(sp16nme)    \
    x(sp1tmp)    \
    x(sp2tmp)    \
    x(sp3tmp)    \
    x(sp4tmp)    \
    x(sp5tmp)    \
    x(sp6tmp)    \
    x(sp7tmp)    \
    x(sp8tmp)    \
    x(sp9tmp)    \
    x(sp10tmp)    \
    x(sp11tmp)    \
    x(sp12tmp)    \
    x(sp13tmp)    \
    x(sp14tmp)    \
    x(sp15tmp)    \
    x(sp16tmp)    \
    x(sp17tmp)    \
    x(sp18tmp)    \
    x(sp19tmp)    \
    x(sp20tmp)    \
    x(sp21tmp)    \
    x(sp22tmp)    \
    x(sp23tmp)    \
    x(sp24tmp)    \
    x(sp25tmp)    \
    x(sp26tmp)    \
    x(sp27tmp)    \
    x(sp28tmp)    \
    x(sp29tmp)    \
    x(sp30tmp)    \
    x(sp31tmp)    \
    x(sp32tmp)    \
    x(ts1st)    \
    x(ts2st)    \
    x(ts3st)    \
    x(ts4st)    \
    x(ts5st)    \
    x(ts6st)    \
    x(ts7st)    \
    x(ts8st)    \
    x(ts9st)    \
    x(ts10st)    \
    x(ts11st)    \
    x(ts12st)    \
    x(ts13st)    \
    x(ts14st)    \
    x(ts15st)    \
    x(ts16st)    \
    x(ts17st)    \
    x(ts18st)    \
    x(ts19st)    \
    x(ts20st)    \
    x(ts21st)    \
    x(ts22st)    \
    x(ts23st)    \
    x(ts24st)    \
    x(ts25st)    \
    x(ts26st)    \
    x(ts27st)    \
    x(ts28st)    \
    x(ts29st)    \
    x(ts30st)    \
    x(ts31st)    \
    x(ts32st)    \
    x(ts1en)    \
    x(ts2en)    \
    x(ts3en)    \
    x(ts4en)    \
    x(ts5en)    \
    x(ts6en)    \
    x(ts7en)    \
    x(ts8en)    \
    x(ts9en)    \
    x(ts10en)    \
    x(ts11en)    \
    x(ts12en)    \
    x(ts13en)    \
    x(ts14en)    \
    x(ts15en)    \
    x(ts16en)    \
    x(ts1vz)    \
    x(ts2vz)    \
    x(ts3vz)    \
    x(ts4vz)    \
    x(ts5vz)    \
    x(ts6vz)    \
    x(ts7vz)    \
    x(ts8vz)    \
    x(ts9vz)    \
    x(ts10vz)    \
    x(ts11vz)    \
    x(ts12vz)    \
    x(ts13vz)    \
    x(ts14vz)    \
    x(ts15vz)    \
    x(ts16vz)   \
    x(ts17vz)   \
    x(ts18vz)   \
    x(ts19vz)   \
    x(ts20vz)   \
    x(ts21vz)   \
    x(ts22vz)   \
    x(ts23vz)   \
    x(ts24vz)   \
    x(ts25vz)   \
    x(ts26vz)   \
    x(ts27vz)   \
    x(ts28vz)   \
    x(ts29vz)   \
    x(ts30vz)   \
    x(ts31vz)   \
    x(ts32vz)   \
    x(ts1in)    \
    x(ts2in)    \
    x(ts3in)    \
    x(ts4in)    \
    x(ts5in)    \
    x(ts6in)    \
    x(ts7in)    \
    x(ts8in)    \
    x(ts9in)    \
    x(ts10in)    \
    x(ts11in)    \
    x(ts12in)    \
    x(ts13in)    \
    x(ts14in)    \
    x(ts15in)    \
    x(ts16in)    \
    x(pwip)      \
    x(pwhost)    \
    x(pwpass)    \
    x(pwgdhd)    \
    x(pwgdci)    \
    x(pwblhd)    \
    x(pwblhe)    \
    x(pwblcd)    \
    x(pwblce)    \
    x(tday)      \
    x(tpst)      \
    x(tptmp)     \
    x(tsaddvz)   \
    x(tg0_0) \
    x(tg0_1) \
    x(tg0_2) \
    x(tg0_3) \
    x(tg0_4) \
    x(tg0_5) \
    x(tg0_6) \
    x(tg0_7) \
    x(tg1_0) \
    x(tg1_1) \
    x(tg1_2) \
    x(tg1_3) \
    x(tg1_4) \
    x(tg1_5) \
    x(tg1_6) \
    x(tg1_7) \
    x(tg2_0) \
    x(tg2_1) \
    x(tg2_2) \
    x(tg2_3) \
    x(tg2_4) \
    x(tg2_5) \
    x(tg2_6) \
    x(tg2_7) \
    x(tg3_0) \
    x(tg3_1) \
    x(tg3_2) \
    x(tg3_3) \
    x(tg3_4) \
    x(tg3_5) \
    x(tg3_6) \
    x(tg3_7) \
    x(tg4_0) \
    x(tg4_1) \
    x(tg4_2) \
    x(tg4_3) \
    x(tg4_4) \
    x(tg4_5) \
    x(tg4_6) \
    x(tg4_7) \
    x(tg5_0) \
    x(tg5_1) \
    x(tg5_2) \
    x(tg5_3) \
    x(tg5_4) \
    x(tg5_5) \
    x(tg5_6) \
    x(tg5_7) \
    x(tg6_0) \
    x(tg6_1) \
    x(tg6_2) \
    x(tg6_3) \
    x(tg6_4) \
    x(tg6_5) \
    x(tg6_6) \
    x(tg6_7) \
    x(tg7_0) \
    x(tg7_1) \
    x(tg7_2) \
    x(tg7_3) \
    x(tg7_4) \
    x(tg7_5) \
    x(tg7_6) \
    x(tg7_7) \
    x(tct)   \
    x(tcs)   \
    x(thgpio) \
    x(tcgpio) \
    x(tfgpio) \
    x(gpiou) \
    x(gpioif) \
    x(gpioih) \
    x(gpioil) \
    x(gpiooh) \
    x(gpiool)

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
    int i;
    bool new_thermostat_period_found = false;
    int grid_x = 0;
    int grid_y = 0;
    bool first_item_printed = false;
    char gpio_list[192];

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
                printed = snprintf(pcInsert, iInsertLen, "%c%d.%d", web.outside_temperature<0?'-':'\0', abs(web.outside_temperature/10), abs(web.outside_temperature%10)); 
            }
            else
            {
                temp = (web.outside_temperature*9)/5 + 320;
                printed = snprintf(pcInsert, iInsertLen, "%c%ld.%ld", temp<0?'-':'\0', abs(temp)/10, abs(temp%10));
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
                printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", temp/10, temp%10);
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
                printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", temp/10, temp%10);
            }            
        }  
        break;
        case SSI_lstpck: // lstpck
        {
            if (web.us_last_rx_packet)
            {
                us_now = time_us_32();
                printed = snprintf(pcInsert, iInsertLen, "%lu s", (us_now - web.us_last_rx_packet)/1000000);   
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
                printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", temp/10, temp%10);
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
                if (true /*config.personality == SPRINKLER_USURPER*/)
                {
                    printed = snprintf(pcInsert, iInsertLen, "-- : --"); 
                }
                else
                {
                    printed = snprintf(pcInsert, iInsertLen, " "); 
                }                
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
            if (config.day_schedule_enable[iIndex-SSI_dur1])
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", config.day_duration[iIndex-SSI_dur1]);    
            }
            else
            {                 
                if (true /*config.personality == SPRINKLER_USURPER*/)
                {
                    printed = snprintf(pcInsert, iInsertLen, "--"); 
                }
                else
                {
                    printed = snprintf(pcInsert, iInsertLen, " "); 
                } 
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
            printed = snprintf(pcInsert, iInsertLen, "%s", get_pattern_name(config.led_pattern)); 
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
            printed = snprintf(pcInsert, iInsertLen, "%s", get_pattern_name(config.led_pattern_when_irrigation_active));           
        }
        break; 
        case SSI_liu: //liu
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", get_pattern_name(config.led_pattern_when_irrigation_usurped));            
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
                printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", temp/10, temp%10);
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
            case SPRINKLER_CONTROLLER:
                if (config.use_monday_as_week_start)
                {
                    //printf("redirecting to landscape_monday.shtml\n");
                    printed = snprintf(pcInsert, iInsertLen, "/zm_landscape.shtml");
                }
                else
                {
                    //printf("redirecting to landscape.shtml\n");
                    printed = snprintf(pcInsert, iInsertLen, "/zs_landscape.shtml");
                }
                break;                
            case LED_STRIP_CONTROLLER:
                printed = snprintf(pcInsert, iInsertLen, "/led_controller.shtml");
                break;
            case HVAC_THERMOSTAT:
                if (config.use_monday_as_week_start)
                {
                    //printf("redirecting to landscape_monday.shtml\n");
                    printed = snprintf(pcInsert, iInsertLen, "/tm_thermostat.shtml");
                }
                else
                {
                    //printf("redirecting to landscape.shtml\n");
                    printed = snprintf(pcInsert, iInsertLen, "/ts_thermostat.shtml");
                }            
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
        case SSI_z1d1d:
        case SSI_z1d2d:
        case SSI_z1d3d:
        case SSI_z1d4d:
        case SSI_z1d5d:
        case SSI_z1d6d:
        case SSI_z1d7d:
        case SSI_z2d1d:
        case SSI_z2d2d:
        case SSI_z2d3d:
        case SSI_z2d4d:
        case SSI_z2d5d:
        case SSI_z2d6d:
        case SSI_z2d7d:
        case SSI_z3d1d:
        case SSI_z3d2d:
        case SSI_z3d3d:
        case SSI_z3d4d:
        case SSI_z3d5d:
        case SSI_z3d6d:
        case SSI_z3d7d:
        case SSI_z4d1d:
        case SSI_z4d2d:
        case SSI_z4d3d:
        case SSI_z4d4d:
        case SSI_z4d5d:
        case SSI_z4d6d:
        case SSI_z4d7d:
        case SSI_z5d1d:
        case SSI_z5d2d:
        case SSI_z5d3d:
        case SSI_z5d4d:
        case SSI_z5d5d:
        case SSI_z5d6d:
        case SSI_z5d7d:
        case SSI_z6d1d:
        case SSI_z6d2d:
        case SSI_z6d3d:
        case SSI_z6d4d:
        case SSI_z6d5d:
        case SSI_z6d6d:
        case SSI_z6d7d:
        case SSI_z7d1d:
        case SSI_z7d2d:
        case SSI_z7d3d:
        case SSI_z7d4d:
        case SSI_z7d5d:
        case SSI_z7d6d:
        case SSI_z7d7d:
        case SSI_z8d1d:
        case SSI_z8d2d:
        case SSI_z8d3d:
        case SSI_z8d4d:
        case SSI_z8d5d:
        case SSI_z8d6d:
        case SSI_z8d7d:
        {  
            if (config.day_schedule_enable[(iIndex-SSI_z1d1d)%7])
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", config.zone_duration[(iIndex-SSI_z1d1d)/7][(iIndex-SSI_z1d1d)%7]);
            }
            else
            {
                if (true /*config.personality == SPRINKLER_USURPER*/)
                {
                    printed = snprintf(pcInsert, iInsertLen, "--");  
                }
                else
                {
                    printed = snprintf(pcInsert, iInsertLen, " ");  
                }
                
            }                                 
        }
        break;
        case SSI_clpat: //clpat
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.led_current_pattern);
        } 
        break;
        case SSI_cltran: //cltran
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.led_current_transition_delay);
        } 
        break;  
        case SSI_clreq: //clreq
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.led_last_request_ip);
        } 
        break;         
        case SSI_z1gpio:
        case SSI_z2gpio:
        case SSI_z3gpio:
        case SSI_z4gpio:
        case SSI_z5gpio:
        case SSI_z6gpio:
        case SSI_z7gpio:
        case SSI_z8gpio:
        {     
            printed = snprintf(pcInsert, iInsertLen, "%d", config.zone_gpio[(iIndex-SSI_z1gpio)%8]);             
        }
        break;
        case SSI_z1viz:
        case SSI_z2viz:
        case SSI_z3viz:
        case SSI_z4viz:
        case SSI_z5viz:
        case SSI_z6viz:
        case SSI_z7viz:
        case SSI_z8viz:
        {
            if ((iIndex-SSI_z1viz)%8 >= config.zone_max)
            {     
                printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
            }
            else
            {
                printed = 0;
            }             
        }
        break; 
        case SSI_zmax: //zmax
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.zone_max);
        }   
        break;     
        case SSI_rpage: //rpage
        {
            switch(config.personality)
            {
            default:
            case NO_PERSONALITY:
            case SPRINKLER_USURPER:   
            case LED_STRIP_CONTROLLER:         
                printed = snprintf(pcInsert, iInsertLen, "/relay.shtml");
                break;
            case SPRINKLER_CONTROLLER:
                printed = snprintf(pcInsert, iInsertLen, "/z_relay.shtml");
                break;
            }            
            
        }   
        break;                  
        case SSI_z1bviz:
        case SSI_z2bviz:
        case SSI_z3bviz:
        case SSI_z4bviz:
        case SSI_z5bviz:
        case SSI_z6bviz:
        case SSI_z7bviz:
        case SSI_z8bviz:
        {
            if ((iIndex-SSI_z1bviz)%8 >= config.zone_max)
            {     
                printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
            }
            else
            {
                //printed = snprintf(pcInsert, iInsertLen, "style=\"width:14.285714286%%\"");
                printed = snprintf(pcInsert, iInsertLen, "style=\"width:13%%\"");                
            }             
        }
        break; 
        case SSI_z1iviz:
        case SSI_z2iviz:
        case SSI_z3iviz:
        case SSI_z4iviz:
        case SSI_z5iviz:
        case SSI_z6iviz:
        case SSI_z7iviz:
        case SSI_z8iviz:
        {
            if ((iIndex-SSI_z1iviz)%8 >= config.zone_max)
            {     
                printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "style=\"font-size: 28px;\"");
            }             
        }
        break;   
        case SSI_z1zviz:
        case SSI_z2zviz:
        case SSI_z3zviz:
        case SSI_z4zviz:
        case SSI_z5zviz:
        case SSI_z6zviz:
        case SSI_z7zviz:
        case SSI_z8zviz:
        {
            if ((iIndex-SSI_z1zviz)%8 >= config.zone_max)
            {     
                printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
            }
            else
            {
                //printed = snprintf(pcInsert, iInsertLen, "style=\"width:14.285714286%%\"");
                printed = snprintf(pcInsert, iInsertLen, "style=\"width:2%%\"");                
            }             
        } 
        break;      
        case SSI_z1dur:
        {
            if (config.personality == SPRINKLER_USURPER)
            {
                printed = snprintf(pcInsert, iInsertLen, "Duration");
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "Zone 1 Duration");
            }
        }
        break;
        case SSI_pernme: //pernme
        {
            switch(config.personality)
            {
            case SPRINKLER_USURPER:
                printed = snprintf(pcInsert, iInsertLen, "Sprinkler Usurper");
                break;
            case SPRINKLER_CONTROLLER:
                printed = snprintf(pcInsert, iInsertLen, "Sprinkler Controller");
                break;
            case LED_STRIP_CONTROLLER:
                printed = snprintf(pcInsert, iInsertLen, "LED Controller");
                break;
            case HVAC_THERMOSTAT:
                printed = snprintf(pcInsert, iInsertLen, "HVAC Controller");
                break;                                 
            default:
            case NO_PERSONALITY:
                printed = snprintf(pcInsert, iInsertLen, "No personality");
                break;
            }
        }             
        break;
        case SSI_irgnow: //irgnow
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.irrigation_test_enable?"checked":"");
        }   
        break;
        case SSI_sp1viz:
        case SSI_sp2viz:
        case SSI_sp3viz:
        case SSI_sp4viz:
        case SSI_sp5viz:
        case SSI_sp6viz:
        case SSI_sp7viz:
        case SSI_sp8viz:
        case SSI_sp9viz:
        case SSI_sp10viz:
        case SSI_sp11viz:
        case SSI_sp12viz:
        {
            if ((iIndex-SSI_sp1viz)%12 >= config.setpoint_number)
            {     
                printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
            }
            else
            {
                printed = 0;
            }             
        }
        break;         
        case SSI_sp1nme: //sp1nme
        case SSI_sp2nme: //sp2nme
        case SSI_sp3nme: //sp3nme
        case SSI_sp4nme: //sp4nme
        case SSI_sp5nme: //sp5nme
        case SSI_sp6nme: //sp6nme
        case SSI_sp7nme: //sp7nme
        case SSI_sp8nme: //sp8nme
        case SSI_sp9nme: //sp9nme
        case SSI_sp10nme: //sp10nme
        case SSI_sp11nme: //sp11nme
        case SSI_sp12nme: //sp12nme
        case SSI_sp13nme: //sp13nme
        case SSI_sp14nme: //sp14nme
        case SSI_sp15nme: //sp15nme
        case SSI_sp16nme: //sp16nme
        {
            //printed = snprintf(pcInsert, iInsertLen, "%s", config.setpoint_name[iIndex-SSI_sp1nme]); 
            printf("error - setpoint_name is not longer supported\n");
        }                     
        break; 
        case SSI_sp1tmp: //sp1tmp
        case SSI_sp2tmp: //sp2tmp
        case SSI_sp3tmp: //sp3tmp
        case SSI_sp4tmp: //sp4tmp
        case SSI_sp5tmp: //sp5tmp
        case SSI_sp6tmp: //sp6tmp
        case SSI_sp7tmp: //sp7tmp
        case SSI_sp8tmp: //sp8tmp
        case SSI_sp9tmp: //sp9tmp
        case SSI_sp10tmp: //sp10tmp
        case SSI_sp11tmp: //sp11tmp
        case SSI_sp12tmp: //sp12tmp
        case SSI_sp13tmp: //sp13tmp
        case SSI_sp14tmp: //sp14tmp
        case SSI_sp15tmp: //sp15tmp
        case SSI_sp16tmp: //sp16tmp
        case SSI_sp17tmp:
        case SSI_sp18tmp:
        case SSI_sp19tmp:
        case SSI_sp20tmp:
        case SSI_sp21tmp:
        case SSI_sp22tmp:
        case SSI_sp23tmp:
        case SSI_sp24tmp:
        case SSI_sp25tmp:
        case SSI_sp26tmp:
        case SSI_sp27tmp:
        case SSI_sp28tmp:
        case SSI_sp29tmp:
        case SSI_sp30tmp:
        case SSI_sp31tmp:
        case SSI_sp32tmp:                
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", config.setpoint_temperaturex10[iIndex-SSI_sp1tmp]/10); 

            if (strlen(pcInsert) < 2)
            {
                // add leading space to maintain column alignment  -- sensible indoor temperatures are 2 digits!
                printed = snprintf(pcInsert, iInsertLen, "&nbsp;&nbsp;%d", config.setpoint_temperaturex10[iIndex-SSI_sp1tmp]/10);    
            }
        }                     
        break;
        case SSI_ts1st:
        case SSI_ts2st:
        case SSI_ts3st:
        case SSI_ts4st:
        case SSI_ts5st:
        case SSI_ts6st:
        case SSI_ts7st:
        case SSI_ts8st:
        case SSI_ts9st:
        case SSI_ts10st:
        case SSI_ts11st:
        case SSI_ts12st:
        case SSI_ts13st:
        case SSI_ts14st:
        case SSI_ts15st:
        case SSI_ts16st:
        case SSI_ts17st:
        case SSI_ts18st:
        case SSI_ts19st:
        case SSI_ts20st:
        case SSI_ts21st:
        case SSI_ts22st:
        case SSI_ts23st:
        case SSI_ts24st:
        case SSI_ts25st:
        case SSI_ts26st:
        case SSI_ts27st:
        case SSI_ts28st:
        case SSI_ts29st:
        case SSI_ts30st:
        case SSI_ts31st:
        case SSI_ts32st:                        
        {
            //printed = mow_to_string(pcInsert, iInsertLen, config.setpoint_start_mow[iIndex-SSI_ts1st]);
            printed = mow_to_time_string(pcInsert, iInsertLen, config.setpoint_start_mow[iIndex-SSI_ts1st]);            
        }
        break; 
        case SSI_ts1en:
        case SSI_ts2en:
        case SSI_ts3en:
        case SSI_ts4en:
        case SSI_ts5en:
        case SSI_ts6en:
        case SSI_ts7en:
        case SSI_ts8en:
        case SSI_ts9en:
        case SSI_ts10en:
        case SSI_ts11en:
        case SSI_ts12en:
        case SSI_ts13en:
        case SSI_ts14en:
        case SSI_ts15en:
        case SSI_ts16en:
        {
            //printed = snprintf(pcInsert, iInsertLen, "%d", config.thermostat_period_end_mow[iIndex-SSI_ts1en]); 
            //printed = mow_to_string(pcInsert, iInsertLen, config.thermostat_period_end_mow[iIndex-SSI_ts1en]);
            printf("error - thermostat_period_end_mow is no longer supported\n");
        }
        break;                  
        case SSI_ts1vz:
        case SSI_ts2vz:
        case SSI_ts3vz:
        case SSI_ts4vz:
        case SSI_ts5vz:
        case SSI_ts6vz:
        case SSI_ts7vz:
        case SSI_ts8vz:
        case SSI_ts9vz:
        case SSI_ts10vz:
        case SSI_ts11vz:
        case SSI_ts12vz:
        case SSI_ts13vz:
        case SSI_ts14vz:
        case SSI_ts15vz:
        case SSI_ts16vz:
        case SSI_ts17vz:
        case SSI_ts18vz:
        case SSI_ts19vz:
        case SSI_ts20vz:
        case SSI_ts21vz:
        case SSI_ts22vz:
        case SSI_ts23vz:
        case SSI_ts24vz:
        case SSI_ts25vz:
        case SSI_ts26vz:
        case SSI_ts27vz:
        case SSI_ts28vz:
        case SSI_ts29vz:
        case SSI_ts30vz:
        case SSI_ts31vz:
        case SSI_ts32vz:                        
        {
            //printf("row = %d web.day= %d mow = %d dfm = %d\n", (iIndex-SSI_ts1vz)%16, web.thermostat_day, config.setpoint_start_mow[iIndex-SSI_ts1vz], get_day_from_mow(config.setpoint_start_mow[iIndex-SSI_ts1vz]));
            //if ((iIndex-SSI_ts1vz)%16 >= config.thermostat_period_number)
            if ((get_day_from_mow(config.setpoint_start_mow[iIndex-SSI_ts1vz]) != web.thermostat_day) ||
                (config.setpoint_start_mow[iIndex-SSI_ts1vz] <0))
            {     
                printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
            }
            else
            {
                printed = 0;
            }             
        }
        break;   
        case SSI_ts1in:
        case SSI_ts2in:
        case SSI_ts3in:
        case SSI_ts4in:
        case SSI_ts5in:
        case SSI_ts6in:
        case SSI_ts7in:
        case SSI_ts8in:
        case SSI_ts9in:
        case SSI_ts10in:
        case SSI_ts11in:
        case SSI_ts12in:
        case SSI_ts13in:
        case SSI_ts14in:
        case SSI_ts15in:
        case SSI_ts16in:
        {
            //TODO RANGE CHECKING!!!
            //printed = snprintf(pcInsert, iInsertLen, "%s", config.setpoint_name[config.thermostat_period_setpoint_index[iIndex-SSI_ts1in]]); 
            printf("error - thermostat_period_setpoint_index is no longer supported\n");
        }
        break;                      
        case SSI_pwip:
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.powerwall_ip);
        }
        break;
        case SSI_pwhost:
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", config.powerwall_hostname);
        }
        break; 
        case SSI_pwpass:
        {
            //printed = snprintf(pcInsert, iInsertLen, "%s", config.powerwall_password);
            printed = snprintf(pcInsert, iInsertLen, "********");
        }
        break;
        case SSI_pwgdhd:
        {
            printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", config.grid_down_heating_setpoint_decrease/10, config.grid_down_heating_setpoint_decrease%10);
        }
        break;        
        case SSI_pwgdci:
        {
            printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", config.grid_down_cooling_setpoint_increase/10, config.grid_down_cooling_setpoint_increase%10);            
        }
        break;
        case SSI_pwblhd:
        {
            printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", config.grid_down_heating_disable_battery_level/10, config.grid_down_heating_disable_battery_level%10);            
        }
        break;        
        case SSI_pwblhe:
        {
            printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", config.grid_down_heating_enable_battery_level/10, config.grid_down_heating_enable_battery_level%10);            
        }
        break;
        case SSI_pwblcd:
        {
            printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", config.grid_down_cooling_disable_battery_level/10, config.grid_down_cooling_disable_battery_level%10);            
        }
        break;   
        case SSI_pwblce:
        {
            printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", config.grid_down_cooling_enable_battery_level/10, config.grid_down_cooling_enable_battery_level%10);            
        }
        break; 
        case SSI_tday:
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", day_name(web.thermostat_day));            
        }
        break;   
        case SSI_tpst:
        {
            CLIP(web.thermostat_period_row, 0, NUM_ROWS(config.setpoint_start_mow));
            //printed = mow_to_string(pcInsert, iInsertLen, config.setpoint_start_mow[web.thermostat_period_row]);
            printed = mow_to_time_string(pcInsert, iInsertLen, config.setpoint_start_mow[web.thermostat_period_row]);            
        }
        break; 
        case SSI_tptmp:
        {
            CLIP(web.thermostat_period_row, 0, NUM_ROWS(config.setpoint_temperaturex10));
            printed = snprintf(pcInsert, iInsertLen, "%d", config.setpoint_temperaturex10[web.thermostat_period_row]/10); 
        }
        break;         
        case SSI_tsaddvz:
        {
            for(i=0; i < NUM_ROWS(config.setpoint_start_mow); i++)
            {
                if (config.setpoint_start_mow[i] < 0)
                {
                    new_thermostat_period_found = true;
                    break;
                }
            }
            if (!new_thermostat_period_found)
            {     
                printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
            }
            else
            {
                printed = 0;
            }            
        }
        break;
        case SSI_tg0_0:
        case SSI_tg0_1:
        case SSI_tg0_2:
        case SSI_tg0_3:
        case SSI_tg0_4:
        case SSI_tg0_5:
        case SSI_tg0_6:
        case SSI_tg0_7:
        case SSI_tg1_0:
        case SSI_tg1_1:
        case SSI_tg1_2:
        case SSI_tg1_3:
        case SSI_tg1_4:
        case SSI_tg1_5:
        case SSI_tg1_6:
        case SSI_tg1_7:
        case SSI_tg2_0:
        case SSI_tg2_1:
        case SSI_tg2_2:
        case SSI_tg2_3:
        case SSI_tg2_4:
        case SSI_tg2_5:
        case SSI_tg2_6:
        case SSI_tg2_7:
        case SSI_tg3_0:
        case SSI_tg3_1:
        case SSI_tg3_2:
        case SSI_tg3_3:
        case SSI_tg3_4:
        case SSI_tg3_5:
        case SSI_tg3_6:
        case SSI_tg3_7:
        case SSI_tg4_0:
        case SSI_tg4_1:
        case SSI_tg4_2:
        case SSI_tg4_3:
        case SSI_tg4_4:
        case SSI_tg4_5:
        case SSI_tg4_6:
        case SSI_tg4_7:
        case SSI_tg5_0:
        case SSI_tg5_1:
        case SSI_tg5_2:
        case SSI_tg5_3:
        case SSI_tg5_4:
        case SSI_tg5_5:
        case SSI_tg5_6:
        case SSI_tg5_7:
        case SSI_tg6_0:
        case SSI_tg6_1:
        case SSI_tg6_2:
        case SSI_tg6_3:
        case SSI_tg6_4:
        case SSI_tg6_5:
        case SSI_tg6_6:
        case SSI_tg6_7:
        case SSI_tg7_0:
        case SSI_tg7_1:
        case SSI_tg7_2:
        case SSI_tg7_3:
        case SSI_tg7_4:
        case SSI_tg7_5:
        case SSI_tg7_6:
        case SSI_tg7_7:
        {
            grid_x = (iIndex-SSI_tg0_0)%8;  //TODO: compute array length
            grid_y = (iIndex-SSI_tg0_0)/8;

            CLIP(grid_x, 0, 7);
            CLIP(grid_y, 0, 7);

            switch(grid_x)
            {
            case 0:
                printed = mow_to_time_string(pcInsert, iInsertLen, web.thermostat_grid[grid_x][grid_y]);
                break;
            default:
                if (web.thermostat_grid[grid_x][grid_y] > -300)
                {
                    printed = snprintf(pcInsert, iInsertLen, "%d&deg;", web.thermostat_grid[grid_x][grid_y]/10); 
                }
                else
                {
                    printed = snprintf(pcInsert, iInsertLen, "&nbsp;"); 
                }
                break;
            }
        }                     
        break; 
        case SSI_tct:  // thermostat current temperature
        {
            if (!config.use_archaic_units)
            {
                printed = snprintf(pcInsert, iInsertLen, "%c%d.%d", web.thermostat_temperature<0?'-':'\0', abs(web.thermostat_temperature/10), abs(web.thermostat_temperature%10)); 
            }
            else
            {
                temp = (web.thermostat_temperature*9)/5 + 320;
                printed = snprintf(pcInsert, iInsertLen, "%c%ld.%ld", temp<0?'-':'\0', abs(temp)/10, abs(temp%10));
            }  
        }       
        break;  
#ifdef INCORPORATE_THERMOSTAT              
        case SSI_tcs:  // thermostat current setpoint
        {
            temp = update_current_setpoints();

            printed = snprintf(pcInsert, iInsertLen, "%c%ld.%ld", temp<0?'-':'\0', abs(temp)/10, abs(temp%10));                           
        }
        break;
#endif
        case SSI_thgpio:
        {
            if (gpio_valid(config.heating_gpio))
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", config.heating_gpio);
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "none");
            }
        }
        break;
        case SSI_tcgpio:
        {
            if (gpio_valid(config.heating_gpio))
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", config.cooling_gpio);
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "none");
            }            
        }
        break; 
        case SSI_tfgpio:
        {
            if (gpio_valid(config.heating_gpio))
            {
                printed = snprintf(pcInsert, iInsertLen, "%d", config.fan_gpio);
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "none");
            }            
        }
        break;                  
        case SSI_gpiou:
        {
            print_gpio_pins_matching_default(gpio_list, sizeof(gpio_list), GP_UNINITIALIZED);
            printed = snprintf(pcInsert, iInsertLen, "%s", gpio_list);
        }
        break;  
        case SSI_gpioif:
        {

            print_gpio_pins_matching_default(gpio_list, sizeof(gpio_list), GP_INPUT_FLOATING);
            printed = snprintf(pcInsert, iInsertLen, "%s", gpio_list);
        
        }
        break; 
        case SSI_gpioih:
        {
               
            print_gpio_pins_matching_default(gpio_list, sizeof(gpio_list), GP_INPUT_PULLED_HIGH);
            printed = snprintf(pcInsert, iInsertLen, "%s", gpio_list);

        }
        break;   
        case SSI_gpioil:
        {            
            print_gpio_pins_matching_default(gpio_list, sizeof(gpio_list), GP_INPUT_PULLED_LOW);
            printed = snprintf(pcInsert, iInsertLen, "%s", gpio_list);        
        }
        break;          
        case SSI_gpiooh:
        {               
            print_gpio_pins_matching_default(gpio_list, sizeof(gpio_list), GP_OUTPUT_HIGH);
            printed = snprintf(pcInsert, iInsertLen, "%s", gpio_list);        
        }
        break; 
        case SSI_gpiool:
        {              
            print_gpio_pins_matching_default(gpio_list, sizeof(gpio_list), GP_OUTPUT_LOW);
            printed = snprintf(pcInsert, iInsertLen, "%s", gpio_list);       
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