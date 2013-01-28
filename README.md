Elefu Mp3 Module

This is the firmware code for the Mp3 Module from Elefu, as well as the sound files we provide with our 3D printers. You shouldn't need to do anything with the firmware if you're just using the mp3 board with your printer. To use the sound files provided, copy the mp3 files as named in their directory (i.e. Track003.mp3) to a Micro SD card in the root directory. Then pu that micro SD card into the mp3 module and connect it per the RA Assembly Instructions (http://blog.elefu.com/ra-assembly-instructions/). 

If you want to change which track numbers play when, this is controlled by the RA firmware in the file called 'mp3_commands.h' (https://github.com/kiyoshigawa/Elefu-RAv3). The defaults are listed below:

#define STARTUP_TRACK 1
#define PRINT_STARTED_TRACK 2
#define PRCT10_TRACK 3
#define PRCT20_TRACK 4
#define PRCT30_TRACK 5
#define PRCT40_TRACK 6
#define PRCT50_TRACK 7
#define PRCT60_TRACK 8
#define PRCT70_TRACK 9
#define PRCT80_TRACK 10
#define PRCT90_TRACK 11
#define PRINT_COMPLETE_TRACK 12
#define ERROR_TRACK 13

#define SILLY_TRACK 253 //for use with being silly

For support, questions, or comments, check out our contact page: http://blog.elefu.com/contact/

Happy Listening,
     -Tim Anderson
      Elefu.com
