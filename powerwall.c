/* Pico HTTPS request example *************************************************
 *                                                                            *
 *  An HTTPS client example for the Raspberry Pi Pico W                       *
 *                                                                            *
 *  A simple yet complete example C application which sends a single request  *
 *  to a web server over HTTPS and reads the resulting response.              *
 *                                                                            *
 ******************************************************************************/


/* Includes *******************************************************************/
#define _GNU_SOURCE
#include <string.h>

// Pico SDK
#include "pico/stdlib.h"            // Standard library
#include "pico/cyw43_arch.h"        // Pico W wireless

// lwIP
#include "lwip/dns.h"               // Hostname resolution
#include "lwip/altcp_tls.h"         // TCP + TLS (+ HTTP == HTTPS)
#include "altcp_tls_mbedtls_structs.h"
#include "lwip/prot/iana.h"         // HTTPS port number

// from ssl_client1.c example program
#include "mbedtls_config.h"  //local copy from https example for pico
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

// Mbed TLS
//#include "mbedtls/ssl.h"            // Server Name Indication TLS extension
//#ifdef MBEDTLS_DEBUG_C
//#include "mbedtls/debug.h"          // Mbed TLS debugging
//#endif //MBEDTLS_DEBUG_C
//#include "mbedtls/check_config.h"

#include <string.h>

// Pico HTTPS request example
#include "shelly.h"
#include "powerwall.h"              // Options, macros, forward declarations

#include "config.h"
#include "pluto.h"


#define GET_REQUEST "GET / HTTP/1.0\r\n\r\n"

extern char shelly_value[255][128];
extern NON_VOL_VARIABLES_T config;

char copy_buffer[2048];
//har request_buffer[512];

//#define USE_RTOS_MBEDTLS_INTERFACE
#ifdef USE_RTOS_MBEDTLS_INTERFACE
int bilbo(void);
#endif

typedef enum
{
    PW_INITIATE,
    PW_CONNECT,
    PW_LOGIN,
    PW_CONFIRM_LOGIN,
    PW_GET_STATUS,
    PW_CONFIRM_STATUS,
    PW_LOGOUT,
    PW_CONFIRM_LOGOUT,
    PW_TEAR_DOWN
} PW_STATE_T;


void tear_down(struct altcp_pcb* pcb);

/* Main ***********************************************************************/

int strip_first_last_quotes(char *token, int length)
{
    int i;

    if (token && (length > 2)) 
    {
        if (token[0] == '"')
        {
            //printf("start quote strip length = %d\n", length);
            for (i=0; i < length-2; i++)
            {
                //printf("checking: %d[%c][%c]\n", i, token[i+1], token[i+2]);
                if (((token[i+1] == '"') && (token[i+2] == 0)) ||
                     (token[i+1] == 0))
                {
                    //printf("terminated quote strip\n");
                    token[i] = 0;
                    break;
                }
                else
                {
                    token[i] = token[i+1];
                    //printf("copied: %c\n", token[i]);
                }
            }
            //printf("ended quote strip\n");
        }
    }

    return(0);
}


