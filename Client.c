#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SIZE_BUF 1024
#define SIZE_COMMAND 8
#define SIZE_LINE 256
#define LEN_PSEUDO 10
#define PORT 2717
#define SERVER_ADDRESS "192.168.70.236"

int inscription(char *pseudo);
char *readLine(char *line, char *ret, int size, char sep);
void post_message(int usr, int fil, char *message);
int create_socket();
void demander_message(int id, int nb, int fil);
pthread_t sub(int id, int fil, char *pseudo);
void *adress_ecoute(void *arg);
uint16_t fill_header(uint16_t coderec, uint16_t id);

typedef struct InscrBuffer
{
    uint16_t coderec;
    uint16_t id;
    char pseudo[LEN_PSEUDO];
} InscrBuffer;

typedef struct MulticastData
{
    unsigned char *adresse;
    int port;
    char pseudo[LEN_PSEUDO + 1];
} MulticastData;

int l_port;

int main(int argc, char const *argv[])
{
    printf("Welcome to Megaphone ! ฅ^•ﻌ•^ฅ\n");

    if (argc < 2)
        l_port = PORT;
    else
        sscanf(argv[1], "%d", &l_port);

    char pseudo[LEN_PSEUDO + 1];
    memset(pseudo, 0, LEN_PSEUDO + 1);
    int id = -1;

    while (id == -1)
    {
        printf("Entrer votre pseudo : ");
        fgets(pseudo, LEN_PSEUDO + 1, stdin);
        id = inscription(pseudo);
    }
    pseudo[strlen(pseudo) - 1] = '\0';

    int fil = -1;
    pthread_t current_thread;

    char line[SIZE_LINE];
    while (1)
    {
        memset(line, 0, SIZE_LINE);

        if (fil == -1)
            printf("> ");
        else
            printf("[%d]> ", fil);

        fgets(line, SIZE_LINE, stdin);
        char command[SIZE_COMMAND + 1]; // +1 for \0
        char *current = readLine(line, command, SIZE_COMMAND, ' ');

        if (!strcmp(command, "/GET"))
        {
            char nb[6];
            char f[6];

            current = readLine(current, nb, 5, ' ');
            current = readLine(current, f, 5, ' ');

            int number;
            int n_fil;

            sscanf(nb, "%d", &number);
            sscanf(f, "%d", &n_fil);

            demander_message(id, number, n_fil);
        }
        else if (!strcmp(command, "/SEND"))
        {
            char f[6];
            char message[21];

            current = readLine(current, f, 5, ' ');
            current = readLine(current, message, 20, '\n');

            int n_fil;
            sscanf(f, "%d", &n_fil);

            post_message(id, n_fil, message);
        }
        else if (!strcmp(command, "/SUB"))
        {
            char f[6];
            current = readLine(current, f, 5, ' ');
            int fil;
            sscanf(f, "%d", &fil);

            sub(id, fil, pseudo);
        }
        else if (!strcmp(command, "/SEL"))
        {
            if (fil != -1)
                pthread_cancel(current_thread);

            char f[6];
            current = readLine(current, f, 5, ' ');
            sscanf(f, "%d", &fil);

            current_thread = sub(id, fil, pseudo);
        }
        else if (!strcmp(command, "/QUIT"))
        {
            if (fil != -1)
                pthread_cancel(current_thread);
            fil = -1;
        }
        else if (!strcmp(command, "/UPLD"))
        {
            ajouterFichier(id, line + 5);
        }
        else if (!strcmp(command, "/DWNLD"))
        {
            telechargerFichier(id, line + 6);
        }
        else if (fil != -1 && line[0] != '\n')
        {
            post_message(id, fil, line);
        }
    }

    return 0;
}

char *readLine(char *line, char *ret, int size, char sep)
{
    int index = 0;

    for (int i = 0;
         i < size && line[i] != sep && line[i] != '\n' && line[i] != '\0'; i++)
    {
        ret[i] = line[i];
        index++;
    }
    ret[index++] = '\0';

    return line + index;
}

