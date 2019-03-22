export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:~/QUTAG-LX64-V1.1.6

#####################################################################
#Configuration Parameters
#####################################################################

#Sets the coincidence window in [ps]. If you want the program to
#calculate the appropriate coincidence window automatically, set 
#it to -1
CoincidenceWindow=30000

#Don't change this
MasterRate=1

#Number of [us] for each readout cycle.
#This number times the signal rate should be below the 
#max buffer size, or else you will lose data
CollectTime=10000

#Total time that you want to collect data for in [s]
DAQTotalTime=10


#####################################################################
#Internal Calculations
#####################################################################
#Calculate number of readout cycles to take data for DAQTotalTime
CollectRounds=$(($DAQTotalTime*1000000/$CollectTime))

#echo $CoincidenceWindow $MasterRate $CollectTime $CollectRounds

~/QUTAG-LX64-V1.1.6/userlib/QUTAGDAQ/FQNETDAQ signal $CoincidenceWindow $MasterRate $CollectTime $CollectRounds