void powerwall_test(void)
{
    static PW_STATE_T powerwall_connection_state = PW_INITIATE;
    static int retry = 0;
    const int max_retries = 2;
    struct altcp_pcb* pcb = NULL;
    char *start_of_json = NULL;
    char *start_of_cookie = NULL;
    int i;
    char cookie[1024];
    char grid_status[256];
    char authorization_token[256];
       
    // TEMPORARY!!!!
    config.personality = HVAC_THERMOSTAT;
    STRNCPY(config.powerwall_ip, "powerwall.badnet", sizeof(config.powerwall_ip)); 
    STRNCPY(config.powerwall_password, "JWVTC", sizeof(config.powerwall_password));  

    // check if retry limit reached
    if (retry >= max_retries)
    {
        powerwall_connection_state = PW_TEAR_DOWN;
        retry = 0;
    }

    switch(powerwall_connection_state)
    {
        default:
        case PW_INITIATE:
            // resolve server hostname
            ip_addr_t ipaddr;
            char* char_ipaddr;
            printf("Resolving %s\n", PICOHTTPS_HOSTNAME);
            if(!resolve_hostname(&ipaddr))
            {
                printf("Failed to resolve %s\n", PICOHTTPS_HOSTNAME);
                break;
            } 
            else
            {
                cyw43_arch_lwip_begin();
                char_ipaddr = ipaddr_ntoa(&ipaddr);
                cyw43_arch_lwip_end();
                printf("Resolved %s (%s)\n", PICOHTTPS_HOSTNAME, char_ipaddr);

                powerwall_connection_state = PW_CONNECT;
            } 
            // deliberate fall through

        case PW_CONNECT:
            // connect to powerwall
            pcb = NULL;
            printf("Connecting to https://%s:%d\n", char_ipaddr, LWIP_IANA_PORT_HTTPS);
            if(!connect_to_host(&ipaddr, &pcb))
            {
                printf("Failed to connect to https://%s:%d\n", char_ipaddr, LWIP_IANA_PORT_HTTPS);
                retry++;
                break;
            }
            else
            {
                printf("Connected to https://%s:%d\n", char_ipaddr, LWIP_IANA_PORT_HTTPS);
                retry = 0;
                powerwall_connection_state = PW_LOGIN;
            }

            // deliberate fall through

        case PW_LOGIN:
            // send login request to powerwall
            printf("Sending login request\n");
            if(!powerwall_login(pcb))
            {        
                printf("Failed to send login request\n");

                // tear down connection
                tear_down(pcb);
                pcb = NULL;

                powerwall_connection_state = PW_INITIATE;
                break;
            }
            else
            {
                printf("Sent login request to https://%s:%d\n", char_ipaddr, LWIP_IANA_PORT_HTTPS);
                //retry = 0;
                powerwall_connection_state = PW_CONFIRM_LOGIN;                
            }

            // deliberate fall through

        case PW_CONFIRM_LOGIN:            
            // Await HTTP response
            printf("Awaiting login confirmation\n");
            sleep_ms(5000);
            printf("Awaited response\n");

            http_extract_cookies(copy_buffer, cookie, sizeof(cookie));

            printf("parse json\n");
            start_of_json = strcasestr(copy_buffer, "\r\n{");
            if (start_of_json)
            {
                start_of_json += 2; // point to opening brace

                parse_shelly_json(start_of_json);
            }
            //dump_shelly_cache();
            //dump_shelly_tokens(); 

            if (json_get_value("root.\"token\"", authorization_token, sizeof(authorization_token), false))
            {
                printf("login failed.");

                // tear down connection
                tear_down(pcb);
                pcb = NULL;

                powerwall_connection_state = PW_INITIATE;
                break;
            } 
            else            
            {
                strip_first_last_quotes(authorization_token, sizeof(authorization_token));
                printf("Login successful.  Authorization token is = %s\n", authorization_token);
                retry = 0;
                powerwall_connection_state = PW_GET_STATUS; 
            } 
           
            // deliberate fallthrough

        case PW_GET_STATUS:
            // send status request to powerwall
            printf("Sending status request to powerwall\n");
            if(!powerwall_get_grid_status(pcb, authorization_token, cookie))
            {        
                printf("Failed to send grid status request\n");

                // tear down connection
                tear_down(pcb);
                pcb = NULL;

                powerwall_connection_state = PW_INITIATE;
                break;
            }
            else
            {
                printf("Sent status request in to https://%s:%d\n", char_ipaddr, LWIP_IANA_PORT_HTTPS);
                //retry = 0;
                powerwall_connection_state = PW_CONFIRM_STATUS;                
            }

            // deliberate fallthrough

        case PW_CONFIRM_STATUS:  
            // Await status response
            printf("Awaiting status response\n");
            sleep_ms(5000);
            printf("Awaited response\n");

            printf("parse json\n");
            start_of_json = strcasestr(copy_buffer, "\r\n{");
            if (start_of_json)
            {
                start_of_json += 2; // point to opening brace

                parse_shelly_json(start_of_json);
            }

            if (json_get_value("root.\"grid_status\"", grid_status, sizeof(grid_status), false))
            {
                printf("FAILED TO GET GRID STATUS\n");

                // tear down connection
                tear_down(pcb);
                pcb = NULL;

                powerwall_connection_state = PW_INITIATE;
                break;
            }
            else
            {
                printf("Grid Status from JSON cache is = %s\n", grid_status);
                retry = 0;
                powerwall_connection_state = PW_LOGOUT;
            }     

            // deliberate fallthrough

        case PW_LOGOUT:
            // send logout request to powerwall
            printf("Sending login request\n");
            if(!powerwall_logout(pcb, authorization_token, cookie))
            {        
                printf("Failed to send logout request\n");

                // tear down connection
                tear_down(pcb);
                pcb = NULL;

                powerwall_connection_state = PW_INITIATE;                
                break;
            }
            else
            {
                printf("Sent login request to https://%s:%d\n", char_ipaddr, LWIP_IANA_PORT_HTTPS);
                //retry = 0;
                powerwall_connection_state = PW_CONFIRM_LOGIN;                
            }
            
            // deliberate fallthrough
            
        case PW_CONFIRM_LOGOUT:  
            // Await status response
            printf("Awaiting logout response\n");
            sleep_ms(5000);
            printf("Awaited response\n");

            // Response: HTTP/2 204  date: Thu, 03 Oct 2019 13:48:10 GMT
            printf("parse json\n");
            if (strcasestr(copy_buffer, "date"))
            {
                printf("LOGOUT OK\n");
            }
            else
            {
                printf("LOGOUT FAILED\n");
            }

            // deliberate fallthrough
 
        case PW_TEAR_DOWN:
            tear_down(pcb);
            pcb = NULL;

            retry = 0;  
            powerwall_connection_state = PW_INITIATE;      
            break;
    }

    // TEST TEST TEST
    initialize_shelly_cache();
    sprintf(grid_status, "UNKNOWN");
    sprintf(copy_buffer, "XXXXXXXXXXXXXXXXXXXXXXXXX");

    printf("Exiting\n");
    return;
}

