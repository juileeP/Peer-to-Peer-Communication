#include <boost/foreach.hpp>
#include <boost/cstdint.hpp>

#ifndef foreach
#define foreach BOOST_FOREACH
#endif

#include <iostream>
#include "time.h"
#include <list>

using namespace std;

#include "target.h"

#ifdef _WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif


//#include <sal.h>

#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include "process.h"

#ifndef socket_t
#define socklen_t int
#endif

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
// #pragma comment(lib, "ws2.lib") // for windows mobile 6.5 and earlier
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <math.h>
#include "helperClasses.h"
#include "someFunctions.h"

#define TARGET_NUMBER_OF_NEIGHBORS 4
// function ideas

void removeOldNeighbors(list<NeighborInfo> &bidirectionalNeighbors,list<NeighborInfo> &unidirectionalNeighbors,bool searchingForNeighborFlag,NeighborInfo &tempNeighbor);
void sendHelloToAllNeighbors(bool searchingForNeighborFlag,list<NeighborInfo> &bidirectionalNeighbors,list<NeighborInfo> &unidirectionalNeighbors,NeighborInfo &tempNeighbor,HostId &thisHost,UdpSocket &udpSocket);
void receiveHello(Packet &pkt,bool &searchingForNeighborFlag,list<NeighborInfo> &bidirectionalNeighbors,list<NeighborInfo> &unidirectionalNeighbors,NeighborInfo &tempNeighbor,HostId &thisHost);

