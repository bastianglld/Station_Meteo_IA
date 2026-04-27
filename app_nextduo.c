/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_netxduo.c
  * @author  MCD Application Team
  * @brief   NetXDuo applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_netxduo.h"
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
#include "nxd_dhcp_client.h"
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PING_ADDRESS      IP_ADDRESS(8, 8, 8, 8)
#define PING_DATA_SIZE    4
#define MAX_PING_COUNT    4

/* Définition du serveur ThingSpeak (api.thingspeak.com = 184.106.153.149) */
#define THINGSPEAK_SERVER_ADDRESS IP_ADDRESS(184, 106, 153, 149)
#define THINGSPEAK_SERVER_PORT    80
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TX_THREAD      NxAppThread;
NX_PACKET_POOL NxAppPool;
NX_IP          NetXDuoEthIpInstance;
TX_SEMAPHORE   DHCPSemaphore;
NX_DHCP        DHCPClient;
/* USER CODE BEGIN PV */
TX_THREAD AppTCPThread;
TX_THREAD AppLinkThread;
TX_THREAD AppPingThread;

NX_TCP_SOCKET TCPSocket;

ULONG          IpAddress;
ULONG          NetMask;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID nx_app_thread_entry (ULONG thread_input);
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr);
/* USER CODE BEGIN PFP */
static VOID App_TCP_Thread_Entry(ULONG thread_input);
static VOID App_Link_Thread_Entry(ULONG thread_input);
static VOID App_Ping_Thread_Entry(ULONG thread_input);
/* USER CODE END PFP */

