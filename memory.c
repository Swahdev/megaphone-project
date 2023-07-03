#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"

pthread_mutex_t verrou = PTHREAD_MUTEX_INITIALIZER;

char* get_pseudo(IDCard* card) {

    User* user = card->memory->users;

    while(user != NULL) {
        if(user->id == card->id) 
            return user->pseudo;
        user = user->next;
    }

    return NULL;
}

unsigned int add_user(IDCard* card) {

    User* user = card->memory->users;
    unsigned int prevId = 0;

    while(user != NULL) {
	    if(strcmp(user->pseudo, card->pseudo) == 0)
            return 0;
        if(user->id > prevId) prevId = user->id;

        if(user->next == NULL) break;
        user = user->next;
    }

    User* newUser = malloc(sizeof(User));
    newUser->id = prevId + 1;
    memcpy(newUser->pseudo, card->pseudo, 10);

    pthread_mutex_lock(&verrou);

    if(user == NULL) card->memory->users = newUser;
    else user->next = newUser;

    pthread_mutex_unlock(&verrou);

    return prevId + 1;
}

int add_message(Room* room, unsigned int len, char* message, char* sender) {
    Message* newMessage = malloc(sizeof(Message));

    newMessage->sender = sender; // direct link to User->pseudo, no need to copy
    newMessage->content = malloc(len + 1);
    memcpy(newMessage->content, message, len);
    newMessage->content[len] = '\0';
    pthread_mutex_lock(&verrou);

    //if(room->lastMessage == NULL) room->lastMessageBroadcasted = newMessage;

    newMessage->prev = NULL;
    newMessage->next = room->lastMessage;

    if(room->lastMessage != NULL) 
        room->lastMessage->prev = newMessage;
    room->lastMessage = newMessage;
    room->length++;

    pthread_mutex_unlock(&verrou);
    return 0;
}

Room* createRoom(IDCard* card, Room* room) {
    Room* newRoom = malloc(sizeof(Room));
    newRoom->id = card->numfil;
    newRoom->creator = card->pseudo;

    pthread_mutex_lock(&verrou);

    if(room == NULL) card->memory->rooms = newRoom;
    else room->next = newRoom;

    pthread_mutex_unlock(&verrou);

    return newRoom;
}

int post(IDCard* card) {

    Room* room = card->memory->rooms;

    while(room != NULL) {
        if(room->id == card->numfil) 
            return add_message(room, card->datalen, card->data, card->pseudo);

        if(room->next == NULL) break;
        room = room->next;
    }

    Room* newRoom = createRoom(card, room);

    return add_message(newRoom, card->datalen, card->data, card->pseudo);
}

int get_message_number(IDCard* card) {

    Room* room = card->memory->rooms;

    unsigned int size = 0;
    unsigned int nb_fil = 0;

    while(room != NULL) {
        if(card->numfil == 0 || card->numfil == room->id) {
            size += card->nb == 0 || card->nb > room->length ? room->length : card->nb;
            nb_fil++;
        }
        room = room->next;
    }
    card->datalen = size;
    return nb_fil;
}

void create_multicast_adress(Room* room) {
    char* adress = malloc(40);

    char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    memcpy(adress, "ff12:0000:0000:0000:0000:0000:0000:0000", 40);
    adress[37] = hex[room->id / 16];
    adress[38] = hex[room->id % 16];
    // char sup[4];
    // memset(sup, 0, 4);
    // for(int i = 0; i < 4; i++) {
    //     int r = val % 16;
    //     val /= 16;
    //     sup[4-i] = hex[r];

    //     if(val == 0) break;
    // }
    // int v = 0;
    // for(int i = 0; i < 4; i++) {
    //     if(sup[i] != 0) {
    //         adress[6+v] = sup[i];
    //         v++;
    //     }
    // }
    // adress[6+v] = '\0';
    // adress[6] = hex[room->id];
    room->broadcast = adress;
}

char* get_room_broadcast(IDCard* card) {
    Room* room = card->memory->rooms;

    while(room != NULL) {
        if(room->id == card->numfil) {
            if(room->broadcast == NULL) 
                create_multicast_adress(room);

            char* result = malloc(16);
            memset(result, 0, 16);
            result[0] = 255;
            result[1] = 18;
            result[15] = room->id;
            return result;
        }

        if(room->next == NULL) break;
        room = room->next;
    }

    Room* newRoom = createRoom(card, room);
    create_multicast_adress(newRoom);
    char* result = malloc(16);
    memset(result, 0, 16);
    result[0] = 255;
    result[1] = 18;
    result[15] = newRoom->id;
    return result;
}