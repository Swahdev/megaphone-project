// MEMO : PROTEGER LE CTR-D

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "memory.h"

#define TAILLE_MSG 255
#define SIZE_BUF 1024

void *security(void *arg);
void ticketing(IDCard *card);
void reception(IDCard *card);
void archive(IDCard *card);
void return_adress(IDCard *card);
void ajout_fichier(IDCard *card);
void telecharge(IDCard *card);

void respond(int socket, unsigned int codereq, unsigned int id,
             unsigned int numfil, unsigned int nb);

void* broadcast(void *args);

int main(int argc, char **argv)
{

    Memory *memory = malloc(sizeof(Memory));

    /* creation de la socket serveur */
    int sock = socket(PF_INET6, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Error : Failed to create socket");
        exit(1);
    }

    if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &(int){0}, sizeof(int)) < 0)
    {
        perror("setsockopt(IPV6_V6ONLY) failed");
        exit(1);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    /* creation de l’adresse du destinataire (serveur) */
    struct sockaddr_in6 adrsock;
    memset(&adrsock, 0, sizeof(adrsock));
    adrsock.sin6_family = AF_INET6;
    adrsock.sin6_port = htons(2717);
    adrsock.sin6_addr = in6addr_any;

    int r = bind(sock, (struct sockaddr *)&adrsock, sizeof(adrsock));
    if (r < 0)
    {
        perror("Error while binding");
        exit(1);
    }

    r = listen(sock, 0);
    if (r < 0)
    {
        perror("Error while initializing connexion");
        exit(1);
    }

    struct sockaddr_storage adrclient;
    memset(&adrclient, 0, sizeof(adrclient));
    socklen_t size = sizeof(adrclient);

    printf("\nWelcome to Megaphone \\_°-°_/\n\n");

    pthread_t broadcast_thread;
    if (pthread_create(&broadcast_thread, NULL, broadcast, memory) == -1)
    {
        perror("Error : Failed to create broadcast thread");
    }

    while (1)
    {
        int sockclient;
        sockclient = accept(sock, (struct sockaddr *)&adrclient, &size);

        // printf("Client connected...\n");

        if (sockclient < 0)
        {
            // TODO ERROR
        }

        IDCard *card = malloc(sizeof(IDCard));
        card->memory = memory;
        card->socket = sockclient;

        pthread_t thread;
        if (pthread_create(&thread, NULL, security, card) == -1)
        {
            perror("Error : Failed to create thread");
            continue;
        }
    }

    return 0;
}

void free_card(IDCard* card) {
    //free(card->data);
    free(card);
}

void *security(void *arg)
{

    IDCard *card = (IDCard *)arg;

    char buf[SIZE_BUF + 1];
    memset(buf, 0, sizeof(buf));

    int recu = recv(card->socket, buf, SIZE_BUF, 0);
    if (recu < 0)
    {
        close(card->socket);
        // TODO ERROR
        return NULL;
    }
    if (recu <= 6)
    {
        close(card->socket);
        // TODO INCORRECT FORMAT ERROR
        return NULL;
    }

    uint16_t head_rec;
    memcpy(&head_rec, buf, 2);
    head_rec = ntohs(head_rec);
    card->codereq = head_rec & ((1 << 5) - 1);
    card->id = head_rec >> 5;

    // Login
    if (card->codereq == 1)
    {
        if (recu != 12)
        {
            perror("Incorect login format");
            return NULL;
        }
        card->datalen = 10;
        card->pseudo = malloc(10);
        // memcpy(card->pseudo, buf + 2, 10);

        for (int i = 0; i < 10; i++)
        {
            if (buf[2 + i] == '#' || buf[2 + i] == '\n')
            {
                card->pseudo[i] = '\0';
                break;
            }
            else
                card->pseudo[i] = buf[2 + i];
        }

        ticketing(card);
        return NULL;
    }
    // Verifie id and parse
    else
    {
        memcpy(&card->numfil, buf+2, 2);
        card->numfil = ntohs(card->numfil);
        memcpy(&card->nb, buf+4, 2);
        card->nb = ntohs(card->nb);

        //printf("%u+%u : %u\n", (u_int16_t)(buf[4] * 256), (u_int16_t)buf[5], card->nb);
        card->datalen = ntohs(buf[6]);
        if (card->datalen > 0)
        {
            card->data = malloc(card->datalen);
            memcpy(card->data, buf+7, card->datalen);
        }
        card->pseudo = get_pseudo(card);
        if (card->pseudo == NULL)
        {
            printf("Error, unknown user\n");
            respond(card->socket, 31, 0, 0, 0);
        }
        else
        {
            reception(card);
        }
    }

    // destroy_card(card);
    close(card->socket);
    return NULL;
}