void tear_down(struct altcp_pcb* pcb)
{
    if(pcb)
    {
        
        if (((struct altcp_callback_arg*)(pcb->arg))->config)
        {
            // free connection configuration
            altcp_free_config(((struct altcp_callback_arg*)(pcb->arg))->config);    
        }
        
        if ((struct altcp_callback_arg*)(pcb->arg))
        {
            // free connection callback argument
            altcp_free_arg((struct altcp_callback_arg*)(pcb->arg));                 
        }

        // free connection PCB
        altcp_free_pcb(pcb);                                                    
        
        pcb = NULL;
    }
}


// Resolve hostname
bool resolve_hostname(ip_addr_t* ipaddr){

    // Zero address
    ipaddr->addr = IPADDR_ANY;

    // Attempt resolution
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = dns_gethostbyname(
        PICOHTTPS_HOSTNAME,
        ipaddr,
        callback_gethostbyname,
        ipaddr
    );
    cyw43_arch_lwip_end();
    if(lwip_err == ERR_INPROGRESS){

        // Await resolution
        //
        //  IP address will be made available shortly (by callback) upon DNS
        //  query response.
        //
        while(ipaddr->addr == IPADDR_ANY)
            sleep_ms(PICOHTTPS_RESOLVE_POLL_INTERVAL);
        if(ipaddr->addr != IPADDR_NONE)
            lwip_err = ERR_OK;

    }

    // Return
    return !((bool)lwip_err);

}