/**
  * @brief  Application NetXDuo Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
  UINT ret = NX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
  CHAR *pointer;

  /* USER CODE BEGIN MX_NetXDuo_MEM_POOL */

  /* USER CODE END MX_NetXDuo_MEM_POOL */
  /* USER CODE BEGIN 0 */
  printf("\r\n========================================\r\n");
  printf("<début d'application IoT ThingSpeak>\r\n");
  printf("========================================\r\n");
  /* USER CODE END 0 */

  /* Initialize the NetXDuo system. */
  nx_system_initialize();

  /* Allocate the memory for packet_pool.  */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_PACKET_POOL_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the Packet pool to be used for packet allocation,
   * If extra NX_PACKET are to be used the NX_APP_PACKET_POOL_SIZE should be increased
   */
  ret = nx_packet_pool_create(&NxAppPool, "NetXDuo App Pool", DEFAULT_PAYLOAD_SIZE, pointer, NX_APP_PACKET_POOL_SIZE);

  if (ret != NX_SUCCESS)
  {
    return NX_POOL_ERROR;
  }

  /* Allocate the memory for Ip_Instance */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, Nx_IP_INSTANCE_THREAD_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the main NX_IP instance */
  ret = nx_ip_create(&NetXDuoEthIpInstance, "NetX Ip instance", NX_APP_DEFAULT_IP_ADDRESS, NX_APP_DEFAULT_NET_MASK, &NxAppPool, nx_stm32_eth_driver,
                     pointer, Nx_IP_INSTANCE_THREAD_SIZE, NX_APP_INSTANCE_PRIORITY);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Allocate the memory for ARP */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, DEFAULT_ARP_CACHE_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Enable the ARP protocol and provide the ARP cache size for the IP instance */

  /* USER CODE BEGIN ARP_Protocol_Initialization */

  /* USER CODE END ARP_Protocol_Initialization */

  ret = nx_arp_enable(&NetXDuoEthIpInstance, (VOID *)pointer, DEFAULT_ARP_CACHE_SIZE);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable the ICMP */

  /* USER CODE BEGIN ICMP_Protocol_Initialization */

  /* USER CODE END ICMP_Protocol_Initialization */

  ret = nx_icmp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable TCP Protocol */

  /* USER CODE BEGIN TCP_Protocol_Initialization */
  /* Allocate the memory for TCP server thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* create the TCP server thread */
  ret = tx_thread_create(&AppTCPThread, "App TCP Thread", App_TCP_Thread_Entry, 0, pointer, NX_APP_THREAD_STACK_SIZE,
                         NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_DONT_START);

  if (ret != TX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }
  /* USER CODE END TCP_Protocol_Initialization */

  ret = nx_tcp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable the UDP protocol required for  DHCP communication */

  /* USER CODE BEGIN UDP_Protocol_Initialization */

  /* USER CODE END UDP_Protocol_Initialization */

  ret = nx_udp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Allocate the memory for main thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the main thread */
  ret = tx_thread_create(&NxAppThread, "NetXDuo App thread", nx_app_thread_entry , 0, pointer, NX_APP_THREAD_STACK_SIZE,
                         NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

  if (ret != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* Create the DHCP client */

  /* USER CODE BEGIN DHCP_Protocol_Initialization */

  /* USER CODE END DHCP_Protocol_Initialization */

  ret = nx_dhcp_create(&DHCPClient, &NetXDuoEthIpInstance, "DHCP Client");

  if (ret != NX_SUCCESS)
  {
    return NX_DHCP_ERROR;
  }

  /* set DHCP notification callback  */
  ret = tx_semaphore_create(&DHCPSemaphore, "DHCP Semaphore", 0);

  if (ret != NX_SUCCESS)
  {
    return NX_DHCP_ERROR;
  }

  /* USER CODE BEGIN MX_NetXDuo_Init */
  /* Allocate the memory for Link thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* create the Link thread */
  ret = tx_thread_create(&AppLinkThread, "App Link Thread", App_Link_Thread_Entry, 0, pointer, NX_APP_THREAD_STACK_SIZE,
                         LINK_PRIORITY, LINK_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

  if (ret != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* Allocate the memory for Ping thread */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* create the Ping thread */
  ret = tx_thread_create(&AppPingThread, "App Ping Thread", App_Ping_Thread_Entry, 0, pointer, NX_APP_THREAD_STACK_SIZE,
                         NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_DONT_START);

  if (ret != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }
  /* USER CODE END MX_NetXDuo_Init */

  return ret;
}

/**
* @brief  ip address change callback.
* @param ip_instance: NX_IP instance
* @param ptr: user data
* @retval none
*/
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr)
{
  /* release the semaphore as soon as an IP address is available */
  if (nx_ip_address_get(&NetXDuoEthIpInstance, &IpAddress, &NetMask) != NX_SUCCESS)
  {
    Error_Handler();
  }
  if(IpAddress != NULL_ADDRESS)
  {
    tx_semaphore_put(&DHCPSemaphore);
  }
}

/**
* @brief  Main thread entry.
* @param thread_input: ULONG user argument used by the thread entry
* @retval none
*/
static VOID nx_app_thread_entry (ULONG thread_input)
{
  UINT ret = NX_SUCCESS;
  NX_PARAMETER_NOT_USED(thread_input);

  /* register the IP address change callback */
  ret = nx_ip_address_change_notify(&NetXDuoEthIpInstance, ip_address_change_notify_callback, NULL);
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  /* start the DHCP client */
  ret = nx_dhcp_start(&DHCPClient);
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }
  printf("Recherche sur le serveur DHCP ..\r\n");

  /* wait until an IP address is ready */
  if(tx_semaphore_get(&DHCPSemaphore, TX_WAIT_FOREVER) != TX_SUCCESS)
  {
    Error_Handler();
  }

  PRINT_IP_ADDRESS(IpAddress);

  /* the network is correctly initialized, start the TCP server thread */
  tx_thread_resume(&AppTCPThread);

  /* start ping thread */
  tx_thread_resume(&AppPingThread);

  /* if this thread is not needed any more, we relinquish it */
  tx_thread_relinquish();

  return;
}

/* USER CODE BEGIN 1 */
/**
* @brief  TCP thread entry - Modifié pour ThingSpeak
* @param thread_input: thread user data
* @retval none
*/
static VOID App_TCP_Thread_Entry(ULONG thread_input)
{
  UINT ret;
  UINT count = 0;

  ULONG bytes_read;
  UCHAR data_buffer[512];
  char http_request[256];

  ULONG source_ip_address;
  UINT source_port;

  NX_PACKET *server_packet = NX_NULL;
  NX_PACKET *data_packet = NX_NULL;

  NX_PARAMETER_NOT_USED(thread_input);

  while(count++ < MAX_PACKET_COUNT)
  {
    /* 1. Création de la socket avant chaque envoi (L'API ThingSpeak ferme la connexion après chaque requête) */
    ret = nx_tcp_socket_create(&NetXDuoEthIpInstance, &TCPSocket, "TCP Client Socket", NX_IP_NORMAL, NX_FRAGMENT_OKAY,
                               NX_IP_TIME_TO_LIVE, WINDOW_SIZE, NX_NULL, NX_NULL);
    if (ret != NX_SUCCESS) break;

    ret =  nx_tcp_client_socket_bind(&TCPSocket, NX_ANY_PORT, NX_WAIT_FOREVER);
    if (ret != NX_SUCCESS) break;

    printf("\r\n--- Requête %u/%u vers ThingSpeak ---\r\n", count, MAX_PACKET_COUNT);

    /* 2. Connexion au serveur ThingSpeak */
    ret = nx_tcp_client_socket_connect(&TCPSocket, THINGSPEAK_SERVER_ADDRESS, THINGSPEAK_SERVER_PORT, NX_WAIT_FOREVER);
    if (ret != NX_SUCCESS)
    {
      printf("Erreur de connexion a ThingSpeak.\r\n");
      nx_tcp_client_socket_unbind(&TCPSocket);
      nx_tcp_socket_delete(&TCPSocket);
      continue;
    }

    /* Log de simulation de lecture des capteurs */
    printf("<LectureInterface Capteurs>\r\n");

    /* Valeurs simulées (on fait varier légèrement la température pour voir la courbe sur ThingSpeak) */
    float temp = 25.5f + ((float)count * 0.2f);
    float hum = 60.0f;
    float press = 1013.2f;

    /* 3. Construction de la requête HTTP GET */
    /* RAPPEL : Remplace "VOTRE_CLEF_API_WRITE" par ta vraie clef ThingSpeak */
    snprintf(http_request, sizeof(http_request),
             "GET /update?api_key=5PH31ZBO1VEMGZFL&field1=%.1f&field2=%.1f&field3=%.1f HTTP/1.1\r\n"
             "Host: api.thingspeak.com\r\n"
             "Connection: close\r\n\r\n",temp, hum, press);

    TX_MEMSET(data_buffer, '\0', sizeof(data_buffer));

    /* allocate the packet to send over the TCP socket */
    ret = nx_packet_allocate(&NxAppPool, &data_packet, NX_TCP_PACKET, TX_WAIT_FOREVER);
    if (ret != NX_SUCCESS) break;

    /* append the HTTP message to send into the packet */
    ret = nx_packet_data_append(data_packet, (VOID *)http_request, strlen(http_request), &NxAppPool, TX_WAIT_FOREVER);
    if (ret != NX_SUCCESS)
    {
      nx_packet_release(data_packet);
      break;
    }

    /* Log indiquant que la communication réseau commence */
    printf("<Communication Réseau - Envoi de la trame HTTP>\r\n");

    /* send the packet over the TCP socket */
    ret = nx_tcp_socket_send(&TCPSocket, data_packet, DEFAULT_TIMEOUT);
    data_packet = NX_NULL;

    /* wait for the server response (ThingSpeak doit répondre un "200 OK") */
    ret = nx_tcp_socket_receive(&TCPSocket, &server_packet, DEFAULT_TIMEOUT);

    if (ret == NX_SUCCESS)
    {
      nx_udp_source_extract(server_packet, &source_ip_address, &source_port);
      nx_packet_data_retrieve(server_packet, data_buffer, &bytes_read);

      printf("Reponse de ThingSpeak reçue (%lu bytes)\r\n", bytes_read);

      nx_packet_release(server_packet);
      server_packet = NX_NULL;
    }
    else
    {
      printf("Aucune reponse ou timeout.\r\n");
    }

    /* Déconnexion propre de la socket après l'envoi */
    nx_tcp_socket_disconnect(&TCPSocket, DEFAULT_TIMEOUT);
    nx_tcp_client_socket_unbind(&TCPSocket);
    nx_tcp_socket_delete(&TCPSocket);

    /* 4. TEMPO OBLIGATOIRE POUR THINGSPEAK (15 Secondes) */
    if (count < MAX_PACKET_COUNT) {
        printf("Attente de 15 secondes (Limitation ThingSpeak)...\r\n");
        tx_thread_sleep(NX_IP_PERIODIC_RATE * 15);
    }
  }

  /* print test summary on the UART */
  if (count == MAX_PACKET_COUNT + 1)
  {
    printf("\r\n-------------------------------------\r\n\tSUCCESS : %u / %u packets sent to Cloud\r\n-------------------------------------\r\n", count - 1, MAX_PACKET_COUNT);
    Success_Handler();
  }
  else
  {
    printf("\r\n-------------------------------------\r\n\tFAIL : %u / %u packets sent\r\n-------------------------------------\r\n", count - 1, MAX_PACKET_COUNT);
    Error_Handler();
  }
}

/**
* @brief  Ping thread entry
* @param thread_input: thread user data
* @retval none
*/
static VOID App_Ping_Thread_Entry(ULONG thread_input)
{
  UINT ret;
  UINT ping_count = 0;
  NX_PACKET *ping_response = NX_NULL;
  CHAR ping_data[PING_DATA_SIZE] = { 'P', 'I', 'N', 'G' };

  NX_PARAMETER_NOT_USED(thread_input);

  while(ping_count < MAX_PING_COUNT)
  {
    if (IpAddress != NULL_ADDRESS)
    {
      ret = nx_icmp_ping(&NetXDuoEthIpInstance,
                         PING_ADDRESS,
                         ping_data,
                         sizeof(ping_data),
                         &ping_response,
                         NX_IP_PERIODIC_RATE);

      if (ret == NX_SUCCESS)
      {
        printf("Ping 8.8.8.8 OK\r\n");

        /* Clignotement de la LED VERTE pendant 3 secondes puis OFF */
        for (UINT i = 0; i < 6; i++)
        {
          HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
          tx_thread_sleep(NX_IP_PERIODIC_RATE / 2);
        }

        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);

        if (ping_response != NX_NULL)
        {
          nx_packet_release(ping_response);
          ping_response = NX_NULL;
        }
      }
      else
      {
        printf("Ping 8.8.8.8 FAIL\r\n");
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
      }

      ping_count++;
    }
    else
    {
      HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
    }

    tx_thread_sleep(NX_IP_PERIODIC_RATE);
  }

  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
  printf("Ping test finished after %u attempts\r\n", MAX_PING_COUNT);

  tx_thread_terminate(&AppPingThread);
}

