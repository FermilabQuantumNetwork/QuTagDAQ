# QUTAGDAQ

****************************************
Installation
****************************************
1) Install the QUTAG software to any desired location on your linux PC. The QUTAG software that we are currently using is saved in the QUTAGSoftware/ subdirectory. 

You may need to install other pre-requisite packages. Follow the installation instructions from the QUTAG software.

2) clone the custom FQNET DAQ package into the "userlib/" subdirectory of the QUTAG software:

git clone git@github.com:FQNet/QUTAGDAQ.git QUTAG-LX64-V1.1.6/userlib/QUTAGDAQ



****************************************
Running the DAQ
****************************************
1) To initiate the DAQ, simply run the script provided: ./RunFQNETDAQ.sh

The script sets the LD_LIBRARY_PATH to locate the QUTAG software libraries. Please set this to the location that you installed your QUTAG software at. A default location is currently specified. 

The script initiates some relevant parameters described below, and then executes the FQNETDAQ program that will initiate the data taking. 

Trigger Condition:
Currently only events which have a coincidence between 2 or more channels are written to disk. All other events with only a single channel having a signal is discarded. 

****************************************
Parameters
****************************************

1) CoincidenceWindow
This sets the size of the time window that you will consider as one "event" for the DAQ program. The units are in ps. The program can also automatically calculate an appropriate coincidence window, if you specify -1 for this parameter. 

2) CollectTime
This parameters sets the time window for each readout cycle. The parameter is ideally set to be such that the total data rate times this time window is between 40-60% of the total buffer size (ie. ~50000)

3) DAQTOTALTime
This parameter sets the amount of time you want your current data acqusition to run. 

****************************************
Output
****************************************

The output file is currently written to a default filename: data_test.txt
The output format is a text file with 4 columns for the timestamps of each of the 4 channels. 