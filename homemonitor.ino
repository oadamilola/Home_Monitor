//#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
//#define DEBUG
//#define DEBUG_ARRAY
#ifdef DEBUG
#define debug_print(x) Serial.println(x)
#else
#define debug_print(x) do {} while(0)
#endif

#ifdef DEBUG
#define debug_print2(x) Serial.println(x)
#else
#define debug_print2(x) do {} while(0)
#endif

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}
//Defining the states for my FSM
enum States{
  SYNCHRONISATION,
  NORMAL_DISPLAY,
  OFF_DEVICES,
  ON_DEVICES,
  ID_DISPLAY,
}states;

//Defining my up arrow
byte uparrow[8] = {
  0b00100,
  0b01110,
  0b11111,
  0b00100,
  0b00100,
  0b00100,
  0b00000,
  0b00000,
};
//Defining my degrees celcius symbol
byte celsc[8] = {
  0b00100,
  0b01010,
  0b01010,
  0b00100,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
};
//Defining my down arrow
byte downarrow[8] = {
  0b00000,
  0b00000,
  0b00100,
  0b00100,
  0b00100,
  0b11111,
  0b01110,
  0b00100,
};
//Device class constructor
class Device{ //?
  public:
      String ID;
      String type;
      String location;
      String state;
      int PoworTemp;
      // method to set state
      void setstate(String state){
        this->state= state;
      }
      // method to set Power Or Temperature
      void setpowortemp(int PoworTemp){
        this->PoworTemp= PoworTemp;
      }
}; 
//Defining global Variables
int count =0; //points to the next available space in Device Array
int counter =0; //points to the current device being shown in NORMAL_DISPLAY state
int counteroff=0;//points to the current device being shown in OFF_DEVICES state
int counteron=0;//points to the current device being shown in ON_DEVICES state
String userinp;//holds the user input from serial monitor
Device Devices[8]={}; //Class Array of Devices
Device subDevices[8]={};//Class Array of Devices used to store OFF or ON devices dependant on state
uint8_t buttons;//defines variable to store the value read from buttons 
bool validinp; //holds whether an input is valid or not
bool offdevices=false;//holds whether the FSM is in OFF_DEVICES state
bool ondevices=false;//holds whether the FSM is in ON_DEVICES state
bool normaldevices=true;//holds whether the FSM is in NORMAL_DISPLAY state
String id;
String setto;//input that user wants to set State Or Power/Temperature to
unsigned long currentTime=0;//used to store current time passed since program start
unsigned long previousTime=0;//used to store last checked time
unsigned long checktime=0;//used to store current time passed since program start
int a=0;//index for scroll function

