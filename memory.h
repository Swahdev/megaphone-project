#include <pthread.h>
#include <stdint.h>

typedef struct Message Message;
struct Message {

    char* sender;
    char* content;

    Message* prev;
    Message* next;

};

typedef struct Room {

    char* creator;

    unsigned int id;
    unsigned int length;
    Message* lastMessage;

    char* broadcast;
    Message* lastMessageBroadcasted;

    struct Room* next;

} Room;

typedef struct User{

    unsigned int id;
    char pseudo[10];

    struct User* next;

} User;

typedef struct {
    Room* rooms;

    User* users;

} Memory;

typedef struct {
    Memory* memory;

    uint16_t codereq;
    uint16_t id;
    uint16_t numfil;
    uint16_t nb;

    unsigned int datalen;
    char* data;

    int socket;
    char* pseudo;
} IDCard;

char* get_pseudo(IDCard* card);
unsigned int add_user(IDCard* card);
int post(IDCard* card);
int get_message_number(IDCard* card);
char* get_room_broadcast(IDCard* card);