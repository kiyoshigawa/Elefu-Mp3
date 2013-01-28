/*
4-21-2012
Elefu.com mods by Tim Anderson
Nathan, if e'er we meet, a beer shall indeed be yours.
     -Tim Anderson

 4-28-2011
 Spark Fun Electronics 2011
 Nathan Seidle
 
 This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).
 
 This example code plays a MP3 from the SD card called 'track001.mp3'. The theory is that you can load a 
 microSD card up with a bunch of MP3s and then play a given 'track' depending on some sort of input such 
 as which pin is pulled low.
 
 It relies on the sdfatlib from Bill Greiman: 
 http://code.google.com/p/sdfatlib/
 You will need to download and install his library. To compile, you MUST change Sd2PinMap.h of the SDfatlib! 
 The default SS_PIN = 10;. You must change this line under the ATmega328/Arduino area of code to 
 uint8_t const SS_PIN = 9;. This will cause the sdfatlib to use pin 9 as the 'chip select' for the 
 microSD card on pin 9 of the Arduino so that the layout of the shield works.
 
 Attach the shield to an Arduino. Load code (after editing Sd2PinMap.h) then open the terminal at 57600bps. This 
 example shows that it takes ~30ms to load up the VS1053 buffer. We can then do whatever we want for ~100ms 
 before we need to return to filling the buffer (for another 30ms).
 
 This code is heavily based on the example code I wrote to control the MP3 shield found here:
 http://www.sparkfun.com/products/9736
 This example code extends the previous example by reading the MP3 from an SD card and file rather than from internal
 memory of the ATmega. Because the current MP3 shield does not have a microSD socket, you will need to add the microSD 
 shield to your Arduino stack.
 
 The main gotcha from all of this is that you have to make sure your CS pins for each device on an SPI bus is carefully
 declared. For the SS pin (aka CS) on the SD FAT libaray, you need to correctly set it within Sd2PinMap.h. The default 
 pin in Sd2PinMap.h is 10. If you're using the SparkFun microSD shield with the SparkFun MP3 shield, the SD CS pin 
 is pin 9. 
 
 Four pins are needed to control the VS1503:
 DREQ
 CS
 DCS
 Reset (optional but good to have access to)
 Plus the SPI bus
 
 Only the SPI bus pins and another CS pin are needed to control the microSD card.
 
 What surprised me is the fact that with a normal MP3 we can do other things for up to 100ms while the MP3 IC crunches
 through it's fairly large buffer of 2048 bytes. As long as you keep your sensor checks or serial reporting to under 
 100ms and leave ~30ms to then replenish the MP3 buffer, you can do quite a lot while the MP3 is playing glitch free.
 
 */

#include <SPI.h>

//Add the SdFat Libraries
#include <SdFat.h>
#include <SdFatUtil.h> 

//#include Elefu TWI conventions
#include "Elefu_TWI.h"

//TWI library
#include <Wire.h>

//Create the variables to be used by SdFat Library
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile track;

//This is the name of the file on the microSD card you would like to play
//Stick with normal 8.3 nomeclature. All lower-case works well.
//Note: you must name the tracks on the SD card with 001, 002, 003, etc. 
//For example, the code is expecting to play 'track002.mp3', not track2.mp3.
char trackName[] = "track001.mp3";
unsigned long trackNumber = 1;
bool playing = false;
int i = 0;
char errorMsg[100]; //This is a generic array used for sprintf of error messages


#define TRUE  1
#define FALSE  0

//defines for track_change function
#define UP 1
#define DOWN 0

//max number of tracks that can be accessed
#define MAX_TRACKS 255

//mp3 volume numbers
#define DEFAULT_VOLUME 0
#define MAX_VOLUME 0
#define MIN_VOLUME 100
#define VOLUME_INCR 3
//variable for tracking volume
unsigned int mp3_volume = DEFAULT_VOLUME;

