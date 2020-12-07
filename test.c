// The MIT License (MIT)
//
// Copyright (c) 2020 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

//Written by 
//Ujjwal Bajagain 1001643752
//Alisha Kunwar 1001668106 

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_NUM_ARGUMENTS 4
#define NUM_ENTRIES 16

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

struct __attribute__((__packed__)) DirectoryEntry
{
  char        DIR_Name[11];
  u_int8_t    DIR_Attr;
  u_int8_t    unused1[8];
  u_int16_t   DIR_FirstClusterHigh;
  u_int8_t    unused2[4];
  u_int16_t   DIR_FirstClusterLow;
  u_int32_t   DIR_Filesize;
};

struct DirectoryEntry dir[16];
/*


int compare (char IMG_Name[11], char *input);
char* TO_UPPER(char*);
*/
int16_t BPB_BytesPerSec;
int8_t  BPB_SecPerClus;
int16_t BPB_RsvdSecCnt;
int8_t  BPB_NumFATS;
int32_t BPB_FATSz32;

 FILE *fp;
 int file_open = 0;

int32_t LBAToOffset(int32_t sector)
{
  return ((sector -2 )* BPB_BytesPerSec) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytesPerSec) + (BPB_RsvdSecCnt * BPB_BytesPerSec);
}

int16_t NextLB(int32_t sector)
{
  uint32_t FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt) + (sector*4);
  int16_t value;
  fseek(fp, FATAddress, SEEK_SET);
  fread(&value, 2, 1, fp);
  return value;
}

int compare(char * userString, char * directoryString)
{
  char * dotdot ="..";
  if(strncmp(dotdot, userString, 2) == 0)
  {
    if (strncmp(userString, directoryString, 2 ) == 0)
    {
      return 1;
    }
    return 0;
  }
  char IMG_Name[12];
  strncpy (IMG_Name, directoryString, 11);
  IMG_Name[11] = '\0';

  char input[11];
  memset(input, 0, 11);
  strncpy(input, userString, strlen(userString) );
  
  char expanded_name[12];
  memset(expanded_name, ' ', 12);

  char *token = strtok(input, ".");

  strncpy(expanded_name, token, strlen(token));

  token = strtok(NULL, ".");

  if(token)
  {
    strncpy( (char*)(expanded_name + 8), token, strlen(token) );

  }
  expanded_name[11] = '\0';

  int i;

  for(i = 0; i < 11; i++)
  {
    expanded_name[i] = toupper(expanded_name[i]);

  }
  if(strncmp(expanded_name, IMG_Name, 11) == 0)
  {
  
    return 1;
  }
  return 0;

}

