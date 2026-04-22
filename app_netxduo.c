/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_netxduo.c
  * @brief   NetXDuo - Envoi capteurs vers ThingSpeak (HTTP GET)
  *          + Prediction IA avec modele meteostat_v3 (X-CUBE-AI)
  *
  * Modifications :
  *   - Cle ThingSpeak mise a jour (channel 3339275)
  *   - Ajout inference IA : entrees [temp, pression, accel_z] -> sortie [0,1,2]
  *     0 = Beau temps, 1 = Nuageux, 2 = Pluie
  *   - field6 = classe predite par le modele IA
  *   - field7 = confiance (probabilite max en %)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "app_netxduo.h"
#include "nxd_dhcp_client.h"

/* USER CODE BEGIN Includes */
#include "hts221_reg.h"
#include "lps22hh_reg.h"
#include "lsm6dso16is_reg.h"
#include "main.h"

/* --- X-CUBE-AI headers --- */
#include "network.h"
#include "network_data.h"
#include "ai_platform.h"
#include "ai_datatypes_defines.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
TX_THREAD      NxAppThread;
NX_PACKET_POOL NxAppPool;
NX_IP          NetXDuoEthIpInstance;
TX_SEMAPHORE   DHCPSemaphore;
NX_DHCP        DHCPClient;

/* USER CODE BEGIN PV */

/* =========================================================================
 * Constantes ThingSpeak
 * Channel ID  : 3339275
 * Write API Key : 5PH31ZBO1VEMGZFL
 * ========================================================================= */
#define THINGSPEAK_WRITE_KEY    "5PH31ZBO1VEMGZFL"
#define THINGSPEAK_SEND_PERIOD  (15 * NX_IP_PERIODIC_RATE) /* 15 secondes */
#define TCP_SERVER_ADDRESS      IP_ADDRESS(184, 106, 153, 149)
#define TCP_SERVER_PORT         80
#define WINDOW_SIZE             1024
#define DEFAULT_TIMEOUT         (5 * NX_IP_PERIODIC_RATE)

/* =========================================================================
 * Etiquettes de prediction IA (3 classes)
 * Le modele meteostat_v3 classifie : Beau(0), Nuageux(1), Pluie(2)
 * ========================================================================= */
#define AI_CLASS_SUNNY   0
#define AI_CLASS_CLOUDY  1
#define AI_CLASS_RAINY   2

static const char* ai_class_labels[3] = {
    "Beau",
    "Nuageux",
    "Pluie"
};

/* --- Contextes capteurs (declarés dans main.c via extern) --- */
extern stmdev_ctx_t dev_ctx_hts221;
extern stmdev_ctx_t dev_ctx_lps22hh;
extern stmdev_ctx_t dev_ctx_lsm6dso;

/* --- Calibration HTS221 (déclarée dans main.c via extern) --- */
extern float t0_degC, t1_degC, t0_out, t1_out;

/* --- Buffers X-CUBE-AI --- */
static ai_handle        network_handle = AI_HANDLE_NULL;
static ai_float         ai_input_data[AI_NETWORK_IN_1_SIZE];   /* 3 floats */
static ai_float         ai_output_data[AI_NETWORK_OUT_1_SIZE]; /* 3 floats */
static ai_u8            activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID nx_app_thread_entry(ULONG thread_input);
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr);

/* USER CODE BEGIN PFP */
static int  thingspeak_send(float temp_c, float press_hpa, float ax, float ay, float az,
                             int ai_class, float ai_confidence);
static int  ai_model_init(void);
static int  ai_model_run(float temp_c, float press_hpa, float accel_z,
                          int *pred_class, float *pred_confidence);
/* USER CODE END PFP */

