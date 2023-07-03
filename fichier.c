#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utils.h"

#define SIZE_BUF 1024
#define LEN_PSEUDO 10
#define PORT 2717
#define SERVER_ADDRESS "127.0.0.1"
#define TAILLE_MSG 255

int envoyerFichierUDP(int port, int fd) {
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
    server_addr.sin_port = htons(port); 
    
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
        printf("%d\n", seq_num);
        if (sendto(udp_sock, packet, bytes_read + sizeof(int), 0, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
            perror("Erreur lors de l'envoi des données du fichier en UDP");
            exit(EXIT_FAILURE);
        }
        seq_num++;
    }
    close(udp_sock);
    return (1);
}

int ajouterFichier(int id, char* line) {// /UPLD n_fil a.txt
    int i = 0;
    line[strlen(line) - 1] = '\0';
    while (isspace(line[i]))
        i++;
    int n_fil = atoi(line+i); 
    if(!n_fil){
        printf("/UPLD numeroDuFil nomDuFichier\n");//234
        return 0; 
    }
    while (isdigit(line[i]))
    {
        i++;
    }
    while (isspace(line[i]))
        i++;
    if(line[i] == '\0'){
        printf("/UPLD numeroDuFil nomDuFichier\n");
        return 0;
    }
    int fd = open(line+i, O_RDONLY);

    if(fd<0){
        perror(line + i);
        return 0;
    }
    //trouver le dernier slash si chemin/nomDuFichier
    int slash = i;

    while(line[i]){
        if(line[i]== '/'){// /test.txt
            slash = i+1;
        }
        i++;
    }
    
    char *nomDuFichier = line + slash;
  
    // Envoi de la requête d'ajout de fichier au serveur
    char msg[TAILLE_MSG];
    size_t len = strlen(nomDuFichier);
    
    memset(msg, 0, TAILLE_MSG);
    msg[0] = (5 << 3) + id / 256;
    msg[1] = id % 256;
    msg[2] = n_fil / 256;
    msg[3] = n_fil % 256;
    //nb est 0
    msg[6] = len;
    memcpy(msg + 7, nomDuFichier, len);
    int tcp_sock = create_socket();
    if (send(tcp_sock, msg, TAILLE_MSG, 0) != TAILLE_MSG)
        return EXIT_FAILURE;

    // Réception de la réponse du serveur avec le numéro de port
    char reponse[TAILLE_MSG];
    memset(reponse, 0, TAILLE_MSG);
    recv(tcp_sock, reponse, TAILLE_MSG, 0);
    uint8_t   code = reponse[0] >> 3;
    printf("Code de réponse : %u\n", code);
    if (code == 31) {
        printf("Server error\n");
        return 0;
    }
    u_int16_t numPort = (reponse[4] << 8) + reponse[5];
    printf("Le serveur écoute sur le port : %u\n", numPort);

    // Envoi des données du fichier au serveur en utilisant le protocole UDP
    envoyerFichierUDP(numPort, fd);

    printf("Le fichier a été envoyé avec succès.\n");
    return 1;
}

int telechargerFichier(int id, char* line) {
    int i = 0;
    line[strlen(line) - 1] = '\0';
    while (isspace(line[i]))
        i++;
    int n_fil = atoi(line+i); 
    if(!n_fil){
        printf("/DWLD numeroDuFil nomDuFichier");//234
        return 0; 
    }
    while (isdigit(line[i]))
    {
        i++;
    }
    while (isspace(line[i]))
        i++;
    if(line[i] == '\0'){
        printf("/DWLD numeroDuFil nomDuFichier");
        return 0;
    }
    char *nomDuFichier = line + i;
    
    //creer socket udp
    int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) {
        perror("Erreur lors de la création du socket UDP");
        return -1;
    }
    //on mets 0 pour que l'ordi choisi un port dispo
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 

    //bind
    if (bind(udp_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erreur lors de la liaison de la socket à un numéro de port disponible");
        return -1;
    }

    // recupere numéro de port
    struct sockaddr_in sin;
    socklen_t lensock = sizeof(sin);
    if (getsockname(udp_sock, (struct sockaddr*)&sin, &lensock) == -1) {
        return -1;
    }
    u_int16_t port = sin.sin_port;
    printf("Port dispo : %u\n", port);
    char msg[TAILLE_MSG];
    size_t len = strlen(nomDuFichier);
    //envoie la requete au serveur 
    //j'en suis pas sure les valeurs de paquet
    memset(msg, 0, 7);
    msg[0] = (6 << 3) + id / 256;
    msg[1] = id % 256;
    msg[2] = n_fil / 256;
    msg[3] = n_fil % 256;
    msg[4] = port / 256;
    msg[5] = port % 256; // mod does not work ?
    printf("%u+%u : %u\n", (u_int16_t)(msg[4] * 256), (u_int16_t)msg[5], port); 
    msg[6] = len;
    memcpy(msg + 7, nomDuFichier, len);
    int tcp_sock = create_socket();
    if (send(tcp_sock, msg, TAILLE_MSG, 0) != TAILLE_MSG){
        close(tcp_sock);
        return EXIT_FAILURE;
    }
    char reponse[TAILLE_MSG];
    memset(reponse, 0, TAILLE_MSG);
    if (recv(tcp_sock, reponse, TAILLE_MSG, 0) < 0){
        close(tcp_sock);
        return EXIT_FAILURE;
    }
    close(tcp_sock);
    uint8_t   code = reponse[0] >> 3;
    printf("Code de réponse : %u\n", code);
    if (code == 31) {
        printf("Server error\n");
        return 0;
    }
    // ouvrir le fichier à remplir des données téléchargées
    int fd = open(nomDuFichier, O_TRUNC | O_WRONLY | O_CREAT, 0666);
    if (fd < 1) {
        perror(nomDuFichier);
        close(udp_sock);
        return (0);
    }

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
    printf("Fichier bien téléchargé !\n");
    // Fermeture du socket UDP
    close(udp_sock);
    return (1);
}
