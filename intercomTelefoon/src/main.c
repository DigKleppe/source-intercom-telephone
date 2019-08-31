#include <gst/gst.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "telefoon.h"
#include "videoThread.h"
#include "audioTransmit.h"
#include "audioReceiveThread.h"
#include "ringThread.h"
#include "connections.h"
#include "testThread.h"
#include "updateThread.h"
#include "io.h"
#include "keys.h"

#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdlib.h>

/* in root :
./configure \
   --host=arm-none-linux-gnueabihf\
   PATH="/home/dig/nanoPiFire2A/tools/4.9.3/bin/:$PATH"
*/


/*
 ssh root@192.168.2.9

 gst-inspect-1.0
 audio
 tx
 gst-launch-1.0 audiotestsrc ! "audio/x-raw,rate=24000" ! vorbisenc ! rtpvorbispay config-interval=1 ! rtpstreampay ! udpsink port=5001 host=192.168.2.255

 255 is broadcast

 gst-launch-1.0 audiotestsrc ! "audio/x-raw,rate=24000" ! autoaudiosink
 gst-launch-1.0 audiotestsrc ! "audio/x-raw,rate=24000" ! audioresample ! alsasink device=hw:3


 card 1 =
 gst-launch-1.0 alsasrc device=hw:3 ! audioconvert ! "audio/x-raw,rate=24000" ! audioresample ! alsasink device=hw:0

 rx
 gst-launch-1.0 udpsrc port=5001 do-timestamp=true ! "application/x-rtp-stream,media=audio,clock-rate=24000,encoding-name=VORBIS" ! rtpstreamdepay ! rtpvorbisdepay ! decodebin ! audioconvert ! audioresample ! autoaudiosink
 gst-launch-1.0 udpsrc port=5000 do-timestamp=true ! "application/x-rtp-stream,media=audio,clock-rate=24000,encoding-name=VORBIS" ! rtpstreamdepay ! rtpvorbisdepay ! decodebin ! audioconvert ! audioresample ! autoaudiosink
 Zie Vorbis-receive.txt
 https://help.ubuntu.com/community/UbuntuStudio/UsbAudioDevices
 ssh: ssh pi@192.168.2.7  pi pi

 cat /proc/asound/cards
 alsa output devices: aplay -l
 alsa input devices: arecord --list-devices

 https://superuser.com/questions/53957/what-do-alsa-devices-like-hw0-0-mean-how-do-i-figure-out-which-to-use

 https://wiki.audacityteam.org/wiki/USB_mic_on_Linux
 alsamixer -c 1  (= card 1)

4-6-19
alsamixer USB audio: Speaker 6(-39db )
Capture  51 (19.5db)

 sudo alsactl store

 gst-launch-1.0 -v audiotestsrc !  alsasink
 gst-launch-1.0 filesrc location=audio8k16S.wav  ! wavparse ! audioconvert ! audioresample ! alsasink
 gst-launch-1.0 filesrc location=audio8k16S.wav  ! wavparse  ! deinterleave name=d  d.src0 ! audioconvert ! audioresample ! alsasink
 gst-launch-1.0 filesrc location=audio8k16S.wav ! decodebin ! audioconvert ! "audio/x-raw,channels=2" ! deinterleave name=d  d.src_0 ! queue ! audioconvert ! vorbisenc ! oggmux ! filesink location=channel1.ogg  d.src_1 ! queue ! audioconvert ! vorbisenc ! oggmux ! filesink location=channel2.ogg
 gst-launch-1.0 filesrc location=audio8k16S.wav  ! wavparse  ! audioconvert ! "audio/x-raw,channels=2" ! deinterleave name=d  d.src_0 ! queue ! audioconvert ! alsasink
 gst-launch-1.0 filesrc location=audio8k16S.wav ! decodebin ! audioconvert ! "audio/x-raw,channels=2" ! deinterleave name=d  d.src_0 ! queue ! audioconvert ! vorbisenc ! oggmux ! filesink location=channel1.ogg  d.src_1 ! queue ! audioconvert ! autoaudiosink

 video

 met jpeg ipv h264:
 gst-launch-1.0 --gst-debug -e -v v4l2src device=/dev/video0 ! image/jpeg,framerate=30/1,width=800,height=600 ! rtpjpegpay ! udpsink port=5000 host=192.168.2.255
 gst-launch-1.0 --gst-debug -e -v v4l2src device=/dev/video0 ! image/jpeg,framerate=20/1,width=800,height=600 ! jpegdec ! videoconvert ! autovideosink

 gst-launch-1.0 -v udpsrc port=5001 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! videoconvert !  videoflip method=counterclockwise ! nxvideosink
 gst-launch-1.0 -v udpsrc port=5000 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! videoconvert !  autovideosink
 gst-launch-1.0 udpsrc port=5001 ! rtph264depay  video/x-raw,format=I420,framerate=30/1,width=800,height=600 ! autovideosink

 */