// Free TCP + TLS protocol control block
void altcp_free_pcb(struct altcp_pcb* pcb){
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = altcp_close(pcb);         // Frees PCB
    cyw43_arch_lwip_end();
    while(lwip_err != ERR_OK)
        sleep_ms(PICOHTTPS_ALTCP_CONNECT_POLL_INTERVAL);
        cyw43_arch_lwip_begin();
        lwip_err = altcp_close(pcb);                // Frees PCB
        cyw43_arch_lwip_end();
}

// Free TCP + TLS connection configuration
void altcp_free_config(struct altcp_tls_config* config){
    cyw43_arch_lwip_begin();
    altcp_tls_free_config(config);
    cyw43_arch_lwip_end();
}

// Free TCP + TLS connection callback argument
void altcp_free_arg(struct altcp_callback_arg* arg){
    if(arg){
        free(arg);
    }
}

// Establish TCP + TLS connection with server
bool connect_to_host(ip_addr_t* ipaddr, struct altcp_pcb** pcb){

    // Instantiate connection configuration
    u8_t ca_cert[] = PICOHTTPS_CA_ROOT_CERT;
    cyw43_arch_lwip_begin();
    struct altcp_tls_config* config = altcp_tls_create_config_client(
        /*ca_cert,
        LEN(ca_cert)*/ NULL, 0
    );
    cyw43_arch_lwip_end();
    if(!config) return false;

    // Instantiate connection PCB
    
    //  Can also do this more generically using;
    
    //    altcp_allocator_t allocator = {
    //      altcp_tls_alloc,       // Allocator function
    //      config                 // Allocator function argument (state)
    //    };
    //    altcp_new(&allocator);
    
    //  No benefit in doing this though; altcp_tls_alloc calls altcp_tls_new
    //  under the hood anyway.
    
    cyw43_arch_lwip_begin();
    *pcb = altcp_tls_new(config, IPADDR_TYPE_V4);
    cyw43_arch_lwip_end();
    if(!(*pcb)){
        altcp_free_config(config);
        return false;
    }

    // Configure hostname for Server Name Indication extension
    //
    //  Many servers nowadays require clients to support the [Server Name
    //  Indication[wiki-sni] (SNI) TLS extension. In this extension, the
    //  hostname is included in the in the ClientHello section of the TLS
    //  handshake.
    //
    //  Mbed TLS provides client-side support for SNI extension
    //  (`MBEDTLS_SSL_SERVER_NAME_INDICATION` option), but requires the
    //  hostname in order to do so. Unfortunately, the Mbed TLS port supplied
    //  with lwIP (ALTCP TLS) does not currently provide an interface to pass
    //  the hostname to Mbed TLS. This is a [known issue in lwIP][gh-lwip-pr].
    //
    //  As a workaround, the hostname can instead be set using the underlying
    //  Mbed TLS interface (viz. `mbedtls_ssl_set_hostname` function). This is
    //  somewhat inelegant as it tightly couples our application code to the
    //  underlying TLS library (viz. Mbed TLS). Given that the Pico SDK already
    //  tightly couples us to lwIP though, and that any fix is unlikely to be
    //  backported to the lwIP version in the Pico SDK, this doesn't feel like
    //  too much of a crime…
    //
    //  [wiki-sni]: https://en.wikipedia.org/wiki/Server_Name_Indication
    //  [gh-lwip-pr]: https://github.com/lwip-tcpip/lwip/pull/47/commits/c53c9d02036be24a461d2998053a52991e65b78e
    //
    cyw43_arch_lwip_begin();
    mbedtls_err_t mbedtls_err = mbedtls_ssl_set_hostname(
        &(
            (
                (altcp_mbedtls_state_t*)((*pcb)->state)
            )->ssl_context
        ),
        PICOHTTPS_HOSTNAME
    );
    cyw43_arch_lwip_end();
    if(mbedtls_err){
        altcp_free_pcb(*pcb);
        altcp_free_config(config);
        return false;
    }

    // Configure common argument for connection callbacks
    //
    //  N.b. callback argument must be in scope in callbacks. As callbacks may
    //  fire after current function returns, cannot declare argument locally,
    //  but rather should allocate on the heap. Must then ensure allocated
    //  memory is subsequently freed.
    //
    struct altcp_callback_arg* arg = malloc(sizeof(*arg));
    if(!arg){
        altcp_free_pcb(*pcb);
        altcp_free_config(config);
        return false;
    }
    arg->config = config;
    arg->connected = false;
    cyw43_arch_lwip_begin();
    altcp_arg(*pcb, (void*)arg);
    cyw43_arch_lwip_end();

    // Configure connection fatal error callback
    cyw43_arch_lwip_begin();
    altcp_err(*pcb, callback_altcp_err);
    cyw43_arch_lwip_end();

    // Configure idle connection callback (and interval)
    cyw43_arch_lwip_begin();
    altcp_poll(
        *pcb,
        callback_altcp_poll,
        PICOHTTPS_ALTCP_IDLE_POLL_INTERVAL
    );

    //TODO newman found this on internet for non-tls connection
    //pcb->keep_intvl = 1000; // send "keep-alive" every 1000ms

    cyw43_arch_lwip_end();

    // Configure data acknowledge callback
    cyw43_arch_lwip_begin();
    altcp_sent(*pcb, callback_altcp_sent);
    cyw43_arch_lwip_end();

    // Configure data reception callback
    cyw43_arch_lwip_begin();
    altcp_recv(*pcb, callback_altcp_recv);
    cyw43_arch_lwip_end();

    // Send connection request (SYN)
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = altcp_connect(
        *pcb,
        ipaddr,
        LWIP_IANA_PORT_HTTPS,
        callback_altcp_connect
    );
    cyw43_arch_lwip_end();

    // Connection request sent
    if(lwip_err == ERR_OK){

        // Await connection
        //
        //  Sucessful connection will be confirmed shortly in
        //  callback_altcp_connect.
        //
        while(!(arg->connected))
            sleep_ms(PICOHTTPS_ALTCP_CONNECT_POLL_INTERVAL);

    } else {

        // Free allocated resources
        altcp_free_pcb(*pcb);
        altcp_free_config(config);
        altcp_free_arg(arg);

    }

    //Return
    return !((bool)lwip_err);

}