int main(int argc, char* argv[])
{

    #ifdef _WINDOWS

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2,1),&wsaData) != 0)
    {

        printf("WSAStartup failed: %d\n",GetLastError());

        return(1);
    }

    #endif

    bool searchingForNeighborFlag = false;
    list<NeighborInfo> bidirectionalNeighbors;
    list<NeighborInfo> unidirectionalNeighbors;
    list<HostId> allHosts;
    NeighborInfo tempNeighbor;
    HostId thisHost;

    if (argc!=3)

    {
        printf("usage: MaintainNeighbors PortNumberOfThisHost FullPathAndFileNameOfListOfAllHosts\n");
            return 0;
    }

    #ifdef _WINDOWS
    srand( _getpid() );
    #else
    srand( getpid() );
    #endif

    fillThisHostIP(thisHost);
    thisHost._port = atoi(argv[1]);
    readAllHostsList(argv[2],allHosts, thisHost);

    printf("Running on ip %s and port %d\n",thisHost._address,thisHost._port);
    printf("press any key to begin\n");

    char c=getchar();

    time_t lastTimeHellosWereSent;
    time( &lastTimeHellosWereSent );
    time_t currentTime;
    time_t lastTimeStatusWasShown;
    time(&lastTimeStatusWasShown);

    UdpSocket udpSocket;

    if (udpSocket.bindToPort(thisHost._port)!=0) 
    {
        cout <<"bind failed\n";
        exit(-1);
    }

    Packet pkt;
    while (1)

    {
        pkt.getPacketReadyForWriting();
        HelloMessage helloMessage;

		//rececive Part
		//if packet is present we go into this loop

        if (udpSocket.checkForNewPacket(pkt, 2) > 0) {
            helloMessage.getFromPacket(pkt);       
            
            //Check if sender is in unidirectional list
            bool isUniDir = false;
            foreach(NeighborInfo &neighbor, unidirectionalNeighbors) {
                    if (neighbor.hostId == helloMessage.source)
                    {
                        neighbor.updateTimeToCurrentTime();
                        isUniDir = true;
                        foreach(HostId &host, helloMessage.neighbors)
                        {
                                if (host == thisHost) {
           
                                    addOrUpdateList(neighbor, bidirectionalNeighbors);
                                    unidirectionalNeighbors.remove(neighbor);
                                    break;
                                }        
                        }    
                         break;
                    }
            }

           
           //check if sender is in bidirectional list
           bool isBiDir = false;
           foreach(NeighborInfo &neighbor, bidirectionalNeighbors) {
                    if (neighbor.hostId == helloMessage.source)
                    {
                        neighbor.updateTimeToCurrentTime();
                        isBiDir = true;
                        bool isNeighbor = false;

                        //recently heard
                        foreach(HostId &host, helloMessage.neighbors)
                        {
                                if (host == thisHost) {
                                    isNeighbor = true;
                                    break;
                                }
                        }

                        //If node is not a neighbor, make it unidirectional
                        if (isNeighbor == false)
                        {
                             neighbor.updateTimeToCurrentTime();
                             addOrUpdateList(neighbor, unidirectionalNeighbors);
                             bidirectionalNeighbors.remove(neighbor);
                                
                        }
                        break; 
                    }
           } 


           //If sender is a tempNeighbor
           if ( isUniDir == false && isBiDir == false) 
           {
                searchingForNeighborFlag = false;
                bool isNeighbor = false;
                foreach(HostId &host, helloMessage.neighbors)
                {
                        if (host == thisHost) {
                             NeighborInfo neighbor;
                             neighbor.hostId = helloMessage.source;
                             neighbor.updateTimeToCurrentTime();
                             addOrUpdateList(neighbor, bidirectionalNeighbors);
                             isNeighbor = true;
                             break;
                        }
                }

                if (isNeighbor == false)
                {
                    NeighborInfo neighbor;
                    neighbor.hostId = helloMessage.source;
                    neighbor.updateTimeToCurrentTime();
                    addOrUpdateList(neighbor, unidirectionalNeighbors);
                }
           }
        } // end of if loop for check new packets


           //Searching for a tempNeighbor
           if ((bidirectionalNeighbors.size() + unidirectionalNeighbors.size()) < TARGET_NUMBER_OF_NEIGHBORS && searchingForNeighborFlag ==false)
           {  
            searchingForNeighborFlag =true;
            tempNeighbor = selectNeighbor(bidirectionalNeighbors, unidirectionalNeighbors, allHosts, thisHost);
            tempNeighbor.updateTimeToCurrentTime();
            //printf("\n total no of neighbors is ",(bidirectionalNeighbors.size() + unidirectionalNeighbors.size()));
            tempNeighbor.show();
            }
           

            foreach(NeighborInfo &neighbor, bidirectionalNeighbors)
           {
                time( & currentTime );
                if (difftime(currentTime, neighbor.timeWhenLastHelloArrived)>=40)
				{
                    neighbor.show();
                    removeFromList(neighbor, bidirectionalNeighbors);
                    break;
                }
           } 

            foreach(NeighborInfo &neighbor, unidirectionalNeighbors)
           {
                time( & currentTime );
                if (difftime(currentTime, neighbor.timeWhenLastHelloArrived)>=40)
                {
                    printf("Culprit uni....\n");
                    unidirectionalNeighbors.remove(neighbor);
                    break;
                }
           }

           time( & currentTime);
           if ((difftime(currentTime, tempNeighbor.timeWhenLastHelloArrived) >= 40) && searchingForNeighborFlag == true)
           {
               searchingForNeighborFlag = false;
           }

		   //HelloMessage prepare
            time( & currentTime );
            if (difftime(currentTime, lastTimeHellosWereSent)>=10)
            { 
            pkt.getPacketReadyForWriting(); 
            HelloMessage helloMessage1;     
            helloMessage1.type = HELLO_MESSAGE_TYPE;
            helloMessage1.source = thisHost;
            lastTimeHellosWereSent = currentTime;

            //Fill up pkt with neighbors list
            foreach(NeighborInfo &neighborInfo, bidirectionalNeighbors)
            {
                    helloMessage1.addToNeighborsList(neighborInfo.hostId);
            }

            foreach(NeighborInfo &neighborInfo, unidirectionalNeighbors)
            {
                     helloMessage1.addToNeighborsList(neighborInfo.hostId);
            }

            helloMessage1.addToPacket(pkt);

            //Send hellos to Bi, Uni and tempNeigbhor
            foreach(NeighborInfo &neighborInfo, bidirectionalNeighbors)
            {
                    udpSocket.sendTo(pkt, neighborInfo.hostId);
                   
            }

            foreach(NeighborInfo &neighborInfo, unidirectionalNeighbors)
            {
                    udpSocket.sendTo(pkt,neighborInfo.hostId);
                 
            }

            if (searchingForNeighborFlag) {
                udpSocket.sendTo(pkt, tempNeighbor.hostId);
            }
            }

            // every 10 seconds, print out the status
            time( & currentTime );
            if (difftime(currentTime, lastTimeStatusWasShown)>=10) {            
                time( & lastTimeStatusWasShown );
                showStatus( searchingForNeighborFlag, 
                            bidirectionalNeighbors, 
                            unidirectionalNeighbors, 
                            tempNeighbor, 
                            thisHost,
                            TARGET_NUMBER_OF_NEIGHBORS);
                  
             }

     } // end of while loop

}