void setup() {
  // put your setup code here, to run once:
  lcd.begin(16,2);
  Serial.begin(9600);
  states=SYNCHRONISATION;//set state to first state
}
//FUNCTIONS
//Bubble Sort Function to sort Devices Array alphabetically by ID
void sortdevices(class Device Devices[8]){
  Device temp;
  for(int i=0;i<8;i++){
    for(int j=0;j<7;j++){
      if((Devices[j].ID>Devices[j+1].ID)&&(Devices[j+1].ID!="X")){ //if Device ID is X then Device IDs do not swap
        temp=Devices[j];
        Devices[j]=Devices[j+1];
        Devices[j+1]=temp;
      }
    }
  }
  /* USED FOR DEBUGGING
  for(int a=0;a<12;a++){
    debug_print2(a); 
    debug_print2(Devices[a].ID); prints sorted list
  }*/
}
int numofdevs(Device Devices[8],String inpstate){
  int num=0;
  for(int i=0;i<8;i++){
    if(Devices[i].state==inpstate){
      num++;
    }
  } 
  return num; 
}
//Linear Search Function
int searchdevices(Device Devices[8],String userinp){
  String id=userinp.substring(2,5);
  bool found=false;
  int count=-1;
  int i =0;
  while((i<8)&& (found == false)){
    if(Devices[i].ID==id){ //searches until ID matches ID of user input
      count= i;
      found=true;             
    }
    i++;
  }
  return count;//returns position in Array or -1 if not found
}
//executes buttonpresses
int buttonpressed(uint8_t buttons,int counter,Device Devices[8]){
  if((buttons& BUTTON_DOWN)&&(counter<count)){
        counter=incrcounter(Devices,counter);//points to next Device
        lcd.clear();
        a=-1;//resets index
        debug_print("DOWN");
        counter=dispdevice(Devices,counter);
  }else if((buttons& BUTTON_UP)&&(counter>0)){
        counter=decrcounter(Devices,counter);//points to previous Device
        lcd.clear();      
        a=-1;
        debug_print("UP");
        counter=dispdevice(Devices,counter);
  }else if(buttons& BUTTON_SELECT){
        checktime=millis();
        //if(millis()-checktime>1000){
        states=ID_DISPLAY; //changes state
        a=-1;
        debug_print("SELECT");
        //break;
  }else if((buttons& BUTTON_RIGHT)&&(count>0)){
        //switchdisplay
        debug_print("RIGHT");
        if((ondevices==true)&&(offdevices==false)){ //ON_DEVICE can only be turned off if in ON_DEVICE state
          ondevices=false;
          states=NORMAL_DISPLAY;
          normaldevices=true;
          //counter=0;//resets counter
          a=-1;
          lcd.clear();         
        }else if((ondevices==false)&&(offdevices==false)){ //ON_DEVICE can only be turned on if in NORMAL_DISPLAY
          ondevices=true;
          states=ON_DEVICES;
          normaldevices=false;
          //counter=0; 
          a=-1;
          lcd.clear();          
        }
      }else if((buttons& BUTTON_LEFT)&&(count>0)){
        //switchdisplay
        debug_print("LEFT");
        if((offdevices==true)&&(ondevices==false)){ //OFF_DEVICE can only be turned off if in OFF_DEVICE state
          offdevices=false;
          states=NORMAL_DISPLAY;
          normaldevices=true;
          //counter=0;
          a=-1;
          lcd.clear();
        }else if((offdevices==false)&&(ondevices==false)){ //OFF_DEVICE can only be turned on if in NORMAL_DISPLAY
          offdevices=true;
          states=OFF_DEVICES;
          normaldevices=false;
          //counter=0;
          a=-1;
          lcd.clear();
        }
                
      }
      return counter;//returns new counter value
}
//scroll function
int scroll(String location,int a){
  currentTime=millis();//checks number of seconds passed and stores it in currentTime
  int n=location.length();
  if(a==-1){
    a++; //index is incremented if -1
  }
  lcd.setCursor(5,0);
  lcd.print(location.substring(a,n));  //prints part of location from index to the end of the string
  if(n-a<11){ //if the substring being printed is less than 11 a space is printed after followed by beginning of string until index
    lcd.print(" ");
    lcd.print(location.substring(0,a));
  }
  if((currentTime-previousTime)>=2000){ //if 2 seconds has passed since last checked
    if(a==n){
      a=0;// if index has reached the end it is reset
    }
    lcd.setCursor(5,0);
    lcd.print(location.substring(a,n));  
    if(n-a<11){
      lcd.print(" ");
      lcd.print(location.substring(0,a));
    }
    previousTime=currentTime;
    a++;
  }
  return a;//new index is returned
}
//POINTS TO NEXT AVAILABLE SPACE FOR DEVICE TO BE ADDED
int incrcount(Device Devices[8]){
  int i=0;
  int count;
  bool found= false;
  while((i<8)&& (found == false)){
      if(Devices[i].ID=="X"){ //finds the first instance of X in the sorted Array
        count=i;
        found= true;
      }else{
        i++;
        count=8;
      }        
  }
  return count;//returns position of next available space or 8 if full
}
//increments counter
int incrcounter(Device Devices[8],int counter){
  int i=counter+1;
  return i;
}
//decrements counter
int decrcounter(Device Devices[8],int counter){
  int i=counter-1;
  return i;
}
//displays current device to lcd
int dispdevice(Device Devices[8],int counter){
  //creates my special characters
  lcd.createChar(0,uparrow);
  lcd.createChar(1,celsc);
  lcd.createChar(2,downarrow);
  //if counter is pointing to empty device or 8 counter is decremented
  if(((Devices[counter].ID=="X")&&(counter!=0))||(counter==8)){
    counter--;
  }
  //sets backlight according to state
  if (Devices[counter].state =="OFF"){
    lcd.setBacklight(3);
  }else if(Devices[counter].state =="ON "){
    lcd.setBacklight(2);
  }
  //if current device is not empty
  if(Devices[counter].ID!="X"){
      //if(Devices[counter])
      if(counter!=0){ //prints up arrow
        lcd.setCursor(0,0);
        lcd.write((byte)0);
      }
      lcd.setCursor(1,0);
      lcd.print(Devices[counter].ID);
      if(Devices[counter].location.length()>11){ //if location is longer than 11 it scrolls
        a=scroll(Devices[counter].location,a);
      }else{
        lcd.setCursor(5,0);
        lcd.print(Devices[counter].location);
      }
      if((Devices[counter+1].ID!="X")&&(counter!=7)){ //checks if there is a device after current device
        //prints down arrow
        lcd.setCursor(0,1);
        lcd.write((byte)2);
      }
      lcd.setCursor(1,1);
      lcd.print(Devices[counter].type);
      lcd.setCursor(3,1);
      lcd.print(Devices[counter].state);
      //if socket or light
      if((Devices[counter].type=="S")||(Devices[counter].type=="L")){
        lcd.setCursor(7,1);
        lcd.print(Devices[counter].PoworTemp);
        lcd.print("%");
      //if thermostat
      }else if(Devices[counter].type=="T"){
        lcd.setCursor(7,1);
        lcd.print(Devices[counter].PoworTemp);
        lcd.write((byte)1);
        lcd.print("C");
      }
  
  }else if(normaldevices==true){//if Array is empty backlight is set to white
    lcd.setBacklight(7);
  }else if(offdevices==true){ //if Array is empty and in OFF_DEVICES state
    lcd.setCursor(0,0);
    lcd.print("NOTHING'S OFF");
  }else if(ondevices==true){ //if Array is empty and in ON_DEVICES state
    lcd.setCursor(0,0);
    lcd.print("NOTHING'S ON");
  }
  
  return counter;//returns current position of device being displayed
}
//creates list of ON or OFF devices
void setsublist(Device Devices[8],Device subDevices[8],String sortstate){
  int i=0;
  int j=0;
  for(i;i<8;i++){
    if(Devices[i].state==sortstate){ //adds each device to subDevices if state is equal to state provided
      subDevices[j].ID = Devices[i].ID;
      subDevices[j].type = Devices[i].type; 
      subDevices[j].location = Devices[i].location ;
      subDevices[j].state =Devices[i].state;
      if((Devices[i].type=="S")||(Devices[i].type=="L")||(Devices[i].type=="T")){
        subDevices[j].PoworTemp =Devices[i].PoworTemp;
      }
      j++;
    }
  }
  for(j;j<8;j++){ //fills the rest of subDevices with empty Devices
    subDevices[j].ID = "X";
    subDevices[j].type = "X";
    subDevices[j].location = "X";
    subDevices[j].state ="X";
    if((Devices[i].type=="S")||(Devices[i].type=="L")||(Devices[i].type=="T")){
        subDevices[j].state ="X";
    }
  }
}
//adds new device
void addevice(Device Devices[8],String userinp,int count){
  int a = userinp.length();
  if (a>23){
    a=23;
  }
  //creates an object of Device and stores it in Devices Array
  Devices[count].ID = userinp.substring(2,5);
  Devices[count].type = userinp.substring(6,7); 
  Devices[count].location = userinp.substring(8,a);
  Devices[count].state ="OFF";
  if((userinp.substring(6,7)=="S")||(userinp.substring(6,7)=="L")){
    Devices[count].PoworTemp =0;
  }else if(userinp.substring(6,7)=="T"){
    Devices[count].PoworTemp =9;
  }
}
//sets power or temperature
void setpower(Device Devices[8],int newvalue,int pos){
  if((Devices[pos].type=="T")&& (newvalue>=9)&& (newvalue<=32)){
    lcd.clear();
    Devices[pos].setpowortemp(newvalue);
  }else if(((Devices[pos].type=="S")||(Devices[pos].type=="L"))&& (newvalue>=0)&& (newvalue<=100)){
    lcd.clear();
    Devices[pos].setpowortemp(newvalue);    
  }else{
    Serial.print("ERROR:"+userinp);
  } 
}
//checks if input is only letters if a!=0 or letters and numbers if a==0
bool isString(String inp,int a){
  bool valid=true;
  for(int i=0;i<inp.length();i++){
    if(isAlpha(inp[i])){
      debug_print("Letter");
      }
    else if((a==0)&&(isdigit(inp[i]))){
      debug_print("Number");
      }
    else{
      //Serial.println("invalid");
      valid=false;
    }
  }
  return valid;
}
//checks if input is a number
bool isNum(String inp){
  bool valid=true;
  for(int i=0;i<inp.length();i++){
    if(isdigit(inp[i])){
      debug_print("Number");
      }
    else{
      debug_print("Invalid");
      valid=false;
    }
  }
  return valid;
}
//checks if userinput is valid
bool isvalid(String userinp){
  bool isval = false;
  int a=userinp.length();
  if (userinp[0]=='A'){
    if ((userinp[1]=='-')&&(userinp[5]=='-')&&(userinp[7]=='-')&&(a>8)){
      //correct length
      if (a>23){
        a=23;
      }
      String location=userinp.substring(8,a);
      String id=userinp.substring(2,5);
      if ((userinp[6]=='S') ||(userinp[6]=='O')||(userinp[6]=='L')||(userinp[6]=='T')||(userinp[6]=='C')){
        // VALID TYPE
        if((isString(location,0))&&(isString(id,1))){
          isval= true;
        } 
      }else{
        //false invalid device type
        debug_print("invalid device type");
      }
    }else {
      //invalid request format
      debug_print("invalid request format");
    }
  }else if(userinp[0]=='S'){
    if ((userinp[1]=='-')&&(userinp[5]=='-')){
      String state=userinp.substring(6,a);
      if ((state=="OFF") || (state == "ON")){
        //VALID
        isval=true;
      }else{
        //invalid state
        debug_print("invalid state");
      }
    }else{
      //invalid request format
      debug_print("invalid request format");
    } 
  }else if (userinp[0]=='P'){
    if ((userinp[1]=='-')&&(userinp[5]=='-')&&(isNum(userinp.substring(6,a)))){
      //valid
      int power= userinp.substring(6,a).toInt();
      isval=true;
    }else{
      //invalid request format
      debug_print("invalid request format");
    }
  }else if (userinp[0]=='R'){
    if ((userinp[1]=='-')&&(sizeof(userinp)==6)){
      // valid
      isval= true;
    }else {}
  }else {
    //valid inp
    //invalid request format
    debug_print("invalid request format");
  }
  return isval;
}
//executes user input if isvalid returns true
int executeinp(Device Devices[8],String userinp,bool validinp,int count){
  if(validinp!=1){
          Serial.print("ERROR:"+userinp);
        }
        if((validinp==1)&&(userinp[0]!='A')&&(count==0)){
          Serial.print("ERROR:A Device Must Be Added First\n");
        }else if((validinp==1)&&(userinp[0]=='A')){
          int newc=searchdevices(Devices,userinp);//checks if Device ID already exists 
          if((count==8)&&(newc==-1)){
            Serial.print("ERROR:FULL, DELETE A DEVICE\n");
          }else{
            if(newc==-1){
              addevice(Devices,userinp,count);//adds new device to array
            }else{  
              addevice(Devices,userinp,newc);//updates Device details  
            }
            sortdevices(Devices);//sorts Devices
            lcd.clear();
            count=incrcount(Devices);//points to next availaable space
          }
        }else if((validinp==1)&&(userinp[0]=='S')) {
          setto=userinp.substring(6,userinp.length());
          int pos=searchdevices(Devices,userinp);//gets position of device if in array
          if (setto=="ON"){
            setto="ON ";//adds a space to end of ON
          }
          if(pos==-1){
           // Serial.print("Device Not Found\n");
           Serial.print("ERROR:"+userinp);
          }else{
            Devices[pos].setstate(setto);
          }         
          //SET DEVICE
        }else if((validinp==1)&&(userinp[0]=='P')&&(isNum(userinp.substring(6,userinp.length())))) {
          //POWER OUTPUT
          int power= userinp.substring(6,userinp.length()).toInt();
          int pos=searchdevices(Devices,userinp);
          if(pos==-1){
            debug_print("Device Not Found\n");
            Serial.print("ERROR:"+userinp);
          }else{
            setpower(Devices,power,pos);
          }
              
        }else if((validinp==1)&&(userinp[0]=='R')) {
          int pos=searchdevices(Devices,userinp);
          if(pos==-1){
            debug_print("Device Not Found\n");
            Serial.print("ERROR:"+userinp);
          }else{//sets device attributes to X 
            Devices[pos].ID = "X";
            Devices[pos].type = "X";
            Devices[pos].location = "X";
            Devices[pos].state ="X";
            sortdevices(Devices);//resorts devices
            lcd.clear();
            count=incrcount(Devices);//point to new next available space
          }           
        }
  return count;
}