// DNS response callback
void callback_gethostbyname(
    const char* name,
    const ip_addr_t* resolved,
    void* ipaddr
){
    if(resolved) *((ip_addr_t*)ipaddr) = *resolved;         // Successful resolution
    else ((ip_addr_t*)ipaddr)->addr = IPADDR_NONE;          // Failed resolution
}

// TCP + TLS connection error callback
void callback_altcp_err(void* arg, lwip_err_t err){

    // Print error code
    printf("Connection error [lwip_err_t err == %d]\n", err);

    // Free ALTCP TLS config
    if( ((struct altcp_callback_arg*)arg)->config )
        altcp_free_config( ((struct altcp_callback_arg*)arg)->config );

    // Free ALTCP callback argument
    altcp_free_arg((struct altcp_callback_arg*)arg);

}

// TCP + TLS connection idle callback
lwip_err_t callback_altcp_poll(void* arg, struct altcp_pcb* pcb){
    // Callback not currently used
    return ERR_OK;
}

// TCP + TLS data acknowledgement callback
lwip_err_t callback_altcp_sent(void* arg, struct altcp_pcb* pcb, u16_t len){
    ((struct altcp_callback_arg*)arg)->acknowledged = len;
    return ERR_OK;
}

// TCP + TLS data reception callback
lwip_err_t callback_altcp_recv(
    void* arg,
    struct altcp_pcb* pcb,
    struct pbuf* buf,
    lwip_err_t err
){

    // Store packet buffer at head of chain
    //
    //  Required to free entire packet buffer chain after processing.
    //
    struct pbuf* head = buf;

    switch(err){

        // No error receiving
        case ERR_OK:

            // Handle packet buffer chain
            //
            //  * buf->tot_len == buf->len — Last buf in chain
            //    * && buf->next == NULL — last buf in chain, no packets in queue
            //    * && buf->next != NULL — last buf in chain, more packets in queue
            //

            if(buf){

                // Print packet buffer
                u16_t i;
                int copy_index = 0;
                while(buf->len != buf->tot_len)
                {
                    for(i = 0; i < buf->len; i++)
                    {
                        putchar(((char*)buf->payload)[i]);
                        if (copy_index < sizeof(copy_buffer)) copy_buffer[copy_index++] = ((char*)buf->payload)[i];
                    } 
                    buf = buf->next;
                }
                for(i = 0; i < buf->len; i++)
                {
                    putchar(((char*)buf->payload)[i]);
                    if (copy_index < sizeof(copy_buffer)) copy_buffer[copy_index++] = ((char*)buf->payload)[i];
                } 
                copy_buffer[copy_index] = 0; // ensure zero termination
                assert(buf->next == NULL);

                // Advertise data reception
                altcp_recved(pcb, head->tot_len);

            }

            // …fall-through…

        case ERR_ABRT:

            // Free buf
            pbuf_free(head);        // Free entire pbuf chain

            // Reset error
            err = ERR_OK;           // Only return ERR_ABRT when calling tcp_abort()

    }

    printf("\nRCV CALLBACK COMPLETED err = %d\n", err);
    // Return error
    return err;

}

