/*
 * echoserverts.c - A concurrent echo server using threads
 * and a message buffer.
 */
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <string.h>
#include <string>

using namespace std;

/* Simplifies calls to bind(), connect(), and accept() */
typedef struct sockaddr SA;

/* Max text line length */
#define MAXLINE 8192

/* Second argument to listen() */
#define LISTENQ 1024

// We will use this as a simple circular buffer of incoming messages.
char message_buf[20][50];

// This is an index into the message buffer.
int msgi = 0;
int room_count = 0;//keep tracking the current room number.
int user_count = 0;//keep tracking the current user number.

typedef struct {
  char username[10];//nickname
  char joined_room[10];//the room this user has joined.
  int clientfd;//a unique socket used to send message to the user.
}User;

typedef struct {
  vector<User> users;//a user list stored every user in this room.
  char id[10];//room name.
  int numUsers;//the number of users in this room.
}Room;

User* Users;
Room* Rooms;

// A lock for the message buffer.
pthread_mutex_t lock;

// Initialize the message buffer to empty strings.
void init_message_buf() {
  int i;
  for (i = 0; i < 20; i++) {
    strcpy(message_buf[i], "");
  }
}

void init_rooms(){
  Rooms = (Room* ) malloc(100 * sizeof(Room));
}

void init_users(){
  Users = (User* ) malloc(100 * sizeof(User));
}

// This function adds a message that was received to the message buffer.
// Notice the lock around the message buffer.
void add_message(char *buf) {
  pthread_mutex_lock(&lock);
  strncpy(message_buf[msgi % 20], buf, 50);
  int len = strlen(message_buf[msgi % 20]);
  message_buf[msgi % 20][len] = '\0';
  msgi++;
  pthread_mutex_unlock(&lock);
}

// Destructively modify string to be upper case
void upper_case(char *s) {
  while (*s) {
    *s = toupper(*s);
    s++;
  }
}

// A wrapper around recv to simplify calls.
int receive_message(int connfd, char *message) {
  return recv(connfd, message, MAXLINE, 0);
}

// A wrapper around send to simplify calls.
int send_message(int connfd, char *message) {
  return send(connfd, message, strlen(message), 0);
}

// A predicate function to test incoming message.
int is_list_message(char *message) { 
  return strncmp(message, "-", 1) == 0;
}

// A function determine the incoming command.
int is_cmd_rooms(char *message) {
  return strncmp(message, "\\ROOMS", 5) == 0;
}

// A function determine the incoming command.
int is_cmd_join(char *message) {
  return strncmp(message, "\\JOIN", 5) == 0;
}

// A function determine the incoming command.
int is_cmd_leave(char *message) {
  return strncmp(message, "\\LEAVE", 6) == 0;
}

// A function determine the incoming command.
int is_cmd_who(char *message) {
  return strncmp(message, "\\WHO", 4) == 0;
}

// A function determine the incoming command.
int is_cmd_help(char *message) {
  return strncmp(message, "\\HELP", 5) == 0;
}

// A function determine the incoming command.
int is_send_message(char *message) {
  char *m = strdup(message);
  char *token = strtok(m, " ");
  for(int i = 0; i < user_count; ++i){
    char nickname[10] = "\\";
    strcat(nickname, Users[i].username);
    if(strcmp(nickname, token) == 0){
      return 1;
    }
  }
  return 0;
}

// A function determine the incoming command.
int is_group_message(char *message) {
  return strncmp(message, "\\GROUP", 6) == 0;
}

// A function determine the incoming command.
int is_wrong_cmd(char *message) {
  if(strncmp(message, "\\", 1) == 0){
    if(is_cmd_rooms(message) || is_cmd_join(message) 
      || is_cmd_help(message) || is_cmd_leave(message) || is_cmd_who(message)
      || strncmp(message, "\\X23HJ", 6) == 0){
      return 0;
    }
    return 1;  
  }
  return 0;
}

int send_list_message(int connfd) {
  char message[20 * 50] = "";
  for (int i = 0; i < 20; i++) {
    if (strcmp(message_buf[i], "") == 0) break;
    strcat(message, message_buf[i]);
    strcat(message, ",");
  }

  // End the message with a newline and empty. This will ensure that the
  // bytes are sent out on the wire. Otherwise it will wait for further
  // output bytes.
  strcat(message, "\n\0");
  printf("Sending: %s", message);

  return send_message(connfd, message);
}

int send_echo_message(int connfd, char *message) {
  upper_case(message);
  add_message(message);
  return send_message(connfd, message);
}

//list all rooms
int list_rooms(int connfd){
  char list[30] = "";
  for(int i = 0; i < room_count; ++i){
    char temp[30] = "";
    sprintf(temp, "%s", Rooms[i].id);
    strcat(list, temp);
  }
  strcat(list, "\n\0");
  printf("rooms: %s", list);
  return send_message(connfd, list);
}

