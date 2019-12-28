#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

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
    //// Variable Section////
    
    //declare rcpt list
    user_list_t rcptlist = create_user_list();
    
    //incoming buff
    char buff[MAX_LINE_LENGTH];
    
    //copy of buff
    char cpy[MAX_LINE_LENGTH];
    
    //ordering booleans
    int acceptHELO = 1;
    int acceptMAIL = 0;
    int acceptRCPT = 0;
    int acceptDATA = 0;
    
    //format checking vars
    char section[MAX_LINE_LENGTH];
    int cond1 = 0;  //front part of the format
    int cond2 = 0;  //back part of the format
    
    //others
    int checksendstring = 0;
    
    //// End of Variable Section ////
    
    //welcoming message
    checksendstring = send_string(fd, "220 HELLO\r\n");
    if (checksendstring < 0){
        return;
    }
    
    
    //keep listening and operating till QUIT command
    while(1){
        memset(&buff[0], 0, sizeof(buff));
        memset(&cpy[0], 0, sizeof(cpy));
        net_buffer_t nbnb = nb_create(fd, MAX_LINE_LENGTH);
        int line_length = nb_read_line(nbnb, buff);
        strcpy(cpy, buff);
        buff[4] = '\0';
        
        //hehe
        if (!Strequ(buff,"EHLO")){
            if (!Strequ(buff,"RSET")){
                if (!Strequ(buff,"VRFY")){
                    if (!Strequ(buff,"EXPN")){
                        if (!Strequ(buff,"HELP")){
                            if (!Strequ(buff,"HELO")){
                                if(!Strequ(buff,"MAIL")){
                                    if (!Strequ(buff,"RCPT")){
                                        if (!Strequ(buff,"DATA")){
                                            if(!Strequ(buff,"NOOP")){
                                                if (!Strequ(buff,"QUIT")){
                                                    checksendstring = send_string(fd,"500 Command Not Recognized \r\n");
                                                    if (checksendstring < 0){
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        if(line_length >MAX_LINE_LENGTH){
            checksendstring = send_string(fd,"552 exceed line limit \r\n");
            if (checksendstring < 0){
                break;
            }
            continue;
        }
        // Properly closes connection if read (or nb_read_line) returns <= 0
        if(line_length <= 0){
            break;
        }
        
        
        
        if (Strequ(buff, "QUIT")){
            checksendstring = send_string(fd,"221 Ok\r\n");
            if (checksendstring < 0){
                break;
            }
            acceptHELO = 1;
            acceptMAIL = 0;
            acceptRCPT = 0;
            acceptDATA = 0;
            break;
        }
        else if (Strequ(buff,"HELO") && acceptHELO){
            checksendstring = send_string(fd,"250 Ok\r\n");
            if (checksendstring < 0){
                break;
            }
            acceptMAIL = 1;
            acceptHELO = 0;
        }
        else if (Strequ(buff,"HELO") && (acceptHELO == 0)){
            
            checksendstring = send_string(fd,"503 command out of sequence\r\n");
            if (checksendstring < 0){
                break;
            }
        }
        else if (Strequ(buff,"MAIL") && acceptMAIL){
            cond1 = 0;
            cond2 = 0;
            
            //create the list of recipients
            rcptlist = create_user_list();
            
            memset(section, '\0', sizeof(section));
            int len = (int) strlen(cpy);
            strncpy(section, cpy + 4, 7);
            if (strcmp(section, " FROM:<") == 0){
                cond1 = 1;
            }
            memset(section, '\0', sizeof(section));
            strncpy(section, cpy + len - 3, 3);
            if (strcmp(section, ">\r\n") == 0){
                cond2 = 1;
            }
            if (cond1 && cond2){
                checksendstring = send_string(fd,"250 Ok\r\n");
                if (checksendstring < 0){
                    break;
                }
                
                acceptRCPT = 1;
                acceptMAIL = 0;
                memset(section, '\0', sizeof(section));
            }
            else if (cond1 == 0 || cond2 == 0){
                checksendstring = send_string(fd,"553 MAIL Command valid, but syntax error\r\n");
                if (checksendstring < 0){
                    break;
                }
            }
        }
        else if (Strequ(buff,"MAIL") && (acceptMAIL == 0)){
            checksendstring = send_string(fd,"503 command out of sequence\r\n");
            if (checksendstring < 0){
                break;
            }
        }
        
        else if (Strequ(buff,"RCPT") && acceptRCPT){
            cond1 = 0;
            cond2 = 0;
            memset(section, '\0', sizeof(section));
            int len = (int) strlen(cpy);
            strncpy(section, cpy + 4, 5);
            if (strcmp(section, " TO:<") == 0){
                cond1 = 1;
            }
            memset(section, '\0', sizeof(section));
            strncpy(section, cpy + len - 3, 3);
            if (strcmp(section, ">\r\n") == 0){
                cond2 = 1;
            }
            //Correct Format
            if (cond1 && cond2){
                // get the rcpt email content
                memset(section, '\0', sizeof(section));
                strncpy(section, cpy + 9, len - 12);
                //check if valid user
                if(is_valid_user(section, NULL)){
                    checksendstring = send_string(fd,"250 Ok\r\n");
                    if (checksendstring < 0){
                        break;
                    }

                    //add to list of recipients
                    add_user_to_list(&rcptlist, section);
                    
                    memset(section, '\0', sizeof(section));
                }
                else if (is_valid_user(section, NULL) == 0){
                    checksendstring = send_string(fd,"550 Mailbox Not Found\r\n");
                    if (checksendstring < 0){
                        break;
                    }
                }
                acceptDATA = 1;
            }
            else if (cond1 == 0 || cond2 == 0){
                checksendstring = send_string(fd,"553 RCPT Command valid, but syntax error\r\n");
                if (checksendstring < 0){
                    break;
                }
            }
        }
        else if (Strequ(buff,"RCPT") && (acceptRCPT == 0)){
            checksendstring = send_string(fd,"503 command out of sequence\r\n");
            if (checksendstring < 0){
                break;
            }
        }
        else if (Strequ(buff,"DATA") && acceptDATA){
            if (rcptlist == NULL){
                checksendstring = send_string(fd, "554 no valid recipients\r\n");
                if (checksendstring < 0){
                    break;
                }
            }
            else if (rcptlist != NULL){
                checksendstring = send_string(fd,"354 received\r\n");
                if (checksendstring < 0){
                    break;
                }
                
                char databuffer [MAX_LINE_LENGTH];
                memset(databuffer, '\0', sizeof(databuffer));
                net_buffer_t newNB = nb_create(fd, MAX_LINE_LENGTH);
                
                char tmpfname[] = "tempXXXXXX";
                int tmpfd = mkstemp(tmpfname);
                FILE *fpt = fdopen(tmpfd, "w");
                while(1){
                    int len = nb_read_line(newNB, databuffer);
                    if (len <= 0){
                        continue;
                    }
                    if (len > MAX_LINE_LENGTH){
                        checksendstring = send_string(fd,"552 exceed line limit \r\n");
                        if (checksendstring < 0){
                            break;
                        }
                        continue;
                    }
                    
                    // Properly closes connection if read (or nb_read_line) returns <= 0
                    if(line_length <= 0){
                        break;
                    }
                    
                    if (Strequ(databuffer, ".\r\n")){
                        checksendstring = send_string(fd,"250 full stop received \r\n");
                        if (checksendstring < 0){
                            break;
                        }
                        break;
                    }
                    fwrite(databuffer, sizeof(char), strlen(databuffer), fpt);
                }
                save_user_mail(tmpfname, rcptlist);
                fclose(fpt);
                remove(tmpfname);
                destroy_user_list(rcptlist);
                nb_destroy(newNB);
                memset(databuffer, '\0', sizeof(databuffer));
                acceptDATA = 0;
                acceptRCPT = 0;
                acceptMAIL = 1;
            }
        }
        else if (Strequ(buff,"DATA") && (acceptDATA == 0)){
            checksendstring = send_string(fd,"503 command out of sequence\r\n");
            if (checksendstring < 0){
                break;
            }
        }
        else if (Strequ(buff,"NOOP")){
            checksendstring = send_string(fd,"250 Ok\r\n");
            if (checksendstring < 0){
                break;
            }
        }
        else if (Strequ(buff,"EHLO") || Strequ(buff,"RSET") || Strequ(buff,"VRFY") || Strequ(buff,"EXPN") || Strequ(buff,"HELP")){
            checksendstring = send_string(fd,"502 Unsupported Command\r\n");
            if (checksendstring < 0){
                break;
            }
        }
        nb_destroy(nbnb);
    }
}