// TCP + TLS connection establishment callback
lwip_err_t callback_altcp_connect(
    void* arg,
    struct altcp_pcb* pcb,
    lwip_err_t err
){
    ((struct altcp_callback_arg*)arg)->connected = true;
    return ERR_OK;
}


int powerwall_init(void)
{
    initialize_shelly_cache();
    // test_http(1);
    // dump_shelly_cache(); 

    return(0); 
}


// Send HTTP request
bool http_request(struct altcp_pcb* pcb, HTTP_REQUEST_TYPE_T type, char *url, char *host, char *content, char *auth_token, char *cookies)
{
    bool err = false;
    char request[2048];
    int length = 0;
    char length_string[8];
    lwip_err_t lwip_err = -1;
   

    // type
    switch(type)
    {
        case HTTP_GET:
            sprintf(request, "GET ");
            break;
        case HTTP_POST:
            sprintf(request, "POST ");
            break;
        default:
            request[0] = 0;
            err = true;
    }

    // url
    STRAPPEND(request, url);
    STRAPPEND(request, " HTTP/1.1\r\n");

    // host
    STRAPPEND(request, "Host: ");
    STRAPPEND(request, host);
    STRAPPEND(request, "\r\n");

    // accept
    STRAPPEND(request, "Accept: */*\r\n");

    // cookies
    if (cookies)
    {
        STRAPPEND(request, "Cookie: ");
        STRAPPEND(request, cookies);
        STRAPPEND(request, "\r\n");        
    }

    // authorization token
    if (auth_token)
    {
        STRAPPEND(request, "Authorization: Bearer ");
        STRAPPEND(request, auth_token);
        STRAPPEND(request, "\r\n");
    }

    // content
    if (content)
    {
        sprintf(length_string, "%d", strlen(content));

        STRAPPEND(request, "Content-Type: application/json\r\n");
        STRAPPEND(request, "Content-Length: ");
        STRAPPEND(request, length_string);
        STRAPPEND(request, "\r\n\r\n");
        STRAPPEND(request, content);
    }
    STRAPPEND(request, "\r\n");

    // request length 
    length = strlen(request) + 1; // TODO figure out WTF the original code is doing by transmitting one less

    if(!err)
    {
        printf("SEND [%d bytes]\n%s\n", length, request); 
        
        // Check send buffer and queue length
        //
        //  Docs state that altcp_write() returns ERR_MEM on send buffer too small
        //  _or_ send queue too long. Could either check both before calling
        //  altcp_write, or just handle returned ERR_MEM — which is preferable?
        //
        if(
        altcp_sndbuf(pcb) < (length - 1)
        || altcp_sndqueuelen(pcb) > TCP_SND_QUEUELEN
        ) return -1;

        // Write to send buffer
        cyw43_arch_lwip_begin();
        lwip_err = altcp_write(pcb, request, length -1, 0);
        cyw43_arch_lwip_end();

        // Written to send buffer
        if(lwip_err == ERR_OK){

            // Output send buffer
            ((struct altcp_callback_arg*)(pcb->arg))->acknowledged = 0;
            cyw43_arch_lwip_begin();
            lwip_err = altcp_output(pcb);
            cyw43_arch_lwip_end();

            // Send buffer output
            if(lwip_err == ERR_OK){

                // Await acknowledgement
                while(
                    !((struct altcp_callback_arg*)(pcb->arg))->acknowledged
                ) sleep_ms(PICOHTTPS_HTTP_RESPONSE_POLL_INTERVAL);
                if(
                    ((struct altcp_callback_arg*)(pcb->arg))->acknowledged
                    != (length - 1)
                ) lwip_err = -1;

            }

        }
    }
    // Return
    return !((bool)lwip_err);

}