/* ==========================================================================
 * MX_NetXDuo_Init()
 * ========================================================================== */
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
    UINT ret = NX_SUCCESS;
    TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
    CHAR *pointer;

    nx_system_initialize();

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer,
                          NX_APP_PACKET_POOL_SIZE, TX_NO_WAIT) != TX_SUCCESS)
        return TX_POOL_ERROR;

    ret = nx_packet_pool_create(&NxAppPool, "NetXDuo App Pool",
                                DEFAULT_PAYLOAD_SIZE,
                                pointer, NX_APP_PACKET_POOL_SIZE);
    if (ret != NX_SUCCESS) return NX_POOL_ERROR;

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer,
                          Nx_IP_INSTANCE_THREAD_SIZE, TX_NO_WAIT) != TX_SUCCESS)
        return TX_POOL_ERROR;

    ret = nx_ip_create(&NetXDuoEthIpInstance, "NetX Ip instance",
                       NX_APP_DEFAULT_IP_ADDRESS, NX_APP_DEFAULT_NET_MASK,
                       &NxAppPool, nx_stm32_eth_driver,
                       pointer, Nx_IP_INSTANCE_THREAD_SIZE,
                       NX_APP_INSTANCE_PRIORITY);
    if (ret != NX_SUCCESS) return NX_NOT_SUCCESSFUL;

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer,
                          DEFAULT_ARP_CACHE_SIZE, TX_NO_WAIT) != TX_SUCCESS)
        return TX_POOL_ERROR;

    ret = nx_arp_enable(&NetXDuoEthIpInstance, (VOID *)pointer, DEFAULT_ARP_CACHE_SIZE);
    if (ret != NX_SUCCESS) return NX_NOT_SUCCESSFUL;

    ret = nx_icmp_enable(&NetXDuoEthIpInstance);
    if (ret != NX_SUCCESS) return NX_NOT_SUCCESSFUL;

    ret = nx_tcp_enable(&NetXDuoEthIpInstance);
    if (ret != NX_SUCCESS) return NX_NOT_SUCCESSFUL;

    ret = nx_udp_enable(&NetXDuoEthIpInstance);
    if (ret != NX_SUCCESS) return NX_NOT_SUCCESSFUL;

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer,
                          NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
        return TX_POOL_ERROR;

    ret = tx_thread_create(&NxAppThread, "NetXDuo App thread",
                           nx_app_thread_entry, 0,
                           pointer, NX_APP_THREAD_STACK_SIZE,
                           NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY,
                           TX_NO_TIME_SLICE, TX_AUTO_START);
    if (ret != TX_SUCCESS) return TX_THREAD_ERROR;

    ret = nx_dhcp_create(&DHCPClient, &NetXDuoEthIpInstance, "DHCP Client");
    if (ret != NX_SUCCESS) return NX_DHCP_ERROR;

    ret = tx_semaphore_create(&DHCPSemaphore, "DHCP Semaphore", 0);
    if (ret != TX_SUCCESS) return NX_DHCP_ERROR;

    return NX_SUCCESS;
}

static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr)
{
    ULONG ip_addr, mask;
    if (nx_ip_address_get(ip_instance, &ip_addr, &mask) == NX_SUCCESS) {
        printf("[NET] IP obtenue : %lu.%lu.%lu.%lu\r\n",
               (ip_addr >> 24) & 0xFF, (ip_addr >> 16) & 0xFF,
               (ip_addr >>  8) & 0xFF,  ip_addr & 0xFF);
    }
    tx_semaphore_put(&DHCPSemaphore);
}

/* USER CODE BEGIN 1 */

/* ==========================================================================
 * ai_model_init() - Initialise le reseau neuronal X-CUBE-AI
 * ========================================================================== */
static int ai_model_init(void)
{
    ai_error err;
    const ai_handle weights[] = { AI_NETWORK_DATA_WEIGHTS_GET() };

    /* Creation du reseau */
    err = ai_network_create_and_init(&network_handle, weights, NULL);
    if (err.type != AI_ERROR_NONE) {
        printf("[AI] ERREUR init : type=%d code=%d\r\n", err.type, err.code);
        return -1;
    }

    printf("[AI] Modele meteostat_v3 initialise (3 entrees -> 3 classes)\r\n");
    return 0;
}

/* ==========================================================================
 * ai_model_run() - Lance l'inference IA
 *
 * Entrees  : temp_c    (temperature degC)
 *            press_hpa (pression hPa)
 *            accel_z   (acceleration Z en mg)
 *
 * Sorties  : pred_class      (0=Beau, 1=Nuageux, 2=Pluie)
 *            pred_confidence (probabilite de la classe en 0.0-1.0)
 *
 * Retourne 0 si succes, -1 si erreur
 * ========================================================================== */
