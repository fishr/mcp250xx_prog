# mcp250xx_prog
Arduino programming code for MCP250XX EPROM


#Canbus commands on Raspi
##set up interface
sudo ip link set can0 type cn bitrate 500000
sudo ifconfig can0 up
##inspect interface health
ip -s -d link show can0
##read messages
candump any,0:0,#FFFFFFFF //interface (typically specifying can0 would also work), message identifier mask, data bits mask
##write to the mcp250xx
###turn on led on gpio6 with mcp receiving on 7E8
cansend can0 7E8#1E40FF (write addr command: addr, mask, data)