//join the room with nickname and room name
int join_room(int connfd, char *message) {
  char *m = strdup(message);
  char *token = strtok(m, " ");
  token = strtok(NULL, " ");//get the username
  char *username = strdup(token);
  token = strtok(NULL, " ");//get the roomname
  char *roomname = strdup(token);
  
  //add user
  pthread_mutex_lock(&lock);
  strcpy(Users[user_count].username, username);
  strcpy(Users[user_count].joined_room, roomname);
  Users[user_count].clientfd = connfd;
  pthread_mutex_unlock(&lock);

  //find room in the existing room.
  for(int i = 0; i < room_count; ++i){
    //pthread_mutex_lock(&lock);
    if(strcmp(Rooms[i].id, roomname) == 0){
      Rooms[i].users.push_back(Users[user_count]);
      Rooms[i].numUsers++;
      room_count++;
      user_count++;
      return send_message(connfd, roomname);
    }
    //pthread_mutex_unlock(&lock);
  }
  //Room not found.
  //pthread_mutex_lock(&lock);
  strcpy(Rooms[room_count].id, roomname);
  Rooms[room_count].users.push_back(Users[user_count]);
  Rooms[room_count].numUsers++;
  room_count++;
  user_count++;
  //pthread_mutex_unlock(&lock);

  return send_message(connfd, roomname);
}

//leave the room the user is currently in.
int leave_room(int connfd) {
  int userIndex = -1;
  int roomIndex = 0;
  //find the user with fd
  for(int i = 0; i < user_count; ++i){
    if(Users[i].clientfd == connfd){
      userIndex = i;
      break;
    }
  }
  //find the room joined by the user
  for(int i = 0; i < room_count; ++i){
    if(strcmp(Users[userIndex].joined_room, Rooms[i].id) == 0){
      roomIndex = i;
      break;
    }
  }
  //remove the user from the room
  for(int i = 0; i < Rooms[roomIndex].numUsers; ++i){
    //pthread_mutex_lock(&lock);
    if(strcmp(Users[userIndex].username, Rooms[roomIndex].users.at(i).username) == 0){
      Rooms[roomIndex].users.erase(Rooms[roomIndex].users.begin() + i);
      Rooms[roomIndex].numUsers--;
    }
    //pthread_mutex_unlock(&lock);
  }
  char leave_message[8] = "GOODBYE";
  return send_message(connfd, leave_message);
}

// List all users in the room the user is currently in.
int list_users(int connfd, char *message) {
  int roomIndex = 0;
  int userIndex = -1;
   //find the user with special fd
  for(int i = 0; i < user_count; ++i){
    if(Users[i].clientfd == connfd){
      userIndex = i;
      break;
    }
  }
   //find the room joined by the user
  for(int i = 0; i < room_count; ++i){
    if(strcmp(Users[userIndex].joined_room, Rooms[i].id) == 0){
      roomIndex = i;
      break;
    }
  }
  char user_list[100] = "";
  //list all users' nickname in that specific room
  for(int i = 0; i < Rooms[roomIndex].numUsers; ++i){
    char temp[50] = "";
    strcpy(temp, Rooms[roomIndex].users.at(i).username);
    strcat(user_list, temp);
    strcat(user_list, " ");
  }
  strcat(user_list, "\n\0");
  send_message(connfd, user_list);
}

// list all the commands.
int list_help(int connfd) {
  char help[150] = "JOIN nickname room\n ROOMS\n LEAVE\n WHO\n nickname message\n send broadcast message\n send message to all users\n\0";
  send_message(connfd, help);
}

// Send message to a user specified by username.
int send_user_message(int connfd, char* message){
  char *m = strdup(message);
  char *token = strtok(m, " ");
  int userIndex = -1;
  for(int i = 0; i < user_count; ++i){
    char nickname[10] = "\\";
    strcat(nickname, Users[i].username);
    if(strcmp(nickname, token) == 0){
      userIndex = i;
      break;
    }
  }
  if(userIndex == -1){
    send_message(connfd, "No such user");
  }
  token = strtok(NULL, " ");
  pthread_mutex_lock(&lock);
  char* talk_message = token;
  send_message(Users[userIndex].clientfd, talk_message);
  pthread_mutex_unlock(&lock);
  return send_echo_message(connfd, token);;
}

// send the message if the command is not recognized.
int send_wrong_message(int connfd, char* message){
  char temp[50] = "";
  strcpy(temp, message);
  char temp1[2] = "\"";
  strcat(temp1, temp);
  strcat(temp1, "\"");
  strcat(temp1, " command not recognized.\n\0");
  send_message(connfd, temp1);
}

//send the message to every user in the room
int send_broadcast_message(int connfd, char* message){
  int roomIndex = 0;
  int userIndex = -1;
  for(int i = 0; i < user_count; ++i){
    if(Users[i].clientfd == connfd){
      userIndex = i;
      break;
    }
  }
  char broadcast_message[100] = "";
  char nickname[10];
  strcpy(nickname, Users[userIndex].username);
  strcpy(broadcast_message, nickname);
  strcat(broadcast_message, ": ");
  strcat(broadcast_message, message);
  strcat(broadcast_message, "\n");
   //find the room joined by the user
  for(int i = 0; i < room_count; ++i){
    if(strcmp(Users[userIndex].joined_room, Rooms[i].id) == 0){
      roomIndex = i;
      break;
    }
  }
  for(int i = 0; i < Rooms[roomIndex].numUsers; ++i){
    pthread_mutex_lock(&lock);
    send_message(Rooms[roomIndex].users.at(i).clientfd, broadcast_message);
    pthread_mutex_unlock(&lock);
  }
  return 1;
}