//MP3 Player Shield pin mapping. See the schematic
#define MP3_XCS 6 //Control Chip Select Pin (for accessing SPI Control/Status registers)
#define MP3_XDCS 7 //Data Chip Select / BSYNC Pin
#define MP3_DREQ 2 //Data Request Pin: Player asks for more data
#define MP3_RESET 8 //Reset is active low
//Remember you have to edit the Sd2PinMap.h of the sdfatlib library to correct control the SD card.


//configure trigger buttons
#define TRIG0 16 //rewind
#define TRIG1 17 //skip previous
#define TRIG2 4 //play/pause

#define SD_DETECT 5


unsigned long track_number = 0; //this is the specific track number to be played. if a TWI command for playing a specific track comes with the track number, save it in this variable

//make sure the array is at least big enough for the below variables
#define NUM_ACTIONS 11

//these are defined as action items for the buttons, they also correspond to TWI commands.
#define RWND 0 //rewind
#define PREV 1 //Previous track
#define PLAY_PAUSE 2 //Play/Pause
#define STOP 3 //Stop
#define NEXT 4 //next Track
#define FFWD 5 //Fast Forward
#define VOL_UP 6 //Volume Up
#define VOL_DN 7 //Volume Down
#define PLAY_TRACK 8 //Skip to a specific track
#define IS_PLAYING 9 //check to see if a track is currently playing
#define SET_VOL 10 //check to see if a track is currently playing

#define GPIO_0 15
#define GPIO_1 14

//define the threshold for determining of an analog input is high or low
#define ANALOG_THRESH 512

#define TRIGGER_DELAY 5 //ms
unsigned long last_trigger_check = 0;//time of last trigger check. Actions only happen when sent TRIGGER_DELAY after this

#define NUM_TRIGGERS 3 //pins used as triggers, not actual triggers possible

//variables for checking triggers
unsigned int triggers[NUM_TRIGGERS] = {TRIG0, TRIG1, TRIG2};
bool trigger_states[NUM_TRIGGERS] = {HIGH, HIGH, HIGH};
bool prev_trigger_states[NUM_TRIGGERS] = {HIGH, HIGH, HIGH};
bool take_action[NUM_ACTIONS];

//variables for TWI commands:
char current_TWI_command[TWI_COMMAND_LENGTH]; //stores the most recent TWI command from a master
bool is_new_TWI_command = false; //thus is true when a new TWI command has been sent. Once it is parsed, it is set back to false.
char TWI_requires_response = false; //this is changed to true if parsing reveals a TWI command that requires more than an echo for response

//VS10xx SCI Registers
#define SCI_MODE 0x00
#define SCI_STATUS 0x01
#define SCI_BASS 0x02
#define SCI_CLOCKF 0x03
#define SCI_DECODE_TIME 0x04
#define SCI_AUDATA 0x05
#define SCI_WRAM 0x06
#define SCI_WRAMADDR 0x07
#define SCI_HDAT0 0x08
#define SCI_HDAT1 0x09
#define SCI_AIADDR 0x0A
#define SCI_VOL 0x0B
#define SCI_AICTRL0 0x0C
#define SCI_AICTRL1 0x0D
#define SCI_AICTRL2 0x0E
#define SCI_AICTRL3 0x0F