static int ai_model_run(float temp_c, float press_hpa, float accel_z,
                         int *pred_class, float *pred_confidence)
{
    if (network_handle == AI_HANDLE_NULL) {
        printf("[AI] Modele non initialise\r\n");
        return -1;
    }

    /* Preparation des buffers d'entree/sortie */
    ai_buffer input_buf = {
        .format   = AI_BUFFER_FORMAT_FLOAT,
        .shape    = AI_BUFFER_SHAPE_INIT(AI_SHAPE_BCWH, 4, 1, 1, 1, AI_NETWORK_IN_1_CHANNEL),
        .size     = AI_NETWORK_IN_1_SIZE,
        .meta_info = NULL,
        .flags    = 0,
        .data     = AI_HANDLE_PTR(ai_input_data)
    };
    ai_buffer output_buf = {
        .format   = AI_BUFFER_FORMAT_FLOAT,
        .shape    = AI_BUFFER_SHAPE_INIT(AI_SHAPE_BCWH, 4, 1, 1, 1, AI_NETWORK_OUT_1_CHANNEL),
        .size     = AI_NETWORK_OUT_1_SIZE,
        .meta_info = NULL,
        .flags    = 0,
        .data     = AI_HANDLE_PTR(ai_output_data)
    };

    /* Remplissage des donnees d'entree */
    ai_input_data[0] = temp_c;
    ai_input_data[1] = press_hpa;
    ai_input_data[2] = accel_z;

    printf("[AI] Inference : temp=%.2f pression=%.2f accel_z=%.2f\r\n",
           temp_c, press_hpa, accel_z);

    /* Execution du reseau */
    ai_i32 n_batch = ai_network_run(network_handle, &input_buf, &output_buf);
    if (n_batch != 1) {
        ai_error err = ai_network_get_error(network_handle);
        printf("[AI] ERREUR run : type=%d code=%d\r\n", err.type, err.code);
        return -1;
    }

    /* Lecture softmax -> classe avec probabilite max */
    int   best_class = 0;
    float best_prob  = ai_output_data[0];

    for (int i = 1; i < AI_NETWORK_OUT_1_SIZE; i++) {
        if (ai_output_data[i] > best_prob) {
            best_prob  = ai_output_data[i];
            best_class = i;
        }
    }

    *pred_class      = best_class;
    *pred_confidence = best_prob;

    printf("[AI] Prediction : %s (classe %d, confiance %.1f%%)\r\n",
           ai_class_labels[best_class], best_class, best_prob * 100.0f);
    printf("[AI] Probabilites -> Beau:%.3f Nuageux:%.3f Pluie:%.3f\r\n",
           ai_output_data[0], ai_output_data[1], ai_output_data[2]);

    return 0;
}

/* ==========================================================================
 * thingspeak_send() - Envoie toutes les donnees vers ThingSpeak
 *
 * Fields ThingSpeak :
 *   field1 = temperature (degC)
 *   field2 = pression (hPa)
 *   field3 = acceleration X (mg)
 *   field4 = acceleration Y (mg)
 *   field5 = acceleration Z (mg)
 *   field6 = classe IA predite (0=Beau, 1=Nuageux, 2=Pluie)
 *   field7 = confiance IA (%)
 * ========================================================================== */