int inscription(char *pseudo)
{
    if (strlen(pseudo) < 3)
    {
        printf("Error: username is too short.\n");
        return -1;
    }

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Error : Failed to create socket");
        return -1;
    }

    struct sockaddr_in adrsock;
    adrsock.sin_family = AF_INET;
    adrsock.sin_port = htons(l_port);
    if (inet_pton(AF_INET, SERVER_ADDRESS, &adrsock.sin_addr) <= 0)
    {
        perror("Error : Failed to create socket");
        return -1;
    }

    int r = connect(sock, (struct sockaddr *)&adrsock, sizeof(adrsock));
    if (r < 0)
    {
        perror("Error while binding");
        return -1;
    }
    InscrBuffer buffer;

    buffer.coderec = 1;
    buffer.id = 0;

    void *bufsend = malloc(sizeof(u_int16_t) + LEN_PSEUDO);
    memset(bufsend, 0, sizeof(u_int16_t) + LEN_PSEUDO);
    char buf[LEN_PSEUDO + 2];
    char buf2[SIZE_BUF + 1];
    memset(buf, 0, LEN_PSEUDO + 2);
    memset(buf2, 0, SIZE_BUF + 1);

    // Envoi de la demande d'inscription
    uint16_t env = (buffer.coderec & ((1 << 5) - 1)) |
                   ((buffer.id & ((1 << 11) - 1)) << 5);

    env = htons(env);

    memmove(bufsend, &env, sizeof(uint16_t));

    memcpy(buf, &env, 2);

    memcpy(bufsend + sizeof(env), pseudo, LEN_PSEUDO);

    int s = send(sock, bufsend, sizeof(u_int16_t) + LEN_PSEUDO, 0);
    if (s < 0)
    {
        perror("Error while sending");
        return -1;
    }

    // Reception du message
    int rec = recv(sock, buf2, SIZE_BUF + 1, 0);
    if (rec < 0)
    {
        perror("Error");
        return -1;
    }

    uint16_t head_rec;
    memcpy(&head_rec, buf2, 2);
    head_rec = ntohs(head_rec);
    uint16_t coderec = head_rec & ((1 << 5) - 1);
    uint16_t id = head_rec >> 5;

    if (coderec == 1)
    {
        printf("Registration successful.\n");
        printf("Your id is: %d\n", id);
        close(sock);

        return id;
    }
    else
    {
        printf("Registration failed.\n");
        close(sock);

        return -1;
    }
}

void demander_message(int id, int nb, int n_fil)
{
    // creation du header
    uint16_t head_env = fill_header(3, id);
    // remplissage du numfil
    uint16_t numfil = htons(n_fil);
    // remplissage du nb
    uint16_t nb_env = htons(nb);
    void *bufsend = malloc(sizeof(uint16_t) * 3 + sizeof(uint8_t));
    memset(bufsend, 0, sizeof(uint16_t) * 3 + sizeof(uint8_t));
    // copie du header
    memcpy(bufsend, &head_env, sizeof(uint16_t));
    // copie du numfil
    memcpy(bufsend + sizeof(uint16_t), &numfil, sizeof(uint16_t));
    // copie du nb
    memcpy(bufsend + sizeof(uint16_t) * 2, &nb_env, sizeof(uint16_t));
    // Envoi du message au serveur
    int sock = create_socket();
    int s = send(sock, bufsend, sizeof(uint16_t) * 3 + sizeof(uint8_t), 0);
    if (s < 0)
    {
        perror("Error while sending");
        exit(1);
    }
    // Reception du message
    char buf2[SIZE_BUF + 1];
    memset(buf2, 0, SIZE_BUF + 1);
    int rec = recv(sock, buf2, SIZE_BUF + 1, 0);

    if (rec < 0)
    {
        perror("Error");
        exit(1);
    }
    uint16_t head_rec;
    memcpy(&head_rec, buf2, 2);
    head_rec = ntohs(head_rec);
    uint16_t coderec = head_rec & ((1 << 5) - 1);
    uint16_t idrec = head_rec >> 5;
    uint16_t nb_rec;
    memcpy(&nb_rec, buf2 + sizeof(uint16_t) * 2, sizeof(uint16_t));
    nb_rec = ntohs(nb_rec);
    if (coderec == 3 && idrec == id)
    {
        int i = 0;
        while (i < nb_rec)
        {
            char buf3[SIZE_BUF + 1];
            memset(buf3, 0, SIZE_BUF + 1);
            rec = recv(sock, buf3, SIZE_BUF + 1, MSG_WAITALL);
            if (rec <= 0)
            {
                perror("recv error");
                return;
            }

            int shift = 0;
            while (rec != 0)
            {
                i++;
                uint16_t numfil;
                memcpy(&numfil, buf3 + shift, sizeof(uint16_t));
                numfil = ntohs(numfil);
                // unsigned int numfil = buf3[shift] * 256 + buf3[shift + 1];
                char origine[10];
                char pseudo[10];
                memcpy(origine, buf3 + shift + 2, 10);
                memcpy(pseudo, buf3 + shift + 12, 10);
                int datalen = buf3[shift + 22];
                char message[datalen];
                memcpy(message, buf3 + shift + 23, datalen);
                printf("[%u] %s : %s\n", numfil, pseudo, message);

                rec -= (datalen + 23);
                shift += (datalen + 23);
            }
        }

        close(sock);
    }
    else
    {
        printf("Message echouee.\n");
    }
    free(bufsend);
    close(sock);
}