void setup() {
  //init the take_action[] array
  for(int i=0; i<NUM_ACTIONS; i++){
	take_action[i] = false;
  }
  
  //init TWI command ot be all null values
  null_TWI();
  
  pinMode(MP3_DREQ, INPUT);
  pinMode(MP3_XCS, OUTPUT);
  pinMode(MP3_XDCS, OUTPUT);
  pinMode(MP3_RESET, OUTPUT);

  //trigger pins are set up now
  pinMode(TRIG2, INPUT);
  pinMode(TRIG1, INPUT);
  pinMode(TRIG0, INPUT);
  pinMode(GPIO_0, OUTPUT);
  pinMode(GPIO_1, OUTPUT);
 
  digitalWrite(TRIG2, HIGH); // pull-up
  digitalWrite(TRIG1, HIGH); // pull-up
  digitalWrite(TRIG0, HIGH); // pull-up
  digitalWrite(GPIO_0, LOW); //mp3 functionality
  digitalWrite(GPIO_1, LOW); //mp3 functionality
 
  digitalWrite(MP3_XCS, HIGH); //Deselect Control
  digitalWrite(MP3_XDCS, HIGH); //Deselect Data
  digitalWrite(MP3_RESET, LOW); //Put VS1053 into hardware reset

  Serial.begin(115200); //Use serial for debugging 
  Serial.println("MP3 Testing");

  //Setup SD card interface
  pinMode(10, OUTPUT);       //Pin 10 must be set as an output for the SD communication to work.
  if (!card.init(SPI_FULL_SPEED))  Serial.println("Error: Card init"); //Initialize the SD card and configure the I/O pins.
  if (!volume.init(&card)) Serial.println("Error: Volume ini"); //Initialize a volume on the SD card.
  if (!root.openRoot(&volume)) Serial.println("Error: Opening root"); //Open the root directory in the volume. 

  //We have no need to setup SPI for VS1053 because this has already been done by the SDfatlib
  
  //From page 12 of datasheet, max SCI reads are CLKI/7. Input clock is 12.288MHz. 
  //Internal clock multiplier is 1.0x after power up. 
  //Therefore, max SPI speed is 1.75MHz. We will use 1MHz to be safe.
  SPI.setClockDivider(SPI_CLOCK_DIV16); //Set SPI bus speed to 1MHz (16MHz / 16 = 1MHz)
  SPI.transfer(0xFF); //Throw a dummy byte at the bus
  //Initialize VS1053 chip 
  delay(10);
  digitalWrite(MP3_RESET, HIGH); //Bring up VS1053
  //delay(10); //We don't need this delay because any register changes will check for a high DREQ
  
  //Let's check the status of the VS1053
  int MP3Mode = Mp3ReadRegister(SCI_MODE);
  int MP3Status = Mp3ReadRegister(SCI_STATUS);
  int MP3Clock = Mp3ReadRegister(SCI_CLOCKF);

  Serial.print(F("SCI_Mode (0x4800) = 0x"));
  Serial.println(MP3Mode, HEX);

  Serial.print(F("SCI_Status (0x48) = 0x"));
  Serial.println(MP3Status, HEX);

  int vsVersion = (MP3Status >> 4) & 0x000F; //Mask out only the four version bits
  Serial.print(F("VS Version (VS1053 is 4) = "));
  Serial.println(vsVersion, DEC); //The 1053B should respond with 4. VS1001 = 0, VS1011 = 1, VS1002 = 2, VS1003 = 3

  Serial.print(F("SCI_ClockF = 0x"));
  Serial.println(MP3Clock, HEX);

  //Now that we have the VS1053 up and running, increase the internal clock multiplier and up our SPI rate
  Mp3WriteRegister(SCI_CLOCKF, 0x60, 0x00); //Set multiplier to 3.0x

  //From page 12 of datasheet, max SCI reads are CLKI/7. Input clock is 12.288MHz. 
  //Internal clock multiplier is now 3x.
  //Therefore, max SPI speed is 5MHz. 4MHz will be safe.
  SPI.setClockDivider(SPI_CLOCK_DIV4); //Set SPI bus speed to 4MHz (16MHz / 4 = 4MHz)

  MP3Clock = Mp3ReadRegister(SCI_CLOCKF);
  Serial.print(F("SCI_ClockF = 0x"));
  Serial.println(MP3Clock, HEX);

  //MP3 IC setup complete
  
  //Set volume to default
	Mp3SetVolume(mp3_volume, mp3_volume); //Set the volume to the new value

  //TWI Setup for remote triggering of actions:
  Wire.begin(MP3_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  null_TWI();
  
  Serial.print(F("Free Ram: "));
  Serial.println(freeRam());  
  Serial.println(F("Setup is complete"));
}

void loop(){
	//first check for button presses, TWI input, or anything else that can control the mp3 module that control the mp3 player
	check_triggers();
	
	take_actions();
}

void take_actions(){

	//these actions work no matter is a song is playing or not.
	if(take_action[VOL_UP]){//if volume down button has been hit
		volume_change(DOWN);//lower the volume by VOLUME_INCREMENT
		Mp3SetVolume(mp3_volume, mp3_volume); //Set the volume to the new value
		take_action[VOL_UP] = false;
	}
	else if(take_action[VOL_DN]){//if volume up button has been hit
		volume_change(UP);//raise the volume by VOLUME_INCREMENT
		Mp3SetVolume(mp3_volume, mp3_volume); //Set the volume to the new value
		take_action[VOL_DN] = false;
	}	
	else if(take_action[SET_VOL]){//if volume up button has been hit
		//volume is set in the TWI_Parse function
		Mp3SetVolume(mp3_volume, mp3_volume); //Set the volume to the new value
		sprintf(errorMsg, "Volume is now %d", mp3_volume);
		Serial.println(errorMsg);
		take_action[SET_VOL] = false;
	}
	
	//when no song is playing, only certain actions will have an effect. This controls those actions.
	if(!playing){//when no current song is playing only these functions will work
		if(take_action[PREV]){//if previous track button has been hit
			track_change(DOWN);//go down one track number
			take_action[PREV] = false;
		}
		else if(take_action[PLAY_PAUSE]){//if play/pause button has been pressed
			sprintf(trackName, "track%03d.mp3", trackNumber);
			take_action[PLAY_PAUSE] = false;
			playMP3(trackName);//start playing at the current track number
		}
		else if(take_action[NEXT]){//if next track button has been hit
			track_change(UP);//go up one track number
			take_action[NEXT] = false;
		}
		else if(take_action[PLAY_TRACK]){//if a specific track has been requested and no song is playing
			//the parser already changed the track number, so start playing a new track
			take_action[PLAY_PAUSE] = true;
			take_action[PLAY_TRACK] = false;
		}
	}
	
	//this is where to check for button presses and cause actions while a track is playing. This only applies to actions that can occur while a track is playing.
	else{
		if(playing){
			check_triggers();
			if(take_action[RWND]){//if rewind track button has been hit
				//figure this out later
				take_action[RWND] = false;
			}
			else if(take_action[PREV]){//if previous track button has been hit
				track_change(DOWN);//go down one track number
				track.close(); //close the current track
				sprintf(trackName, "track%03d.mp3", trackNumber);//update the file name to match the current song
				open_track(trackName, DOWN);
				sprintf(trackName, "track%03d.mp3", trackNumber);//update the file name to match the current song
				take_action[PREV] = false;
			}
			else if(take_action[PLAY_PAUSE]){//if play/pause button has been pressed
				take_action[PLAY_PAUSE] = false;
				while(!take_action[PLAY_PAUSE]){
					//check triggers that still apply while paused
					check_triggers();
					if(take_action[PLAY_PAUSE]){ take_action[PLAY_PAUSE] = false; break; } //resume play
					else if(take_action[PREV]){ take_action[PLAY_PAUSE] = false; break; } //skip previous
					else if(take_action[NEXT]){ take_action[PLAY_PAUSE] = false; break; } //skip next
					else if(take_action[STOP]){ take_action[PLAY_PAUSE] = false; break; } //stop
					else if(take_action[PLAY_TRACK]){ take_action[PLAY_PAUSE] = false; break; } //play specific track
					//all other triggers won't do anything
				}
			}
			else if(take_action[STOP]){//if stop button has been pressed
				while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating transfer is complete
				digitalWrite(MP3_XDCS, HIGH); //Deselect Data
  				track.close(); //Close out this track
				sprintf(errorMsg, "Track %s stopped!", trackName);
				Serial.println(errorMsg);
				playing = false;
				take_action[STOP] = false;
			}
			else if(take_action[NEXT]){//if next track button has been hit
				track_change(UP);//go up one track number
				track.close(); //close the current track
				sprintf(trackName, "track%03d.mp3", trackNumber);//update the file name to match the current song
				open_track(trackName, UP);
				sprintf(trackName, "track%03d.mp3", trackNumber);//update the file name to match the current song
				take_action[NEXT] = false;
			}
			else if(take_action[FFWD]){//if fast forward track button has been hit
				//figure this out later
				take_action[FFWD] = false;
			}
			else if(take_action[PLAY_TRACK]){//if play track button has been hit
				//Track number is already set
				track.close(); //close the current track
				sprintf(trackName, "track%03d.mp3", trackNumber);//update the file name to match the current song
				open_track(trackName, UP);
				sprintf(trackName, "track%03d.mp3", trackNumber);//update the file name to match the current song
				take_action[PLAY_TRACK] = false;
			}
		}
	}
}

//this function will check the trigger pins and TWI to see if any new commands have come through. 
//If there are new commands, it sets the take_action[i] for that command along with any associated variables
//the rest of the mp3 functions are automatically controlled elsewhere in this code, but this section is the interface for the entire board
//if you want to add a trigger behavior or change them, this is the function to do it in.
void check_triggers(){
	if(millis() - last_trigger_check > TRIGGER_DELAY){//if the triggers have changed after the delay has passed
		for(int i=0; i<NUM_TRIGGERS; i++){
			prev_trigger_states[i] = trigger_states[i];//save the previous trigger states
			if(triggers[i] != A6 && triggers[i] != A7){
				trigger_states[i] = digitalRead(triggers[i]);
			}
			else{
				if(analogRead(triggers[i]) > ANALOG_THRESH){
					trigger_states[i] = HIGH;
				}
				else{
					trigger_states[i] = LOW;
				}
			}
			if(prev_trigger_states[i] && !trigger_states[i]){//if something has changed, run through the command procedures and see what to do
				process_buttons();
			}
		}
		//check or TWI commands: note do this last so TWI commands override the trigger button commands by default
		parse_TWI();
		last_trigger_check = millis();
	}
}

void process_buttons(){//this takes the recently changed trigger states and sets take_action[] flags as required.
	//variable for storing the button states as an 8-bit int.
	uint8_t trigger_value = 0;
	for(int i=0; i<NUM_TRIGGERS; i++){
		bitWrite(trigger_value, i, !trigger_states[i]);
	}
	Serial.print("Trigger:");
	Serial.println(trigger_value);
	//check for 8-button input values, then check for the other 249 states.
	if(trigger_value == 0b00000001 || trigger_value == 0b00000010){//button 1 or 2 is pressed on 8-button
		//set track to 1 or two and play it
		trackNumber = trigger_value;
		take_action[PLAY_TRACK] = true;
	}
	else if(trigger_value == 0b00000100){//button 3 is pressed on 8-button
		//previous button
		take_action[PREV] = true;
	}
	else if(trigger_value == 0b00001000){//button 4 is pressed on 8-button
		//play/pause button
		take_action[PLAY_PAUSE] = true;
	}
	else if(trigger_value == 0b00010000){//button 5 is pressed on 8-button
		//stop button
		take_action[STOP] = true;
	}
	else if(trigger_value == 0b00100000){//button 6 is pressed on 8-button
		//next button
		take_action[NEXT] = true;
	}
	else if(trigger_value == 0b01000000){//button 7 is pressed on 8-button
		//previous button
		take_action[VOL_UP] = true;
	}
	else if(trigger_value == 0b10000000){//button 8 is pressed on 8-button
		//previous button
		take_action[VOL_DN] = true;
	}
	else if(trigger_value == 0b00000011){//state 3
		trackNumber = trigger_value;
		take_action[PLAY_TRACK] = true;
	}
	else if(trigger_value > 4 && trigger_value < 8){//states 5-7
		trackNumber = trigger_value-1;//subtract 1 for the trigger value above used by the 8-button board commands
		take_action[PLAY_TRACK] = true;
	}
	else if(trigger_value > 8 && trigger_value < 16){//states 9-15
		trackNumber = trigger_value-2;//subtract 2 for the trigger value above used by the 8-button board commands
		take_action[PLAY_TRACK] = true;
	}
	else if(trigger_value > 16 && trigger_value < 32){//states 17-31
		trackNumber = trigger_value-3;//subtract 3 for the trigger value above used by the 8-button board commands
		take_action[PLAY_TRACK] = true;
	}
	else if(trigger_value > 32 && trigger_value < 64){//states 33-63
		trackNumber = trigger_value-4;//subtract 4 for the trigger value above used by the 8-button board commands
		take_action[PLAY_TRACK] = true;
	}
	else if(trigger_value > 64 && trigger_value < 128){//states 65-127
		trackNumber = trigger_value-5;//subtract 5 for the trigger value above used by the 8-button board commands
		take_action[PLAY_TRACK] = true;
	}
	else if(trigger_value > 128 && trigger_value < 256){//states 129-255
		trackNumber = trigger_value-6;//subtract 6 for the trigger value above used by the 8-button board commands
		take_action[PLAY_TRACK] = true;
	}
}

void receiveEvent(int how_many){
	char command_byte = 0;
	char trash_byte = 0;
	while(Wire.available()){//loop through all available data on wire, but only accept the bytes of a typical 
		if(command_byte < TWI_COMMAND_LENGTH){
			current_TWI_command[command_byte] = Wire.read();
		}
		else{
			trash_byte = Wire.read();
		}
		command_byte++;
	}
	is_new_TWI_command = true;
}

void requestEvent(){
	if(is_new_TWI_command){//parse the command if it hasn't already been done before the response request.
		parse_TWI();
	}
	Serial.println(current_TWI_command);
	if(TWI_requires_response == TWI_FALSE){//for general commands not requiring responses
		//echo back the last command type byte to the master to acknowledge receipt of command
		Wire.write(current_TWI_command);
	}
	else{
		//preform specific tasks depending on commands.
		if(current_TWI_command[1] == TWI_IS_PLAYING){//if the command requests to know if a song is currently playing
			char out_array[TWI_COMMAND_LENGTH];
			//start with the original command and replace data as needed
			for(int i=0; i<TWI_COMMAND_LENGTH; i++){
				out_array[i] = current_TWI_command[i];//set the out_array to be the current_TWI_command
			}
			if(playing){
				out_array[2] = TWI_TRUE;//set playing status
			}
			else{
				out_array[2] = TWI_FALSE;//set playing status
			}
			Wire.write(out_array);
		}
	}
	//once the response has been sent, null the TWI command
	null_TWI();
}

void null_TWI(){
	for(int i=0; i<TWI_COMMAND_LENGTH; i++){
		current_TWI_command[i] = TWI_BLANK;
	}
}

void parse_TWI(){
	if(is_new_TWI_command){//if the command has not been parsed
		//go through all command types that apply to this board
		if(current_TWI_command[0] == MP3_COMMAND){//make sure this is an mp3 module command
			if(current_TWI_command[1] == TWI_RWND){
				take_action[RWND] = true;
			}
			else if(current_TWI_command[1] == TWI_PREV){
				take_action[PREV] = true;
			}
			else if(current_TWI_command[1] == TWI_PLAY_PAUSE){
				take_action[PLAY_PAUSE] = true;
			}
			else if(current_TWI_command[1] == TWI_STOP){
				take_action[STOP] = true;
			}
			else if(current_TWI_command[1] == TWI_NEXT){
				take_action[NEXT] = true;
			}
			else if(current_TWI_command[1] == TWI_FFWD){
				take_action[FFWD] = true;
			}
			else if(current_TWI_command[1] == TWI_VOL_UP){
				take_action[VOL_UP] = true;
			}
			else if(current_TWI_command[1] == TWI_VOL_DN){
				take_action[VOL_DN] = true;
			}			
			else if(current_TWI_command[1] == TWI_SET_VOL){
				char mp3_volume_array[] = {current_TWI_command[2], current_TWI_command[3], current_TWI_command[4], current_TWI_command[5], current_TWI_command[6], 0};
				mp3_volume = atoi(mp3_volume_array);
				take_action[SET_VOL] = true;
			}
			else if(current_TWI_command[1] == TWI_PLAY_TRACK){
				//add up the numbers and multiple by the right power of 10 as needed to get the unsigned long that is the track number
				char track_number_array[] = {current_TWI_command[2], current_TWI_command[3], current_TWI_command[4], current_TWI_command[5], current_TWI_command[6], 0};
				trackNumber = atoi(track_number_array);
				take_action[PLAY_TRACK] = true;
			}
			else if(current_TWI_command[1] == TWI_IS_PLAYING){//does not need an action number, just returns the value of playing
				//does not need an action number, just returns the value of playing
			}
			else{
				//not a valid command, send back invalid command message
				current_TWI_command[0] = UNKNOWN_COMMAND;
				for(int i=1; i<TWI_COMMAND_LENGTH-1; i++){
					current_TWI_command[i] = TWI_BLANK;
				}
				current_TWI_command[0] = TWI_FALSE;
			}
			//set variable to check and see if the command requires a response
			TWI_requires_response = current_TWI_command[TWI_COMMAND_LENGTH-1];
		}
		is_new_TWI_command = false;
	}
}

//PlayMP3 pulls 32 byte chunks from the SD card and throws them at the VS1053
//We monitor the DREQ (data request pin). If it goes low then we determine if
//we need new data or not. If yes, pull new from SD card. Then throw the data
//at the VS1053 until it is full.
void playMP3(char* fileName) {
  if(!playing){ playing = true; }
  open_track(fileName, UP);//default to looking ahead for the next track if a number is bad
  sprintf(fileName, "track%03d.mp3", trackNumber);//update the file name to match the current song
  
  Serial.println("Track open");

  uint8_t mp3DataBuffer[32]; //Buffer of 32 bytes. VS1053 can take 32 bytes at a go.
  //track.read(mp3DataBuffer, sizeof(mp3DataBuffer)); //Read the first 32 bytes of the song
  int need_data = TRUE;
  long replenish_time = millis();

  Serial.println("Start MP3 decoding");

  while(1) {
    while(!digitalRead(MP3_DREQ)) { 
      //DREQ is low while the receive buffer is full
      //You can do something else here, the buffer of the MP3 is full and happy.
      //Maybe set the volume or test to see how much we can delay before we hear audible glitches

	take_actions();
      //If the MP3 IC is happy, but we need to read new data from the SD, now is a great time to do so
      if(need_data == TRUE) {
        if(!track.read(mp3DataBuffer, sizeof(mp3DataBuffer))) { //Try reading 32 new bytes of the song
          //Oh no! There is no data left to read!
          //Time to exit
          break;
        }
        need_data = FALSE;
      }

      //Serial.println("."); //Print a character to show we are doing nothing

      //This is here to show how much time is spent transferring new bytes to the VS1053 buffer. Relies on replenish_time below.
      // Serial.print("Time to replenish buffer: ");
      // Serial.print(millis() - replenish_time, DEC);
      // Serial.println("ms");

      //Test to see just how much we can do before the audio starts to glitch
      //long start_time = millis();
      //delay(150); //Do NOTHING - audible glitches
      //delay(135); //Do NOTHING - audible glitches
      //delay(120); //Do NOTHING - barely audible glitches
      //delay(100); //Do NOTHING - sounds fine
      //Serial.print(" Idle time: ");
      //Serial.print(millis() - start_time, DEC);
      //Serial.println("ms");
      //Look at that! We can actually do quite a lot without the audio glitching

      //Now that we've completely emptied the VS1053 buffer (2048 bytes) let's see how much 
      //time the VS1053 keeps the DREQ line high, indicating it needs to be fed
      replenish_time = millis();
    }


    if(need_data == TRUE){ //This is here in case we haven't had any free time to load new data
      if(!track.read(mp3DataBuffer, sizeof(mp3DataBuffer))) { //Go out to SD card and try reading 32 new bytes of the song
        //Oh no! There is no data left to read!
        //Time to exit
        break;
      }
      need_data = FALSE;
    }

    //Once DREQ is released (high) we now feed 32 bytes of data to the VS1053 from our SD read buffer
    digitalWrite(MP3_XDCS, LOW); //Select Data
    for(int y = 0 ; y < sizeof(mp3DataBuffer) ; y++) {
      SPI.transfer(mp3DataBuffer[y]); // Send SPI byte
    }

    digitalWrite(MP3_XDCS, HIGH); //Deselect Data
    need_data = TRUE; //We've just dumped 32 bytes into VS1053 so our SD read buffer is empty. Set flag so we go get more data
	
	if(!playing){//breaks while loop if the playing flag has been changed (usually by the STOP action).
		break;
	}
  }

  while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating transfer is complete
  digitalWrite(MP3_XDCS, HIGH); //Deselect Data
  
  track.close(); //Close out this track

  sprintf(errorMsg, "Track %s done!", fileName);
  Serial.println(errorMsg);
  playing = false;
}

//this opens the track, or if none is available it opens the next one in the sequence
void open_track(char* file_name, bool dir){
  while(!track.open(&root, file_name, O_READ)) { //Open the file in read mode.
    sprintf(errorMsg, "Failed to open %s", file_name);
    Serial.println(errorMsg);
	//after failing to open a track number, keep increasing the track number till it finds a good one
	track_change(dir);//go down one track number
	sprintf(file_name, "track%03d.mp3", trackNumber);//update the file name to match the current song
  }
}

//Write to VS10xx register
//SCI: Data transfers are always 16bit. When a new SCI operation comes in 
//DREQ goes low. We then have to wait for DREQ to go high again.
//XCS should be low for the full duration of operation.
void Mp3WriteRegister(unsigned char addressbyte, unsigned char highbyte, unsigned char lowbyte){
  while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating IC is available
  digitalWrite(MP3_XCS, LOW); //Select control

  //SCI consists of instruction byte, address byte, and 16-bit data word.
  SPI.transfer(0x02); //Write instruction
  SPI.transfer(addressbyte);
  SPI.transfer(highbyte);
  SPI.transfer(lowbyte);
  while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating command is complete
  digitalWrite(MP3_XCS, HIGH); //Deselect Control
}

//Read the 16-bit value of a VS10xx register
unsigned int Mp3ReadRegister (unsigned char addressbyte){
  while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating IC is available
  digitalWrite(MP3_XCS, LOW); //Select control

  //SCI consists of instruction byte, address byte, and 16-bit data word.
  SPI.transfer(0x03);  //Read instruction
  SPI.transfer(addressbyte);

  char response1 = SPI.transfer(0xFF); //Read the first byte
  while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating command is complete
  char response2 = SPI.transfer(0xFF); //Read the second byte
  while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating command is complete

  digitalWrite(MP3_XCS, HIGH); //Deselect Control

  int resultvalue = response1 << 8;
  resultvalue |= response2;
  return resultvalue;
}

//Set VS10xx Volume Register
void Mp3SetVolume(unsigned char leftchannel, unsigned char rightchannel){
  Mp3WriteRegister(SCI_VOL, leftchannel, rightchannel);
}


//this will increment the volume by VOLUME_INCR in the direction specified
void volume_change(bool dir){
	//remember volume is in reverse, so moving it closer to 0 makes it louder
	if(dir == UP){
		if(mp3_volume >= (MAX_VOLUME + VOLUME_INCR)){
			mp3_volume = mp3_volume - VOLUME_INCR;
		}
		else{
			mp3_volume = MAX_VOLUME;
		}
	}
	if(dir == DOWN){
		if(mp3_volume <= MIN_VOLUME - VOLUME_INCR){
			mp3_volume = mp3_volume + VOLUME_INCR;
		}
		else{
			mp3_volume = MIN_VOLUME;
		}
	}
	sprintf(errorMsg, "Volume is now %d", mp3_volume);
    Serial.println(errorMsg);
}

//this will change the track up or down
void track_change(bool dir){
	if(dir == UP){
		if(trackNumber < MAX_TRACKS){
			trackNumber++;
		}
		else{
			trackNumber = 1;//back to the beginning
		}
	}
	if(dir == DOWN){
		if(trackNumber >= 1){
			trackNumber--;
		}
		else{
			trackNumber = MAX_TRACKS;//up to the top
		}
	}
	sprintf(errorMsg, "Current track is now %03d", trackNumber);
    Serial.println(errorMsg);
}

//awesome free SRAM checker from JeeLabs
int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