volatile threadStatus_t videoThreadStatus = { false, false, NULL };
volatile threadStatus_t audioTransmitThreadStatus = { false, false, NULL };
volatile threadStatus_t audioReceiveThreadStatus = { false, false, NULL };
volatile threadStatus_t testModeThreadStatus = { false, false, NULL };
volatile threadStatus_t ringThreadStatus = { false, false, NULL };
volatile threadStatus_t messageThreadStatus = { false, false, NULL };
volatile threadStatus_t updateThreadStatus = { false, false, NULL };

pthread_t videoThreadID;
pthread_t ringThreadID;
pthread_t audioTransmitID;
pthread_t audioReceiveID;
pthread_t serverThreadID;
pthread_t testModeThreadID;
pthread_t keysThreadID;
pthread_t messageThreadID;
pthread_t updateThreadID;

receiveData_t receiveDataBaseFloor, receiveDataFirstFloor;
transmitData_t transmitData;

int bellKey;
int microCardNo, speakerCardNo, houseNo;
float ringVolume = 1.0;
floor_t floorID = BASE_FLOOR;

#define TESTTHREADS 1  // audio and video streams on

void getIpAddress(char * dest) {

	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	/* I want to get an IPv4 IP address */
	ifr.ifr_addr.sa_family = AF_INET;

	/* I want IP address attached to "eth0" */
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);

	ioctl(fd, SIOCGIFADDR, &ifr);

	close(fd);

	/* display result */
	sprintf(dest, "%s\n", inet_ntoa(((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr));
}
/*
//etc/network/interfaces

auto lo
iface lo inet loopback
auto eth0
iface eth0 inet static
address 192.168.2.130
netmask 255.255.255.0
gateway 192.168.2.254
nameserver 192.168.2.254
nameserver 8.8.8.8
nameserver 127.0.0.53
dns-search lan
 */

void saveIPaddresss(int houseNo ){
	FILE *fptr;
	FILE *fptr2;
	ssize_t result;
	char str[50];
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	fptr = fopen("/etc/network/interfaces","r");
	fptr2 = fopen("/etc/network/temp","w");

	if((fptr == NULL) || (fptr2 == NULL))
	{
		printf("File open Error !\n");
		return;
	}
	do {
		read = getline(&line, &len, fptr);
		if ( read != -1) {
			if ( strstr( line,"address") > 0)
				fprintf( fptr2, "address %s%d\n", BASEADDRESS, 100+houseNo);// new ip = 100+ houseno
			else
				fprintf( fptr2, "%s", line);
		}
	} while (read > 0 );
	free(line);

	fclose(fptr);
	fclose(fptr2);
	remove("/etc/network/interfaces");
	rename("/etc/network/temp","/etc/network/interfaces");

	return;
}



// check ipaddress xxx.xxx.xxx.nnn   nnn= houseno + 100

void getHouseNo() {
	char ipAddress[17];
	int val;
	getIpAddress(ipAddress);
	int n = sscanf(ipAddress, "%d.%d.%d.%d", &val, &val, &val, &val);
	if (val >= 100)
		houseNo = val - 100;
}

void saveSettings() {

	FILE *fptr;
	fptr = fopen(SETTINGSFILENAME,"w");
	if(fptr == NULL)
	{
		printf("File write open Error!\n");
		return;
	}
	fprintf(fptr,"ringvolume:%d",(int) (ringVolume));
	fclose(fptr);
	return;
}

void loadSettings(){
	FILE *fptr;
	int temp;
	int n;
	fptr = fopen(SETTINGSFILENAME,"r");
	if(fptr == NULL)
	{
		printf("File read Error!\n");
		return;
	}
	n = fscanf(fptr,"ringvolume:%d",&temp);
	if ( n == 1)
		ringVolume = (float)temp;
	else
		ringVolume = 10;

	fclose(fptr);
	return;
}

int detectAudioCardNo(char * cardName) {
	bool found = false;
	GstElement *audioSource;
	char *card;
	char deviceName[15];
	int n;

	audioSource = gst_element_factory_make("alsasrc", "audioSource");

	for (n = 0; n < 5 && !found; n++) { // find card with name
		sprintf(deviceName, "hw:%d", n);
		g_object_set(audioSource, "device", deviceName, NULL);
		g_object_get(audioSource, "card-name", &card, NULL);
		if (!(card == (char *) NULL)) {
			if (strncmp(card, cardName, strlen(cardName)) == 0) {
				found = true;
			}
			g_free(card);
		}
	}
	gst_object_unref(audioSource);

	if (!found) {
		return -1;
	} else
		return n - 1;

}
//https://randu.org/tutorials/threads/

int init(void) {

	int result;
	initIo();
	getHouseNo();
	transmitData.softwareversion = SOFTWAREVERSION;
	loadSettings();

	speakerCardNo = detectAudioCardNo(SPEAKER_CARD_NAME1);
	if (speakerCardNo == -1) {
		speakerCardNo = detectAudioCardNo(SPEAKER_CARD_NAME2);
		if (speakerCardNo == -1) {
			printf("%s card not found \n\r", SPEAKER_CARD_NAME1);
			speakerCardNo = -1;
		}
	}
	audioReceiveThreadStatus.cardno = speakerCardNo;  // in telefoon we have 1 card
	audioTransmitThreadStatus.cardno = speakerCardNo; // if -1 then no mic  present;
	testModeThreadStatus.cardno = speakerCardNo;

	audioReceiveThreadStatus.info = ( void *) &floorID;
	audioTransmitThreadStatus.info = ( void *) &floorID;
	videoThreadStatus.info = ( void *) &floorID;

	// test
#ifdef TESTTHREADS
	pthread_create(&videoThreadID, NULL, &videoThread, (void *) &videoThreadStatus);
	backLightOn();

	result = pthread_create(&audioTransmitID, NULL, &audioTransmitThread, (void *) &audioTransmitThreadStatus);
	if (result == 0) {
		printf("audioTransmitThread created successfully.\n");
	} else {
		printf("audioTransmitThread not created.\n");
		return -1;
	}

	result = pthread_create(&audioReceiveID, NULL, &audioReceiveThread, (void *) &audioReceiveThreadStatus);
	if (result == 0) {
		printf("audioReceiveThread created successfully.\n");
	} else {
		printf("audioReceiveThread not created.\n");
		return -1;
	}
#endif

	result = pthread_create(&keysThreadID, NULL, &keysThread, NULL);
	if (result == 0)
		printf("keysThread created successfully.\n");
	else {
		printf("keysThread not created.\n");
		return -1;
	}

	result = pthread_create(&updateThreadID, NULL, &updateServerThread, (void *)&updateThreadStatus);
	if (result == 0)
		printf("updateServerThread created successfully.\n");
	else {
		printf("updateServerThread not created.\n");
		return -1;
	}

	startConnectionThreads();

			if ( keysRT & KEY_HANDSET) { // then horn is off, enter testmode
				result = pthread_create(&testModeThreadID, NULL, &testModeThread, (void *) &testModeThreadStatus);
				if (result == 0)
					printf("testThread created successfully.\n");
				else {
					printf("testThread not created.\n");
					return -1;			if ( keysRT & KEY_HANDSET) { // then horn is off, enter testmode
						result = pthread_create(&testModeThreadID, NULL, &testModeThread, (void *) &testModeThreadStatus);
						if (result == 0)
							printf("testThread created successfully.\n");
						else {
							printf("testThread not created.\n");
							return -1;
						}
					}

				}
			}

	/*creating thread*/
	return 0;
}

int main(int argc, char *argv[]) {
	int result, n;
	time_t startTime, now, startCall, lastSec;
	uint32_t lastConnectCntr1=0,lastConnectCntr2=0;
	uint32_t serverTimeoutTimerBaseFloor= CONNECTIONTIMEOUT,serverTimeoutTimerFirstFloor=CONNECTIONTIMEOUT;
	status_t status = STATUS_SHOW_STARTUPSCREEN;
	backLightOff();

	gst_init(&argc, &argv);
	init();
	startTime = time(NULL);
	char message[100];
	int subStatus = 0;
	messageThreadStatus.info = message;

#ifdef TESTTHREADS
	status = 999;
#endif

	while (1) {
		usleep(10000);
		now = time(NULL);
		transmitData.keys = keysRT;

		if ( lastSec != now ){
			lastSec = now;
			transmitData.uptime = now - startTime; // statistics
			if (connectCntrBaseFloor != lastConnectCntr1) {
				lastConnectCntr1 = connectCntrBaseFloor;
				serverTimeoutTimerBaseFloor = 0;
			}
			else
				serverTimeoutTimerBaseFloor++;

			if (connectCntrFirstFloor != lastConnectCntr2) {
				lastConnectCntr2 = connectCntrFirstFloor;
				serverTimeoutTimerFirstFloor = 0;
			}
			else
				serverTimeoutTimerFirstFloor++;
		}
		switch (status) {
		case STATUS_SHOW_STARTUPSCREEN:
			switch (subStatus) {
			case 0:
				n = sprintf(message, "\rHuisnummer: %d\r\n\n", houseNo);
				sprintf(&message[n], "Softwareversie: %1.1f", SOFTWAREVERSION * 0.1);
				backLightOn();
				pthread_create(&messageThreadID, NULL, &messageThread, (void *) &messageThreadStatus);
				subStatus++;
				break;
			case 1:
				//	if ( ( transmitData.uptime > 5) && (! (keysRT & KEY_HANDSET))) { // then horn is on
				if (!(keysRT & KEY_HANDSET)) { // then horn is on
					backLightOff();
					messageThreadStatus.mustStop = true;
					if (!messageThreadStatus.isRunning) {
						subStatus = 0;
						status = STATUS_IDLE;
					}
				}
				break;
			}
			break;

			case STATUS_IDLE:
				if ((keysRT & KEY_HANDSET) && !videoThreadStatus.isRunning) {  //handset off  and not showing screen?
					status = STATUS_SETUP;
					subStatus = 0;
					usleep(100000); // time to press 2 keys
				}
				else {  // horn on the hook
					if (videoThreadStatus.isRunning) { // still on from last time
						if (now - startCall >= CALL_TIME) { // video off
							videoThreadStatus.mustStop = true;
							backLightOff();
						}
					}
					else { // nothing to do , check timeouts
						if ((serverTimeoutTimerBaseFloor > CONNECTIONTIMEOUT) || (serverTimeoutTimerFirstFloor > CONNECTIONTIMEOUT)) {
							n = sprintf(message, "Geen verbinding met\n");
							n += sprintf(&message[n], "deurpost\n\n");
							if (serverTimeoutTimerBaseFloor > CONNECTIONTIMEOUT)
								n+= sprintf(&message[n], "begane grond\n\n");
							if (serverTimeoutTimerFirstFloor > CONNECTIONTIMEOUT)
								n+= sprintf(&message[n], "eerste verdieping\n");

							switch (subStatus) {
							case 0:
								backLightHalf();
								pthread_create(&messageThreadID, NULL, &messageThread, (void *) &messageThreadStatus);
								subStatus++;
								break;
							}
						}
						else { // no timeouts
							if ( ringVolume == 0) { // show screen if bell off
								switch (subStatus) {
								case 0:
									n = sprintf(message, "Bel staat uit\n\n");
									n+= sprintf(&message[n], "Neem de hoorn van de haak\n");
									n+= sprintf(&message[n], "om de bel aan te zetten");
									backLightHalf();
									pthread_create(&messageThreadID, NULL, &messageThread, (void *) &messageThreadStatus);
									subStatus++;
									break;
								case 1:
									result = pthread_create(&audioReceiveID, NULL, &audioReceiveThread, (void *) &audioReceiveThreadStatus);
									if (result == 0) {
										printf("audioReceiveThread created successfully.\n");
									} else {
										printf("audioReceiveThread not created.\n");
										return -1;
									}
									break;
								}
							}
							else {
								if ( subStatus > 0 ){
									messageThreadStatus.mustStop = true;  // nothing to show
									backLightOff();
									subStatus = 0;
								}
							}
						}
					}
					if ((receiveDataBaseFloor.command == COMMAND_RING) || (receiveDataFirstFloor.command == COMMAND_RING)){
						if (receiveDataBaseFloor.command == COMMAND_RING) {
							receiveDataBaseFloor.command = COMMAND_NONE;
							floorID = BASE_FLOOR;
						}
						else {
							receiveDataFirstFloor.command = COMMAND_NONE;
							floorID = FIRST_FLOOR;
						}
						messageThreadStatus.mustStop = true;
						while (messageThreadStatus.isRunning)
							usleep(10000);

						startCall = now;

						if (!videoThreadStatus.isRunning)
							result = pthread_create(&videoThreadID, NULL, &videoThread, (void *) &videoThreadStatus);

						pthread_create(&ringThreadID, NULL, &ringThread, (void *) &ringThreadStatus);

						while (!videoThreadStatus.isRunning)
							usleep(10000);
						backLightOn();
						status = STATUS_RINGING;
					}
					break;

			case STATUS_RINGING:  // wait until horn gets off
				if (receiveDataBaseFloor.command == COMMAND_RING) {
					floorID = BASE_FLOOR;
					startCall = now;
					printf("ring base\n");
					receiveDataBaseFloor.command = COMMAND_NONE;
					if (!ringThreadStatus.isRunning)  // ring again if not ringing
						pthread_create(&ringThreadID, NULL, &ringThread, (void *) &ringThreadStatus);
				}
				if (receiveDataFirstFloor.command == COMMAND_RING) {
					floorID = FIRST_FLOOR;
					startCall = now;
					printf("ring first\n");
					if (!ringThreadStatus.isRunning)  // ring again if not ringing
						pthread_create(&ringThreadID, NULL, &ringThread, (void *) &ringThreadStatus);
				}


				if (keysRT & KEY_HANDSET) { // then horn is off
					if (ringThreadStatus.isRunning) {  // stop bell
						ringThreadStatus.mustStop = true;
						while (ringThreadStatus.isRunning)
							usleep(1000);
					}
					pthread_create(&audioTransmitID, NULL, &audioTransmitThread,(void *) &audioTransmitThreadStatus);
					pthread_create(&audioReceiveID, NULL, &audioReceiveThread, (void *) &audioReceiveThreadStatus);
					status = STATUS_TALKING;
				}
				if (now - startCall >= CALL_TIME) { // taken too long to answer, stop
					backLightOff();
					audioTransmitThreadStatus.mustStop = true;
					audioReceiveThreadStatus.mustStop = true;
					videoThreadStatus.mustStop = true;
					while (audioTransmitThreadStatus.isRunning)
						usleep(1000);
					while (audioReceiveThreadStatus.isRunning)
						usleep(1000);
					while (videoThreadStatus.isRunning)
						usleep(1000);

					status = STATUS_IDLE;
				}
				break;

			case STATUS_TALKING:
				if (!(keysRT & KEY_HANDSET)) { // then horn is on again
					audioTransmitThreadStatus.mustStop = true;  // stop audio leave video on
					audioReceiveThreadStatus.mustStop = true;
					status = STATUS_IDLE;
				}
				if (now - startCall >= CALL_TIME) { // taken too long to answer, stop
					backLightOff();
					status = STATUS_IDLE;
				}
				break;

			case STATUS_SETUP:
				switch (subStatus) {
				case 0:
					n = sprintf(message, "Sleutel: belvolume (%d)\n\n", (int)ringVolume);
					//	n += sprintf( &message[n] ,"2: spraakvolume\n");
					n += sprintf(&message[n], "2: test\n\n");
					//	n += sprintf(&message[n], "2+3: huisnummer (%d)", houseNo);
					if( !messageThreadStatus.isRunning )
						pthread_create(&messageThreadID, NULL, &messageThread, (void *) &messageThreadStatus);
					subStatus++;
					backLightOn();
					break;

				case 1:
					switch (keysRT) {
					case (KEY_P1 + KEY_HANDSET):
								status = STATUS_SETBELLVOLUME;
					subStatus = 0;
					break;
					case (KEY_P2 + KEY_HANDSET):
								status = STATUS_TESTMODE;
					subStatus = 0;
					break;
					case (KEY_P2 + KEY_P3 + KEY_HANDSET):
								status = STATUS_SETHOUSENO;
					subStatus = 0;
					break;
					}
					if (!(keysRT & KEY_HANDSET)) { // then horn is on again
						messageThreadStatus.mustStop = true;
						backLightOff();
						subStatus = 0;
						status = STATUS_IDLE;
					}
					break;
				}
				break;
				case STATUS_TESTMODE:
					switch (subStatus) {
					case 0:
						messageThreadStatus.mustStop = true;
						backLightOff();
						while (messageThreadStatus.isRunning)
							usleep(1000);
						pthread_create(&testModeThreadID, NULL, &testModeThread, (void *) &testModeThreadStatus);
						backLightOn();
						while( !testModeThreadStatus.isRunning)
							usleep(1000);
						subStatus++;
						startCall = now;
						break;
					case 1:
						if (((now - startCall) > 300) || (!testModeThreadStatus.isRunning)) {
							testModeThreadStatus.mustStop = true;
							backLightOff();
							while (testModeThreadStatus.isRunning)
								usleep(1000);
							subStatus = 0;
							status = STATUS_IDLE;
						}
						break;
					}
					break;

					case STATUS_SETBELLVOLUME:
						keyRepeats = KEY_P2 | KEY_P3;
						n = sprintf(message, "Instellen belvolume\n");
						n += sprintf(&message[n], "2: harder\n");
						n += sprintf(&message[n], "3: zachter\n");
						n += sprintf(&message[n], "stand: %d\n\n", (int) ringVolume);
						n += sprintf(&message[n], "plaats de hoorn terug\nom af te sluiten");

						switch (subStatus) {
						case 0:
							result = pthread_create(&ringThreadID, NULL, &ringThread, (void *) &ringThreadStatus);
							subStatus++;
							break;
						case 1:
							if (!(keysRT & KEY_HANDSET)) {
								backLightOff();
								messageThreadStatus.mustStop = true;				// ready  todo save
								ringThreadStatus.mustStop = true;
								saveSettings();
								status = STATUS_IDLE;
								while (messageThreadStatus.isRunning)
									usleep (10000);
								subStatus = 0;
							}
							if (key(KEY_P2)) {
								if (ringVolume < MAXRINGVOLUME)
									ringVolume += RINGVOLUMESTEP;
							}
							if (key(KEY_P3)) {
								if (ringVolume > MINRINGVOLUME)
									ringVolume -= RINGVOLUMESTEP;
								else
									ringVolume = 0;
							}
							break;
						}
						break;

						case STATUS_SETHOUSENO:
							keyRepeats = KEY_P2 | KEY_P3;
							n = sprintf(message, "Instellen huisnummer\n");
							n += sprintf(&message[n], "2: op\n");
							n += sprintf(&message[n], "3: neer\n\n");
							n += sprintf(&message[n], "huisnummer: %d\n", houseNo);
							n += sprintf(&message[n], "plaats de hoorn terug\nom af te sluiten");
							switch (subStatus) {
							case 0:
								subStatus++;
								key(KEY_P2);
								key(KEY_P3);
								break;
							case 1:
								if (!(keysRT & KEY_HANDSET)) {
									backLightOff();
									messageThreadStatus.mustStop = true;				// ready
									saveIPaddresss(houseNo);
									status = STATUS_IDLE;
									subStatus = 0;
								}
								if (key(KEY_P2)) {
									if (houseNo < 60)
										houseNo += 2;
								}
								if (key(KEY_P3)) {
									if (houseNo > 2)
										houseNo -= 2;
								}
								break;
							}
							break;
				}
		} // end switch status
	}  // end while 1
	// check status threads
	//checkTasksRunning();

	// end application
	videoThreadStatus.mustStop = true;
	audioTransmitThreadStatus.mustStop = true;
	usleep(1000);

	return 0;
}