void ticketing(IDCard *card)
{
    char *pseudo = malloc(10);
    memset(pseudo, '#', 10);
    memcpy(pseudo, card->pseudo, 10);

    unsigned int id = add_user(card);
    if (id > 0)
    {
        printf("-> client %u register as %s\n", id, card->pseudo);
        respond(card->socket, 1, id, 0, 0);
    }
    else
    {
        printf("Failed to register client\n");
        respond(card->socket, 31, 0, 0, 0);
    }

    free_card(card);
}

void reception(IDCard *card)
{
    int len;
    switch (card->codereq)
    {
    case 2:
        post(card);
        printf("[%u] %s : %s\n", card->numfil, card->pseudo, card->data);
        break;
    case 3:
        // int nb_fil = get_message_number(card);
        // printf("fil : %u\n", card->numfil);
        len = get_message_number(card);
        respond(card->socket, 3, card->id, len == 1 ? card->numfil : len,
                card->datalen);
        printf("-> %s asking for messages [%d] : %u messages\n", card->pseudo,
               card->numfil, card->datalen);

        archive(card);

        break;
    case 4:
        return_adress(card);

        printf("-> %s suscribe to channel %d\n", card->pseudo, card->numfil);

        break;
    case 5:
        printf("[%u] %s : %s\n", card->numfil, card->pseudo, card->data);
        ajout_fichier(card);
        break;
    case 6:
        printf("[%u] %s : %s\n", card->numfil, card->pseudo, card->data);
        telecharge(card);
        break;
    default:
        printf("Error, invalid format...\n");
        respond(card->socket, 31, 0, 0, 0);
    }

    free_card(card);
}

void telecharge(IDCard *card) {
    printf("Sending to port :%u\n", card->nb);
    //ouvrir le fichier et vérifier qu'il a déjà été upload
    char filename[266]; // (256 = Max filename len) + len of directory name
    strcpy(filename, "servdata/");
    strcat(filename, card->data);
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror(card->data);
        respond(card->socket, 31, 0, 0, 0);
    }
    respond(card->socket, card->codereq, card->id, card->numfil, card->nb);

    //creer socket et connection en udp
    int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) {
        perror("Erreur lors de la création du socket UDP");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 
    server_addr.sin_port = htons(card->nb); 
    
    char buffer[TAILLE_MSG];
    ssize_t bytes_read = 0;
    int seq_num = 0;
    //while read (buf)
    while ((bytes_read = read(fd, buffer, TAILLE_MSG)) > 0) {
    //mettre buf dans un package udp
        char packet[TAILLE_MSG + sizeof(int)];
        memcpy(packet, &seq_num, sizeof(int));
        memcpy(packet + sizeof(int), buffer, bytes_read);
    //send package
        printf("packet n°%d\n", seq_num);
        if (sendto(udp_sock, packet, bytes_read + sizeof(int), 0, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
            perror("Erreur lors de l'envoi des données du fichier en UDP");
            exit(EXIT_FAILURE);
        }
        seq_num++;
    }
    close(udp_sock);
}

void ajout_fichier(IDCard *card) {
    //creer socket udp
    int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) {
        perror("Erreur lors de la création du socket UDP");
        return ;
    }
    //on mets 0 pour que l'ordi choisi un port dispo
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addr.sin_port = 0; 

    //bind
    if (bind(udp_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erreur lors de la liaison de la socket à un numéro de port disponible");
        return ;
    }

    //ouvrir le fichier et vérifier qu'il a pas déjà été upload
    char filename[266]; // (256 = Max filename len) + len of directory name
    strcpy(filename, "servdata/");
    strcat(filename, card->data);
    int fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 1) {
        perror(filename);
        respond(card->socket, 31, 0, 0, 0);
        close(udp_sock);
        return ;
    }

    // recupere numéro de port
    struct sockaddr_in sin;
    socklen_t lensock = sizeof(sin);
    if (getsockname(udp_sock, (struct sockaddr*)&sin, &lensock) == -1) {
        return ;
    }
    uint16_t port = ntohs(sin.sin_port);
    printf("Port dispo : %u\n", port);
    respond(card->socket, card->codereq, card->id, card->numfil, port);
    
    char packet[TAILLE_MSG + sizeof(int)];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    ssize_t bytes_received;
    while ((bytes_received = 
        recvfrom(udp_sock, packet, sizeof(packet), 0, 
        (struct sockaddr*)&client_addr, &client_addr_len))) {
        if (bytes_received < 0) {
            perror("Erreur lors de la réception du paquet UDP");
            exit(EXIT_FAILURE);
        }
        int seq_num;
        memcpy(&seq_num, packet, sizeof(int));
        char* data = packet + sizeof(int);
        write(fd, data, bytes_received - sizeof(int));
    }
    printf("Fin de l'écriture du fichier.\n");
}