static int thingspeak_send(float temp_c, float press_hpa,
                            float ax, float ay, float az,
                            int ai_class, float ai_confidence)
{
    NX_TCP_SOCKET tcp_socket;
    NXD_ADDRESS   server_ip;
    NX_PACKET    *pkt;
    UINT          status;
    char          request[640];

    server_ip.nxd_ip_version    = NX_IP_VERSION_V4;
    server_ip.nxd_ip_address.v4 = TCP_SERVER_ADDRESS;

    status = nx_tcp_socket_create(&NetXDuoEthIpInstance, &tcp_socket, "TS",
                                  NX_IP_NORMAL, NX_FRAGMENT_OKAY,
                                  NX_IP_TIME_TO_LIVE, WINDOW_SIZE,
                                  NX_NULL, NX_NULL);
    if (status != NX_SUCCESS) {
        printf("[TS] ERR socket_create 0x%02X\r\n", status);
        return -1;
    }

    status = nx_tcp_client_socket_bind(&tcp_socket, NX_ANY_PORT, DEFAULT_TIMEOUT);
    if (status != NX_SUCCESS) {
        printf("[TS] ERR bind 0x%02X\r\n", status);
        nx_tcp_socket_delete(&tcp_socket);
        return -1;
    }

    printf("[TS] Connexion ThingSpeak (channel 3339275)...\r\n");
    status = nxd_tcp_client_socket_connect(&tcp_socket, &server_ip, TCP_SERVER_PORT, DEFAULT_TIMEOUT);
    if (status != NX_SUCCESS) {
        printf("[TS] ERR connect 0x%02X\r\n", status);
        nx_tcp_client_socket_unbind(&tcp_socket);
        nx_tcp_socket_delete(&tcp_socket);
        return -1;
    }

    /* Construction de la requete HTTP GET avec tous les champs */
    snprintf(request, sizeof(request),
             "GET /update?api_key=%s"
             "&field1=%.2f"          /* Temperature */
             "&field2=%.2f"          /* Pression */
             "&field3=%.2f"          /* Accel X */
             "&field4=%.2f"          /* Accel Y */
             "&field5=%.2f"          /* Accel Z */
             "&field6=%d"            /* Classe IA */
             "&field7=%.1f"          /* Confiance IA % */
             " HTTP/1.1\r\nHost: api.thingspeak.com\r\n"
             "Connection: close\r\n\r\n",
             THINGSPEAK_WRITE_KEY,
             temp_c, press_hpa,
             ax, ay, az,
             ai_class,
             ai_confidence * 100.0f);

    status = nx_packet_allocate(&NxAppPool, &pkt, NX_TCP_PACKET, DEFAULT_TIMEOUT);
    if (status != NX_SUCCESS) {
        nx_tcp_socket_disconnect(&tcp_socket, DEFAULT_TIMEOUT);
        nx_tcp_client_socket_unbind(&tcp_socket);
        nx_tcp_socket_delete(&tcp_socket);
        return -1;
    }
    nx_packet_data_append(pkt, request, strlen(request), &NxAppPool, DEFAULT_TIMEOUT);

    status = nx_tcp_socket_send(&tcp_socket, pkt, DEFAULT_TIMEOUT);
    if (status != NX_SUCCESS) {
        nx_packet_release(pkt);
        nx_tcp_socket_disconnect(&tcp_socket, DEFAULT_TIMEOUT);
        nx_tcp_client_socket_unbind(&tcp_socket);
        nx_tcp_socket_delete(&tcp_socket);
        return -1;
    }

    /* Lecture de la reponse */
    NX_PACKET *resp;
    status = nx_tcp_socket_receive(&tcp_socket, &resp, DEFAULT_TIMEOUT);
    if (status == NX_SUCCESS) {
        char buf[128] = {0};
        ULONG n = 0;
        nx_packet_data_extract_offset(resp, 0, buf, sizeof(buf) - 1, &n);
        buf[n] = '\0';
        if (strstr(buf, "200 OK")) {
            printf("[TS] SUCCES - Donnees envoyees ! (temp=%.1f pres=%.1f IA=%s %.0f%%)\r\n",
                   temp_c, press_hpa, ai_class_labels[ai_class], ai_confidence * 100.0f);
        } else {
            printf("[TS] Reponse inattendue : %.64s\r\n", buf);
        }
        nx_packet_release(resp);
    }

    nx_tcp_socket_disconnect(&tcp_socket, DEFAULT_TIMEOUT);
    nx_tcp_client_socket_unbind(&tcp_socket);
    nx_tcp_socket_delete(&tcp_socket);
    return 0;
}

/* ==========================================================================
 * nx_app_thread_entry() - Thread principal
 * ========================================================================== */
