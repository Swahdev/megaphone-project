#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "memory.h"

int ajouterFichier(int id, char* line);
int envoyerFichierUDP(int port, int fd);
int telechargerFichier(int id, char* line);