/**
* @brief  Link thread entry
* @param thread_input: ULONG thread parameter
* @retval none
*/
static VOID App_Link_Thread_Entry(ULONG thread_input)
{
  ULONG actual_status;
  UINT linkdown = 0, status;

  NX_PARAMETER_NOT_USED(thread_input);

  while(1)
  {
    /* Send request to check if the Ethernet cable is connected. */
    status = nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0, NX_IP_LINK_ENABLED,
                                          &actual_status, 10);

    if(status == NX_SUCCESS)
    {
      if(linkdown == 1)
      {
        linkdown = 0;

        printf("Le câble réseau est connecté.\r\n");

        nx_ip_driver_direct_command(&NetXDuoEthIpInstance, NX_LINK_ENABLE,
                                    &actual_status);

        status = nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0, NX_IP_ADDRESS_RESOLVED,
                                              &actual_status, 10);
        if(status == NX_SUCCESS)
        {
          nx_dhcp_stop(&DHCPClient);
          nx_dhcp_reinitialize(&DHCPClient);
          nx_dhcp_start(&DHCPClient);

          if(tx_semaphore_get(&DHCPSemaphore, TX_WAIT_FOREVER) != TX_SUCCESS)
          {
            Error_Handler();
          }

          PRINT_IP_ADDRESS(IpAddress);
        }
        else
        {
          nx_dhcp_client_update_time_remaining(&DHCPClient, 0);
        }
      }
    }
    else
    {
      if(0 == linkdown)
      {
        linkdown = 1;
        printf("The network cable is not connected.\r\n");
        nx_ip_driver_direct_command(&NetXDuoEthIpInstance, NX_LINK_DISABLE,
                                    &actual_status);
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
      }
    }

    tx_thread_sleep(NX_APP_CABLE_CONNECTION_CHECK_PERIOD);
  }
}

/* USER CODE END 1 */
