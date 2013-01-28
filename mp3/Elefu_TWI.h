/*
Elefu_TWI.h
This contains the #defines for typical values in Elefu's TWI projects. 
It includes command values and data formats if there is any supplemental data.
It can be included in any TWI project and should keep protocols well defined between boards
If typical Elefu sensor boards are being used, this document will give good naming conventions for typical addresses and commands to be sent that agree with our TWI code.
*/

//these are variables to be sent over TWI representing typical values that can't be used due to characters 0x00 and 0xFF being reserved for end transmission and no data received respectively
#define TWI_TRUE 'T'
#define TWI_FALSE 'F'
#define TWI_BLANK '_'

//#defines that control the size of commands and other constants regardless of command.
#define TWI_COMMAND_LENGTH 8 //8 bytes per TWO command
//Slaves will echo back only the command as it was received upon receipt of a command. if the command does not require a response
//commands that require responses have the last byte set to TWI_TRUE so slaves will know to send a different response then the original command
//note that the master never echos received responses back to the slaves, it just accepts their data and parses it accordingly


//This defines typical addresses for boards:
//reserve 1-10 for masters in projects
#define MASTER_ADDRESS 1

//reserve 11-99 for normal wired boards
#define MP3_ADDRESS 11 //the mp3 module board's default address
#define TVT_ADDRESS 12 //the TVT's normal wired address

//reserve 101-189 for wireless sensor boards
#define WIRELESS_MOTOR_ADDRESS 101 //wireless motor control module
#define WIRELESS_SONIC_ADDRESS 102 //wireless sonic rangefinder module

//reserve 190-253 for custom boards to be set on a module-by-module basis
//Never define these in this file, do it in the header of that specific module's code



//The #defines at the top level are all command types. Below them are sub-commands and data bytes for the specific command

#define CANCEL_COMMAND 'X'
	//this is to cancel long TWI operations at will
	//example command: {CANCEL_COMMAND, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_FALSE}
#define UNKNOWN_COMMAND '?'
	//this is sent as a response if the command is not known to the device who received it.
	//example response: {UNKNOWN_COMMAND, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_FALSE}
#define TVT_COMMAND 'A'
	//this is for controlling the TVT remotely via TWI: Commands to be added later

#define MP3_COMMAND 'B'
	//These are mp3 actions for byte 1 of MP3_COMMAND TWI messagescontrolling the mp3 module remotely with TWI
	#define TWI_RWND 'A' //rewind
		//All data bytes 2-6 are Null for this action
		//example command: {MP3_COMMAND, RWND, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_FALSE}
	#define TWI_PREV 'B' //Previous track
		//All data bytes 2-6 are Null for this action
		//example command: {MP3_COMMAND, PREV, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_FALSE}
	#define TWI_PLAY_PAUSE 'C' //Play/Pause
		//All data bytes 2-6 are Null for this action
		//example command: {MP3_COMMAND, PLAY_PAUSE, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_FALSE}
	#define TWI_STOP 'D' //Stop
		//All data bytes 2-6 are Null for this action
		//example command: {MP3_COMMAND, STOP, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_FALSE}
	#define TWI_NEXT 'E' //next Track
		//All data bytes 2-6 are Null for this action
		//example command: {MP3_COMMAND, NEXT, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_FALSE}
	#define TWI_FFWD 'F' //Fast Forward
		//All data bytes 2-6 are Null for this action
		//example command: {MP3_COMMAND, FFWD, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_FALSE}
	#define TWI_VOL_UP 'G' //Volume Up
		//All data bytes 2-6 are Null for this action
		//example command: {MP3_COMMAND, VOL_UP, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_FALSE}
	#define TWI_VOL_DN 'H' //Volume Down
		//All data bytes 2-6 are Null for this action
		//example command: {MP3_COMMAND, VOL_DN, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_FALSE}
	#define TWI_PLAY_TRACK 'I' //Skip to a specific track
		//data bytes 2-6 are the track number in ascii numbers.
		//example command: play track 172: {MP3_COMMAND, PLAY_TRACK, '0', '0', '1', '7', '2', TWI_FALSE}
	#define TWI_IS_PLAYING 'J' //check to see if a track is currently playing
		//all daya bytes are null for this command
		//example command: {MP3_COMMAND, IS_PLAYING, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_TRUE}
		//command expects a boolean response on byte 2 of response to master
		//example response: something is playing: {MP3_COMMAND, IS_PLAYING, true, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_FALSE}
	#define TWI_SET_VOL 'K' //Volume Down
		//All data bytes 2-6 are Null for this action
		//example command: {MP3_COMMAND, VOL_DN, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_BLANK, TWI_FALSE}
		
	
	
	
	
	