int powerwall_login(struct altcp_pcb* pcb)
{
    int err = 0;
    char content[256];

    sprintf(content, "{\"username\":\"customer\",\"password\":\"%s\"}", config.powerwall_password);

    err = http_request(pcb, HTTP_POST, "/api/login/Basic", config.powerwall_hostname, content, NULL, NULL);

    return(err);
}



int powerwall_get_grid_status(struct altcp_pcb* pcb, char *auth_token, char *cookies)
{
    int err = 0;   

    err = http_request(pcb, HTTP_GET, "/api/system_status/grid_status", config.powerwall_hostname, NULL, auth_token, cookies);
    
    return(err);
}

int powerwall_logout(struct altcp_pcb* pcb, char *auth_token, char *cookies)
{
    int err = 0;   

    err = http_request(pcb, HTTP_GET, "/api/logout", config.powerwall_hostname, NULL, auth_token, cookies);
    
    return(err);
}

int http_extract_cookies(const char *http_packet, char *cookies, int length)
{
    int err = 0;
    char *source = NULL;
    int i = 0;
    int j = 0;

    // check paramters are sane
    if (!http_packet || !cookies || (length < 1))
    {
        err = 1;
    }
    else
    {
        // source pointer to walk through packet
        source = (char *)http_packet;

        // concatenate cookies found in http header
        do
        {
            source = strcasestr(source, "Set-Cookie: ");
            if (source)
            {
                printf("GOT COOKIE: ");

                // inject a space between cookies
                if (j > 0)
                {
                    cookies[j++] = ' ';
                    cookies[j] = 0;
                }

                // copy cookie
                source += strlen("Set-Cookie: ");
                for (i=0; source[i] && (j < (length - 1)); i++, j++)
                {
                    printf("%c", source[i]);
                    cookies[j] = source[i];

                    if (source[i] == ';')
                    {
                        // terminate string after semi-colon
                        cookies[++j] = 0;
                        break;
                    } 
                }

                printf("\n"); 
                
                // move source pointer past copied cookie
                source += i;
            }        
        } while (source && (j < (length - 1)));
    
        // remove final semi-colon
        if (j > 0)
        {
            cookies[j-1] = 0;
        } 
    }

    return(err);
}


#ifdef USE_RTOS_MBEDTLS_INTERFACE
// apparently the standard mbedtls code does not support lwip sockets, so would have to create that to use the interface below