void post_message(int id, int fil, char *message)
{
    char buf2[SIZE_BUF + 1];

    // creation du header
    uint16_t head_env = fill_header(2, id);
    // remplissage du numfil
    uint16_t numfil = htons(fil);
    // remplissage du nb
    uint16_t nb_env = 0;
    void *bufsend =
        malloc(sizeof(uint16_t) * 3 + sizeof(uint8_t) + strlen(message) + 1);
    memset(bufsend, 0,
           sizeof(uint16_t) * 3 + sizeof(uint8_t) + strlen(message) + 1);
    // copie du header
    memcpy(bufsend, &head_env, sizeof(uint16_t));
    // copie du numfil
    memcpy(bufsend + sizeof(uint16_t), &numfil, sizeof(uint16_t));
    // copie du nb
    memcpy(bufsend + sizeof(uint16_t) * 2, &nb_env, sizeof(uint16_t));
    // copie du datalen
    uint8_t datalen = strlen(message) + 1;
    memcpy(bufsend + sizeof(uint16_t) * 3, &datalen, sizeof(uint8_t));
    memcpy(bufsend + sizeof(uint16_t) * 3 + sizeof(uint8_t), message,
           strlen(message) + 1);
    // Envoi du message au serveur
    int sock = create_socket();

    int s = send(sock, bufsend, strlen(message) + 8, 0);
    if (s < 0)
    {
        perror("Error while sending");
        exit(1);
    }

    // Reception du message

    int rec = recv(sock, buf2, SIZE_BUF + 1, 0);

    if (rec < 0)
    {
        perror("Error c la 1");
        exit(1);
    }
    close(sock);
}

// Fonction qui cree la socket

int create_socket()
{
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Error : Failed to create socket");
        exit(1);
    }

    struct sockaddr_in adrsock;
    adrsock.sin_family = AF_INET;
    adrsock.sin_port = htons(l_port);
    if (inet_pton(AF_INET, SERVER_ADDRESS, &adrsock.sin_addr) <= 0)
    {
        perror("Error : Failed to create socket");
        exit(1);
    }

    int r = connect(sock, (struct sockaddr *)&adrsock, sizeof(adrsock));
    if (r < 0)
    {
        perror("Error while binding");
        exit(1);
    }
    return sock;
}

