#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#include <string.h>


#define MAX_LINE_LENGTH 1024

#define Strequ(s1, s2) (strcmp((s1), (s2)) == 0)

static void handle_client(int fd);

int main(int argc, char *argv[]) {

  if (argc != 2) {
    fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
    return 1;
  }

  run_server(argv[1], handle_client);

  return 0;
}

void handle_client(int fd) {

  send_string(fd, "+OK Server ready\r\n");

  char user[MAX_USERNAME_SIZE];
  char pass[MAX_PASSWORD_SIZE];

  char data[MAX_LINE_LENGTH];
  char data_cpy[MAX_LINE_LENGTH];

  while (1){

    // in authentication state

    memset(&data[0], 0, sizeof(data));
    read(fd, data, MAX_LINE_LENGTH);
    strcpy(data_cpy, data);
    data_cpy[4] = '\0';

    if(Strequ(data_cpy, "USER")){
      char * s2;
      s2 = strtok(NULL, " "); // s2 stores user email

      strcpy(user, s2);

      memset(&data[0], 0, sizeof(data));

      send_string(fd, "+OK enter password\r\n");

    } else if(Strequ(data_cpy, "PASS")){
      char * s3;
      char * s4;
      s3 = strtok(data, " ");
      s4 = strtok(NULL, " ");

      strcpy(pass, s4); // store pasword

      memset(&data[0], 0, sizeof(data));

      if(is_valid_user(user, pass) == 1){
        send_string(fd, "VALID\r\n");
        memset(&data[0], 0, sizeof(data));
        break;

      } else {
        send_string(fd, "INVALID username or password\r\n");
        memset(&user[0], 0, sizeof(user));
        memset(&pass[0], 0, sizeof(pass));
        memset(&data[0], 0, sizeof(data));

      }

    } else if(Strequ(data_cpy, "QUIT")){
      send_string(fd, "+OK BYE \r\n");
      break;

    }
  }

  send_string(fd, "Now in transaction state\r\n");

  if(is_valid_user(user, pass) == 1){

    // in transaction state

    while(1){

      mail_list_t user_mail_list = load_user_mail(user);

      memset(&data[0], 0, sizeof(data));
      memset(&data_cpy[0], 0, sizeof(data_cpy));

      read(fd, data, MAX_LINE_LENGTH);

      strcpy(data_cpy, data);
      data_cpy[4] = '\0';

      if(Strequ(data_cpy, "STAT")){

        unsigned int user_mail_count = get_mail_count(user_mail_list);
        size_t user_mail_size = get_mail_list_size(user_mail_list);

        send_string(fd, "+OK %d %zu\r\n.\r\n", user_mail_count, user_mail_size);

      } else if(Strequ(data_cpy, "LIST")){

        unsigned int count = get_mail_count(user_mail_list);

        char pos = data[5];

        if(pos == '\0'){ // if user writes "LIST" without specifying a message number

        send_string(fd, "+OK %u messages\r\n", count);

        for(int i = 0; i < count; i = i + 1) {

          mail_item_t item = get_mail_item(user_mail_list, i);
          size_t message_size = get_mail_item_size(item);

          if(item != NULL){ // get_mail_item returns NULL if item is deleted
            if(count - i > 1){
              send_string(fd, "%d %zu\r\n", i + 1, message_size);
            }
            else if(count - i == 1){ // when listing the last email, end list by printing "."
            send_string(fd, "%d %zu\r\n.\r\n", i + 1, message_size);
          }
        } else if(item == NULL){
          send_string(fd, "-ERR message has been marked as deleted, or does not exist\r\n");
        }
      }
    }
    else if(pos > 0) { // if the user writes "LIST x" for a specific message number

    if(atoi((char *)&pos) - 1 < count){

      mail_item_t item2 = get_mail_item(user_mail_list, atoi((char *) &pos) - 1);
      size_t message_size = get_mail_item_size(item2);

      send_string(fd, "+OK %c %zu\r\n.\r\n", pos, message_size);

    } else if(atoi((char *)&pos) - 1 >= count){

      send_string(fd, "-ERR invalid message number\r\n");
    }
  }
      } else if(Strequ(data_cpy, "RETR")){

        if(data[5] != '\0'){ // check if RETR has message number

          char pos = data[5]; // char message position
          mail_item_t message = get_mail_item(user_mail_list, atoi((char*) &pos) - 1);

          if(message != NULL){
            // retreive message and print contents

            size_t message_size = get_mail_item_size(message);
            const char * filename = get_mail_item_filename(message); // file pointer

            FILE *file;
            size_t read_size;

            char buff[message_size];

            file = fopen(filename, "r");
            if (file) {
              while ((read_size = fread(buff, 1, sizeof(buff), file)) > 0)
              send_all(fd, buff, read_size);
              fclose(file);
            }

          } else if(message == NULL){
            send_string(fd, "-ERR message has been marked as deleted, or does not exist\r\n");
          }

        } else if(data[5] == '\0') {
          send_string(fd, "-ERR must provide message number\r\n");
        }

      } else if(Strequ(data_cpy, "DELE")){

        char pos[2];

        char * s5;
        char * s6;
        s5 = strtok(data, " ");
        s6 = strtok(NULL, " "); // position of message to delete

        strcpy(pos, s6); // message position stored as char

        mail_item_t item = get_mail_item(user_mail_list, atoi(pos) - 1); // get item

        unsigned int count = get_mail_count(user_mail_list);

        if(atoi((char * )&pos) - 1 < count) { // check valid range

        if(item == NULL){

          send_string(fd, "-ERR message %s has already been deleted, or does not exist\r\n", pos);

        } else if(item != NULL){ // get_mail_item returns null if item is already marked as deleted

          mark_mail_item_deleted(item); // mark item as deleted
          send_string(fd, "+OK message %s has been deleted\r\n", pos);
        }
      } else if (atoi((char * )&pos) - 1 >= count){

        send_string(fd, "-ERR invalid message number\r\n");

      }

      } else if(Strequ(data_cpy, "RSET")){

        reset_mail_list_deleted_flag(user_mail_list); // reset all emails that were marked as deleted
        send_string(fd, "+OK\r\n");

      } else if(Strequ(data_cpy, "NOOP")){

        send_string(fd, "+OK\r\n");

      } else if(Strequ(data_cpy, "QUIT")){

        // in update state

        destroy_mail_list(user_mail_list); // delete emails marked as deleted

        send_string(fd, "+OK BYE\r\n");

        break;
      }
    }
  }
}
