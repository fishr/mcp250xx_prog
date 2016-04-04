/*
MCP250XX programmer

The MCP250XX series chip is a stand-alone CAN I/O expander
with GPIO and PWM (and optionally ADC).  It is One Time
Programmable EPROM to configure the CAN bus parameters, that
are stored to RAM on startup.  Once on-bus, these RAM registers
are modifiable through CAN messages.  Therefore, for proper bus
operation, the chip must be pre-programmed to run at the
desired bus settings.

The programming occurs via a high-voltage serial data and
clock transfer.

On the MCP250XX:
Pin 11 (RST):+13V
Pin 7 (VSS): GND
Pin 14 (VDD): Arduino Pin 8
Pin 5 (DATA): Arduino Pin 11
Pin 6 (CLOCK): Ardunio Pin 13

An Arduino: voltage divider +13v->10k->Pin 7->18k->gnd

Author: Ryan Fish
Email: ryanfishme@gmail.com
Date: 4/2/2016
*/

//1. all pins low
//2. RST high (13V)
//3. VDD high
//4. Increment address 16 times (address table has 0x10 offset)
//5. get at it


//Commands latched on falling edge of clock
//Data must be setup at least 100ns before
//Data must be held at least 100ns after
//1uS delay (clock still low) between command and data
//or data and command
//commands are 6 bits (6 falling clock edges)
//data are 14 bits (16 falling clock edges including start and 
//stop bits)
//data is LSB first
//read operations, data is output after rising edge of clock

#define LOAD_CONF 0b000000 //requires 0,data(14),0
#define LOAD_DATA 0b000010 //requires 0,1,1,0,1,0,0,data(8),0
#define READ_DATA 0b000100 //requires 0,data(14),0
#define INCR_ADDR 0b000110
#define BGN_PROG  0b001000
#define END_PROG  0b001110

#define SETUP_MICROS 3
#define HOLD_MICROS 3
#define PROG_PERIOD_MICROS 10
#define FRONT_PORCH_MICROS (PROG_PERIOD_MICROS-(SETUP_MICROS + HOLD_MICROS))/2
#define BACK_PORCH_MICROS FRONT_PORCH_MICROS

#define MY_SCK 13
#define MY_SDO 11
#define MY_VDD 8
#define MY_VPP 7

//#define TEST
//#define READ
//#define READ_ALL
//#define TEST_WRITE
#define CONF
//#define DATA
//#define VERIFY
//#define COMBINED

byte registers[]={
      0x00,0x00,0x00,0xFF,0x70,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x01,0x12,0x02,0x00,0x0F,
        0xF0,0xAD,0xFF,0xFF,0x0F,0x08,0xFF,0xF8,0xFF,0x10,0xFF,0xFF,0xFF,0x20,0xFF,0xFF,0xF7,0x00,0xFF,
        0xFF,0xF0,0x00,0xFF,0xFF,0xF7,0x20,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F};
        
byte conf_word = 0x03;

void setup_pins(void){
  pinMode(MY_VDD, OUTPUT);
  pinMode(MY_SCK, OUTPUT);
  pinMode(MY_VPP, INPUT);
  
  digitalWrite(MY_VDD, LOW);
  digitalWrite(MY_SCK, LOW);
}

void set_data_write(void){
  pinMode(MY_SDO, OUTPUT);
  digitalWrite(MY_SDO, LOW);
}

void set_data_read(void){
  pinMode(MY_SDO, INPUT);
}

