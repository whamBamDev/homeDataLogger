#include "Arduino.h"

int addtobuffer(char buf[], int bufsize, char str[]){      //add str to end of buf, limited by bufsize
  int p=0;
  int k=strlen(buf);
  while((k+p<bufsize-1)&&str[p]){                          //while there's room
    buf[p+k]=str[p];                                       //add character
    p++;
  }
  buf[p+k]=0;                                              //terminate array
  return p;                                                //number of characters added
}

int addtobuffer(char buf[], int bufsize, char str){       //add char to end of buf, limited by bufsize
  int k=strlen(buf);
  if(k<bufsize-1){                                        //if there's room for one more
    buf[k]=str;                                           //add it
    buf[k+1]=0;
    return 1;                                             //1 character added
  }
  return 0;
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
int strContains(char *str, char *sfind) {
    int found = 0;
    int index = 0;
    int len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        }
        else {
            found = 0;
        }
        index++;
    }

    return 0;
}