void return_adress(IDCard *card)
{
    char *adress = get_room_broadcast(card);

    char message[22];
    memset(message, 0, 22);

    uint16_t env = (card->codereq & ((1 << 5) - 1)) | ((card->id & ((1 << 11) - 1)) << 5);
    env = htons(env);
    uint16_t nfil = htons(card->numfil);
    uint16_t port = htons(4321 + card->numfil);
    memcpy(message, &env, 2);
    memcpy(message+2, &nfil, 2);
    memcpy(message+4, &port, 2);
    memcpy(message + 6, adress, 16);
    free(adress);

    int ecrit = send(card->socket, message, 22, 0);
    if (ecrit <= 0)
    {
        perror("Failed to send response");
    }
}

void archive(IDCard *card)
{
    Room *room = card->memory->rooms;
    while (room != NULL)
    {

        if (card->numfil == 0 || card->numfil == room->id)
        {
            // Inverse message order from latest to newest
            int size = card->datalen == 0 ? room->length : card->datalen;
            // printf("Size : %d\n", size);
            Message *array[size];
            int n = 0;
            Message *message = room->lastMessage;
            while (message != NULL && n < size)
            {
                array[n] = message;
                // printf("n : %d -> %s\n", n, array[n]->content);
                n++;
                message = message->next;
            }

            for (int i = n - 1; i >= 0; i--)
            {
                if (array[i]->content == NULL)
                    continue;

                int message_len = strlen(array[i]->content) + 1;
                char buff[23 + message_len];

                uint16_t fil = htons(room->id);
                memcpy(buff, &fil, 2);
                memcpy(buff + 2, room->creator, 10);
                memcpy(buff + 12, array[i]->sender, 10);
                buff[22] = message_len;
                memcpy(buff + 23, array[i]->content, message_len);
                // printf("-> send %u : %s\n", message_len, array[i]->content);

                int ecrit = send(card->socket, buff, 23 + message_len, 0);
                if (ecrit <= 0)
                {
                    perror("Failed to send response");
                }
                // else printf("Sended : %d\n", ecrit);
            }
        }
        room = room->next;
    }
}

void respond(int socket, unsigned int codereq, unsigned int id, unsigned int numfil, unsigned int nb)
{
    char message[6];

    uint16_t env = (codereq & ((1 << 5) - 1)) | ((id & ((1 << 11) - 1)) << 5);
    env = htons(env);
    uint16_t nfil = htons(numfil);
    uint16_t nb_env = htons(nb);

    memcpy(message, &env, sizeof(uint16_t));
    memcpy(message+2, &nfil, sizeof(uint16_t));
    memcpy(message+4, &nb_env, sizeof(uint16_t));

    int ecrit = send(socket, message, 6, 0);
    if (ecrit <= 0)
    {
        perror("Failed to send response");
    }
}

void* broadcast(void *args)
{
    Memory *mem = (Memory *)args;

    while(1) {

    sleep(1);

    Room *room = mem->rooms;
    while (room != NULL)
    {
        if (room->broadcast != NULL &&
            (room->lastMessageBroadcasted == NULL ||
             room->lastMessageBroadcasted->prev != NULL))
        {
            int sock = socket(AF_INET6, SOCK_DGRAM, 0);

            struct sockaddr_in6 grsock;
            memset(&grsock, 0, sizeof(grsock));
            grsock.sin6_family = AF_INET6;
            inet_pton(AF_INET6, room->broadcast, &grsock.sin6_addr);
            // printf("Multicast to %s\n", room->broadcast);
            grsock.sin6_port = htons(4321+room->id);

            int ifindex = if_nametoindex("eth0");
            if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex,
                           sizeof(ifindex)))
                perror("erreur initialisation de l’interface locale");

            char buf[34];

            uint16_t env = (4 & ((1 << 5) - 1)) | ((0 & ((1 << 11) - 1)) << 5);
            env = htons(env);
            uint16_t numfil = htons(room->id);
            
            memcpy(buf, &env, 2);
            memcpy(buf + 2, &numfil, 2);

            Message *message = room->lastMessageBroadcasted == NULL ? room->lastMessage : room->lastMessageBroadcasted->prev;
            while (message != NULL)
            {
                memcpy(buf + 4, message->sender, 10);
                memcpy(buf + 14, message->content, 20);

                int s = sendto(sock, buf, 34, 0, (struct sockaddr *)&grsock,
                               sizeof(grsock));
                if (s < 0)
                    perror("error broadcast send\n");

                room->lastMessageBroadcasted = message;
                message = message->prev;
            }
        }

        room = room->next;
    }
    }

    // printf("\n!!! Broadcast done !!!\n\n");
    return NULL;
}