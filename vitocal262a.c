/* Program: Modbus RTU Slave emulator for Viessmann ALE3D5F Energy Meter
* Version: 0.1
* Author: chbacher
* Date: 2025-12-06
* Copyright (C) 2025 chbacher
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
* along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "/usr/include/modbus/modbus.h"

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT 1502

// Helper function to pack two ASCII chars into one 16-bit register.
#define PACK_CHARS(c1, c2) ((c1 << 8) | c2)

// Global debug flag. Set to 0 to disable debug output.
#define DEBUG_MODE 0

// --- Global Shared Data ---
// These variables are accessed by multiple threads and must be handled carefully.
modbus_mapping_t *mapping;
int n_shutdown;
pthread_mutex_t mapping_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex to protect 'mapping'

/**
 * @brief Modbus RTU Slave Thread.
 *        Listens for and replies to Modbus requests. Implements a reconnection
 *        strategy for connection failures.
 */
void *ModbusThread(void *lpParam)
{
    uint8_t req[MODBUS_RTU_MAX_ADU_LENGTH]; // Request buffer
    int len;                                // Length of the request/response
    int nStop;

    // Variables for debug request interception
    int slave_id;
    int function_code;
    int start_address;
    int num_registers;

    mapping = modbus_mapping_new(0, 0, 52, 0);
    if (!mapping) {
        printf("Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        exit(1);
    }

    // Initialize registers (52 Holding Registers, address 0 to 51)
    mapping->tab_registers[0] = 10; // Firmware
    mapping->tab_registers[1] = 52; // Modbus supported registers

    // START OF ASN-Funktion: "ALE3D5FD10C2A00"
    // Register 6 (0-based) is Modbus Address 7
    mapping->tab_registers[6] = PACK_CHARS('A', 'L'); // "AL" (0x414C)
    mapping->tab_registers[7] = PACK_CHARS('E', '3'); // "E3" (0x4533)
    //mapping->tab_registers[8]  = PACK_CHARS('D', '5'); // "D5" (0x4435)
    //mapping->tab_registers[9]  = PACK_CHARS('F', 'D'); // "FD" (0x4644)
    //mapping->tab_registers[10] = PACK_CHARS('1', '0'); // "10" (0x3130)
    //mapping->tab_registers[11] = PACK_CHARS('C', '2'); // "C2" (0x4332)
    //mapping->tab_registers[12] = PACK_CHARS('A', '0'); // "A0" (0x4130)
    //mapping->tab_registers[13] = PACK_CHARS('0', '0'); // "00" (0x3030)
    // END OF ASN-Funktion

    // Register 14 is outside the current 8-register poll, but included for completeness:
    mapping->tab_registers[14] = 11; // Typ / ASN-Funktion (Initialized to 0 or another placeholder)
    // End of Typ / ASN-Funktion block

    mapping->tab_registers[21] = 0;   // Comm ok
    mapping->tab_registers[24] = 0;   // Metering ok
    mapping->tab_registers[25] = 0;   // Tarif
    mapping->tab_registers[27] = 0;   // Counter 1 Total high
    mapping->tab_registers[28] = 0;   // Counter 1 Total low
    mapping->tab_registers[29] = 0;   // Counter 1 Partial high
    mapping->tab_registers[30] = 0;   // Counter 1 Partial low
    mapping->tab_registers[31] = 0;   // Counter 2 Total high
    mapping->tab_registers[32] = 0;   // Counter 2 Total low
    mapping->tab_registers[33] = 0;   // Counter 2 Partial high
    mapping->tab_registers[34] = 0;   // Counter 2 Partial low
    mapping->tab_registers[35] = 0; // Wirkspannung Phase 1
    mapping->tab_registers[36] = 0; // IRMS Phase 1 Wirkstrom Phase 1
    mapping->tab_registers[37] = 0;   // Power Phase 1 (in 10W)
    mapping->tab_registers[40] = 0; // Wirkspannung Phase 2
    mapping->tab_registers[41] = 0; // IRMS Phase 1 Wirkstrom Phase 2
    mapping->tab_registers[42] = 0;   // Power Phase 2 (in 10W)
    mapping->tab_registers[45] = 0; // Wirkspannung Phase 2
    mapping->tab_registers[46] = 0; // IRMS Phase 1 Wirkstrom Phase 3
    mapping->tab_registers[47] = 0;   // Power Phase 3 (in 10W)
    mapping->tab_registers[50] = 0;   // Power Phases TOTAL (in 10W)
  //  mapping->tab_registers[51] = 0;   // 

    // Modbus RTU setup
    modbus_t *ctx = modbus_new_rtu("/dev/ttyUSB0", 38400, 'N', 8, 1);
    if (!ctx) {
        printf("Failed to create the context: %s\n", modbus_strerror(errno));
        exit(1);
    }

    modbus_set_slave(ctx, 1); // Slave ID 1

    if (modbus_connect(ctx) == -1) {
        printf("Unable to connect: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        exit(1);
    }

    nStop = 0;
    while ((nStop == 0) && (n_shutdown == 0)) {
        len = modbus_receive(ctx, req);

        if (len == -1) {
            printf("Modbus communication error: %s. Attempting to reconnect...\n", modbus_strerror(errno));
            
            modbus_close(ctx); // Close the broken connection
            
            int retries = 5;
            int success = 0;
            for (int i = 0; i < retries; i++) {
                printf("Reconnect attempt %d of %d...\n", i + 1, retries);
                
                // Wait for 10 seconds before trying again
                sleep(10); 
                
                if (modbus_connect(ctx) == 0) {
                    printf("Reconnection successful.\n");
                    success = 1;
                    break; // Exit the retry loop
                }
            }
            
            if (!success) {
                printf("Failed to reconnect after %d attempts. Shutting down.\n", retries);
                n_shutdown = 1; // Signal all other threads to shut down
                nStop = 1;      // Set flag to exit this thread's main while-loop
            }
        } else if (len > 0) {

#if DEBUG_MODE
            // Intercept and parse Modbus request for debugging.
            slave_id = req[0];
            function_code = req[1];

            // Only analyze 'Read Holding Registers' (0x03) and 'Read Input Registers' (0x04)
            if (function_code == MODBUS_FC_READ_HOLDING_REGISTERS ||
                function_code == MODBUS_FC_READ_INPUT_REGISTERS)
            {
                // The structure for these function codes is:
                // Byte 0: Slave ID
                // Byte 1: Function Code  
                // Byte 2-3: Starting Address (High then Low byte)
                // Byte 4-5: Quantity of Registers (High then Low byte)

                // Combine the two bytes into a 16-bit integer (Big-Endian)
                start_address = (req[2] << 8) | req[3];
                num_registers = (req[4] << 8) | req[5];

                printf("\n--- Modbus Poll Intercepted ---");
                printf("\nSlave ID: %d", slave_id);
                printf("\nFunction Code: 0x%02X (%s)",
                       function_code,
                       (function_code == MODBUS_FC_READ_HOLDING_REGISTERS) ? "Read Holding Registers" : "Read Input Registers");
                // The library uses 0-based addressing internally
                printf("\nStart Address (0-based): %d", start_address);
                printf("\nNumber of Registers: %d", num_registers);
                printf("\n-------------------------------");
            }
#endif

            // Reply to the Modbus request, protecting access to 'mapping'
            pthread_mutex_lock(&mapping_mutex);
            if (mapping != NULL) {
                len = modbus_reply(ctx, req, len, mapping);
            }
            pthread_mutex_unlock(&mapping_mutex);
        }
    }

    printf("\nExit the Modbus loop: %s\n", modbus_strerror(errno));

    // Safely free the mapping and set the global pointer to NULL to prevent use-after-free
    pthread_mutex_lock(&mapping_mutex);
    if (mapping != NULL) {
        modbus_mapping_free(mapping);
        mapping = NULL;
    }
    pthread_mutex_unlock(&mapping_mutex);

    modbus_close(ctx);
    modbus_free(ctx);

    return NULL; // Changed from 0 to NULL for consistency with pthread_create
}

/**
 * @brief Simple TCP Socket Server Thread.
 *        Listens for data on DEFAULT_PORT and uses the received data
 *        to update the Modbus registers in a thread-safe manner.
 */
void *SocketThread(void *lpParam)
{
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[DEFAULT_BUFLEN];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    int16_t iPower;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("ERROR opening socket");
        n_shutdown = 1;
        return NULL;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = DEFAULT_PORT;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    // Allow the socket to be bound again immediately after closing.
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("%d - ", errno);
        printf("ERROR on binding\n");
        close(sockfd);
        n_shutdown = 1;
        return NULL;
    }

    while (n_shutdown == 0) {
        listen(sockfd,5);
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            if (n_shutdown == 0) {
                // Only print error if not shutting down.
                printf("ERROR on accept\n");
            }
            // Do not break the loop on a recoverable error.
            continue;
        }

        bzero(buffer,DEFAULT_BUFLEN);
        n = read(newsockfd,buffer,DEFAULT_BUFLEN-1);
        if (n < 0) {
            printf("ERROR reading from socket\n");
            close(newsockfd);
            continue;
        }

        // Process received data
        if(n > 0) {
            buffer[n] = '\0'; // Null-terminate the string.
            // Assuming the received string is an integer value for power.
            iPower = (int16_t)atoi(buffer);
            printf("\nPower received: %d W", iPower);

            // Update Modbus registers. A mutex is used to ensure thread-safe access 
            // to the global 'mapping' variable.
            pthread_mutex_lock(&mapping_mutex);
            if (mapping != NULL) {
                // Scaling in 10W (as per original code comment)
                int16_t power_in_10w = (int16_t)(iPower / 10);

                // Use received value to update registers.
                mapping->tab_registers[37] = (int16_t) -1 * power_in_10w; // Phase 1 (approx 1/3 total)
//                mapping->tab_registers[42] = (int16_t) -1 * power_in_10w / 3; // Phase 2 (approx 1/3 total)
//                mapping->tab_registers[47] = (int16_t) -1 * power_in_10w / 3; // Phase 3 (approx 1/3 total)
                mapping->tab_registers[50] = (int16_t) -1 * power_in_10w;     // TOTAL Phases (in 10W)

//                mapping->tab_registers[51] = (int16_t) -1 * power_in_10w;     // TEST TOTAL Phases (in 10W)

                // Print the register update (for debug)
                printf(" -> Total Register [50] set to: %d (Value: %d W)\n", (int16_t) mapping->tab_registers[50], iPower);
            }
            pthread_mutex_unlock(&mapping_mutex);
        }

        close(newsockfd);
    }

    close(sockfd);
    // Setting n_shutdown = 1 here is redundant if the loop was the only thing
    // holding the thread open, but ensures other threads know to exit.
    n_shutdown = 1;

    return NULL;
}

/**
 * @brief Main function. Initializes and starts the worker threads,
 *        then waits for them to complete.
 */
int main()
{
    pthread_t threadSer;
    pthread_t threadSock;

    n_shutdown = 0;

    // Start the socket listener thread
    printf("Starting SocketThread...\n");
    if (pthread_create(&threadSock, NULL, SocketThread, NULL) != 0) {
        perror("Error creating SocketThread");
        return 1;
    }

    // Start the Modbus slave thread
    printf("Starting ModbusThread...\n");
    if (pthread_create(&threadSer, NULL, ModbusThread, NULL) != 0) {
        perror("Error creating ModbusThread");
        // Attempt to signal socket thread to shut down before exiting.
        n_shutdown = 1;
        pthread_join(threadSock, NULL);
        return 1;
    }

    // The main thread now simply waits for both worker threads to complete.
    // The threads will exit when n_shutdown becomes 1, which is set on
    // a fatal error or when a thread exits normally.
    printf("Main thread waiting for threads to finish...\n");
    pthread_join(threadSer, NULL);
    pthread_join(threadSock, NULL);

    printf("All threads have shut down. Exiting program.\n");

    return 0;
}

