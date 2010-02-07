/*
Copyright (C) 2005  Georgia Public Library Service 
Bill Erickson <billserickson@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef OSRF_CHAT_H
#define OSRF_CHAT_H

#ifdef __cplusplus
extern "C" {
#endif

struct osrfChatNodeStruct;
typedef struct osrfChatNodeStruct osrfChatNode;

struct osrfChatServerStruct;
typedef struct osrfChatServerStruct osrfChatServer;

/* @param s2sSecret The Server to server secret.  OK to leave NULL if no
	server to server communication is expected
*/
osrfChatServer* osrfNewChatServer( const char* domain, const char* s2sSecret, int s2sport );

int osrfChatServerConnect( osrfChatServer* cs, int port, int s2sport, char* listenAddr );

int osrfChatServerWait( osrfChatServer* server );
void osrfChatServerFree( osrfChatServer* cs );

#ifdef __cplusplus
}
#endif

#endif


