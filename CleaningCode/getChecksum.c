#include <stdio.h>
#include <string.h>

int getChecksum(const char* string){
 
    int sum = 0;
    
    for(int i = 0; i<strlen(string); i++){
        sum += (int)string[i];                  /*This is the entire sum of all characters in the string added*/
    }

    int checksum = (255 - (sum % 255));         /*The checksum cannot be greater than 2^8 = 256 */

    return checksum;
}