String mesg;
void loop() {
  // put your main code here, to run repeatedly:
  Serial.begin(9600);
  switch(states){ //switch case between states
    case SYNCHRONISATION:
      lcd.setBacklight(5); //purple
      while(mesg != "X"){ 
        Serial.print('Q');
        delay(1000);
        if (Serial.available()){
          mesg= Serial.readString();
          mesg.trim();
        }
      }
      Serial.print("UDCHARS,HCI,FREERAM,SCROLL\n");//EXTENSIONS
      delay(100);
      lcd.setBacklight(7);
      states= NORMAL_DISPLAY;  
      for(int i=0;i<8;i++){//sets all devices to X (empty)
        Devices[i].ID = "X";
        Devices[i].type = "X";
        Devices[i].location = "X";
        Devices[i].state ="X";
      }
      break;
      
    case NORMAL_DISPLAY:
      //checks counter is not more than the devices, for if devices are removed while in ID_DISPLAY state
      if((counter>7-numofdevs(Devices,"X"))&&(counter!=0)){
        counter=7-numofdevs(Devices,"X");
      }   
      if(Serial.available()>0){
        userinp= Serial.readString();
        userinp.trim();
        userinp.toUpperCase();//inputs are not case sensitive
        validinp = isvalid(userinp);
        count=executeinp(Devices,userinp,validinp,count);
      }
      counter=dispdevice(Devices,counter);
      buttons=lcd.readButtons();
      counter=buttonpressed(buttons,counter,Devices);
      break;
    case OFF_DEVICES:
      setsublist(Devices,subDevices,"OFF");//creates array of OFF devices
      //checks counter is not more than the devices, for if devices are removed while in ID_DISPLAY state
      if((counteroff>numofdevs(Devices,"OFF")-1)&&(counteroff!=0)){
        counteroff=numofdevs(Devices,"OFF")-1;
      }
      lcd.setBacklight(3);
      if(Serial.available()>0){
        userinp= Serial.readString();
        userinp.trim();
        userinp.toUpperCase();
        validinp = isvalid(userinp);
        count=executeinp(Devices,userinp,validinp,count);
        setsublist(Devices,subDevices,"OFF");//updates array of OFF devices
      }
      counteroff=dispdevice(subDevices,counteroff);
      buttons=lcd.readButtons();
      counteroff=buttonpressed(buttons,counteroff,subDevices);
      break;

    case ON_DEVICES:
      setsublist(Devices,subDevices,"ON ");//creates array of ON devices
      //checks counter is not more than the devices, for if devices are removed while in ID_DISPLAY state
      if((counteron>numofdevs(Devices,"ON ")-1)&&(counteron!=0)){
        counteron=numofdevs(Devices,"ON ")-1;
      }
      lcd.setBacklight(2);
      if(Serial.available()>0){
        userinp= Serial.readString();
        userinp.trim();
        userinp.toUpperCase();
        validinp = isvalid(userinp);
        count=executeinp(Devices,userinp,validinp,count);
        setsublist(Devices,subDevices,"ON ");//updates array of ON devices
      }
      counteron=dispdevice(subDevices,counteron);
      buttons=lcd.readButtons();
      counteron=buttonpressed(buttons,counteron,subDevices);
      break;
    case ID_DISPLAY:
      if(millis()-checktime>1000&&(buttons& BUTTON_SELECT)){
        lcd.clear();
        while( buttons& BUTTON_SELECT){
          lcd.setBacklight(5);
          lcd.setCursor(1,0);
          lcd.print("F227110");
          lcd.setCursor(1,1);
          lcd.print("FREE RAM:");
          lcd.print(freeMemory());
          buttons=lcd.readButtons();
          if(Serial.available()>0){
            userinp= Serial.readString();
            userinp.trim();
            userinp.toUpperCase();
            validinp = isvalid(userinp);
            count=executeinp(Devices,userinp,validinp,count);
          }
        }
      }else if (buttons& BUTTON_SELECT){
      //lcd.clear();
      //while( buttons& BUTTON_SELECT){
        
        //}
      //}
      }else{
        lcd.clear();
      //}
      //sets state back to previous one
        if(normaldevices==true){
          counter=dispdevice(Devices,counter);  
          states=NORMAL_DISPLAY;      
        }else if(ondevices==true){
          states=ON_DEVICES;
        }else if(offdevices==true){
          states=OFF_DEVICES;
        }
      }
      
      break; 
  }
  
}