int bpb()
{
  printf("BPB_BytesPerSec: %d 0*%x\n", BPB_BytesPerSec, BPB_BytesPerSec);
  printf("BPB_SecPerClus:  %d 0*%x\n", BPB_SecPerClus, BPB_SecPerClus);
  printf("BPB_RsvdSecCnt:  %d 0*%x\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
  printf("BPB_NumFATS:     %d 0*%x\n", BPB_NumFATS, BPB_NumFATS);
  printf("BPB_FATSz32:     %d 0*%x\n", BPB_FATSz32, BPB_FATSz32);

  return 0;
}

int ls()
{
  int i;
  for(i = 0; i<NUM_ENTRIES; i++)
  {
    char filename[12];
    strncpy(filename, dir[i].DIR_Name,11);
    filename[11] = '\0';

    if((dir[i].DIR_Attr == ATTR_READ_ONLY || dir[i].DIR_Attr == ATTR_DIRECTORY
        || dir[i].DIR_Attr == ATTR_ARCHIVE)&& filename[0] != 0xffffffe5)
        {
            printf("%s\n", filename);
        }
  }
  return 0;
}

int cd(char * directoryName)
{
  int i;
  int found = 0;
  for(i=0; i<NUM_ENTRIES; i++)
  {
    if(compare(directoryName, dir[i].DIR_Name))
    {
      int cluster = dir[i].DIR_FirstClusterLow;

      if(cluster == 0)
      {
        cluster = 2;
      }

      int offset = LBAToOffset(cluster);
      fseek(fp, offset, SEEK_SET);

      fread(dir, sizeof(struct DirectoryEntry), NUM_ENTRIES, fp);

      found = 1;
      break;
    }
  }
  if(!found)
  {
    printf("Error: Directory not found\n");
    return -1;
  }
  return 0;
}

int statFile(char *filename)
{
  int i;
  int found = 0;
  for(i=0; i<NUM_ENTRIES; i++)
  {
    if(compare(filename, dir[i].DIR_Name))
    {
      char name[12];
      strncpy(name, dir[i].DIR_Name, 11);
      name[11] = '\0';
      printf("%s Attr: %d Size: %d Cluster: %d\n", name, dir[i].DIR_Attr, dir[i].DIR_Filesize, dir[i].DIR_FirstClusterLow);
      found = 1;
    }
  }
  if(!found)
  {
    printf("Error: File not found\n");
  }
  return 0;
}

int getFile( char * originalFilename, char *newFileName)
{
   FILE *ofp;

   if( newFileName == NULL)
   {
       ofp = fopen(originalFilename, "w");
       if( ofp == NULL)
       {
           printf("Could not open file %s\n", originalFilename);
           perror("Error: ");
       }
   }
   
   int i;
   int found = 0;

   //For the command read num.txt 510 4
   //requested_offset = 510
   //requested_bytes = 4;

   for (i =0; i < NUM_ENTRIES; i ++)
   {
       if (compare(originalFilename, dir[i].DIR_Name))
       {
           int cluster = dir[i].DIR_FirstClusterLow;

           found = 1;

           int bytesRemainingToRead = dir[i].DIR_Filesize;
           int offset = 0;
           unsigned char buffer [512];

           //handle the middle section of the file
           //it the middle if the blocks are full

           while( bytesRemainingToRead >= BPB_BytesPerSec)
           {
               offset = LBAToOffset (cluster);
               fseek(fp, offset, SEEK_SET);
               fread(buffer, 1, BPB_BytesPerSec, fp);

               fwrite( buffer, 1, 512, ofp);
               cluster = NextLB (cluster);

               bytesRemainingToRead =  bytesRemainingToRead - BPB_BytesPerSec;
           }

           //handle the last blocks
           if (bytesRemainingToRead)
           {
               offset = LBAToOffset(cluster);
               fseek(fp, offset, SEEK_SET);
               fread( buffer, 1, bytesRemainingToRead, fp);

               fwrite( buffer, 1, bytesRemainingToRead, ofp);
           }

           fclose( ofp );
       }

    }
}

int readFile( char* filename, int requested_offset, int requested_bytes)
{
    int i;
    int found = 0;
    int bytesRemainingToRead = requested_bytes;

    if( requested_bytes <0)
    {
        printf("Error: offset cannot be less than zero\n");
    }

     //For the command read num.txt 510 4
   //requested_offset = 510
   //requested_bytes = 4;

   for (i =0; i < NUM_ENTRIES; i ++)
   {
       if (compare(filename, dir[i].DIR_Name))
       {
           int cluster = dir[i].DIR_FirstClusterLow;

           found = 1;

           int searchSize = requested_offset;

           //search for the begining cluster of the file

           while( searchSize >= BPB_BytesPerSec)
           {
               cluster = NextLB( cluster);
               searchSize = searchSize - BPB_BytesPerSec;
           }

           //Read the first blocks
           int offset = LBAToOffset (cluster);
           int bytesOffset = (requested_offset * BPB_BytesPerSec);
           fseek(fp, offset + bytesOffset, SEEK_SET);

           unsigned char buffer[BPB_BytesPerSec];

           //figure out how many bytes in the first block we need to read
           int firstBlockBytes = BPB_BytesPerSec - requested_offset;
           fread (buffer, 1, firstBlockBytes, fp);

           for(i =0; i<firstBlockBytes; i++)
           {
               printf("%x", buffer[1]);
           }

           bytesRemainingToRead =  bytesRemainingToRead - firstBlockBytes;

           while( bytesRemainingToRead >= 512)
           {
               cluster = NextLB (cluster);
               offset = LBAToOffset (cluster);
               fseek(fp, offset, SEEK_SET);
               fread(buffer, 1, BPB_BytesPerSec, fp);

               for(i =0; i <BPB_BytesPerSec; i++)
               {
                   printf("%x", buffer[i]);
               }

               bytesRemainingToRead =  bytesRemainingToRead - BPB_BytesPerSec;
            }

            //handle the last blocks
           if (bytesRemainingToRead)
           {
               cluster = NextLB (cluster);
               offset = LBAToOffset (cluster);
               fseek(fp, offset, SEEK_SET);
               fread(buffer, 1, BPB_BytesPerSec, fp);

               for(i =0; i <BPB_BytesPerSec; i++)
               {
                   printf("%x", buffer[i]);
               }
           }

           printf("\n");
       }
   }

   if(!found)
   {
       printf("Error: File NOt Found \n");
       return -1;
   }
   return 0;
}

int main ()
{
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;
                                                           
    char *working_str  = strdup( cmd_str );

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) &&
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    if(strcmp(token[0],"open") == 0)
    {
        fp = fopen(token [1],"r");
        if(fp == NULL)
        {
            perror("Could not open file.");
        }
        else
        {
            printf("File is open now\n");
            file_open = 1;
        }
        
        //read the BPB section
        fseek(fp, 11, SEEK_SET);
        fread(&BPB_BytesPerSec, 1, 2, fp);
        
        fseek(fp, 13, SEEK_SET);
        fread(&BPB_SecPerClus, 1, 1, fp);

        fseek(fp, 14, SEEK_SET);
        fread(&BPB_RsvdSecCnt, 2, 1, fp);

        fseek(fp, 16, SEEK_SET);
        fread(&BPB_NumFATS, 1, 2, fp);

        fseek(fp, 36, SEEK_SET);
        fread(&BPB_FATSz32, 4, 1, fp);
        
        //The root directory address is located past the reserved sector are and both FATS
        int rootAddress = (BPB_RsvdSecCnt * BPB_BytesPerSec) + (BPB_NumFATS*BPB_FATSz32*BPB_BytesPerSec);
        
        fseek(fp, rootAddress, SEEK_SET);
        fread(dir, sizeof(struct DirectoryEntry), 16, fp);

    }

    else if (strcmp(token[0], "close")==0)
    {
        if(file_open)
        {
            fclose(fp);
            
            //reset the file stats
            fp =NULL;
            file_open = 0;
            
        }
        else
        {
            printf("Error: File not open.\n");
        }
    }
    else if(strcmp(token[0],"bpb")==0)
    {
        if(file_open)
        {
            bpb();
        }
        else
        {
            printf("Error: File Image Not Opened\n");
        }
    }
    else if(strcmp(token[0],"ls")==0)
    {
        if(file_open)
        {
            ls();
        }
        else
        {
            printf("Error: File not open.\n");
        }
    }
    else if(strcmp(token[0], "cd")==0)
    {
        if(file_open)
        {
            cd(token[1]);
        }
        else
        {
            printf("Error: File Image Not Opened\n");
        }
    }
    else if(strcmp(token[0],"read")==0)
    {
        if(file_open)
        {
            readFile(token[1], atoi(token[2]), atoi(token[3]));
        }
        else
        {
            printf("Error: File Image Not Opened\n");
        }
    }

    else if(strcmp(token[0],"get") == 0)
    {
        if (file_open)
        {
            getFile( token[1], token[2] );
        }
        else
        {
            printf("Error : File Input Not Opened\n");
        }
    }

    else if(strcmp(token[0],"stat") == 0)
    {
        if (file_open)
        {
            statFile( token[1]);
        }
        else
        {
            printf("Error : File Input Not Opened\n");
        }
    }

    free( working_root);
  }

    return 0;
}