//send message to all users
int send_group_message(int connfd, char* message){
  //find the user with fd
  int userIndex = -1;
  for(int i = 0; i < user_count; ++i){
    if(Users[i].clientfd == connfd){
      userIndex = i;
      break;
    }
  }
  char *m = strdup(message);
  char group_message[50] = "group message: ";
  char *token = strtok(m, " ");
  token = strtok(NULL, " ");
  strcat(group_message, token);
  for(int i = 0; i < user_count; ++i){
    pthread_mutex_lock(&lock);
    send_message(Users[i].clientfd, group_message);
    pthread_mutex_unlock(&lock);
  }
  return 1;
}

int process_message(int connfd, char *message) {
  if (is_list_message(message)) {
    printf("Server responding with list response.\n");
    return send_list_message(connfd);
  } 
  else if(is_cmd_rooms(message)) {
    list_rooms(connfd);
  }
  else if(is_cmd_join(message)) {
    join_room(connfd, message);
  }
  else if(is_cmd_leave(message)) {
    leave_room(connfd);
  }
  else if(is_cmd_who(message)) {
    list_users(connfd, message);
  }
  else if(is_cmd_help(message)) {
    list_help(connfd);
  }
  else if(is_send_message(message)) {
    send_user_message(connfd, message);
  }
  else if(strncmp(message, "\\X23HJ", 6) == 0){
    send_message(connfd, "Connected");
  }
  else if(is_group_message(message)) {
    send_group_message(connfd, message);
  }
  else if(is_wrong_cmd(message)) {
    send_wrong_message(connfd, message);
  }
  else {
    printf("Server responding with broadcast message.\n");
    return send_broadcast_message(connfd, message);
  }
}

// The main function that each thread will execute.
void echo(int connfd) {
  size_t n;

  // Holds the received message.
  char message[MAXLINE];

  while ((n = receive_message(connfd, message)) > 0) {
    message[n] = '\0';  // null terminate message (for string operations)
    printf("Server received message %s (%d bytes)\n", message, (int)n);
    n = process_message(connfd, message);
  }
}

// Helper function to establish an open listening socket on given port.
int open_listenfd(int port) {
  int listenfd;    // the listening file descriptor.
  int optval = 1;  //
  struct sockaddr_in serveraddr;

  /* Create a socket descriptor */
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

  /* Eliminates "Address already in use" error from bind */
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
                 sizeof(int)) < 0)
    return -1;

  /* Listenfd will be an endpoint for all requests to port
     on any IP address for this host */
  bzero((char *)&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)port);
  if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0) return -1;

  /* Make it a listening socket ready to accept connection requests */
  if (listen(listenfd, LISTENQ) < 0) return -1;
  return listenfd;
}

// thread function prototype as we have a forward reference in main.
void *thread(void *vargp);

int main(int argc, char **argv) {
  // Check the program arguments and print usage if necessary.
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }

  // initialize message buffer.
  init_message_buf();
  init_rooms();
  init_users();

  // Initialize the message buffer lock.
  pthread_mutex_init(&lock, NULL);

  // The port number for this server.
  int port = atoi(argv[1]);

  // The listening file descriptor.
  int listenfd = open_listenfd(port);

  // The main server loop - runs forever...
  while (1) {
    // The connection file descriptor.
    int *connfdp = (int *)malloc(sizeof(int));

    // The client's IP address information.
    struct sockaddr_in clientaddr;

    // Wait for incoming connections.
    socklen_t clientlen = sizeof(struct sockaddr_in);
    *connfdp = accept(listenfd, (SA *)&clientaddr, &clientlen);

    /* determine the domain name and IP address of the client */
    struct hostent *hp =
        gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
                      sizeof(clientaddr.sin_addr.s_addr), AF_INET);

    // The server IP address information.
    char *haddrp = inet_ntoa(clientaddr.sin_addr);

    // The client's port number.
    unsigned short client_port = ntohs(clientaddr.sin_port);

    printf("server connected to %s (%s), port %u\n", hp->h_name, haddrp,
           client_port);

    // Create a new thread to handle the connection.
    pthread_t tid;
    pthread_create(&tid, NULL, thread, connfdp);
  }
}

/* thread routine */
void *thread(void *vargp) {
  // Grab the connection file descriptor.
  int connfd = *((int *)vargp);
  // Detach the thread to self reap.
  pthread_detach(pthread_self());
  // Free the incoming argument - allocated in the main thread.
  free(vargp);
  // Handle the echo client requests.
  echo(connfd);
  printf("client disconnected.\n");
  // Don't forget to close the connection!
  close(connfd);
  return NULL;
}