int bilbo(void)
{
    int ret = 1, len;
    int exit_code = MBEDTLS_EXIT_FAILURE;
    mbedtls_net_context server_fd;
    uint32_t flags;
    unsigned char buf[1024];
    const char *pers = "ssl_client1";
    // ip_addr_t ipaddr;
    // char* char_ipaddr;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;

    /*
     * 0. Initialize the RNG and the session data
     */
    mbedtls_net_init(&server_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    mbedtls_printf("\n  . Seeding the random number generator...");
    fflush(stdout);


    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                     (const unsigned char *) pers,
                                     strlen(pers))) != 0) {
        mbedtls_printf(" failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret);
        return(1);
    }

    mbedtls_printf(" ok\n");    

    //HERE LOADING THE CERTIFICATE WAS LEFT OUT -- see ssl_client1.c for original code

    /*
     * 1. Start the connection
     */
    mbedtls_printf("  . Connecting to tcp/%s/%s...", PICOHTTPS_HOSTNAME, "443");
    fflush(stdout);

    if ((ret = mbedtls_net_connect(&server_fd, PICOHTTPS_HOSTNAME,
        "443", MBEDTLS_NET_PROTO_TCP)) != 0) {
        mbedtls_printf(" failed\n  ! mbedtls_net_connect returned %d\n\n", ret);
        return(2);
    }

    mbedtls_printf(" ok\n");

     /*
     * 2. Setup stuff
     */
    mbedtls_printf("  . Setting up the SSL/TLS structure...");
    fflush(stdout);

    if ((ret = mbedtls_ssl_config_defaults(&conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        mbedtls_printf(" failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
        return(3);
    }

    mbedtls_printf(" ok\n");

    /* OPTIONAL is not optimal for security,
     * but makes interop easier in this simplified example */
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    //mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    //mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
        mbedtls_printf(" failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
        return(4);
    }

    if ((ret = mbedtls_ssl_set_hostname(&ssl, PICOHTTPS_HOSTNAME)) != 0) {
        mbedtls_printf(" failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret);
        return(5);
    }

    mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    /*
     * 4. Handshake
     */
    mbedtls_printf("  . Performing the SSL/TLS handshake...");
    fflush(stdout);

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            mbedtls_printf(" failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n",
                           (unsigned int) -ret);
        return(6);
        }
    }

    mbedtls_printf(" ok\n");

    //HERE CHECKING THE CERTIFICATE WAS LEFT OUT -- see ssl_client1.c for original code

    /*
     * 3. Write the GET request
     */
    mbedtls_printf("  > Write to server:");
    fflush(stdout);

    len = sprintf((char *) buf, GET_REQUEST);

    while ((ret = mbedtls_ssl_write(&ssl, buf, len)) <= 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            mbedtls_printf(" failed\n  ! mbedtls_ssl_write returned %d\n\n", ret);
            return(7);
        }
    }

    len = ret;
    mbedtls_printf(" %d bytes written\n\n%s", len, (char *) buf);

    /*
     * 7. Read the HTTP response
     */
    mbedtls_printf("  < Read from server:");
    fflush(stdout);

    do {
        len = sizeof(buf) - 1;
        memset(buf, 0, sizeof(buf));
        ret = mbedtls_ssl_read(&ssl, buf, len);

        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }

        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            break;
        }

        if (ret < 0) {
            mbedtls_printf("failed\n  ! mbedtls_ssl_read returned %d\n\n", ret);
            break;
        }

        if (ret == 0) {
            mbedtls_printf("\n\nEOF\n\n");
            break;
        }

        len = ret;
        mbedtls_printf(" %d bytes read\n\n%s", len, (char *) buf);
    } while (1);

    mbedtls_ssl_close_notify(&ssl);


    mbedtls_net_free(&server_fd);
    //mbedtls_x509_crt_free(&cacert);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return(0);
}
#endif