#include "main.h"
#include "app_netxduo.h"

extern UART_HandleTypeDef huart3;
extern NX_IP NetXDuoEthIpInstance;
extern NX_PACKET_POOL NxAppPool;

NX_TCP_SOCKET TCPSocket;
ULONG IpAddress;
ULONG NetMask;

TX_THREAD receive_thread;
TX_THREAD send_thread;

#define TCP_THREAD_STACK_SIZE 2*1024
CHAR send_thread_stack[TCP_THREAD_STACK_SIZE];
CHAR recv_thread_stack[TCP_THREAD_STACK_SIZE];


void receive_thread_entry(ULONG thread_input);
void send_thread_entry(ULONG thread_input);
void cleanup_and_relisten(void);

static VOID tcp_listen_callback(NX_TCP_SOCKET *socket_ptr, UINT port)
{
    printf("tcp_listen_callback\n");
}

VOID cangateway_main(ULONG thread_input)
{
    printf("\nmyMain start\n");
    UINT ret;

    /*
     * Print IPv4
     */
    ret = nx_ip_address_get(&NetXDuoEthIpInstance, &IpAddress, &NetMask);
    if (ret != TX_SUCCESS)
    {
        Error_Handler();
    }
    else
    {
        PRINT_IP_ADDRESS(IpAddress);
    }

    printf("tp1\n");
    /*
     * Create a TCP Socket
     */
    ret = nx_tcp_socket_create(&NetXDuoEthIpInstance, &TCPSocket, "TCP Server Socket", NX_IP_NORMAL, NX_FRAGMENT_OKAY, NX_IP_TIME_TO_LIVE, 512, NX_NULL, NX_NULL);
    if (ret != NX_SUCCESS)
    {
        Error_Handler();
    }

    printf("tp2\n");

    ret = nx_tcp_server_socket_listen(&NetXDuoEthIpInstance, DEFAULT_PORT, &TCPSocket, MAX_TCP_CLIENTS, tcp_listen_callback);
    if (ret != NX_SUCCESS)
    {
        Error_Handler();
    }
    else
    {
        printf("TCP Server listening on PORT %d ..\n", DEFAULT_PORT);
    }

    while (1)
    {
        printf("tp3 wait accept\n");
        ret = nx_tcp_server_socket_accept(&TCPSocket, TX_WAIT_FOREVER);
        if (ret != NX_SUCCESS)
        {
            Error_Handler();
        }

        printf("tp4\n");

        // スレッドの作成
        ret = tx_thread_create(&receive_thread,
                               "Receive Thread",
                               receive_thread_entry,
                               0,
							   recv_thread_stack,
							   TCP_THREAD_STACK_SIZE,
                               NX_APP_THREAD_PRIORITY,
                               NX_APP_THREAD_PRIORITY,
                               TX_NO_TIME_SLICE,
                               TX_AUTO_START);
        if (ret != NX_SUCCESS)
        {
            Error_Handler();
        }

        printf("tp5\n");

        ret = tx_thread_create(&send_thread,
                               "Send Thread",
                               send_thread_entry,
                               0,
							   send_thread_stack,
							   TCP_THREAD_STACK_SIZE,
                               NX_APP_THREAD_PRIORITY,
                               NX_APP_THREAD_PRIORITY,
                               TX_NO_TIME_SLICE,
                               TX_AUTO_START);
        if (ret != NX_SUCCESS)
        {
            Error_Handler();
        }

        printf("wait for disconnect\n");

        // クライアントの切断を待つ
        while (1)
        {
            ULONG socket_state;

            // printf("check state\n");
            ret = nx_tcp_socket_info_get(&TCPSocket, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &socket_state, NULL, NULL, NULL);
            if (ret != NX_SUCCESS)
            {
                Error_Handler();
            }

            if (socket_state != NX_TCP_ESTABLISHED)
            {
                printf("Connection lost, resetting socket\n");
                break;
            }

            tx_thread_sleep(10); // 少し待機
        }

        // クライアントが切断された場合のクリーンアップ
        cleanup_and_relisten();
    }
}

void receive_thread_entry(ULONG thread_input)
{
    UINT ret;
    NX_PACKET *data_packet;
    UCHAR data_buffer[512];
    ULONG bytes_read;

    printf("start receive thread\n");
    while (1)
    {
        TX_MEMSET(data_buffer, '\0', sizeof(data_buffer));

        // printf("wait receive\n");
        /* Receive the TCP packet sent by the client */
        ret = nx_tcp_socket_receive(&TCPSocket, &data_packet, NX_WAIT_FOREVER);

        if (ret == NX_SUCCESS)
        {
            /* Retrieve the data sent by the client */
            nx_packet_data_retrieve(data_packet, data_buffer, &bytes_read);

            /* Print the received data */
            printf("Received data: %s\n", data_buffer);

            /* Release the packet */
            nx_packet_release(data_packet);
        }
        else
        {
        	tx_thread_sleep(1);
        }
    }
}

void send_thread_entry(ULONG thread_input)
{
    UINT ret;
    NX_PACKET *data_packet;
    UCHAR data_buffer[512] = "Periodic message from server"; // 送信するデータ

    printf("start send thread\n");
    while (1)
    {
        /* Allocate a packet */
        ret = nx_packet_allocate(&NxAppPool, &data_packet, NX_TCP_PACKET, TX_WAIT_FOREVER);
        if (ret != NX_SUCCESS)
        {
            Error_Handler();
        }

        /* Append data to the packet */
        ret = nx_packet_data_append(data_packet, data_buffer, sizeof(data_buffer), &NxAppPool, TX_WAIT_FOREVER);
        if (ret != NX_SUCCESS)
        {
            nx_packet_release(data_packet);
            Error_Handler();
        }

        /* Send the packet */
        ret = nx_tcp_socket_send(&TCPSocket, data_packet, TX_WAIT_FOREVER);
        if (ret != NX_SUCCESS)
        {
            nx_packet_release(data_packet);
            printf("nx_tcp_socket_send error %d\n", ret);
            Error_Handler();
        }

        /* Wait for a while before sending the next packet */
        tx_thread_sleep(100); // 1秒待機
    }
}

void cleanup_and_relisten(void)
{
    // スレッドの削除
    tx_thread_terminate(&receive_thread);
    tx_thread_delete(&receive_thread);

    tx_thread_terminate(&send_thread);
    tx_thread_delete(&send_thread);

    // ソケットの切断とリセット
    nx_tcp_socket_disconnect(&TCPSocket, TX_WAIT_FOREVER);
    nx_tcp_server_socket_unaccept(&TCPSocket);
    nx_tcp_server_socket_relisten(&NetXDuoEthIpInstance, DEFAULT_PORT, &TCPSocket);
}
