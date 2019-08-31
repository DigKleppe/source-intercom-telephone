/*
 * connections.h
 *
 *  Created on: Feb 18, 2019
 *      Author: dig
 */

#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_


void startConnectionThreads( void);
extern uint32_t connectCntrBaseFloor, connectCntrFirstFloor;  // connections to doorstations

void UDPSend( floor_t);
void UDPsendMessage (char * message );

#endif /* CONNECTIONS_H_ */