static VOID nx_app_thread_entry(ULONG thread_input)
{
    UINT ret;

    /* --- Initialisation du modele IA --- */
    if (ai_model_init() != 0) {
        printf("[APP] ERREUR : impossible d'initialiser le modele IA\r\n");
        /* On continue quand meme sans IA */
    }

    /* --- Connexion reseau via DHCP --- */
    ret = nx_ip_address_change_notify(&NetXDuoEthIpInstance, ip_address_change_notify_callback, NX_NULL);
    ret = nx_dhcp_start(&DHCPClient);
    printf("[NET] Recherche DHCP...\r\n");

    tx_semaphore_get(&DHCPSemaphore, TX_WAIT_FOREVER);
    printf("[APP] Reseau pret - Boucle d'acquisition\r\n");
    printf("[APP] Channel ThingSpeak : 3339275\r\n");
    printf("[APP] Cle Write         : %s\r\n", THINGSPEAK_WRITE_KEY);
    printf("[APP] Modele IA         : meteostat_v3 (3 classes meteo)\r\n\r\n");

    while (1)
    {
        /* --- LED VERT ON, ROUGE OFF (disponible) --- */
        HAL_GPIO_WritePin(GPIOG, LED_GREEN_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOG, LED_RED_Pin,   GPIO_PIN_SET);

        printf("\r\n--- Cycle capteurs ---\r\n");
        tx_thread_sleep(2 * NX_IP_PERIODIC_RATE);

        /* --- LED ROUGE ON (acquisition) --- */
        HAL_GPIO_WritePin(GPIOG, LED_GREEN_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOG, LED_RED_Pin,   GPIO_PIN_RESET);

        /* ------------------------------------------------------------------ */
        /* Lecture des capteurs                                                */
        /* ------------------------------------------------------------------ */
        uint8_t data_ready = 0;
        float   temp_c    = 0.0f;
        float   press_hpa = 0.0f;
        float   ax = 0.0f, ay = 0.0f, az = 0.0f;
        uint8_t temp_ok   = 0, press_ok = 0, accel_ok = 0;

        /* HTS221 - Temperature */
        hts221_temp_data_ready_get(&dev_ctx_hts221, &data_ready);
        if (data_ready) {
            int16_t raw_t = 0;
            hts221_temperature_raw_get(&dev_ctx_hts221, &raw_t);
            if ((t1_out - t0_out) != 0.0f) {
                temp_c  = ((float)raw_t - t0_out) * (t1_degC - t0_degC)
                          / (t1_out - t0_out) + t0_degC;
                temp_ok = 1;
                printf("HTS221  Temp : %.2f degC\r\n", temp_c);
            }
        }
        data_ready = 0;

        /* LPS22HH - Pression */
        lps22hh_press_flag_data_ready_get(&dev_ctx_lps22hh, &data_ready);
        if (data_ready) {
            uint32_t raw_p = 0;
            lps22hh_pressure_raw_get(&dev_ctx_lps22hh, &raw_p);
            press_hpa = lps22hh_from_lsb_to_hpa(raw_p);
            press_ok  = 1;
            printf("LPS22HH Pres : %.2f hPa\r\n", press_hpa);
        }
        data_ready = 0;

        /* LSM6DSO - Accelerometre */
        lsm6dso16is_xl_flag_data_ready_get(&dev_ctx_lsm6dso, &data_ready);
        if (data_ready) {
            int16_t raw_a[3];
            lsm6dso16is_acceleration_raw_get(&dev_ctx_lsm6dso, raw_a);
            ax = lsm6dso16is_from_fs2g_to_mg(raw_a[0]);
            ay = lsm6dso16is_from_fs2g_to_mg(raw_a[1]);
            az = lsm6dso16is_from_fs2g_to_mg(raw_a[2]);
            accel_ok = 1;
            printf("LSM6DSO Acc  : X=%.2f Y=%.2f Z=%.2f mg\r\n", ax, ay, az);
        }

        /* ------------------------------------------------------------------ */
        /* Inference IA et envoi ThingSpeak                                   */
        /* ------------------------------------------------------------------ */
        if (temp_ok && press_ok) {
            int   ai_class       = 0;
            float ai_confidence  = 0.0f;

            /* Utilise accel_z si disponible, sinon 0 */
            float accel_z_val = accel_ok ? az : 0.0f;

            /* Prediction meteo */
            int ai_ret = ai_model_run(temp_c, press_hpa, accel_z_val,
                                       &ai_class, &ai_confidence);
            if (ai_ret != 0) {
                printf("[APP] Inference IA echouee, envoi sans IA\r\n");
                ai_class      = -1; /* -1 indique erreur IA */
                ai_confidence = 0.0f;
            }

            /* Envoi ThingSpeak */
            HAL_GPIO_WritePin(GPIOG, LED_RED_Pin, GPIO_PIN_SET);
            printf("[APP] Envoi vers ThingSpeak...\r\n");
            thingspeak_send(temp_c, press_hpa,
                             accel_ok ? ax : 0.0f,
                             accel_ok ? ay : 0.0f,
                             accel_ok ? az : 0.0f,
                             (ai_ret == 0) ? ai_class : 0,
                             (ai_ret == 0) ? ai_confidence : 0.0f);
        } else {
            printf("[APP] Donnees capteurs insuffisantes (temp_ok=%d press_ok=%d)\r\n",
                   temp_ok, press_ok);
        }

        /* Attente avant prochain cycle */
        tx_thread_sleep(THINGSPEAK_SEND_PERIOD);
    }
}
/* USER CODE END 1 */
