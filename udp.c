/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/stdlib.h"
#include "pico/rand.h"

#include "lwip/sockets.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"

#include "udp.h"
#include "weather.h"

//#define DEBUG_UDP_MESSAGES

extern WEB_VARIABLES_T web;

/*!
 * \brief create a socket for UDP communication
 *
 * \param receive_port port that we listen on
 * 
 * \return socket on success or -1 on error
 */
SOCKET upd_establish_socket (int receive_port)
{
    SOCKADDR_IN sInAddr;
    int nResult;
    SOCKET new_socket = -1;

    new_socket = socket (PF_INET, SOCK_DGRAM, 0);    //PF

    if (new_socket >= 0)
    {
        if (new_socket > web.socket_max) web.socket_max = new_socket;

        // bind socket to UDP port
        memset (&sInAddr, 0, sizeof (sInAddr));
        sInAddr.sin_family = PF_INET;
        sInAddr.sin_addr.s_addr = INADDR_ANY;
        sInAddr.sin_port = htons (6969);

        nResult = bind(new_socket, (struct sockaddr *) &sInAddr, sizeof (sInAddr));

        if (nResult < 0)
        {
            printf("Could not bind socket to UDP port %d, result = %d.  Closing socket.\n", receive_port, nResult);
            close(new_socket); 
            new_socket = -1;
            web.bind_failures++;
        }
    }
    else
    {
        printf ("Could not create UDP socket\n");
    }

    return (new_socket);
}

/*!
 * \brief transmit a UDP packet
 *
 * \param socket                socket to use 
 * \param buffer                pointer to transmit buffer
 * \param buffer_length         length of transmit buffer 
 * \param destination_address   where to send the buffer 
 *
 * \return number of bytes sent or negative value on error
 */
int udp_transmit (SOCKET socket, char *buffer, int buffer_length, SOCKADDR_IN destination_address)
{
    int sent_bytes;

#ifdef DEBUG_UDP_MESSAGES
    int x;
    char *address = NULL;

    address = inet_ntoa(destination_address.sin_addr);

    printf("[%s] TX MSG = ", address);
    for(x=0; x<buffer_length; x++) printf("%0x ", buffer[x]);
    printf("\n");
#endif

    sent_bytes = sendto(socket, buffer, buffer_length, 0, (SOCKADDR *) &destination_address, SOCKADDR_LEN);

    if (sent_bytes < 0)
    {
        perror("sendto failed: ");
        web.pluto_transmit_failures++;
    }

    return (sent_bytes);
}

/*!
 * \brief receive a UDP packet
 *
 * \param socket                socket to use 
 * \param buffer                pointer to receive buffer
 * \param buffer_length         length of receive buffer 
 * \param source_address        where the buffer came from 
 * \param usec_timeout          max time to wait for a packet in microseconds  
 *
 * \return number of bytes received or negative value on error
 */
int udp_receive (SOCKET socket, char *buffer, size_t buffer_length, SOCKADDR_IN *source_address, int usec_timeout)
{
     int received_bytes;
     socklen_t source_address_length;
     fd_set rfds;
     struct timeval tv;

     FD_ZERO(&rfds);
     FD_SET(socket, &rfds);

     // we must set this timeout prior to each select() call as some moronic implementations modify tv
     tv.tv_sec = 0;
     tv.tv_usec = usec_timeout;

     received_bytes = select(socket+1, &rfds, NULL, NULL, &tv);


     if (received_bytes > 0)
     {
         // select got something so read socket
         source_address_length = sizeof(SOCKADDR);  // max length, recvfrom will then modify to actual length
         received_bytes = recvfrom (socket, buffer, buffer_length, 0, (struct sockaddr *)source_address, &source_address_length);
     }

    if (received_bytes > 0)
    {
#ifdef DEBUG_UDP_MESSAGES
        int x;
        char *address = NULL;

        address = inet_ntoa(source_address->sin_addr);

        printf("[%s] RX MSG = ", address);
        for(x=0; x<received_bytes; x++) printf("%0x ", buffer[x]);
        printf("\n");
#endif
    }


    return (received_bytes);
}