void wait_voltage(void){
  while(digitalRead(MY_VPP)==LOW){
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Done");
  delay(1000);
}

void init_prog(void){
  digitalWrite(MY_VDD, HIGH);
  delay(1);
}

void shift_out(unsigned int data, byte bits){
  for(int i=0; i<bits; i++){
    shift_single_out(data & 0x0001);
    data=data>>1;
  }
}
  
void shift_single_out(unsigned int bit0){
  digitalWrite(MY_SCK, HIGH);
  delayMicroseconds(FRONT_PORCH_MICROS);
  digitalWrite(MY_SDO,bit0);
  delayMicroseconds(SETUP_MICROS);
  digitalWrite(MY_SCK, LOW);
  delayMicroseconds(HOLD_MICROS);
  digitalWrite(MY_SDO, LOW);
  delayMicroseconds(BACK_PORCH_MICROS);
}
  

void write_command(byte cmd){
  shift_out(cmd, 6);
  delayMicroseconds(2);
}

void exit(void){
  Serial.println("Exiting...");
  pinMode(MY_VDD, INPUT);
  pinMode(MY_SCK, INPUT);
  pinMode(MY_VPP, INPUT);
  pinMode(MY_SDO, INPUT);

  while(1){
    delay(1);
  }
}

void send_(unsigned int data){
  //pad LSbit as a zero
  data=data<<1;
  //verify MSbit is zero
  if(data & 0x8000){
    Serial.print("Invalid data: ");
    Serial.print(data, BIN);
    Serial.println("check register settings.");
    exit();
  }
  write_command(LOAD_DATA);
  shift_out(data,16);
  write_command(BGN_PROG);
  delayMicroseconds(110);
  write_command(END_PROG);
  delayMicroseconds(110);
}

void send_conf(byte wrd){
  unsigned int data = 0x3FF8 | wrd;
  send_(data);
}

void send_data(byte data){
  unsigned int out=0x3400 | data;
  send_(out);
}

byte shift_in_single(void){
  byte data=0;
  clock_pulse1();
  if(digitalRead(MY_SDO)!=LOW)
    data=1;
  clock_pulse2();
  return data;
}

void clock_pulse1(void){
  delayMicroseconds(FRONT_PORCH_MICROS);
  delayMicroseconds(SETUP_MICROS);
  digitalWrite(MY_SCK, HIGH);
  delayMicroseconds(HOLD_MICROS);
}

void clock_pulse2(void){
  delayMicroseconds(BACK_PORCH_MICROS);
  digitalWrite(MY_SCK, LOW);
}

void clock_pulse(void){
  clock_pulse1();
  clock_pulse2();
}

unsigned int shift_in(){
  write_command(READ_DATA);
  
  set_data_read();
  
  clock_pulse();
  
  unsigned int data=0;
  for(int i=0; i<14; i++){
    data = data | (shift_in_single()<<i);
  }
  
  clock_pulse();
  
  set_data_write();
  
  unsigned int data_high = data>>8;
  if(data_high != 0b00110100){
    Serial.print("Data read corruption: 0b");
    Serial.println(data, BIN);
    exit();
  }
  return data & 0x00FF;
}  
  
void verify_data(byte data){
  unsigned int in = shift_in();
  if(in==data){
    return;
  }
  Serial.print("Data verification error. Got 0b");
  Serial.print(in, BIN);
  Serial.print(" expected 0b");
  Serial.println(data, BIN);
  exit();
}

void verify_conf(byte wrd){
  unsigned int in = shift_in() & 0x0007;
  if(in==wrd){
    return;
  }
  Serial.print("Data verification error. Got 0b");
  Serial.print(in, BIN);
  Serial.print(" expected 0b");
  Serial.println(wrd, BIN);
  exit();
}

void go_to(byte address){
  for(int i=0; i<(16+address);i++){
    write_command(INCR_ADDR);
  }
}

void setup(){
  Serial.begin(9600);
  Serial.println("Welcome to the MCP250XX Programmer");
  Serial.println("Configuring Pins...");
  setup_pins();
  set_data_write();
  
  Serial.println("Waiting for programming voltage...");
  wait_voltage();
  Serial.println("Applying VDD");
  init_prog();
  
#if defined(CONF)
  write_command(LOAD_CONF);

  for (int i=0; i<7;i++){
    write_command(INCR_ADDR);
  }
  for(int i=0; i<100; i++){
    send_conf(conf_word);
  }
  
  verify_conf(conf_word);
  Serial.println("Conf verification complete!");
  
  return;
  
#elif defined(DATA)
  go_to(0x00);
  
  for(int i=0; i<sizeof(registers); i++){
    for(int j=0; j<100; j++){
      send_data(registers[i]);
    }
    
    verify_data(registers[i]);
    
    Serial.print("Verified register: 0x");
    Serial.print(i, HEX);
    Serial.print("  Data: 0b");
    Serial.println(registers[i],BIN);
    
    write_command(INCR_ADDR);
  }
  return;
   
#elif defined(TEST_WRITE)
  byte address = 0x44;
  go_to(address);//USERF
  
  byte progdata = 0b01000110;
  Serial.print("Address: 0x");
  Serial.print(address, HEX);
  
  for(int i=0; i<100; i++){
    send_data(progdata);
  }
  
  Serial.print("   Read Value: 0b");
  Serial.println(shift_in(), BIN);
  
  verify_data(progdata);
  Serial.println("Data Verified!");
  
  return;  
#elif defined(READ)
  byte address = 0x10;
  go_to(address);//stcon
  
  Serial.print("Address: 0x");
  Serial.print(address, HEX);
  Serial.print("   Read Value: 0b");
  Serial.println(shift_in(), BIN);
  return;  
#elif defined(READ_ALL)
  go_to(0);
  for(int i=0; i<0x46; i++){
    Serial.print("Address: 0x");
    Serial.print(i, HEX);
    Serial.print("   Read Value: 0b");
    Serial.println(shift_in(), BIN);
    write_command(INCR_ADDR);
  }
  return;  
#elif defined(VERIFY)
  go_to(0);
  for(int i=0; i<sizeof(registers); i++){
    Serial.print("Address: 0x");
    Serial.print(i, HEX);
    verify_data(registers[i]);
    Serial.print("   Verified Value: 0b");
    Serial.println(registers[i], BIN);
    write_command(INCR_ADDR);
  }
  return;
#else
  Serial.println("Please define a mode");
  return;
#endif
}
void loop(){
  exit();
}
  