void *adress_ecoute(void *arg)
{
    MulticastData *data = (MulticastData *)arg;
    char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    char adress[40];
    char user_pseudo[LEN_PSEUDO + 1];
    memcpy(user_pseudo, data->pseudo, LEN_PSEUDO + 1);
    memset(adress, 0, 40);
    int shift = 0;
    for (int i = 0; i < 16; i++)
    {
        if (i != 0 && i % 2 == 0)
            adress[2 * i + shift++] = ':';

        adress[2 * i + shift] = hex[data->adresse[i] / 16];
        adress[2 * i + 1 + shift] = hex[data->adresse[i] % 16];
    }
    adress[39] = '\0';

    // adress[6] = hex[data->adresse[15]];
    unsigned int port = data->port;
    printf("Listening on : %s:%u\n", adress, port);
    free(data);

    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("Error : Failed to create socket");
        exit(1);
        return NULL;
    }

    int ok = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &ok, sizeof(ok)) < 0)
    {
        perror("echec de SO_REUSEADDR");
        close(sock);
        return NULL;
    }

    struct sockaddr_in6 grsock;
    memset(&grsock, 0, sizeof(grsock));
    grsock.sin6_family = AF_INET6;
    grsock.sin6_addr = in6addr_any;
    grsock.sin6_port = htons(port);

    if (bind(sock, (struct sockaddr *)&grsock, sizeof(grsock)))
    {
        perror("erreur bind");
        close(sock);
        return NULL;
    }

    struct ipv6_mreq group;
    inet_pton(AF_INET6, adress, &group.ipv6mr_multiaddr.s6_addr);
    group.ipv6mr_interface = if_nametoindex("eth0");

    if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &group, sizeof(group)) <
        0)
    {
        perror("setsockopt");
        return NULL;
    }

    char buf[1024];

    while (1)
    {
        if (recv(sock, buf, SIZE_BUF + 1, 0) < 0)
        {
            printf("Error while reading multicast");
        }

        uint16_t head_rec;
        memcpy(&head_rec, buf, 2);
        head_rec = ntohs(head_rec);
        uint16_t codereq = head_rec & ((1 << 5) - 1);

        if (codereq != 4)
            printf("Error with codereq\n");

        uint16_t numfil;
        memcpy(&numfil, buf + 2, 2);
        numfil = ntohs(numfil);

        char pseudo[10];
        memcpy(pseudo, buf + 4, 10);

        if (!strcmp(user_pseudo, pseudo))
            continue;

        char message[20];
        memcpy(message, buf + 14, 20);

        printf("\n[%d] %s : %s[%d]> ", numfil, pseudo, message, numfil);
        fflush(stdout);
    }
}

pthread_t sub(int id, int fil, char *pseudo)
{
    // creation du header
    uint16_t head_env = fill_header(4, id);
    // remplissage du numfil
    uint16_t numfil = htons(fil);
    void *bufsend = malloc(sizeof(uint16_t) * 3 + sizeof(uint8_t));
    memset(bufsend, 0, sizeof(uint16_t) * 3 + sizeof(uint8_t));
    // copie du header
    memcpy(bufsend, &head_env, sizeof(uint16_t));
    // copie du numfil
    memcpy(bufsend + sizeof(uint16_t), &numfil, sizeof(uint16_t));
    // Envoi du message au serveur
    int sock = create_socket();

    int s = send(sock, bufsend, sizeof(uint16_t) * 3 + sizeof(uint8_t), 0);
    if (s < 0)
    {
        perror("Error while sending");
        exit(1);
    }
    // Reception du message
    unsigned char buf2[SIZE_BUF + 1];
    memset(buf2, 0, SIZE_BUF + 1);
    int rec = recv(sock, buf2, SIZE_BUF + 1, 0);
    if (rec < 0)
    {
        perror("Error recv");
        exit(1);
    }

    uint16_t head_rec;
    memcpy(&head_rec, buf2, 2);
    head_rec = ntohs(head_rec);
    uint16_t coderec = head_rec & ((1 << 5) - 1);
    uint16_t num_fil;
    memcpy(&num_fil, buf2 + 2, 2);
    num_fil = ntohs(num_fil);

    if (coderec != 4 || num_fil != fil)
    {
        perror("Error coderec");
        return 0;
    }
    MulticastData *max = malloc(sizeof(struct MulticastData));
    max->port = buf2[4] * 256 + buf2[5];
    max->adresse = malloc(16);
    memset(max->adresse, 0, 16);
    memcpy(max->adresse, buf2 + 6, 16);
    memcpy(max->pseudo, pseudo, LEN_PSEUDO + 1);

    // Creation du thread d'ecoute

    pthread_t multicast_thread;
    if (pthread_create(&multicast_thread, NULL, adress_ecoute, max) == -1)
    {
        perror("Error : Failed to create broadcast thread");
    }
    close(sock);

    return multicast_thread;
}

uint16_t fill_header(uint16_t coderec, uint16_t id)
{
    uint16_t env = (coderec & ((1 << 5) - 1)) | ((id & ((1 << 11) - 1)) << 5);
    env = htons(env);
    return env;
}