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
#include "wifi.h"


typedef struct
{
    uint32_t country_code;
    char *country_name;
} WIFI_COUNTRY_T;

WIFI_COUNTRY_T wifi_country[] =
{
    {CYW43_COUNTRY_WORLDWIDE, "World Wide"},
    {CYW43_COUNTRY_AUSTRALIA, "Australia"},
    {CYW43_COUNTRY_AUSTRIA, "Austria"},
    {CYW43_COUNTRY_BELGIUM, "Belgium"},
    {CYW43_COUNTRY_BRAZIL, "Brazil"},
    {CYW43_COUNTRY_CANADA, "Canada"},
    {CYW43_COUNTRY_CHILE, "Chile"},
    {CYW43_COUNTRY_CHINA, "China"},
    {CYW43_COUNTRY_COLOMBIA, "Columbia"},
    {CYW43_COUNTRY_CZECH_REPUBLIC, "Czech Republic"},
    {CYW43_COUNTRY_DENMARK, "Denmark"},
    {CYW43_COUNTRY_ESTONIA, "Estonia"},
    {CYW43_COUNTRY_FINLAND, "Finland"},
    {CYW43_COUNTRY_FRANCE, "France"},
    {CYW43_COUNTRY_GERMANY, "Germany"},
    {CYW43_COUNTRY_GREECE, "Greece"},
    {CYW43_COUNTRY_HONG_KONG, "Hong Kong"},
    {CYW43_COUNTRY_HUNGARY, "Hungary"},
    {CYW43_COUNTRY_ICELAND, "Iceland"},
    {CYW43_COUNTRY_INDIA, "India"},
    {CYW43_COUNTRY_ISRAEL, "Israel"},
    {CYW43_COUNTRY_ITALY, "Italy"},
    {CYW43_COUNTRY_JAPAN, "Japan"},
    {CYW43_COUNTRY_KENYA, "Kenya"},
    {CYW43_COUNTRY_LATVIA, "Latvia"},
    {CYW43_COUNTRY_LIECHTENSTEIN, "Liechtenstein"},
    {CYW43_COUNTRY_LITHUANIA, "Lithuania"},
    {CYW43_COUNTRY_LUXEMBOURG, "Luxembourg"},
    {CYW43_COUNTRY_MALAYSIA, "Malaysia"},
    {CYW43_COUNTRY_MALTA, "Malta"},
    {CYW43_COUNTRY_MEXICO, "Mexico"},
    {CYW43_COUNTRY_NETHERLANDS, "Netherlands"},
    {CYW43_COUNTRY_NEW_ZEALAND, "New Zealand"},
    {CYW43_COUNTRY_NIGERIA, "Nigeria"},
    {CYW43_COUNTRY_NORWAY, "Norway"},
    {CYW43_COUNTRY_PERU, "Peru"},
    {CYW43_COUNTRY_PHILIPPINES, "Philippines"},
    {CYW43_COUNTRY_POLAND, "Poland"},
    {CYW43_COUNTRY_PORTUGAL, "Portugal"},
    {CYW43_COUNTRY_SINGAPORE, "Singapore"},
    {CYW43_COUNTRY_SLOVAKIA, "Slovakia"},
    {CYW43_COUNTRY_SLOVENIA, "Slovenia"},
    {CYW43_COUNTRY_SOUTH_AFRICA, "South Africa"},
    {CYW43_COUNTRY_SOUTH_KOREA, "South Korea"},
    {CYW43_COUNTRY_SPAIN, "Spain"},
    {CYW43_COUNTRY_SWEDEN, "Sweden"},
    {CYW43_COUNTRY_SWITZERLAND, "Switzerland"},
    {CYW43_COUNTRY_TAIWAN, "Taiwan"},
    {CYW43_COUNTRY_THAILAND, "Thailand"},
    {CYW43_COUNTRY_TURKEY, "Turkey"},
    {CYW43_COUNTRY_UK, "UK"},
 // {CYW43_COUNTRY_UK, "United Kingdom"},    
    {CYW43_COUNTRY_USA, "USA"},
 // {CYW43_COUNTRY_USA, "United States of America"},    
};



/*!
 * \brief look up wifi country code by country name 
 *
 * \param country_name name of country in English
 * 
 * \return country code used by CYW43 wifi driver
 */
uint32_t get_wifi_country_code(char *country_name)
{
    uint32_t country_code = CYW43_COUNTRY_WORLDWIDE;
    int i;

    for(i=0; i< NUM_ROWS(wifi_country); i++)
    {
        if (strcasecmp(wifi_country[i].country_name, country_name) == 0)
        {
            country_code = wifi_country[i].country_code;
            break;
        }
    }

    return(country_code);
}

/*!
 * \brief print wifi country options as html snippet
 *
 * \param none
 * 
 * \return nothing
 */
void print_wifi_country_html_options(void)
{
    int i;

    for(i=0; i< NUM_ROWS(wifi_country); i++)
    {
        printf("<option value=\"%s\">%s</option>\n", wifi_country[i].country_name, wifi_country[i].country_name);
    }    
}
